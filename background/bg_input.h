#ifndef PSPOTIFY_BG_INPUT_H
#define PSPOTIFY_BG_INPUT_H

/* GAME controller syscall filter. It hides menu actions from the game
 * while the PRX's own worker continues receiving real controls. */
int bg_input_init(void);
void bg_input_shutdown(void);
int bg_input_ready(void);
void bg_input_guard_release(void);

#endif
