/* BG-14D: opt-in, canary-only module lifecycle diagnostic.
 * Deliberately never loads the network worker or initializes networking.
 * Called only by the BG worker thread, not by display/input hooks.
 */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspmodulemgr.h>
#include <string.h>
#include "bg_user_loader.h"

#define BG_CANARY_PATH "ms0:/seplugins/pspotify_bg/bg_user_canary.prx"
#define BG_LOADER_ENABLE "ms0:/seplugins/pspotify_bg/user-loader.txt"
static SceUID canary_uid = -1;
static int canary_started;
static unsigned canary_started_us;
static int canary_auto_stop_pending;
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

static int user_opted_in(void)
{
    char c = 0;
    SceUID fd = sceIoOpen(BG_LOADER_ENABLE, PSP_O_RDONLY, 0);
    if (fd < 0) return 0;
    int n = sceIoRead(fd, &c, 1);
    sceIoClose(fd);
    return n == 1 && c == '1';
}

void bg_user_loader_start(void (*logline)(const char *))
{
    if (canary_uid >= 0 || !user_opted_in()) return;
    if (sceKernelFindModuleByName("PSPotify BG Canary")) {
        logline("BGLOAD: canary already loaded by another owner; skipping");
        return;
    }
    SceUID fd = sceIoOpen(BG_CANARY_PATH, PSP_O_RDONLY, 0);
    if (fd < 0) {
        logline("BGLOAD: canary PRX absent; no load attempted");
        return;
    }
    sceIoClose(fd);
    logline("BGLOAD: loading user-mode canary into user partition");
    SceKernelLMOption option;
    memset(&option, 0, sizeof option);
    option.size = sizeof option;
    option.mpidtext = PSP_MEMORY_PARTITION_USER;
    option.mpiddata = PSP_MEMORY_PARTITION_USER;
    SceUID uid = sceKernelLoadModule(BG_CANARY_PATH, 0, &option);
    if (uid < 0) {
        logline("BGLOAD: load failed; no further action");
        return;
    }
    canary_uid = uid;
    int status = -1;
    int started = sceKernelStartModule(uid, 0, NULL, &status, NULL);
    if (started < 0 || status < 0) {
        logline("BGLOAD: start failed; cleanup guarded");
        if (started >= 0) {
            /* Start returned success; the module may still be resident. */
            int stop_status = -1;
            if (sceKernelStopModule(uid, 0, NULL, &stop_status, NULL) < 0 ||
                stop_status < 0) {
                logline("BGLOAD: failed-start canary could not stop; retained");
                canary_started = 1;
                return;
            }
        }
        if (sceKernelUnloadModule(uid) >= 0) canary_uid = -1;
        return;
    }
    canary_started = 1;
    canary_started_us = sceKernelGetSystemTimeLow();
    canary_auto_stop_pending = 1;
    logline("BGLOAD: user-mode canary started");
    logline("BGLOAD: scheduled canary stop/unload after 8 seconds in GAME");
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
            logline("BGLOAD: stop API failed; leaving canary resident");
            return;
        }
        if (status < 0) {
            logline("BGLOAD: stop returned negative module status; leaving canary resident");
            return;
        }
        canary_started = 0;
    }
    int unload_ret = sceKernelUnloadModule(canary_uid);
    bgload_log_code(logline, "BGLOAD: unload API return", unload_ret);
    if (unload_ret >= 0) {
        logline("BGLOAD: user-mode canary unloaded");
        canary_uid = -1;
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
    if (elapsed < BG_CANARY_LIFETIME_US) return;
    canary_auto_stop_pending = 0; /* One attempt only; no unload retry loop. */
    logline("BGLOAD: scheduled canary shutdown requested");
    bg_user_loader_stop(logline);
}
