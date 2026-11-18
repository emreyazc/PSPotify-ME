#ifndef PSPOTIFY_BG_PCM_SOURCE_H
#define PSPOTIFY_BG_PCM_SOURCE_H
#include <psptypes.h>
#include <pspiofilemgr.h>
#include <stdint.h>

/* Stereo PCM source contract: 1024 frames per request, 44.1 kHz. */
#define BG_FRAMES 1024
#define BG_RATE 44100
typedef struct {
    int data_offset;
    unsigned int data_bytes;
    unsigned int remaining;
    int channels;
    int is_pcm16;
} BgWav;

SceUID bg_pcm_open(const char *name, BgWav *wav);
int bg_pcm_read(SceUID fd, BgWav *wav, int16_t *out, const volatile int *stop);
void bg_pcm_close(SceUID fd);
#endif
