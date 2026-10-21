#pragma once
#include "render.hpp"
namespace bricks {
enum Key : unsigned { K_A=1,K_B=2,K_X=4,K_Y=8,K_START=16,K_SELECT=32,K_LEFT=64,K_RIGHT=128,K_UP=256,K_DOWN=512,K_L=1024,K_R=2048 };
struct Input { unsigned pressed=0,held=0; bool touching=false,touchDown=false; int x=0,y=0; float axis=0; };
struct Controller {
    float target=200;
    bool quit=false;
    // Inputs are edges; each update can perform at most one scene transition.
    void update(Game& game,View& view,const Input& input,float seconds);
};
}
