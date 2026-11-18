/* BG-14: optional, passive GAME network-state diagnostic.
 * No network initialization, connection requests, or disconnection.
 * Resolve only an already-loaded APCTL export; fail closed if missing.
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

void bg_net_probe_poll(void (*logline)(const char *))
{
    const unsigned now = sceKernelGetSystemTimeLow();
    if (initialized && (unsigned)(now - last_check_us) < 10000000u) return;
    last_check_us = now;

    if (!initialized) {
        initialized = 1;
        u32 fn = sctrlHENFindFunction("sceNetApctl_Library", "sceNetApctl", APCTL_GET_STATE_NID);
        if (!fn)
            fn = sctrlHENFindFunction("sceNetApctl_Library", "sceNetApctl_Library", APCTL_GET_STATE_NID);
        if (!fn) {
            logline("BGNET: APCTL export unavailable; passive probe disabled");
            return;
        }
        get_state = (ApctlGetState)fn;
        logline("BGNET: APCTL export resolved; passive state check only");
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
