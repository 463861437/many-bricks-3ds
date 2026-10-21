#include "game.hpp"
#include "render.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <vector>
using namespace bricks;
namespace {
constexpr size_t GUARD=64;
struct Buffer {
    std::vector<uint8_t> pixels;int width,height;
    Buffer(int w,int h):pixels(static_cast<size_t>(w)*h*3+2*GUARD,0xA9),width(w),height(h){}
    Surface surface(){return {pixels.data()+GUARD,width,height};}
    bool intact()const{return std::all_of(pixels.begin(),pixels.begin()+GUARD,[](uint8_t v){return v==0xA9;})&&std::all_of(pixels.end()-GUARD,pixels.end(),[](uint8_t v){return v==0xA9;});}
    uint32_t color(int x,int y)const{const size_t off=GUARD+(x*height+height-y-1)*3;return pixels[off+2]*65536u+pixels[off+1]*256u+pixels[off];}
};
}
int main(){
    int frames=0,checks=0;Game game;View view;
    Buffer top(400,240),bottom(320,240);
    for(int level=0;level<LEVEL_COUNT;++level){
        game.profile.unlocked=level+1;game.startLevel(level);view.selected=level;view.page=level/12;
        assert(game.level==level);++checks;
        for(Scene scene:{Scene::Title,Scene::Select,Scene::Ready,Scene::Playing,Scene::Paused,Scene::Cleared,Scene::GameOver,Scene::Finished}){
            game.scene=scene;view.touching=level%2;view.touchX=level%2?0:320;
            view.audioAvailable=level%3;view.saveFailed=level%5==0;view.quitConfirm=level%7==0;
            render(game,view,top.surface(),bottom.surface());++frames;
            assert(top.intact());assert(bottom.intact());assert(bottom.color(0,0)==0x0D1427);checks+=3;
        }
    }
    game.startLevel(0);game.launch();for(int i=0;i<10;++i)game.applyPower(Power::Split);
    for(int i=0;i<180;++i){game.step(1.0f/120,200);render(game,view,top.surface(),bottom.surface());++frames;assert(top.intact()&&bottom.intact());++checks;}
    // Native pixel layout and button visuals match the controller's hit regions.
    game.scene=Scene::Title;view={};render(game,view,top.surface(),bottom.surface());++frames;
    assert(bottom.color(22,84)==0x58E0C1);assert(bottom.color(22,134)==0x1C2941);checks+=2;
    game.scene=Scene::Ready;render(game,view,top.surface(),bottom.surface());++frames;
    assert(bottom.color(16,200)==0x58E0C1);assert(bottom.color(168,200)==0x1C2941);checks+=2;
    // The combo badge must never hide a ball crossing its screen rectangle.
    game.scene=Scene::Playing;game.combo=20;game.comboTimer=1;game.pierceTimer=0;
    for(Ball& ball:game.balls)ball.active=false;
    game.balls[0]={true,340,190,0,0,340,190};
    render(game,view,top.surface(),bottom.surface());++frames;assert(top.color(340,190)==0xE9F4F4);++checks;
    // Compare an isolated brick with the actual empty-board background. A
    // 2x2 square faces leave every adjacent gap and the border untouched.
    // Earlier wide-brick renderers would paint these pixels instead.
    Game fixture;fixture.scene=Scene::Playing;
    for(Brick& brick:fixture.bricks)brick={};
    for(Wall& wall:fixture.walls)wall={};
    for(Ball& ball:fixture.balls)ball.active=false;
    for(const auto cell:std::array<std::array<int,2>,2>{{{{7,11}},{{ROWS-1,COLS-1}}}}){
        render(fixture,view,top.surface(),bottom.surface());++frames;
        const Buffer empty=top;
        const int row=cell[0],col=cell[1];
        const int x=static_cast<int>(BRICK_X+col*BRICK_W),y=static_cast<int>(BRICK_Y+row*BRICK_H);
        fixture.bricks[row*COLS+col]={1,0};
        render(fixture,view,top.surface(),bottom.surface());++frames;
        int changed=0;
        for(int py=y-1;py<=y+7;++py)for(int px=x-1;px<=x+7;++px){
            const bool face=px>=x+1&&px<=x+2&&py>=y+1&&py<=y+2;
            const bool differs=top.color(px,py)!=empty.color(px,py);
            assert(differs==face);++checks;changed+=differs;
        }
        assert(changed==4);assert(top.intact()&&bottom.intact());checks+=2;
        fixture.bricks[row*COLS+col]={};
    }
    // Fixed rails are visible, flash on collision, and never cover ball sprites.
    fixture.walls[0]={true,100,130,60,3};
    render(fixture,view,top.surface(),bottom.surface());++frames;
    assert(top.color(100,130)==0xA1AEC6);++checks;
    fixture.wallFlash=0.1f;
    render(fixture,view,top.surface(),bottom.surface());++frames;
    assert(top.color(100,130)==0xE9F4F4);++checks;
    fixture.wallFlash=0;fixture.balls[0]={true,120,131,0,0,120,131};
    render(fixture,view,top.surface(),bottom.surface());++frames;
    assert(top.color(120,131)==0xE9F4F4);++checks;
    // Clip all rendering to unusually small surfaces; reject null/empty targets.
    for(int side:{1,7,31,120}){Buffer tiny(side,side);render(game,view,tiny.surface(),tiny.surface());++frames;assert(tiny.intact());++checks;}
    render(game,view,{nullptr,400,240},{nullptr,320,240});++frames;
    render(game,view,{nullptr,0,0},{nullptr,0,0});++frames;checks+=2;
    std::printf("Renderer: %d frames, %d assertions; native hit regions, all scenes/levels, square bricks, rails, clipping and guard buffers OK.\n",frames,checks);
}
