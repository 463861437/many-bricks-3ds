#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>
#define mkdir bricks_test_mkdir
using u32 = uint32_t;
enum : u32 {
    KEY_A=1u<<0, KEY_B=1u<<1, KEY_X=1u<<2, KEY_Y=1u<<3,
    KEY_START=1u<<4, KEY_SELECT=1u<<5, KEY_DLEFT=1u<<6, KEY_DRIGHT=1u<<7,
    KEY_DUP=1u<<8, KEY_DDOWN=1u<<9, KEY_L=1u<<10, KEY_R=1u<<11, KEY_TOUCH=1u<<12
};
enum APT_HookType { APTHOOK_ONSUSPEND, APTHOOK_ONSLEEP, APTHOOK_ONRESTORE, APTHOOK_ONWAKEUP };
struct aptHookCookie { void(*callback)(APT_HookType,void*)=nullptr; void* parameter=nullptr; };
struct circlePosition { int16_t dx=0,dy=0; };
struct touchPosition { uint16_t px=0,py=0; };
enum { GFX_TOP, GFX_BOTTOM, GFX_LEFT };
namespace mainMock {
struct Frame { u32 held=0,down=0; int x=0,y=0; uint64_t milliseconds=16; unsigned hooks=0; };
inline std::vector<Frame> frames;
inline int frame=-1;
inline uint64_t clock=1000;
inline aptHookCookie* hook=nullptr;
inline int gfxStarts=0,gfxStops=0,unhooks=0,swaps=0;
inline int audioStarts=0,audioStops=0,audioExits=0,saves=0;
inline bool audioLive=false;
inline Frame current() { assert(frame>=0 && frame<int(frames.size())); return frames[frame]; }
}
inline void gfxInitDefault() { ++mainMock::gfxStarts; }
inline void gfxSet3D(bool) {}
inline void gfxExit() { assert(!mainMock::audioLive); ++mainMock::gfxStops; }
inline uint8_t* gfxGetFramebuffer(int,int,void*,void*) { static uint8_t unused[1]; return unused; }
inline void gfxFlushBuffers() {}
inline void gfxSwapBuffers() { ++mainMock::swaps; }
inline void gspWaitForVBlank() {}
inline uint64_t osGetTime() { return mainMock::clock; }
inline void aptHook(aptHookCookie* cookie,void(*callback)(APT_HookType,void*),void* parameter) {
    cookie->callback=callback; cookie->parameter=parameter; mainMock::hook=cookie;
}
inline void aptUnhook(aptHookCookie* cookie) { assert(mainMock::hook==cookie); mainMock::hook=nullptr; ++mainMock::unhooks; }
inline bool aptMainLoop() {
    if(++mainMock::frame>=int(mainMock::frames.size())) return false;
    auto frame=mainMock::current(); mainMock::clock+=frame.milliseconds;
    for(unsigned hook=0;hook<4;++hook) if(frame.hooks&(1u<<hook))
        mainMock::hook->callback(static_cast<APT_HookType>(hook),mainMock::hook->parameter);
    return true;
}
inline void hidScanInput() {}
inline u32 hidKeysHeld() { return mainMock::current().held; }
inline u32 hidKeysDown() { return mainMock::current().down; }
inline void hidCircleRead(circlePosition* circle) { *circle={}; }
inline void hidTouchRead(touchPosition* touch) { auto f=mainMock::current(); *touch={uint16_t(f.x),uint16_t(f.y)}; }
