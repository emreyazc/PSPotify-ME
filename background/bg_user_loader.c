/* BG-14F: explicit two-flag network worker load, or legacy canary.
 * Loader never initializes networking itself; user worker owns resources.
 * Called only by the BG worker thread, not by display/input hooks.
 */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspmodulemgr.h>
#include <string.h>
#include "bg_user_loader.h"
#include "bg_pause.h"

#define BG_CANARY_PATH "ms0:/seplugins/pspotify_bg/bg_user_canary.prx"
#define BG_NETWORK_PATH "ms0:/seplugins/pspotify_bg/pspotify_bg_net.prx"
#define BG_NETWORK_ENABLE "ms0:/seplugins/pspotify_bg/net-loader.txt"
#define BG_WORKER_ENABLE "ms0:/seplugins/pspotify_bg/net-worker-enable.txt"
#define BG_LOADER_ENABLE "ms0:/seplugins/pspotify_bg/user-loader.txt"
static SceUID canary_uid = -1;
static int canary_started;
static unsigned canary_started_us;
static int canary_auto_stop_pending;
static int loaded_network;
static volatile int supervisor_cancel;
static SceUID supervisor_thread = -1;
static SceUID stop_guard = -1;
static int supervisor_attempted;
#define BG_NETWORK_LIFETIME_US 45000000u
#define BG_CANARY_LIFETIME_US 8000000u

/* No stdio dependency in kernel GAME PRX: fixed-size hex diagnostics. */
static void bgload_log_code(void (*logline)(const char *),
                            const char *message, int value)
{
    static const char hex[] = "0123456789ABCDEF";
    char line[96];
    int n = 0;
    while (message[n] && n < 65) { line[n] = message[n]; ++n; }
    line[n++] = ' '; line[n++] = '0'; line[n++] = 'x';
    for (int shift = 28; shift >= 0; shift -= 4)
        line[n++] = hex[((unsigned int)value >> shift) & 15u];
    line[n] = '\0';
    logline(line);
}

static void (*supervisor_logger)(const char *);

/* The audio output call can block the GAME worker. The network
 * preflight owns a separate low-priority kernel timer thread instead. */
static int bg_network_supervisor(SceSize args, void *argp)
{
    (void)args; (void)argp;
    while (!supervisor_cancel) {
        const unsigned elapsed = (unsigned)(sceKernelGetSystemTimeLow() - canary_started_us);
        if (elapsed >= BG_NETWORK_LIFETIME_US && !bg_pause_active())
            break;
        sceKernelDelayThread(100000);
    }
    if (!supervisor_cancel) {
        supervisor_attempted = 1;
        supervisor_logger("BGLOAD: independent network shutdown requested");
        bg_user_loader_stop(supervisor_logger);
    }
    return 0;
}

static int flag_on(const char *path)
{
    char ch = 0;
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0) return 0;
    int n = sceIoRead(fd, &ch, 1);
    sceIoClose(fd);
    return n == 1 && ch == '1';
}

void bg_user_loader_start(void (*logline)(const char *))
{
    if (canary_uid >= 0) return;
    supervisor_logger = logline;

    const int want_canary = flag_on(BG_LOADER_ENABLE);
    const int want_network = flag_on(BG_NETWORK_ENABLE);
    /* Never load both user-mode PRXs in the same session. */
    if (want_canary && want_network) {
        logline("BGLOAD: both canary and network requested; refusing both");
        return;
    }
    if (!want_canary && !want_network) return;

    if (want_network && !flag_on(BG_WORKER_ENABLE)) {
        logline("BGLOAD: net-loader on but worker permission off; skipping");
        return;
    }

    const char *module_name = want_network ? "PSPotify BG Network" : "PSPotify BG Canary";
    const char *module_path = want_network ? BG_NETWORK_PATH : BG_CANARY_PATH;

    if (sceKernelFindModuleByName(module_name)) {
        logline("BGLOAD: requested module already loaded; skipping");
        return;
    }
    SceUID fd = sceIoOpen(module_path, PSP_O_RDONLY, 0);
    if (fd < 0) {
        logline("BGLOAD: requested PRX absent; no load attempted");
        return;
    }
    sceIoClose(fd);

    logline(want_network ? "BGLOAD: loading user network PRX" :
                           "BGLOAD: loading user-mode canary into user partition");
    SceKernelLMOption option;
    memset(&option, 0, sizeof option);
    option.size = sizeof option;
    option.mpidtext = PSP_MEMORY_PARTITION_USER;
    option.mpiddata = PSP_MEMORY_PARTITION_USER;
    SceUID uid = sceKernelLoadModule(module_path, 0, &option);
    bgload_log_code(logline, "BGLOAD: load API return", uid);
    if (uid < 0) {
        logline("BGLOAD: load failed; no further action");
        return;
    }
    canary_uid = uid;
    loaded_network = want_network;
    int status = -1;
    int started = sceKernelStartModule(uid, 0, NULL, &status, NULL);
    bgload_log_code(logline, "BGLOAD: start API return", started);
    bgload_log_code(logline, "BGLOAD: start module status", status);
    if (started < 0 || status < 0) {
        logline("BGLOAD: start failed; guarded cleanup");
        if (started >= 0) {
            int stop_status = -1;
            int stop_ret = sceKernelStopModule(uid, 0, NULL, &stop_status, NULL);
            bgload_log_code(logline, "BGLOAD: failed-start stop API", stop_ret);
            bgload_log_code(logline, "BGLOAD: failed-start stop status", stop_status);
            if (stop_ret < 0 || stop_status < 0) {
                logline("BGLOAD: failed-start PRX retained (unsafe to unload)");
                canary_started = 1;
                return;
            }
        }
        int unloaded = sceKernelUnloadModule(uid);
        bgload_log_code(logline, "BGLOAD: failed-start unload API", unloaded);
        if (unloaded >= 0) {
            canary_uid = -1;
            loaded_network = 0;
        }
        return;
    }
    canary_started = 1;
    canary_started_us = sceKernelGetSystemTimeLow();
    canary_auto_stop_pending = 1;
    if (loaded_network) {
        logline("BGLOAD: network PRX started; bounded 45s hardware preflight");
        /* Serialize explicit GAME exit and timer-driven teardown. */
        stop_guard = sceKernelCreateSema("bg_net_stop_guard", 0, 1, 1, NULL);
        if (stop_guard < 0) {
            bgload_log_code(logline, "BGLOAD: stop guard create failed", stop_guard);
            bg_user_loader_stop(logline);
            return;
        }
        supervisor_cancel = 0;
        supervisor_attempted = 0;
        supervisor_thread = sceKernelCreateThread(
            "bg_net_supervisor", bg_network_supervisor, 0x39, 0x2400, 0, NULL);
        if (supervisor_thread < 0) {
            bgload_log_code(logline, "BGLOAD: supervisor create failed", supervisor_thread);
            bg_user_loader_stop(logline);
            return;
        }
        int run_ret = sceKernelStartThread(supervisor_thread, 0, NULL);
        if (run_ret < 0) {
            bgload_log_code(logline, "BGLOAD: supervisor start failed", run_ret);
            sceKernelDeleteThread(supervisor_thread);
            supervisor_thread = -1;
            bg_user_loader_stop(logline);
            return;
        }
        logline("BGLOAD: independent network shutdown supervisor started (45s)");
    } else {
        logline("BGLOAD: user-mode canary started");
        logline("BGLOAD: scheduled canary stop/unload after 8 seconds in GAME");
    }
}

void bg_user_loader_stop(void (*logline)(const char *))
{
    if (stop_guard >= 0) {
        int ret = sceKernelWaitSema(stop_guard, 1, NULL);
        if (ret < 0) {
            if (logline) bgload_log_code(logline, "BGLOAD: stop guard wait failed", ret);
            return;
        }
    }
    if (canary_uid < 0) {
        if (stop_guard >= 0) sceKernelSignalSema(stop_guard, 1);
        return;
    }
    if (canary_started) {
        int status = -1;
        int ret = sceKernelStopModule(canary_uid, 0, NULL, &status, NULL);
        bgload_log_code(logline, "BGLOAD: stop API return", ret);
        bgload_log_code(logline, "BGLOAD: stop module status", status);
        if (ret < 0) {
            logline("BGLOAD: stop API failed; leaving module resident");
            if (stop_guard >= 0) sceKernelSignalSema(stop_guard, 1);
            return;
        }
        if (status < 0) {
            logline("BGLOAD: stop returned negative module status; leaving module resident");
            if (stop_guard >= 0) sceKernelSignalSema(stop_guard, 1);
            return;
        }
        canary_started = 0;
    }
    int unload_ret = sceKernelUnloadModule(canary_uid);
    bgload_log_code(logline, "BGLOAD: unload API return", unload_ret);
    if (unload_ret >= 0) {
        logline(loaded_network ? "BGLOAD: network PRX unloaded" :
                                 "BGLOAD: user-mode canary unloaded");
        canary_uid = -1;
        loaded_network = 0;
    } else {
        logline("BGLOAD: unload failed; leaving module resident");
    }
    if (stop_guard >= 0) sceKernelSignalSema(stop_guard, 1);
}

/* Normal GAME exit does not reliably invoke our PRX's module_stop.
 * Exercise stop/unload while the GAME worker is still alive instead.
 * Called on the existing worker thread, never on a display/input hook. */
void bg_user_loader_poll(void (*logline)(const char *))
{
    if (!canary_auto_stop_pending || canary_uid < 0 || loaded_network) return;
    const unsigned elapsed = (unsigned)(sceKernelGetSystemTimeLow() - canary_started_us);
    if (elapsed < (loaded_network ? BG_NETWORK_LIFETIME_US : BG_CANARY_LIFETIME_US)) return;
    canary_auto_stop_pending = 0; /* One attempt only; no unload retry loop. */
    logline(loaded_network ? "BGLOAD: scheduled network shutdown requested" :
                            "BGLOAD: scheduled canary shutdown requested");
    bg_user_loader_stop(logline);
}

/* Called after GAME worker shutdown and from module_stop as a failsafe.
 * The supervisor cannot outlive the kernel GAME plugin's code. */
void bg_user_loader_shutdown(void (*logline)(const char *))
{
    supervisor_cancel = 1;
    if (supervisor_thread >= 0) {
        SceUInt timeout = 4000000;
        int wait_ret = sceKernelWaitThreadEnd(supervisor_thread, &timeout);
        if (wait_ret >= 0) {
            sceKernelDeleteThread(supervisor_thread);
            supervisor_thread = -1;
        } else {
            bgload_log_code(logline, "BGLOAD: supervisor join failed", wait_ret);
            /* Do not forcibly terminate a thread during module API teardown. */
            return;
        }
    }
    if (loaded_network && !supervisor_attempted)
        logline("BGLOAD: GAME shutdown before 45s network timer");
    bg_user_loader_stop(logline);
    if (canary_uid < 0 && stop_guard >= 0) {
        sceKernelDeleteSema(stop_guard);
        stop_guard = -1;
    }
}
