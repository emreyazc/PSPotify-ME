/* BG-14D: minimal user-mode module canary. No network, threads or memory alloc. */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <string.h>

PSP_MODULE_INFO("PSPotify BG Canary", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);
#define CANARY_LOG "ms0:/seplugins/pspotify_bg/user-canary.log"

static void marker(const char *message)
{
    SceUID fd = sceIoOpen(CANARY_LOG, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd < 0) return;
    sceIoWrite(fd, message, (int)strlen(message));
    sceIoWrite(fd, "\n", 1);
    sceIoClose(fd);
}

int module_start(SceSize args, void *argp)
{
    (void)args; (void)argp;
    marker("BGCANARY: user-mode module started");
    return 0;
}

int module_stop(SceSize args, void *argp)
{
    (void)args; (void)argp;
    marker("BGCANARY: user-mode module stopped");
    return 0;
}
