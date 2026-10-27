/*
 * PSPotify BG: temporary pause for GAME user threads only.
 *
 * Do NOT use sceKernelSuspendAllUserThreads: it has no matching tracked
 * "resume all" operation. Suspend only precisely enumerated,
 * currently-unsuspended 0x80000000 user GAME threads; retain every
 * successful UID and resume those exact UIDs upon menu close/unload.
 *
 * Risk: PSP games may delegate gameplay to kernel threads or keep queued
 * GE/audio work. This is experimental and may not pause every title.
 * A suspend failure rolls back every suspension in this attempt.
 */
#include <pspkernel.h>
#include <pspthreadman.h>
#include <pspiofilemgr.h>
#include "bg_pause.h"

#define BG_MAX_GAME_THREADS 128
/* Create this small file containing ASCII '0' to turn OFF game freezing
 * without rebuilding PRX. Absent file = feature ON in this test branch. */
#define BG_PAUSE_TOGGLE "ms0:/seplugins/pspotify_bg/pause-game.txt"

static SceUID all_ids[BG_MAX_GAME_THREADS];
static SceUID suspended_ids[BG_MAX_GAME_THREADS];
static volatile int suspended_count;
static volatile int enabled = 1;

void bg_pause_init(void)
{
    suspended_count = 0;
    enabled = 1;
    SceUID file = sceIoOpen(BG_PAUSE_TOGGLE, PSP_O_RDONLY, 0);
    if (file >= 0) {
        char flag = 0;
        int n = sceIoRead(file, &flag, 1);
        sceIoClose(file);
        if (n == 1 && flag == '0') enabled = 0;
    }
}

int bg_pause_enabled(void) { return enabled; }
int bg_pause_active(void) { return suspended_count > 0; }
int bg_pause_count(void) { return suspended_count; }

int bg_pause_resume(void)
{
    int resumed = 0;
    /* Reverse the original order and NEVER touch threads that were already
     * suspended before we opened the menu. */
    while (suspended_count > 0) {
        SceUID id = suspended_ids[--suspended_count];
        /* If a suspended game's UID became stale during shutdown, the
         * firmware returns an error. Only successful resumes are counted. */
        if (sceKernelResumeThread(id) >= 0) ++resumed;
    }
    return resumed;
}

int bg_pause_game(void)
{
    if (!enabled) return 0;
    if (suspended_count) return suspended_count;

    int listed = 0;
    int rc = sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, all_ids,
                                        BG_MAX_GAME_THREADS, &listed);
    if (rc < 0 || listed < 0 || listed > BG_MAX_GAME_THREADS)
        return -1; /* no incomplete/silent freeze */

    const SceUID current = sceKernelGetThreadId();
    int candidates = 0;
    for (int i = 0; i < listed; ++i) {
        SceUID id = all_ids[i];
        if (id == current) continue;

        SceKernelThreadInfo info;
        info.size = sizeof(info);
        if (sceKernelReferThreadStatus(id, &info) < 0) continue;

        /* Exactly PSP_THREAD_ATTR_USER, excluding all kernel workers,
         * USB/WLAN 0xA... and VSH 0xC... thread groups. */
        if ((info.attr & 0xF0000000u) != PSP_THREAD_ATTR_USER) continue;

        /* Do not resume a game thread suspended by another plugin.
         * Dormant/stopped threads are not participating in gameplay. */
        if (info.status & (PSP_THREAD_SUSPEND | PSP_THREAD_STOPPED |
                           PSP_THREAD_KILLED)) continue;
        if (++candidates > BG_MAX_GAME_THREADS) {
            bg_pause_resume();
            return -2;
        }
        if (sceKernelSuspendThread(id) < 0) {
            bg_pause_resume();
            return -3; /* revert partial pause */
        }
        suspended_ids[suspended_count++] = id;
    }
    return suspended_count; /* 0 means no user threads found */
}
