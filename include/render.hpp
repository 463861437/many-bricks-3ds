#pragma once
#include "game.hpp"
#include <cstdint>
namespace bricks {
struct Surface { uint8_t* pixels; int width,height; };
struct View {
    int selected=0, page=0;
    bool touching=false, audioAvailable=true, saveFailed=false, quitConfirm=false;
    float touchX=160;
};
// BGR8, column-major, bottom-to-top, matching libctru framebuffers.
void render(const Game& game,const View& view,Surface top,Surface bottom);
}
