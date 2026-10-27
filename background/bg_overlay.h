#ifndef PSPOTIFY_BG_OVERLAY_H
#define PSPOTIFY_BG_OVERLAY_H

/* No graphics API calls into the running game. Own framebuffer overlay. */
int bg_overlay_start(void);
void bg_overlay_stop(void);
void bg_overlay_show(int show);
void bg_overlay_heartbeat(void); /* controls/music worker is still responding */
int bg_overlay_visible(void);
int bg_overlay_flip_hook_available(void); /* 1=display hook installed, 0=poll-only */
void bg_overlay_state(int active, int music_vol, int game_vol,
                      int audio_hooks, int paused, int row, int track);

#endif
