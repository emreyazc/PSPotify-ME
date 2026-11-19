#ifndef PSPOTIFY_BG_USER_LOADER_H
#define PSPOTIFY_BG_USER_LOADER_H
/* BG-14G: explicit opt-in canary/network user PRX lifecycle. */
void bg_user_loader_start(void (*logline)(const char *));
void bg_user_loader_stop(void (*logline)(const char *));
/* Canary-only 8s poll; network has its independent 45s timer. */
void bg_user_loader_poll(void (*logline)(const char *));
/* Join supervisor and clean any owned PRX during GAME shutdown. */
void bg_user_loader_shutdown(void (*logline)(const char *));
#endif
