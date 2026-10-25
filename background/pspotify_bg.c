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
        if (wav->remaining < bytes_per_frame) {
            if (!bg_loop) break;
            if (sceIoLseek32(fd, wav->data_offset, PSP_SEEK_SET) < 0)
                break;
            wav->remaining = wav->data_bytes;
        }

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

static int bg_main_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    bg_log("BG: worker started");
    SceUID wav_fd = sceIoOpen(BG_FILE, PSP_O_RDONLY, 0);
    if (wav_fd < 0) {
        bg_log("BG: music.wav missing; game continues normally");
        return 0;
    }

    BgWav wav;
    if (bg_parse_wav(wav_fd, &wav) < 0) {
        bg_log("BG: invalid WAV; use PCM16, 44100Hz, mono/stereo");
        sceIoClose(wav_fd);
        return 0;
    }

    /* First try a spare PSP hardware PCM channel (game audio preserved). */
    int channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, BG_FRAMES,
                                    PSP_AUDIO_FORMAT_STEREO);
    int use_src = 0;
    if (channel < 0) {
        /* A separate SRC channel is a fallback; it can also be occupied. */
        if (sceAudioSRCChReserve(BG_FRAMES, BG_RATE, 2) >= 0) {
            use_src = 1;
            bg_log("BG: fallback SRC channel acquired");
        } else {
            bg_log("BG: no audio channel; game continues normally");
            sceIoClose(wav_fd);
            return 0;
        }
    } else {
        bg_log("BG: spare PCM channel acquired");
    }

    bg_log("BG: audio playback started");
    int audio_volume = PSP_AUDIO_VOLUME_MAX * bg_volume / 100;
    while (!bg_stop) {
        int count = bg_fill(wav_fd, &wav);
        if (!count || bg_stop) break;

        int result = use_src ?
           sceAudioSRCOutputBlocking(audio_volume, bg_out) :
           sceAudioOutputBlocking(channel, audio_volume, bg_out);
        if (result < 0) {
            bg_log("BG: audio output failed; stopping safely");
            break;
        }
        if (!bg_loop && wav.remaining == 0) break;
    }

    if (use_src) sceAudioSRCChRelease();
    else sceAudioChRelease(channel);
    sceIoClose(wav_fd);
    bg_log("BG: worker stopped");
    return 0;
}

int module_start(SceSize args, void *argp)
{
    (void)args; (void)argp;
    bg_stop = 0;
    bg_load_settings();
    bg_log("BG: module loaded (GAME); no hooks / no flash changes");
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
    return 0;
}
