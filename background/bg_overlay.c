/*
 * PSPotify BG in-game menu (GAME plugin).
 *
 * Old implementation wrote to the currently visible framebuffer at arbitrary
 * intervals, fighting the game's double-buffer swap and causing severe flicker.
 *
 * New primary path: draw to the *incoming* framebuffer in the game's
 * sceDisplaySetFrameBuf syscall, before passing it to the display service.
 * This keeps each flipped frame covered. For titles not issuing that syscall,
 * a vblank-paced polling fallback remains (may still flicker on some engines).
 *
 * No game renderer or saved content is modified. Unknown or RAM framebuffer
 * pointers are intentionally not touched. The glyphs are original 5x7 bitmaps.
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspsdk.h>
#include <pspthreadman.h>
#include <stdint.h>
#include "bg_overlay.h"

/* ARK-5 SystemCtrlForKernel; imported by systemctrl_stubs.S. */
extern u32 sctrlHENFindFunction(char *modname, char *libname, u32 nid);
extern void sctrlHENPatchSyscall(void *oldfunc, void *newfunc);

typedef struct { char c; uint8_t row[7]; } Glyph;
static const Glyph font[] = {
 {'A',{14,17,17,31,17,17,17}},
 {'B',{30,17,17,30,17,17,30}},
 {'C',{15,16,16,16,16,16,15}},
 {'D',{30,17,17,17,17,17,30}},
 {'E',{31,16,16,30,16,16,31}},
 {'F',{31,16,16,30,16,16,16}},
 {'G',{15,16,16,19,17,17,15}},
 {'H',{17,17,17,31,17,17,17}},
 {'I',{31,4,4,4,4,4,31}},
 {'J',{7,2,2,2,18,18,12}},
 {'K',{17,18,20,24,20,18,17}},
 {'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}},
 {'N',{17,25,21,19,17,17,17}},
 {'O',{14,17,17,17,17,17,14}},
 {'P',{30,17,17,30,16,16,16}},
 {'Q',{14,17,17,17,21,18,13}},
 {'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}},
 {'T',{31,4,4,4,4,4,4}},
 {'U',{17,17,17,17,17,17,14}},
 {'V',{17,17,17,17,17,10,4}},
 {'W',{17,17,17,21,21,21,10}},
 {'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}},
 {'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}},
 {'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}},
 {'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}},
 {'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}},
 {'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}},
 {'9',{14,17,17,15,1,1,14}},
 {' ',{0,0,0,0,0,0,0}},
 {':',{0,4,4,0,4,4,0}},
 {'/',{1,1,2,4,8,16,16}},
 {'-',{0,0,0,31,0,0,0}},
 {'+', {0,4,4,31,4,4,0}},
 {'%',{17,2,4,8,16,17,0}},
 {'.',{0,0,0,0,0,12,12}},
 {'<',{2,4,8,16,8,4,2}},
 {'>',{8,4,2,1,2,4,8}}
};

static volatile int visible, running, track_idx;
static volatile int music_active, music_volume = 70, game_volume = 100;
static volatile int game_hook, music_paused, selected;
static SceUID draw_thread = -1;
/* Hook is in the GAME syscall dispatcher: callers still receive the result
 * from the original unmodified sceDisplaySetFrameBuf implementation. */
static int (*real_set_frame_buf)(void *, int, int, int);
static volatile unsigned flip_calls;
static volatile int painting;
static volatile unsigned last_flip_us;
static volatile int hook_installed;

static uint32_t cvt32(unsigned r, unsigned g, unsigned b)
{
    /* PSP GU_COLOR_8888 is ABGR in 32-bit word layout. */
    return 0xff000000u | (b << 16) | (g << 8) | r;
}
static uint16_t cvt16(int fmt, unsigned r, unsigned g, unsigned b)
{
    if (fmt == 0) return (uint16_t)((r>>3)|((g>>2)<<5)|((b>>3)<<11));
    if (fmt == 1) return (uint16_t)(0x8000u|(r>>3)|((g>>3)<<5)|((b>>3)<<10));
    return (uint16_t)(0xf000u|(r>>4)|((g>>4)<<4)|(b>>4)<<8);
}
typedef struct { uint8_t *p; int stride, fmt; } Canvas;
/* Fast clipped rectangle writes. Computing colour conversion once per
 * rect (instead of per pixel) keeps the display-flip syscall inexpensive. */
static void rect(Canvas *f, int x, int y, int w, int h, uint32_t color)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > 480 ? 480 : x + w;
    int y1 = y + h > 272 ? 272 : y + h;
    if (x1 > f->stride) x1 = f->stride;
    if (x0 >= x1 || y0 >= y1) return;

    if (f->fmt == 3) {
        for (int py = y0; py < y1; ++py) {
            uint32_t *dst = (uint32_t *)f->p + py * f->stride + x0;
            for (int px = x0; px < x1; ++px) *dst++ = color;
        }
    } else {
        const unsigned r = color & 255u;
        const unsigned g = (color >> 8) & 255u;
        const unsigned b = (color >> 16) & 255u;
        const uint16_t value = cvt16(f->fmt, r, g, b);
        for (int py = y0; py < y1; ++py) {
            uint16_t *dst = (uint16_t *)f->p + py * f->stride + x0;
            for (int px = x0; px < x1; ++px) *dst++ = value;
        }
    }
}
static const uint8_t *rows(char c)
{
    if (c >= 'a' && c <= 'z') c -= ('a'-'A');
    for(unsigned i=0;i<sizeof(font)/sizeof(font[0]);++i)
        if(font[i].c==c) return font[i].row;
    return font[sizeof(font)/sizeof(font[0])-1].row;
}
static void txt(Canvas *f,int x,int y,const char *s,uint32_t color,int scale)
{
    for(;*s && x<450;++s,x+=6*scale){
        const uint8_t *glyph=rows(*s);
        for(int ry=0;ry<7;++ry)
            for(int rx=0;rx<5;++rx)
                if(glyph[ry]&(1u<<(4-rx)))
                    rect(f,x+rx*scale,y+ry*scale,scale,scale,color);
    }
}
static void num(char *out,int number)
{
    if(number<0)number=0;
    if(number>999)number=999;
    out[0]=(char)('0'+(number/100)%10);
    out[1]=(char)('0'+(number/10)%10);
    out[2]=(char)('0'+number%10);
    out[3]='%';out[4]=0;
}
static void draw_menu(Canvas *f)
{
    const uint32_t black=cvt32(15,21,28), panel=cvt32(28,38,47);
    const uint32_t green=cvt32(36,193,110), white=cvt32(232,240,239);
    const uint32_t muted=cvt32(162,181,190), selected_bg=cvt32(39,69,62);
    const int x=88,y=34,w=304,h=207;
    rect(f,x,y,w,h,black);
    rect(f,x+2,y+2,w-4,27,green);
    txt(f,x+14,y+8,"PSPOTIFY BG MENU",black,2);
    rect(f,x+4,y+31,w-8,h-35,panel);
    static const char* title[6]={"MUSIC","MUSIC VOL","GAME VOL","NEXT TRACK",
                                 "PREV TRACK","PAUSE / PLAY"};
    for(int r=0;r<6;++r) {
        int py=y+39+r*22;
        if(r==selected)rect(f,x+9,py-4,w-18,20,selected_bg);
        txt(f,x+16,py,title[r],r==selected?green:white,1);
        if(r==0)txt(f,x+223,py,music_active?"ON":"OFF",green,1);
        if(r==1||r==2) {
            char value[5];
            num(value,r==1?music_volume:game_volume);
            txt(f,x+228,py,value,(r==2&&!game_hook)?muted:green,1);
        }
    }
    if(!game_hook)txt(f,x+16,y+172,"GAME AUDIO HOOK UNAVAILABLE",muted,1);
    else txt(f,x+16,y+172,music_paused?"PAUSED":"PLAYING",muted,1);
    txt(f,x+16,y+186,"SELECT TRI MENU  X ACTION",muted,1);
}

/* Validate an exact candidate buffer and never paint beyond PSP eDRAM.
 * Buffers can be supplied as cached or uncached pointers; normalize both. */
static int paint_buffer(void *addr, int stride, int fmt)
{
    if (!visible || !running || !addr || stride < 480 || stride > 1024 ||
        fmt < 0 || fmt > 3) return -1;

    uintptr_t norm = ((uintptr_t)addr) & 0x0fffffffu;
    if (norm < 0x04000000u || norm >= 0x04200000u) return -1;
    uintptr_t needed = (uintptr_t)stride * 272u * (fmt == 3 ? 4u : 2u);
    if (norm + needed > 0x04200000u) return -1;

    /* PSP uncached eDRAM alias: no stale CPU cache lines from our painting. */
    Canvas c = { (uint8_t *)(norm | 0x40000000u), stride, fmt };
    draw_menu(&c);
    return 0;
}

/* Hook only paints; no file access or logging from the game's display call.
 * The original address is a kernel entry point, not a patched syscall stub,
 * so calling it directly does not recurse into this hook. */
static int bg_set_frame_buf_hook(void *top, int stride, int fmt, int sync)
{
    last_flip_us = sceKernelGetSystemTimeLow();
    ++flip_calls;
    if (visible && running && !painting) {
        painting = 1;
        paint_buffer(top, stride, fmt);
        painting = 0;
    }
    if (real_set_frame_buf)
        return real_set_frame_buf(top, stride, fmt, sync);
    return -1;
}

static int install_flip_hook(void)
{
    /* Official PSP NID for sceDisplaySetFrameBuf. ARK searches this module's
     * export table; prefer the user-facing library used by most PSP games. */
    u32 addr = sctrlHENFindFunction("sceDisplay_Service", "sceDisplay",
                                    0x289D82FEu);
    if (!addr)
        addr = sctrlHENFindFunction("sceDisplay_Service", "sceDisplay_driver",
                                    0x289D82FEu);
    if (!addr) return -1; /* Poll-only fallback, never patch unknown address. */

    real_set_frame_buf = (int (*)(void *, int, int, int))addr;
    sctrlHENPatchSyscall((void *)addr, (void *)bg_set_frame_buf_hook);
    hook_installed = 1;
    return 0;
}

/* In engines bypassing SetFrameBuf, paint the current screen on vblank.
 * This cannot ensure every game frame is covered, but stops the old two
 * arbitrary 8.5-ms writes per frame which caused visible tearing. */
static int poll_paint(void)
{
    void *addr = NULL;
    int stride = 0, fmt = -1;
    int k1 = pspSdkSetK1(0);
    int result = sceDisplayGetFrameBuf(&addr, &stride, &fmt,
                                      PSP_DISPLAY_SETBUF_IMMEDIATE);
    pspSdkSetK1(k1);
    if (result < 0) return -1;
    return paint_buffer(addr, stride, fmt);
}

static int overlay_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    while (running) {
        if (!visible) {
            sceKernelDelayThread(30000);
            continue;
        }
        /* Avoid colliding with the synchronous frame-flip draw. Prefer
         * syscall painting whenever a frame has flipped in the last 200 ms.
         * If a title bypasses the hook, the poll is synchronized to vblank. */
        const unsigned now = sceKernelGetSystemTimeLow();
        if (hook_installed && flip_calls &&
            (unsigned)(now - last_flip_us) <= 200000u) {
            sceKernelDelayThread(16000);
            continue;
        }
        sceDisplayWaitVblankStart();
        if (running && visible && !painting) {
            painting = 1;
            poll_paint();
            painting = 0;
        }
    }
    return 0;
}

int bg_overlay_start(void)
{
    running = 1;
    visible = 0;
    flip_calls = 0;
    painting = 0;
    hook_installed = 0;
    last_flip_us = 0;
    /* No hook is added if the CFW cannot find the official display export. */
    install_flip_hook();
    draw_thread = sceKernelCreateThread("bg_menu_draw", overlay_thread,
                                         0x32, 0x2000, 0, 0);
    if (draw_thread < 0) {
        running = 0;
        if (hook_installed) {
            sctrlHENPatchSyscall((void *)bg_set_frame_buf_hook,
                                  (void *)real_set_frame_buf);
            hook_installed = 0;
        }
        return -1;
    }
    if (sceKernelStartThread(draw_thread, 0, 0) < 0) {
        sceKernelDeleteThread(draw_thread);
        draw_thread = -1;
        running = 0;
        if (hook_installed) {
            sctrlHENPatchSyscall((void *)bg_set_frame_buf_hook,
                                  (void *)real_set_frame_buf);
            hook_installed = 0;
        }
        return -1;
    }
    return 0;
}

void bg_overlay_stop(void)
{
    visible = 0;
    running = 0;
    if (draw_thread >= 0) {
        SceUInt timeout = 500000;
        if (sceKernelWaitThreadEnd(draw_thread, &timeout) < 0)
            sceKernelTerminateThread(draw_thread);
        sceKernelDeleteThread(draw_thread);
        draw_thread = -1;
    }
    /* Remove the syscall replacement before unloading the PRX. Never leave
     * the game's display dispatcher pointing at freed code. */
    if (hook_installed) {
        sctrlHENPatchSyscall((void *)bg_set_frame_buf_hook,
                              (void *)real_set_frame_buf);
        hook_installed = 0;
        real_set_frame_buf = NULL;
    }
}

/* 1 = syscall renderer was installed (not proof the game calls it);
 * 0 = poll-only fallback. This is written to status.log outside hook. */
int bg_overlay_flip_hook_available(void) { return hook_installed; }

void bg_overlay_show(int show){visible=show;}
int bg_overlay_visible(void){return visible;}
void bg_overlay_state(int active,int mvol,int gvol,int hooks,int paused,int row,int track)
{
    music_active=active;music_volume=mvol;game_volume=gvol;
    game_hook=hooks;music_paused=paused;selected=row;track_idx=track;
    (void)track_idx;
}
