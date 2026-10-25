#ifndef PSPOTIFY_BG_OVERLAY_H
#define PSPOTIFY_BG_OVERLAY_H

/* No graphics API calls into the running game. Own framebuffer overlay. */
int bg_overlay_start(void);
void bg_overlay_stop(void);
void bg_overlay_show(int show);
int bg_overlay_visible(void);
void bg_overlay_state(int active, int music_vol, int game_vol,
                      int audio_hooks, int paused, int row, int track);

#endif
