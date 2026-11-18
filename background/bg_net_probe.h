#ifndef PSPOTIFY_BG_NET_PROBE_H
#define PSPOTIFY_BG_NET_PROBE_H
/* Read-only state probe. Caller enables it explicitly and owns logging. */
void bg_net_probe_poll(void (*logline)(const char *));
#endif
