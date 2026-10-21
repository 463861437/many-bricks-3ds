#pragma once
#include <array>
#include <cstdint>

namespace bricks {
constexpr int LEVEL_COUNT = 108;
constexpr int COLS = 128, ROWS = 26, MAX_BALLS = 256, MAX_DROPS = 32, MAX_PARTICLES = 192;
constexpr int MAX_WALLS = 8;
constexpr float LEFT = 8, RIGHT = 392, CEILING = 31, FLOOR = 241;
// Keep the dense wall in the upper half of the landscape screen. The lower
// half is travel space, giving players time to track a returning ball.
constexpr float BRICK_X = 8, BRICK_Y = 38, BRICK_W = 3, BRICK_H = 3;
constexpr float PADDLE_Y = 218, PADDLE_HALF_HEIGHT = 3, BALL_RADIUS = 1.8f;
enum class Scene { Title, Select, Ready, Playing, Paused, Cleared, GameOver, Finished };
enum class Power { Split, Wide, Pierce, Slow };
enum Sound : unsigned { S_NONE=0, S_BRICK=1, S_PADDLE=2, S_POWER=4, S_LOST=8, S_WIN=16, S_LAUNCH=32 };
struct Brick { uint8_t hp=0, color=0; };
struct Wall { bool active=false; float x=0,y=0,w=0,h=0; };
struct Ball { bool active=false; float x=0,y=0,vx=0,vy=0,previousX=0,previousY=0,idleTime=0,steerCooldown=0; };
struct Drop { bool active=false; float x=0,y=0; Power kind=Power::Split; };
struct Particle { float x=0,y=0,vx=0,vy=0,life=0; uint8_t color=0; };
struct Profile {
    uint32_t bestScore=0;
    std::array<uint32_t,LEVEL_COUNT> levelBest{};
    int unlocked=1;
    bool sound=true, easy=false;
};
struct Game {
    Scene scene=Scene::Title;
    Profile profile;
    std::array<Brick,COLS*ROWS> bricks{};
    std::array<Wall,MAX_WALLS> walls{};
    std::array<Ball,MAX_BALLS> balls{};
    std::array<Drop,MAX_DROPS> drops{};
    std::array<Particle,MAX_PARTICLES> particles{};
    int level=0, lives=3, totalBricks=0, destroyed=0, combo=0, maxCombo=0;
    uint32_t score=0, rng=1;
    float paddleX=200, wideTimer=0, pierceTimer=0, slowTimer=0, comboTimer=0, elapsed=0;
    float wallFlash=0, comboWallBudget=0;
    unsigned sounds=0;
    bool dirty=false;
    Game();
    void startLevel(int index);
    void launch();
    void step(float seconds,float targetX);
    void pause();
    void resume();
    void returnToTitle();
    void applyPower(Power power);
    int remainingBricks() const;
    int ballCount() const;
    float paddleWidth() const;
};
const char* levelName(int index);
const char* powerName(Power power);
}
