/* BG-14B: safe GAME-network preflight. Does not load or initialize modules.
 * Probe is optional and performs no connection/disconnection or socket work.
 */
#include <pspkernel.h>
#include <stdint.h>
#include "bg_net_probe.h"

extern u32 sctrlHENFindFunction(char *modname, char *libname, u32 nid);
#define APCTL_GET_STATE_NID 0x5DEAC81Bu
#define APCTL_GOT_IP 4
typedef int (*ApctlGetState)(int *);

static ApctlGetState get_state;
static int initialized;
static int last_state = -999;
static int last_result = -999;
static unsigned last_check_us;

/* These names come from the PSP network-module inventory; individual
 * firmware/CFW builds may export different module names. */
static void log_module(const char *name, const char *present,
                       const char *absent, void (*logline)(const char *))
{
    if (sceKernelFindModuleByName(name)) logline(present);
    else logline(absent);
}

static ApctlGetState find_apctl(void)
{
    static const struct {
        const char *mod;
        const char *lib;
    } candidates[] = {
        { "sceNetApctl_Library", "sceNetApctl" },
        { "sceNetApctl_Library", "sceNetApctl_Library" },
        { "sceNetApctl", "sceNetApctl" },
        { "sceNetApctl", "sceNetApctl_Library" },
        { "sceNetApctl_Driver", "sceNetApctl" },
        { "sceNetApctl_Driver", "sceNetApctl_Library" }
    };
    for (unsigned i = 0; i < sizeof(candidates)/sizeof(candidates[0]); ++i) {
        u32 fn = sctrlHENFindFunction((char *)candidates[i].mod,
                                    (char *)candidates[i].lib,
                                    APCTL_GET_STATE_NID);
        if (fn) return (ApctlGetState)fn;
    }
    return (ApctlGetState)0;
}

void bg_net_probe_poll(void (*logline)(const char *))
{
    const unsigned now = sceKernelGetSystemTimeLow();
    if (initialized && (unsigned)(now - last_check_us) < 10000000u) return;
    last_check_us = now;

    if (!initialized) {
        initialized = 1;
        logline("BGNET: preflight begins; no network modules will be loaded");
        log_module("sceNet_Library",
                   "BGNET: sceNet module PRESENT", "BGNET: sceNet module ABSENT", logline);
        log_module("sceNetInet_Library",
                   "BGNET: sceNetInet module PRESENT", "BGNET: sceNetInet module ABSENT", logline);
        log_module("sceNetApctl_Library",
                   "BGNET: sceNetApctl module PRESENT", "BGNET: sceNetApctl module ABSENT", logline);
        log_module("sceNetResolver_Library",
                   "BGNET: sceNetResolver module PRESENT", "BGNET: sceNetResolver module ABSENT", logline);
        log_module("sceNet_Service",
                   "BGNET: sceNet_Service module PRESENT", "BGNET: sceNet_Service module ABSENT", logline);

        get_state = find_apctl();
        if (!get_state) {
            logline("BGNET: APCTL export unresolved; user-mode network bootstrap required");
            return;
        }
        logline("BGNET: APCTL export resolved; reading existing state only");
    }
    if (!get_state) return;

    int state = -1;
    int result = get_state(&state);
    if (result == last_result && (result < 0 || state == last_state)) return;
    last_result = result;
    last_state = state;
    if (result < 0) logline("BGNET: APCTL GetState returned error");
    else if (state == APCTL_GOT_IP) logline("BGNET: APCTL state GOT_IP (existing connection)");
    else logline("BGNET: APCTL state not connected");
}
