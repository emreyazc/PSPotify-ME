/*
 * PSPotify BG - in-game control capture.
 *
 * Kernel GAME PRX installs user syscall wrappers for standard controller
 * Read/Peek Positive/Negative and the two Latch APIs. When the BG menu
 * is showing, games receive a NEUTRAL controller report. The PRX's own
 * worker reads the real report, so navigating our menu still works.
 *
 * The SELECT+TRIANGLE open chord is also hidden while the menu is
 * initially closed. Once closed, held buttons are suppressed until ALL
 * controls have been released, preventing e.g. CIRCLE from closing the
 * menu and also accidentally acting on the game.
 *
 * This is a voluntary GAME-only, reversible ARK syscall patch.
 * Do not install in VSH or POPS. Other plugins that patch the same
 * syscall addresses can conflict; restore only our own mapping.
 */
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspsdk.h>
#include <string.h>
#include "bg_input.h"
#include "bg_overlay.h"
#include "bg_audio.h"

/* Imported using background/systemctrl_stubs.S (ARK-5). */
extern u32 sctrlHENFindFunction(char *modname, char *libname, u32 nid);
extern void sctrlHENPatchSyscall(void *oldfunc, void *newfunc);

#define INPUT_FUNCS 6
static void *originals[INPUT_FUNCS];
static void *replacements[INPUT_FUNCS];
static int present[INPUT_FUNCS];
static int hooked_count;
static volatile int until_released;

typedef int (*ReadFn)(SceCtrlData *, int);
typedef int (*LatchFn)(SceCtrlLatch *);

/* When an end-of-menu button is still down, mask it in subsequent
 * frames until every button has been let go. */
void bg_input_guard_release(void) { until_released = 1; }

static int should_hide(u32 pressed)
{
    if (bg_overlay_visible()) {
        until_released = 1;
        return 1;
    }

    const u32 open_keys = PSP_CTRL_SELECT | PSP_CTRL_TRIANGLE;
    if ((pressed & open_keys) == open_keys) {
        until_released = 1;
        return 1;
    }

    if (until_released) {
        if (pressed == 0) until_released = 0;
        else return 1;
    }
    return 0;
}

/* Only valid buffers and sane counts returned by the firmware are edited.
 * No modification to the controller's internal global state. */
static void filter_samples(SceCtrlData *pads, int got, int requested, int neg)
{
    if (!pads || got <= 0 || requested <= 0 ||
        bg_music_is_own_thread()) return;

    int limit = got < requested ? got : requested;
    if (limit > 64) limit = 64;
    if (limit <= 0) return;

    /* The newest event drives visibility / release state; change every
     * returned element to avoid buffered stale button events in games. */
    u32 pressed = neg ? ~pads[limit - 1].Buttons : pads[limit - 1].Buttons;
    if (!should_hide(pressed)) return;

    int oldk1 = pspSdkSetK1(0);
    for (int i = 0; i < limit; ++i) {
        pads[i].Buttons = neg ? ~0u : 0u;
        pads[i].Lx = 128;
        pads[i].Ly = 128;
    }
    pspSdkSetK1(oldk1);
}

static int read_pos(SceCtrlData *p, int n)
{
    int result = ((ReadFn)originals[0])(p, n);
    filter_samples(p, result, n, 0);
    return result;
}
static int peek_pos(SceCtrlData *p, int n)
{
    int result = ((ReadFn)originals[1])(p, n);
    filter_samples(p, result, n, 0);
    return result;
}
static int read_neg(SceCtrlData *p, int n)
{
    int result = ((ReadFn)originals[2])(p, n);
    filter_samples(p, result, n, 1);
    return result;
}
static int peek_neg(SceCtrlData *p, int n)
{
    int result = ((ReadFn)originals[3])(p, n);
    filter_samples(p, result, n, 1);
    return result;
}

static void filter_latch(SceCtrlLatch *p, int result)
{
    if (!p || result < 0 || bg_music_is_own_thread()) return;
    /* A latch does not expose the current button state. The worker's
     * release guard was established by a normal controller read.
     * Suppress all press/break transitions while menu is open. */
    if (bg_overlay_visible() || until_released) {
        int oldk1 = pspSdkSetK1(0);
        memset(p, 0, sizeof(*p));
        pspSdkSetK1(oldk1);
    }
}
static int peek_latch(SceCtrlLatch *p)
{
    int result = ((LatchFn)originals[4])(p);
    filter_latch(p, result);
    return result;
}
static int read_latch(SceCtrlLatch *p)
{
    int result = ((LatchFn)originals[5])(p);
    filter_latch(p, result);
    return result;
}

/* PSP user library NIDs, as documented by pspsdk / PPSSPP. */
static const u32 nids[INPUT_FUNCS] = {
    0x1F803938u, /* sceCtrlReadBufferPositive */
    0x3A622550u, /* sceCtrlPeekBufferPositive */
    0x60B81F86u, /* sceCtrlReadBufferNegative */
    0xC152080Au, /* sceCtrlPeekBufferNegative */
    0xB1D0E5CDu, /* sceCtrlPeekLatch */
    0x0B588501u  /* sceCtrlReadLatch */
};

int bg_input_init(void)
{
    replacements[0] = (void *)read_pos;
    replacements[1] = (void *)peek_pos;
    replacements[2] = (void *)read_neg;
    replacements[3] = (void *)peek_neg;
    replacements[4] = (void *)peek_latch;
    replacements[5] = (void *)read_latch;
    hooked_count = 0;
    until_released = 0;

    for (int i = 0; i < INPUT_FUNCS; ++i) {
        /* Missing exports are not patched. In particular, never
         * hook a guessed address in the controller kernel module. */
        u32 addr = sctrlHENFindFunction("sceController_Service", "sceCtrl",
                                        nids[i]);
        if (!addr)
            addr = sctrlHENFindFunction("sceController_Service",
                                        "sceCtrl_driver", nids[i]);
        if (!addr) {
            present[i] = 0;
            continue;
        }
        originals[i] = (void *)addr;
        sctrlHENPatchSyscall((void *)addr, replacements[i]);
        present[i] = 1;
        ++hooked_count;
    }
    /* At least Peek Positive and Read Positive need to be hooked,
     * otherwise the current game's input path may be unsupported.
     * Partial hooks are kept for coverage but reported unverified. */
    return bg_input_ready() ? 0 : -1;
}

int bg_input_ready(void)
{
    return present[0] && present[1];
}

void bg_input_shutdown(void)
{
    until_released = 0;
    for (int i = 0; i < INPUT_FUNCS; ++i) {
        if (present[i]) {
            sctrlHENPatchSyscall(replacements[i], originals[i]);
            present[i] = 0;
            originals[i] = 0;
        }
    }
    hooked_count = 0;
}
