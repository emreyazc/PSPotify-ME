/*
 * PSPotify BG VSH-style in-game menu: framebuffer overlay, no game hooks.
 *
 * Poll the currently displayed buffer while visible. Prefer VRAM-only
 * buffers and skip unfamiliar locations/pixel formats rather than touching
 * arbitrary RAM. Some games may overdraw this polling overlay or flicker;
 * a display-flip hook can be evaluated after device testing.
 *
 * The glyphs below are simple original 5x7 bitmap patterns.
 */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspsdk.h>
#include <pspthreadman.h>
#include <stdint.h>
#include "bg_overlay.h"

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
static void dot(Canvas *f, int x, int y, uint32_t color)
{
    if (x < 0 || x >= 480 || y < 0 || y >= 272 || x >= f->stride) return;
    if (f->fmt == 3) ((uint32_t *)f->p)[y*f->stride+x] = color;
    else {
        unsigned r = color & 255, g = (color>>8)&255, b = (color>>16)&255;
        ((uint16_t *)f->p)[y*f->stride+x] = cvt16(f->fmt,r,g,b);
    }
}
static void rect(Canvas *f,int x,int y,int w,int h,uint32_t color)
{
    for (int py=y;py<y+h;++py)
        for(int px=x;px<x+w;++px) dot(f,px,py,color);
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

static int paint(void)
{
    void *addr=0;
    int stride=0,fmt=-1;
    int k1=pspSdkSetK1(0);
    int rc=sceDisplayGetFrameBuf(&addr,&stride,&fmt,PSP_DISPLAY_SETBUF_IMMEDIATE);
    pspSdkSetK1(k1);
    if(rc<0||!addr||stride<480||stride>1024||fmt<0||fmt>3)return -1;
    /* Only touch real VRAM, no guessed or stale user RAM allocations. */
    uintptr_t top=(uintptr_t)addr;
    uintptr_t norm=top & 0x0fffffffu;
    if(norm<0x04000000u||norm>=0x04200000u)return -1;
    /* Never render outside the mapped 2 MiB EDRAM aperture, even when a
     * game reports an unusual stride or framebuffer placement. */
    uintptr_t needed=(uintptr_t)stride*272u*(fmt==3?4u:2u);
    if(norm+needed>0x04200000u)return -1;
    /* Use PSP's uncached VRAM alias to avoid stale CPU cache lines. */
    Canvas c={(uint8_t*)(norm|0x40000000u),stride,fmt};
    draw_menu(&c);
    return 0;
}
static int overlay_thread(SceSize args,void *argp)
{
    (void)args;(void)argp;
    while(running){
        if(visible){
            /* Draw twice per frame; most games flip framebuffers. */
            paint();
            sceKernelDelayThread(8500);
            if(running&&visible)paint();
            sceKernelDelayThread(8500);
        }else sceKernelDelayThread(30000);
    }
    return 0;
}
int bg_overlay_start(void)
{
    running=1;
    draw_thread=sceKernelCreateThread("bg_menu_draw",overlay_thread,0x32,0x2000,0,0);
    if(draw_thread<0){running=0;return -1;}
    if(sceKernelStartThread(draw_thread,0,0)<0){
        sceKernelDeleteThread(draw_thread);draw_thread=-1;running=0;return -1;
    }
    return 0;
}
void bg_overlay_stop(void)
{
    visible=0;running=0;
    if(draw_thread>=0){
        SceUInt timeout=500000;
        if(sceKernelWaitThreadEnd(draw_thread,&timeout)<0)
            sceKernelTerminateThread(draw_thread);
        sceKernelDeleteThread(draw_thread);
        draw_thread=-1;
    }
}
void bg_overlay_show(int show){visible=show;}
int bg_overlay_visible(void){return visible;}
void bg_overlay_state(int active,int mvol,int gvol,int hooks,int paused,int row,int track)
{
    music_active=active;music_volume=mvol;game_volume=gvol;
    game_hook=hooks;music_paused=paused;selected=row;track_idx=track;
    (void)track_idx;
}
