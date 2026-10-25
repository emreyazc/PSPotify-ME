#ifndef PSPOTIFY_BG_AUDIO_H
#define PSPOTIFY_BG_AUDIO_H

/* Per-game output gain: 0 .. 100%. Hooked independently of BG music. */
void bg_game_audio_init(void);
void bg_game_audio_shutdown(void);
int bg_game_audio_available(void);
int bg_game_audio_volume(void);
void bg_game_audio_set_volume(int value);
int bg_music_is_own_thread(void); /* implemented in pspotify_bg.c */

#endif
