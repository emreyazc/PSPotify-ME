#ifndef PSPOTIFY_BG_PAUSE_H
#define PSPOTIFY_BG_PAUSE_H

/* Reversible, experimental GAME user-thread suspension.
 * Never freezes kernel threads (PSPotify worker, overlay, WLAN/USB drivers). */
void bg_pause_init(void);
int bg_pause_game(void);
int bg_pause_resume(void);
int bg_pause_active(void);
int bg_pause_enabled(void);
int bg_pause_count(void);

#endif
