/* BG-14F: explicit two-flag network worker load, or legacy canary.
 * Loader never initializes networking itself; user worker owns resources.
 * Called only by the BG worker thread, not by display/input hooks.
 */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspmodulemgr.h>
#include <string.h>
#include "bg_user_loader.h"

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
        logline("BGLOAD: scheduled network stop/unload after 45 seconds in GAME");
    } else {
        logline("BGLOAD: user-mode canary started");
        logline("BGLOAD: scheduled canary stop/unload after 8 seconds in GAME");
    }
}

void bg_user_loader_stop(void (*logline)(const char *))
{
    if (canary_uid < 0) return;
    if (canary_started) {
        int status = -1;
        int ret = sceKernelStopModule(canary_uid, 0, NULL, &status, NULL);
        bgload_log_code(logline, "BGLOAD: stop API return", ret);
        bgload_log_code(logline, "BGLOAD: stop module status", status);
        if (ret < 0) {
            logline("BGLOAD: stop API failed; leaving module resident");
            return;
        }
        if (status < 0) {
            logline("BGLOAD: stop returned negative module status; leaving module resident");
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
}

/* Normal GAME exit does not reliably invoke our PRX's module_stop.
 * Exercise stop/unload while the GAME worker is still alive instead.
 * Called on the existing worker thread, never on a display/input hook. */
void bg_user_loader_poll(void (*logline)(const char *))
{
    if (!canary_auto_stop_pending || canary_uid < 0) return;
    const unsigned elapsed = (unsigned)(sceKernelGetSystemTimeLow() - canary_started_us);
    if (elapsed < (loaded_network ? BG_NETWORK_LIFETIME_US : BG_CANARY_LIFETIME_US)) return;
    canary_auto_stop_pending = 0; /* One attempt only; no unload retry loop. */
    logline(loaded_network ? "BGLOAD: scheduled network shutdown requested" :
                            "BGLOAD: scheduled canary shutdown requested");
    bg_user_loader_stop(logline);
}
