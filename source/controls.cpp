#include "controls.hpp"
#include <algorithm>
#include <cmath>
namespace bricks {
namespace {
constexpr int PAGE_SIZE=12;
constexpr int PAGE_COUNT=(LEVEL_COUNT+PAGE_SIZE-1)/PAGE_SIZE;
bool tap(const Input& i,int x,int y,int w,int h) {
    return i.touchDown && i.x>=x && i.x<x+w && i.y>=y && i.y<y+h;
}
void select(Game& g,View& v) {
    g.scene=Scene::Select;
    v.selected=std::clamp(g.level,0,std::clamp(g.profile.unlocked,1,LEVEL_COUNT)-1);
    v.page=v.selected/PAGE_SIZE;
}
}
void Controller::update(Game& g,View& v,const Input& i,float dt) {
    v.touching=i.touching && i.y>=54 && i.y<174 && i.x>=12 && i.x<308;
    v.touchX=static_cast<float>(std::clamp(i.x,0,319));
    if(v.quitConfirm) {
        if((i.pressed&K_A)||tap(i,20,116,280,38)) quit=true;
        else if((i.pressed&(K_B|K_SELECT))||tap(i,20,164,280,36)) v.quitConfirm=false;
        return;
    }
    if(i.pressed&K_SELECT) { g.pause();v.quitConfirm=true;return; }
    if(g.scene==Scene::Title) {
        if((i.pressed&(K_A|K_START))||tap(i,20,74,280,42)) {
            g.startLevel(g.profile.unlocked-1);target=g.paddleX;return;
        }
        if((i.pressed&K_B)||tap(i,20,124,280,36)) { select(g,v);return; }
        if((i.pressed&K_X)||tap(i,20,170,134,28)) {g.profile.sound=!g.profile.sound;g.dirty=true;}
        if((i.pressed&K_Y)||tap(i,166,170,134,28)) {g.profile.easy=!g.profile.easy;g.dirty=true;}
        return;
    }
    if(g.scene==Scene::Select) {
        v.selected=std::clamp(v.selected,0,LEVEL_COUNT-1);
        v.page=std::clamp(v.page,0,PAGE_COUNT-1);
        if((i.pressed&K_B)||tap(i,112,181,96,28)) {g.returnToTitle();return;}
        int move=0;
        if(i.pressed&K_LEFT)move=-1;
        if(i.pressed&K_RIGHT)move=1;
        if(i.pressed&K_UP)move=-4;
        if(i.pressed&K_DOWN)move=4;
        if(move) {v.selected=std::clamp(v.selected+move,0,LEVEL_COUNT-1);v.page=v.selected/PAGE_SIZE;}
        if((i.pressed&K_L)||tap(i,14,181,90,28)) {v.page=std::max(0,v.page-1);v.selected=v.page*PAGE_SIZE;}
        if((i.pressed&K_R)||tap(i,216,181,90,28)) {v.page=std::min(PAGE_COUNT-1,v.page+1);v.selected=v.page*PAGE_SIZE;}
        bool chosen=(i.pressed&K_A)!=0;
        for(int n=0;n<PAGE_SIZE;++n) if(tap(i,14+(n%4)*74,46+(n/4)*43,68,36)) {
            const int level=v.page*PAGE_SIZE+n;
            if(level<LEVEL_COUNT) {v.selected=level;chosen=true;}
            break;
        }
        if(chosen&&v.selected>=0&&v.selected<LEVEL_COUNT&&v.selected<g.profile.unlocked) {g.startLevel(v.selected);target=g.paddleX;}
        return;
    }
    if(g.scene==Scene::Playing||g.scene==Scene::Ready) {
        if((i.pressed&(K_B|K_START))||tap(i,164,190,144,34)||
           (g.scene==Scene::Playing&&tap(i,12,190,144,34))) {g.pause();return;}
        dt=std::isfinite(dt)?std::clamp(dt,0.0f,0.05f):0;
        if(v.touching) target=LEFT+(i.x-12)*(RIGHT-LEFT)/296.0f;
        else {
            float axis=std::isfinite(i.axis)?std::clamp(i.axis,-1.0f,1.0f):0;
            if(std::abs(axis)<0.12f) axis=0;
            if(i.held&K_LEFT)axis=-1;
            if(i.held&K_RIGHT)axis=1;
            target+=axis*360*dt;
        }
        target=std::clamp(target,LEFT+g.paddleWidth()/2,RIGHT-g.paddleWidth()/2);
        if(g.scene==Scene::Ready&&((i.pressed&K_A)||tap(i,12,190,144,34)))g.launch();
        return;
    }
    const bool primary=(i.pressed&(K_A|K_START))||tap(i,20,116,280,38);
    if((i.pressed&K_B)||tap(i,20,164,280,36)) {select(g,v);return;}
    if(!primary)return;
    if(g.scene==Scene::Paused)g.resume();
    else if(g.scene==Scene::Cleared) {g.startLevel(g.level+1);target=g.paddleX;}
    else if(g.scene==Scene::GameOver) {g.startLevel(g.level);target=g.paddleX;}
    else if(g.scene==Scene::Finished)g.returnToTitle();
}
}
