#include "game.hpp"
#include "render.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
using namespace bricks;
void ppm(const std::string& path,Surface s){
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<s.width<<" "<<s.height<<"\n255\n";
    for(int y=0;y<s.height;++y)for(int x=0;x<s.width;++x){const size_t off=(x*s.height+s.height-y-1)*3;
        const char rgb[]={static_cast<char>(s.pixels[off+2]),static_cast<char>(s.pixels[off+1]),static_cast<char>(s.pixels[off])};out.write(rgb,3);}
}
int main(int argc,char** argv){
    if(argc!=3)return 2;
    const int mode=std::atoi(argv[1]);Game game;View view;
    game.profile.unlocked=18;game.profile.bestScore=38720;game.profile.levelBest[0]=18450;game.profile.levelBest[1]=22100;
    const int galleryLevels[]={0,17,38,59,80,107};
    if(mode==3||mode==6||mode>=8)game.profile.unlocked=LEVEL_COUNT;
    game.startLevel(mode>=8&&mode<14?galleryLevels[mode-8]:((mode==3||mode==6)?LEVEL_COUNT-1:0));
    if(mode==0||mode==4||mode==5||mode==6||mode==7){
        // Grant split pickups to stage a multi-ball preview; the following
        // 1.1 seconds run real physics, not a recorded player session.
        game.launch();for(int i=0;i<5;++i)game.applyPower(Power::Split);
        for(int f=0;f<132;++f)game.step(1.0f/120.0f,200+std::sin(f*0.025f)*110);
        view.touching=true;view.touchX=game.paddleX*0.8f;
    }
    // Staged results use a cleared board, and Finished belongs to the last level.
    if(mode==5||mode==6){for(Brick& brick:game.bricks)brick={};game.destroyed=game.totalBricks;}
    switch(mode){
        case 1:game.scene=Scene::Title;break;
        case 2:game.scene=Scene::Ready;break;
        case 3:game.scene=Scene::Select;view.selected=LEVEL_COUNT-1;view.page=view.selected/12;break;
        case 4:game.scene=Scene::Paused;break;
        case 5:game.scene=Scene::Cleared;break;
        case 6:game.scene=Scene::Finished;break;
        case 7:view.quitConfirm=true;break;
        default:break;
    }
    std::vector<uint8_t> a(400*240*3),b(320*240*3);Surface top{a.data(),400,240},bottom{b.data(),320,240};
    render(game,view,top,bottom);ppm(std::string(argv[2])+"-top.ppm",top);ppm(std::string(argv[2])+"-bottom.ppm",bottom);
    std::printf("Scene %d, %d balls, %d bricks remaining\n",mode,game.ballCount(),game.remainingBricks());
}
