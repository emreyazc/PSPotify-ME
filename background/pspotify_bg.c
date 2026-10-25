/*
 * PSPotify BG / experimental in-game WAV player
 *
 * Separate kernel PRX: it does not call into the PSPotify EBOOT, modify
 * flash or hook the game's functions. No Spotify credentials are read.
 *
 * Source is original for this repository; references to older PSP game
 * music plugins informed feasibility, not copied implementation.
 */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspaudio.h>
#include <pspctrl.h>
#include <pspsdk.h>
#include "bg_audio.h"
#include "bg_overlay.h"
#include <pspthreadman.h>
#include <pspsdk.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

PSP_MODULE_INFO("PSPotify BG", 0x1000, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

#define BG_FILE  "ms0:/seplugins/pspotify_bg/music.wav"
#define BG_CONF  "ms0:/seplugins/pspotify_bg/config.txt"
#define BG_LOG   "ms0:/seplugins/pspotify_bg/status.log"
#define BG_ENABLE "ms0:/seplugins/pspotify_bg/enabled.txt"
#define BG_MIXER "ms0:/seplugins/pspotify_bg/mixer.txt"

/* Numbered WAV slots: next/previous shortcuts cycle through existing files.
 * The old single music.wav remains a backwards-compatible fallback. */
static const char *const bg_names[] = {
    "ms0:/seplugins/pspotify_bg/music01.wav",
    "ms0:/seplugins/pspotify_bg/music02.wav",
    "ms0:/seplugins/pspotify_bg/music03.wav",
    "ms0:/seplugins/pspotify_bg/music04.wav",
    "ms0:/seplugins/pspotify_bg/music05.wav",
    "ms0:/seplugins/pspotify_bg/music06.wav",
    "ms0:/seplugins/pspotify_bg/music07.wav",
    "ms0:/seplugins/pspotify_bg/music08.wav",
    "ms0:/seplugins/pspotify_bg/music.wav"
};
#define BG_MAX_TRACKS 9
static unsigned char bg_tracks[BG_MAX_TRACKS];
static int bg_track_count;
#define BG_FRAMES 1024
#define BG_RATE   44100
#define BG_PCM_BYTES (BG_FRAMES * 4)

typedef struct {
    int data_offset;
    unsigned int data_bytes;
    unsigned int remaining;
    int channels;
    int is_pcm16;
} BgWav;

static volatile int bg_stop;
static SceUID bg_thread = -1;
static int bg_volume = 70;
static int bg_loop = 1;
/* Global buffers avoid a large thread stack and other game threads' memory. */
static int16_t bg_out[BG_FRAMES * 2] __attribute__((aligned(64)));
static uint8_t bg_raw[BG_PCM_BYTES] __attribute__((aligned(64)));

static void bg_log(const char *message)
{
    SceUID fd = sceIoOpen(BG_LOG, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd < 0) return;
    sceIoWrite(fd, message, (int)strlen(message));
    sceIoWrite(fd, "\n", 1);
    sceIoClose(fd);
}

static unsigned int bg_le16(const uint8_t *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int bg_le32(const uint8_t *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

/* Deliberately limited to 16-bit, uncompressed, 44.1kHz mono/stereo WAV:
 * these can use the PSP's 44.1kHz PCM channels without a resampler/decoder. */
static int bg_parse_wav(SceUID fd, BgWav *wav)
{
    uint8_t head[12], chunk[8], fmt[16];
    int seen_format = 0, seen_data = 0;
    memset(wav, 0, sizeof *wav);

    if (sceIoRead(fd, head, sizeof head) != (int)sizeof head ||
        memcmp(head, "RIFF", 4) || memcmp(head + 8, "WAVE", 4))
        return -1;

    for (int i = 0; i < 64; ++i) {
        if (sceIoRead(fd, chunk, sizeof chunk) != (int)sizeof chunk) break;
        unsigned int size = bg_le32(chunk + 4);
        int start = sceIoLseek32(fd, 0, PSP_SEEK_CUR);
        if (start < 0 || size > 0x7ffffff0u ||
            (unsigned int)start + size + (size & 1u) > 0x7fffffffu)
            return -1;

        if (memcmp(chunk, "fmt ", 4) == 0 && size >= sizeof fmt) {
            if (sceIoRead(fd, fmt, sizeof fmt) != (int)sizeof fmt) return -1;
            wav->channels = (int)bg_le16(fmt + 2);
            wav->is_pcm16 = (bg_le16(fmt) == 1 &&
                bg_le32(fmt + 4) == BG_RATE &&
                bg_le16(fmt + 14) == 16 &&
                bg_le16(fmt + 12) == (unsigned)(wav->channels * 2));
            seen_format = 1;
        } else if (memcmp(chunk, "data", 4) == 0 && !seen_data) {
            wav->data_offset = start;
            wav->data_bytes = size;
            seen_data = 1;
        }

        if (sceIoLseek32(fd, start + (int)(size + (size & 1u)), PSP_SEEK_SET) < 0)
            return -1;
        if (seen_format && seen_data) break;
    }

    if (!seen_format || !seen_data || !wav->is_pcm16 ||
        (wav->channels != 1 && wav->channels != 2) ||
        wav->data_bytes < (unsigned)(wav->channels * 2) ||
        wav->data_bytes % (unsigned)(wav->channels * 2) != 0)
        return -1;

    wav->remaining = wav->data_bytes;
    return sceIoLseek32(fd, wav->data_offset, PSP_SEEK_SET) < 0 ? -1 : 0;
}

/* Read/loop 1024 stereo frames. Small sequential file reads, no heap use.
 * The buffer is zero-filled so stopping/EOF cannot leak stale samples. */
static int bg_fill(SceUID fd, BgWav *wav)
{
    int filled = 0;
    const unsigned int bytes_per_frame = (unsigned int)wav->channels * 2u;
    memset(bg_out, 0, sizeof bg_out);

    while (filled < BG_FRAMES && !bg_stop) {
        if (wav->remaining < bytes_per_frame)
            break; /* End of current track: outer loop advances playlist. */

        unsigned int take = (unsigned int)(BG_FRAMES - filled) * bytes_per_frame;
        if (take > wav->remaining) take = wav->remaining;
        int actual = sceIoRead(fd, bg_raw, (int)take);
        if (actual <= 0) break;
        int frames = actual / (int)bytes_per_frame;
        if (frames <= 0) break;
        wav->remaining -= (unsigned int)frames * bytes_per_frame;

        for (int i = 0; i < frames; ++i) {
            uint8_t *p = bg_raw + (unsigned int)i * bytes_per_frame;
            int16_t left = (int16_t)bg_le16(p);
            int16_t right = wav->channels == 2 ?
                            (int16_t)bg_le16(p + 2) : left;
            bg_out[(filled + i) * 2] = left;
            bg_out[(filled + i) * 2 + 1] = right;
        }
        filled += frames;
        if (actual < (int)take) break; /* I/O interruption; never spin */
    }
    return filled;
}

static int bg_parse_int(const char *s, int lo, int hi, int def)
{
    int n = 0, digits = 0;
    while (*s >= '0' && *s <= '9' && digits < 4) {
        n = n * 10 + *s++ - '0';
        ++digits;
    }
    if (!digits || (*s && *s != '\r' && *s != '\n')) return def;
    return n >= lo && n <= hi ? n : def;
}

static void bg_load_settings(void)
{
    char cfg[256];
    SceUID fd = sceIoOpen(BG_CONF, PSP_O_RDONLY, 0);
    if (fd < 0) return; /* safe defaults */
    int n = sceIoRead(fd, cfg, sizeof cfg - 1);
    sceIoClose(fd);
    if (n <= 0) return;
    cfg[n] = '\0';
    char *line = cfg;
    while (*line) {
        char *end = line;
        while (*end && *end != '\n') ++end;
        char saved = *end;
        *end = '\0';
        if (strncmp(line, "music_volume=", 13) == 0)
            bg_volume = bg_parse_int(line + 13, 0, 100, bg_volume);
        else if (strncmp(line, "loop=", 5) == 0)
            bg_loop = bg_parse_int(line + 5, 0, 1, bg_loop);
        if (!saved) break;
        line = end + 1;
    }
}

/* Independent of the app's settings.ini. Absent/invalid file = OFF.
 * This prevents the plugin from playing merely because it is in PLUGINS.TXT. */
static int bg_enabled(void)
{
    char b = 0;
    SceUID fd = sceIoOpen(BG_ENABLE, PSP_O_RDONLY, 0);
    if (fd < 0) return 0;
    int read = sceIoRead(fd, &b, 1);
    sceIoClose(fd);
    return read == 1 && b == '1';
}

static void bg_scan_tracks(void)
{
    bg_track_count = 0;
    for (int i = 0; i < BG_MAX_TRACKS - 1; ++i) {
        SceUID fd = sceIoOpen(bg_names[i], PSP_O_RDONLY, 0);
        if (fd >= 0) {
            sceIoClose(fd);
            bg_tracks[bg_track_count++] = (unsigned char)i;
        }
    }
    if (!bg_track_count) {
        SceUID fd = sceIoOpen(bg_names[BG_MAX_TRACKS - 1], PSP_O_RDONLY, 0);
        if (fd >= 0) {
            sceIoClose(fd);
            bg_tracks[0] = BG_MAX_TRACKS - 1;
            bg_track_count = 1;
        }
    }
}

static SceUID bg_open_track(int at, BgWav *wav)
{
    if (at < 0 || at >= bg_track_count) return -1;
    const char *name = bg_names[bg_tracks[at]];
    SceUID fd = sceIoOpen(name, PSP_O_RDONLY, 0);
    if (fd < 0) return -1;
    if (bg_parse_wav(fd, wav) < 0) {
        bg_log("BG: invalid playlist WAV; PCM16 44100 Hz required");
        sceIoClose(fd);
        return -1;
    }
    bg_log("BG: track opened");
    return fd;
}

/* GAME PRX worker owns every audio request. Only its output is exempt
 * from the CFW game-volume hook; other game audio remains independently
 * adjustable. */
int bg_music_is_own_thread(void)
{
    return bg_thread >= 0 && sceKernelGetThreadId() == bg_thread;
}

static void bg_write_enabled(int on)
{
    SceUID fd = sceIoOpen(BG_ENABLE, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) { bg_log("BG: cannot persist on/off"); return; }
    char c = on ? '1' : '0';
    sceIoWrite(fd, &c, 1);
    sceIoClose(fd);
}
static void bg_load_mixer(void)
{
    char buf[128];
    SceUID fd = sceIoOpen(BG_MIXER, PSP_O_RDONLY, 0);
    if (fd < 0) return;
    int n = sceIoRead(fd, buf, sizeof buf - 1);
    sceIoClose(fd);
    if (n < 0) return;
    buf[n] = '\0';
    char *line = buf;
    while (*line) {
        char *end = line;
        while (*end && *end != '\n') ++end;
        char save = *end;
        *end = '\0';
        if (!strncmp(line, "music_volume=", 13))
            bg_volume = bg_parse_int(line + 13, 0, 100, bg_volume);
        else if (!strncmp(line, "game_volume=", 12))
            bg_game_audio_set_volume(bg_parse_int(line + 12, 0, 100, 100));
        if (!save) break;
        line = end + 1;
    }
}
static void bg_save_mixer(void)
{
    char out[64],num[4];
    int mv = bg_volume, gv = bg_game_audio_volume();
    SceUID fd = sceIoOpen(BG_MIXER, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) return;
    /* Small decimal encoder; no dependence on stdio formatters. */
    int n = 0;
    const char *prefix = "music_volume=";
    while (*prefix) out[n++] = *prefix++;
    num[0] = (char)('0'+mv/100);
    num[1] = (char)('0'+(mv/10)%10);
    num[2] = (char)('0'+mv%10);
    for (int i=0;i<3;++i)out[n++]=num[i];
    out[n++]='\n';
    prefix = "game_volume=";
    while (*prefix) out[n++]=*prefix++;
    num[0] = (char)('0'+gv/100);
    num[1] = (char)('0'+(gv/10)%10);
    num[2] = (char)('0'+gv%10);
    for (int i=0;i<3;++i)out[n++]=num[i];
    out[n++]='\n';
    sceIoWrite(fd, out, n);
    sceIoClose(fd);
}

/* Low-collision hotkeys. SELECT+TRIANGLE opens the in-game menu.
 * Selecting UP/DOWN and LEFT/RIGHT still reaches the game; on-screen
 * menus are deliberately not using an invasive controller API hook.
 *
 * Returns 1=next, -1=previous, 2=pause, 3=toggle enabled.
 */
static int bg_controls(int *enabled, int *paused, int *row, int *at)
{
    static unsigned old;
    SceCtrlData pad;
    int k1 = pspSdkSetK1(0);
    int read = sceCtrlPeekBufferPositive(&pad, 1);
    pspSdkSetK1(k1);
    if (read < 1) return 0;
    const unsigned btn = pad.Buttons;
    const unsigned pressed = btn & ~old;
    old = btn;

    if ((btn & (PSP_CTRL_SELECT|PSP_CTRL_TRIANGLE)) ==
               (PSP_CTRL_SELECT|PSP_CTRL_TRIANGLE) &&
        (pressed & (PSP_CTRL_SELECT|PSP_CTRL_TRIANGLE))) {
        bg_overlay_show(!bg_overlay_visible());
        return 0;
    }
    if (bg_overlay_visible()) {
        if (pressed & PSP_CTRL_CIRCLE) {
            bg_overlay_show(0);
            return 0;
        }
        if (pressed & PSP_CTRL_UP) *row = (*row + 5) % 6;
        if (pressed & PSP_CTRL_DOWN) *row = (*row + 1) % 6;
        int dir = (pressed & PSP_CTRL_RIGHT) ? 1 :
                  (pressed & PSP_CTRL_LEFT) ? -1 : 0;
        const int action = (pressed & PSP_CTRL_CROSS) != 0;
        switch (*row) {
        case 0: /* MUSIC switch */
            if (dir || action) {
                *enabled = !*enabled;
                bg_write_enabled(*enabled);
                bg_log(*enabled ? "BG: menu music ON" : "BG: menu music OFF");
                if (*enabled) *paused = 0;
                return 3;
            }
            break;
        case 1: /* MUSIC VOLUME */
            if (dir) {
                int v = bg_volume + dir * 10;
                bg_volume = v < 0 ? 0 : v > 100 ? 100 : v;
                bg_save_mixer();
            }
            break;
        case 2: /* GAME VOLUME */
            if (dir && bg_game_audio_available()) {
                int v = bg_game_audio_volume() + dir * 10;
                bg_game_audio_set_volume(v);
                bg_save_mixer();
            }
            break;
        case 3: if (action || dir > 0) return 1; break;
        case 4: if (action || dir < 0) return -1; break;
        case 5:
            if (action) {
                *paused = !*paused;
                return 2;
            }
            break;
        }
        return 0;
    }

    if ((btn & PSP_CTRL_SELECT) && (pressed & PSP_CTRL_RTRIGGER)) return 1;
    if ((btn & PSP_CTRL_SELECT) && (pressed & PSP_CTRL_LTRIGGER)) return -1;
    if ((btn & PSP_CTRL_SELECT) && (pressed & PSP_CTRL_SQUARE)) {
        *paused = !*paused;
        return 2;
    }
    (void)at;
    return 0;
}

static int bg_main_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    bg_log("BG: worker started");
    bg_log("BG: waiting 4 seconds for game startup");
    for (int i = 0; i < 40 && !bg_stop; ++i)
        sceKernelDelayThread(100000);
    if (bg_stop) return 0;

    if (sceKernelFindModuleByName("PSPotify ME") != NULL) {
        bg_log("BG: PSPotify app detected, no game menu/audio");
        return 0;
    }

    bg_load_mixer();
    bg_game_audio_init();
    bg_log(bg_game_audio_available() ? "BG: game audio hooks active" :
                                      "BG: game audio hooks unavailable");
    if (bg_overlay_start() < 0) bg_log("BG: overlay draw thread failed");

    bg_scan_tracks();
    int enabled = bg_enabled(),paused = 0,row = 0,at = 0,invalid = 0;
    int channel = -1,use_src = 0;
    SceUID wav_fd = -1;
    BgWav wav;
    memset(&wav,0,sizeof wav);
    bg_log("BG: SELECT+TRIANGLE menu; no Spotify streaming yet");

    while (!bg_stop) {
        const int command = bg_controls(&enabled,&paused,&row,&at);
        bg_overlay_state(enabled,bg_volume,bg_game_audio_volume(),
                         bg_game_audio_available(),paused,row,at);

        if (command == 1 || command == -1) {
            if (wav_fd >= 0) sceIoClose(wav_fd);
            wav_fd = -1;
            if (bg_track_count)
                at=(at + (command==1 ? 1 : bg_track_count-1))%bg_track_count;
            invalid = 0;
            bg_log(command==1?"BG: next track":"BG: previous track");
        } else if (command == 2) {
            bg_log(paused?"BG: paused":"BG: resumed");
        }

        if (!enabled) {
            if (wav_fd >= 0) { sceIoClose(wav_fd); wav_fd=-1; }
            if (channel >= 0) { sceAudioChRelease(channel); channel=-1; }
            if (use_src) { sceAudioSRCChRelease(); use_src=0; }
            sceKernelDelayThread(25000);
            continue;
        }
        if (paused || !bg_track_count) {
            sceKernelDelayThread(25000);
            continue;
        }
        if (channel < 0 && !use_src) {
            channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,BG_FRAMES,
                                        PSP_AUDIO_FORMAT_STEREO);
            if (channel < 0) {
                if (sceAudioSRCChReserve(BG_FRAMES,BG_RATE,2) >= 0) {
                    use_src=1;
                    bg_log("BG: SRC output reserved");
                } else {
                    bg_log("BG: no available audio channel");
                    sceKernelDelayThread(500000);
                    continue;
                }
            } else bg_log("BG: spare PCM channel reserved");
        }
        if (wav_fd < 0) {
            wav_fd=bg_open_track(at,&wav);
            if (wav_fd < 0) {
                if (++invalid >= bg_track_count) {
                    bg_log("BG: no valid WAV in playlist");
                    enabled=0;
                    bg_write_enabled(0);
                } else at=(at+1)%bg_track_count;
                sceKernelDelayThread(30000);
                continue;
            }
            invalid=0;
            bg_log("BG: audio playback started");
        }
        int frames=bg_fill(wav_fd,&wav);
        if (bg_stop) break;
        if (frames > 0) {
            int volume = PSP_AUDIO_VOLUME_MAX * bg_volume / 100;
            int result = use_src ?
               sceAudioSRCOutputBlocking(volume,bg_out) :
               sceAudioOutputBlocking(channel,volume,bg_out);
            if (result < 0) {
                bg_log("BG: audio output failed");
                enabled=0; bg_write_enabled(0);
                continue;
            }
        }
        if (wav.remaining < (unsigned)(wav.channels*2) || !frames) {
            sceIoClose(wav_fd);
            wav_fd=-1;
            if (!bg_loop && at==bg_track_count-1) {
                enabled=0; bg_write_enabled(0);
            } else at=(at+1)%bg_track_count;
        }
    }

    bg_overlay_stop(); /* join before GAME tears framebuffer down */
    if (wav_fd >= 0) sceIoClose(wav_fd);
    if (channel >= 0) sceAudioChRelease(channel);
    if (use_src) sceAudioSRCChRelease();
    bg_game_audio_shutdown();
    bg_log("BG: worker stopped");
    return 0;
}

int module_start(SceSize args, void *argp)
{
    (void)args; (void)argp;
    bg_stop = 0;
    bg_load_settings();
    bg_log("BG: module loaded (GAME); framebuffer menu and audio volume hooks experimental");
    bg_thread = sceKernelCreateThread("pspotify_bg_audio", bg_main_thread,
                                       0x28, 0x3000, 0, NULL);
    if (bg_thread < 0) {
        bg_log("BG: cannot create thread");
        return 0; /* fail open: game still starts */
    }
    if (sceKernelStartThread(bg_thread, 0, NULL) < 0) {
        bg_log("BG: cannot start thread");
        sceKernelDeleteThread(bg_thread);
        bg_thread = -1;
    }
    return 0;
}

int module_stop(SceSize args, void *argp)
{
    (void)args; (void)argp;
    bg_stop = 1;
    if (bg_thread >= 0) {
        /* Give the blocking audio call time to finish normally. */
        SceUInt timeout = 300000;
        if (sceKernelWaitThreadEnd(bg_thread, &timeout) < 0)
            sceKernelTerminateThread(bg_thread);
        sceKernelDeleteThread(bg_thread);
        bg_thread = -1;
    }
    /* Failsafe after an interrupted audio thread: no stale draw worker
     * or patched audio syscall may survive the plugin unload. */
    bg_overlay_stop();
    bg_game_audio_shutdown();
    return 0;
}
