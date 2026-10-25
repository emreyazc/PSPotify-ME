/*
 * EXPERIMENTAL GAME AUDIO GAIN via CFW syscall hooks.
 * Only adjusts the game's requested hardware output volume.
 * Own PSPotify BG audio worker bypasses the gain; original driver calls
 * are preserved and called directly (no recursion).
 *
 * Based on public PSP audio syscall interfaces / ARK exports. No source
 * from other plugins has been copied.
 * Hooks remain OFF if no applicable driver functions can be found.
 */
#include <pspkernel.h>
#include <pspaudio.h>
#include <pspthreadman.h>
#include <stdint.h>
#include "bg_audio.h"

/* u32 is already defined by the PSP SDK's psptypes.h. */
extern u32 sctrlHENFindFunction(char *modname, char *libname, u32 nid);
extern void sctrlHENPatchSyscall(void *oldfunc, void *newfunc);

static volatile int game_volume = 100;
static int hooked = 0;
static void *originals[6];
static void *handlers[6];

static int scaled(int volume)
{
    if (bg_music_is_own_thread() || game_volume >= 100) return volume;
    if (volume <= 0) return volume;
    return (int)(((long long)volume * game_volume) / 100);
}

static int out(int ch, int vol, void *buf)
{
    int (*fn)(int,int,void*) = (void*)originals[0];
    return fn(ch, scaled(vol), buf);
}
static int out_block(int ch, int vol, void *buf)
{
    int (*fn)(int,int,void*) = (void*)originals[1];
    return fn(ch, scaled(vol), buf);
}
static int out_pan(int ch, int left, int right, void *buf)
{
    int (*fn)(int,int,int,void*) = (void*)originals[2];
    return fn(ch, scaled(left), scaled(right), buf);
}
static int out_pan_block(int ch, int left, int right, void *buf)
{
    int (*fn)(int,int,int,void*) = (void*)originals[3];
    return fn(ch, scaled(left), scaled(right), buf);
}
static int out_src(int vol, void *buf)
{
    int (*fn)(int,void*) = (void*)originals[4];
    return fn(scaled(vol), buf);
}
static int out2(int vol, void *buf)
{
    int (*fn)(int,void*) = (void*)originals[5];
    return fn(scaled(vol), buf);
}

static const u32 nids[6] = {
    0x8C1009B2u, /* sceAudioOutput */
    0x136CAF51u, /* sceAudioOutputBlocking */
    0xE2D56B2Du, /* sceAudioOutputPanned */
    0x13F592BCu, /* sceAudioOutputPannedBlocking */
    0xE0727056u, /* sceAudioSRCOutputBlocking */
    0x2D53F36Eu  /* sceAudioOutput2OutputBlocking */
};

void bg_game_audio_init(void)
{
    handlers[0] = (void*)out;
    handlers[1] = (void*)out_block;
    handlers[2] = (void*)out_pan;
    handlers[3] = (void*)out_pan_block;
    handlers[4] = (void*)out_src;
    handlers[5] = (void*)out2;
    for (int i = 0; i < 6; ++i) {
        /* Find only known original audio driver entry points; absent
         * NIDs are skipped rather than patched at a guessed address. */
        u32 addr = sctrlHENFindFunction("sceAudio_Driver", "sceAudio", nids[i]);
        if (!addr) continue;
        originals[i] = (void*)addr;
        sctrlHENPatchSyscall((void*)addr, handlers[i]);
        ++hooked;
    }
}

void bg_game_audio_shutdown(void)
{
    /* Restore precisely our replacements; never patch a guessed address. */
    for (int i = 0; i < 6; ++i)
        if (originals[i]) {
            sctrlHENPatchSyscall(handlers[i], originals[i]);
            originals[i] = 0;
        }
    hooked = 0;
}
int bg_game_audio_available(void) { return hooked > 0; }
int bg_game_audio_volume(void) { return game_volume; }
void bg_game_audio_set_volume(int value)
{
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    game_volume = value;
}
