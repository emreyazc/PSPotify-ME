/* PSPotify BG-14: user-mode infrastructure networking companion.
 * Deliberately NOT loaded by GAME plugin until module lifecycle is validated.
 * No Spotify login, credential access, sockets or GAME hooks here.
 */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <pspnet_apctl.h>
#include <pspnet_resolver.h>
#include <psputility_netmodules.h>
#include <psputility_netparam.h>
#include <pspwlan.h>
#include <string.h>

PSP_MODULE_INFO("PSPotify BG Network", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

#define LOG_PATH "ms0:/seplugins/pspotify_bg/net-worker.log"
#define ENABLE_PATH "ms0:/seplugins/pspotify_bg/net-worker-enable.txt"
#define PROFILE_TIMEOUT_STEPS 150
#define WAIT_STEP_US 100000
#define NET_ALREADY_LOADED ((int)0x80110801u)
#define NET_ALREADY_INITIALIZED ((int)0x80410201u)

static volatile int stop_requested;
static SceUID worker_id = -1;
static int own_common, own_inet_module, own_net, own_inet, own_resolver, own_apctl;
static int own_connection;

static void log_text(const char *text)
{
    SceUID fd = sceIoOpen(LOG_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd < 0) return;
    sceIoWrite(fd, text, (int)strlen(text));
    sceIoWrite(fd, "\n", 1);
    sceIoClose(fd);
}

static void log_error(const char *prefix, int code)
{
    static const char digits[] = "0123456789ABCDEF";
    char buffer[96];
    int i = 0;
    while (prefix[i] && i < 65) { buffer[i] = prefix[i]; ++i; }
    buffer[i++] = ' ';
    buffer[i++] = '0'; buffer[i++] = 'x';
    for (int shift = 28; shift >= 0; shift -= 4)
        buffer[i++] = digits[((unsigned int)code >> shift) & 15u];
    buffer[i] = '\0';
    log_text(buffer);
}

static int enabled(void)
{
    char ch = 0;
    SceUID fd = sceIoOpen(ENABLE_PATH, PSP_O_RDONLY, 0);
    if (fd < 0) return 0;
    int n = sceIoRead(fd, &ch, 1);
    sceIoClose(fd);
    return n == 1 && ch == '1';
}

/* This companion is the only owner of anything it successfully creates.
 * Inherited/previously initialized services are never terminated. */
static int initialize_network(void)
{
    int ret = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
    if (ret == 0) own_common = 1;
    else if (ret != NET_ALREADY_LOADED) { log_error("BGNET: common load failed", ret); return -1; }

    ret = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
    if (ret == 0) own_inet_module = 1;
    else if (ret != NET_ALREADY_LOADED) { log_error("BGNET: inet module load failed", ret); return -1; }

    ret = sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
    if (ret == 0) own_net = 1;
    else if (ret != NET_ALREADY_INITIALIZED) { log_error("BGNET: net init failed", ret); return -1; }

    ret = sceNetInetInit();
    if (ret == 0) own_inet = 1;
    else if (ret != NET_ALREADY_INITIALIZED) { log_error("BGNET: inet init failed", ret); return -1; }

    ret = sceNetResolverInit();
    if (ret == 0) own_resolver = 1;
    else if (ret != NET_ALREADY_INITIALIZED) { log_error("BGNET: resolver init failed", ret); return -1; }

    ret = sceNetApctlInit(0x1800, 0x30);
    if (ret == 0) own_apctl = 1;
    else if (ret != NET_ALREADY_INITIALIZED) { log_error("BGNET: apctl init failed", ret); return -1; }

    log_text("BGNET: user-mode network services initialized");
    return 0;
}

static void cleanup_network(void)
{
    /* Do not disconnect an existing connection owned by the game. */
    if (own_connection) {
        sceNetApctlDisconnect();
        own_connection = 0;
    }
    if (own_apctl) { sceNetApctlTerm(); own_apctl = 0; }
    if (own_resolver) { sceNetResolverTerm(); own_resolver = 0; }
    if (own_inet) { sceNetInetTerm(); own_inet = 0; }
    if (own_net) { sceNetTerm(); own_net = 0; }
    if (own_inet_module) {
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
        own_inet_module = 0;
    }
    if (own_common) {
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
        own_common = 0;
    }
}

static int connect_saved_profile(void)
{
    int state = 0;
    if (sceNetApctlGetState(&state) == 0 &&
        state == PSP_NET_APCTL_STATE_GOT_IP) {
        log_text("BGNET: pre-existing connection GOT_IP; no new connection requested");
        return 0;
    }
    if (!sceWlanGetSwitchState()) {
        log_text("BGNET: WLAN switch OFF");
        return -1;
    }

    int found = 0;
    for (int profile = 1; profile <= 10 && !stop_requested; ++profile) {
        if (sceUtilityCheckNetParam(profile) != 0) continue;
        ++found;
        int ret = sceNetApctlConnect(profile);
        if (ret < 0) {
            log_error("BGNET: profile connect request failed", ret);
            continue;
        }
        /* The connection request is asynchronous; GOT_IP is success. */
        own_connection = 1;
        for (int i = 0; i < PROFILE_TIMEOUT_STEPS && !stop_requested; ++i) {
            state = 0;
            ret = sceNetApctlGetState(&state);
            if (ret < 0) {
                log_error("BGNET: profile state read failed", ret);
                break;
            }
            if (state == PSP_NET_APCTL_STATE_GOT_IP) {
                log_text("BGNET: profile connected and GOT_IP");
                return 0;
            }
            sceKernelDelayThread(WAIT_STEP_US);
        }
        sceNetApctlDisconnect();
        own_connection = 0;
        log_text("BGNET: profile timed out or failed; trying next");
        sceKernelDelayThread(250000);
    }
    if (!found) log_text("BGNET: no saved infrastructure profiles");
    else log_text("BGNET: no profile acquired GOT_IP");
    return -1;
}

static int network_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    log_text("BGNET: user-mode network companion started");
    if (initialize_network() == 0 && !stop_requested) {
        if (connect_saved_profile() == 0 && !stop_requested) {
            log_text("BGNET: connectivity preflight finished (no Spotify traffic)");
            /* Keep our connection while loaded; don't drop it immediately. */
            while (!stop_requested) sceKernelDelayThread(100000);
        }
    }
    cleanup_network();
    log_text("BGNET: companion stopped and owned resources released");
    return 0;
}

int module_start(SceSize args, void *argp)
{
    (void)args; (void)argp;
    if (!enabled()) return 0;
    stop_requested = 0;
    worker_id = sceKernelCreateThread("pspotify_bg_net", network_thread,
                                      0x32, 0x10000, PSP_THREAD_ATTR_USER, NULL);
    if (worker_id < 0) {
        log_error("BGNET: create user worker failed", worker_id);
        return 0;
    }
    if (sceKernelStartThread(worker_id, 0, NULL) < 0) {
        sceKernelDeleteThread(worker_id);
        worker_id = -1;
    }
    return 0;
}

int module_stop(SceSize args, void *argp)
{
    (void)args; (void)argp;
    stop_requested = 1;
    if (worker_id >= 0) {
        SceUInt timeout = 3000000;
        if (sceKernelWaitThreadEnd(worker_id, &timeout) == 0) {
            sceKernelDeleteThread(worker_id);
        } else {
            /* Never force-kill a thread during network-module teardown. */
            log_text("BGNET: shutdown wait timed out; worker remains active");
            return -1;
        }
        worker_id = -1;
    }
    return 0;
}
