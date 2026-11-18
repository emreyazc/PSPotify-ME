#ifndef PSPOTIFY_BG_USER_LOADER_H
#define PSPOTIFY_BG_USER_LOADER_H
/* BG-14D: explicit opt-in, canary-only user PRX load/unload. */
void bg_user_loader_start(void (*logline)(const char *));
void bg_user_loader_stop(void (*logline)(const char *));
#endif
