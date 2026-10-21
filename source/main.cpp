#include <3ds.h>
#include "game.hpp"
#include "controls.hpp"
#include "audio.hpp"
#include "save.hpp"
#include <algorithm>
#include <atomic>
#include <sys/stat.h>

namespace {
std::atomic<bool> interrupted{false};
void onApt(APT_HookType hook,void*) {
    if(hook==APTHOOK_ONSUSPEND||hook==APTHOOK_ONSLEEP||hook==APTHOOK_ONRESTORE||hook==APTHOOK_ONWAKEUP)
        interrupted.store(true);
}
unsigned keys(u32 native) {
    unsigned out=0;
    const struct {u32 native;unsigned mapped;} mapping[]={
        {KEY_A,bricks::K_A},{KEY_B,bricks::K_B},{KEY_X,bricks::K_X},{KEY_Y,bricks::K_Y},
        {KEY_START,bricks::K_START},{KEY_SELECT,bricks::K_SELECT},
        {KEY_DLEFT,bricks::K_LEFT},{KEY_DRIGHT,bricks::K_RIGHT},{KEY_DUP,bricks::K_UP},{KEY_DDOWN,bricks::K_DOWN},
        {KEY_L,bricks::K_L},{KEY_R,bricks::K_R}
    };
    for(const auto& key:mapping)if(native&key.native)out|=key.mapped;
    return out;
}
}
int main() {
    gfxInitDefault();gfxSet3D(false);
    static bricks::Game game;
    bricks::View view;bricks::Controller controller;
    mkdir("sdmc:/3ds",0777);
    bricks::SaveStore save("sdmc:/3ds/many-bricks-3ds");
    const auto loaded=save.load(game.profile);
    view.saveFailed=loaded==bricks::LoadStatus::Invalid;
    view.audioAvailable=bricks::audioInit();
    aptHookCookie hook{};aptHook(&hook,onApt,nullptr);
    uint64_t previous=osGetTime();float accumulator=0;
    bool requireRelease=false;
    constexpr float step=1.0f/120.0f;
    while(aptMainLoop()&&!controller.quit) {
        hidScanInput();const u32 held=hidKeysHeld(),down=hidKeysDown();
        const uint64_t now=osGetTime();
        float dt=std::min(0.05f,float(now-previous)/1000.0f);previous=now;
        if(interrupted.exchange(false)) {game.pause();bricks::audioStop();accumulator=0;dt=0;requireRelease=true;}
        circlePosition circle{};hidCircleRead(&circle);
        touchPosition touch{};if(held&KEY_TOUCH)hidTouchRead(&touch);
        bricks::Input input{keys(down),keys(held),(held&KEY_TOUCH)!=0,(down&KEY_TOUCH)!=0,
                            int(touch.px),int(touch.py),circle.dx/156.0f};
        // Held inputs from the previous screen cannot accept a new screen's action.
        if(requireRelease) {
            input.pressed=0;input.touchDown=false;
            if(!(held&(KEY_A|KEY_B|KEY_X|KEY_Y|KEY_START|KEY_SELECT|KEY_TOUCH)))requireRelease=false;
        }
        const auto before=game.scene;
        controller.update(game,view,input,dt);
        if(controller.quit)break;
        if(before!=game.scene){accumulator=0;requireRelease=true;}
        const auto simulationScene=game.scene;
        if(game.scene==bricks::Scene::Playing||game.scene==bricks::Scene::Ready) {
            accumulator+=dt;
            while(accumulator>=step) {game.step(step,controller.target);accumulator-=step;}
        } else accumulator=0;
        if(game.scene!=simulationScene){accumulator=0;requireRelease=true;}
        // Best scores change on each brick hit. Commit between play segments so
        // synchronous SD writes never interrupt a multi-ball rally.
        if(game.dirty&&game.scene!=bricks::Scene::Playing) {
            game.dirty=false;view.saveFailed=!save.save(game.profile);
        }
        if(game.scene==bricks::Scene::Paused)bricks::audioStop();
        else bricks::audioPlay(game.sounds,game.profile.sound);
        game.sounds=0;
        bricks::render(game,view,
            {gfxGetFramebuffer(GFX_TOP,GFX_LEFT,nullptr,nullptr),400,240},
            {gfxGetFramebuffer(GFX_BOTTOM,GFX_LEFT,nullptr,nullptr),320,240});
        gfxFlushBuffers();gfxSwapBuffers();gspWaitForVBlank();
    }
    if(game.dirty||view.saveFailed)save.save(game.profile);
    aptUnhook(&hook);bricks::audioExit();gfxExit();return 0;
}
