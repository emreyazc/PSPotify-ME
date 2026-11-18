/* BG-13: WAV-to-PCM source, isolated from the GAME audio output sink. */
#include "bg_pcm_source.h"
#include <string.h>
#define BG_PCM_BYTES (BG_FRAMES * 4)
static uint8_t bg_raw[BG_PCM_BYTES] __attribute__((aligned(64)));

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


SceUID bg_pcm_open(const char *name, BgWav *wav)
{
    SceUID fd = sceIoOpen(name, PSP_O_RDONLY, 0);
    if (fd < 0) return -1;
    if (bg_parse_wav(fd, wav) < 0) {
        sceIoClose(fd);
        return -1;
    }
    return fd;
}

/* Read/loop 1024 stereo frames. Small sequential file reads, no heap use.
 * The buffer is zero-filled so stopping/EOF cannot leak stale samples. */
int bg_pcm_read(SceUID fd, BgWav *wav, int16_t *out, const volatile int *stop)
{
    int filled = 0;
    const unsigned int bytes_per_frame = (unsigned int)wav->channels * 2u;
    memset(out, 0, BG_FRAMES * 2 * sizeof(*out));

    while (filled < BG_FRAMES && !*stop) {
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
            out[(filled + i) * 2] = left;
            out[(filled + i) * 2 + 1] = right;
        }
        filled += frames;
        if (actual < (int)take) break; /* I/O interruption; never spin */
    }
    return filled;
}


void bg_pcm_close(SceUID fd)
{
    if (fd >= 0) sceIoClose(fd);
}
