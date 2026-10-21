#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

using namespace bricks;
namespace {
constexpr float TOP = PADDLE_Y - 3.0f;
constexpr float BOTTOM = PADDLE_Y + 3.0f;
constexpr float CONTACT = TOP - BALL_RADIUS;
int assertions = 0, failures = 0, cases = 0;
const char* currentCase = "";
void check(bool condition, const char* expression, int line) {
    ++assertions;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL " << currentCase << ':' << line << ": " << expression << '\n';
    }
}
#define CHECK(value) check(bool(value), #value, __LINE__)
bool near(float a, float b, float tolerance = 0.004f) { return std::fabs(a - b) <= tolerance; }

// All fixtures use the public Game::step path, including its real substeps,
// input interpolation, power timers, ball loss and sound/particle handling.
Game field(bool easy = false, bool wide = false, bool slow = false, int level = 0) {
    Game game;
    game.profile.easy = easy;
    game.profile.unlocked = LEVEL_COUNT;
    game.startLevel(level);
    game.launch();
    for (Brick& brick : game.bricks) brick = {};
    for (Wall& wall : game.walls) wall = {};
    for (Ball& ball : game.balls) ball = {};
    // A remote brick keeps the game in Playing without affecting these paths.
    game.bricks[0] = {1, 0};
    game.totalBricks = 1;
    game.destroyed = 0;
    game.sounds = 0;
    if (wide) game.applyPower(Power::Wide);
    if (slow) game.applyPower(Power::Slow);
    game.sounds = 0;
    return game;
}
void ball(Game& game, float x, float y, float vx = 0, float vy = 126) {
    game.balls[0] = {true, x, y, vx, vy, x, y};
}
float expectedSpeed(const Game& game) {
    Game probe;
    probe.profile = game.profile;
    probe.startLevel(game.level);
    probe.launch();
    return std::hypot(probe.balls[0].vx, probe.balls[0].vy);
}
void caught(const Game& game, int index = 0) {
    const Ball& b = game.balls[index];
    CHECK(b.active);
    CHECK(b.vy < 0);
    CHECK(b.y < CONTACT);
    CHECK(std::isfinite(b.x) && std::isfinite(b.y));
    CHECK(near(std::hypot(b.vx, b.vy), expectedSpeed(game), 0.01f));
    CHECK(b.vy <= -expectedSpeed(game) * 0.42f);
    CHECK(game.sounds & S_PADDLE);
    CHECK(game.lives == 3 && game.scene == Scene::Playing);
}
void missed(const Game& game) {
    CHECK(game.balls[0].active);
    CHECK(game.balls[0].vy > 0);
    CHECK(!(game.sounds & S_PADDLE));
}

void lateOverlap() {
    for (float y : {CONTACT + 0.3f, PADDLE_Y, BOTTOM + 1.0f}) {
        Game game = field();
        ball(game, 200, y);
        game.step(1.0f / 240, 200);
        caught(game);
        CHECK(game.balls[0].idleTime < 0.001f);
        CHECK(game.score == 0 && game.destroyed == 0);
    }
}
void reportedLateDrag() {
    Game game = field();
    ball(game, 237, 212.8f, 0, 126);
    game.step(1.0f / 120, 200);
    missed(game);
    CHECK(game.balls[0].y > CONTACT);
    game.step(1.0f / 120, 206);
    caught(game);
}
void reportedOblique() {
    Game game = field();
    ball(game, 234, 212.5f, -100, 76.7f);
    game.step(1.0f / 30, 200);
    caught(game);
    CHECK(game.balls[0].vx > 0);
}
void bothSideEntries() {
    for (float side : {-1.0f, 1.0f}) {
        Game game = field();
        ball(game, 200 + side * 34, 212.5f, -side * 100, 76.7f);
        game.step(1.0f / 30, 200);
        caught(game);
        CHECK(game.balls[0].vx * side > 0);
    }
}
void sweepBetweenEndpoints() {
    for (bool slow : {false, true}) for (float direction : {-1.0f, 1.0f}) {
        Game game = field(false, false, slow);
        game.paddleX = 200 - direction * 100;
        const float target = 200 + direction * 100;
        ball(game, 200, 216);
        CHECK(std::fabs(game.paddleX - 200) > game.paddleWidth() / 2 + BALL_RADIUS);
        CHECK(std::fabs(target - 200) > game.paddleWidth() / 2 + BALL_RADIUS);
        // One physical substep: neither endpoint intersects the ball.
        game.step(1.0f / 240, target);
        caught(game);
        // Contact position, not the final touch target, defines the bounce.
        CHECK(game.balls[0].vx * direction > 100);
        CHECK(near(game.paddleX, target));
    }
}
void movingAwayMisses() {
    Game side = field();
    ball(side, 234, 212.5f, -100, 76.7f);
    side.step(1.0f / 30, 100);
    missed(side);
    Game top = field();
    ball(top, 200, 210);
    top.step(1.0f / 30, 300);
    missed(top);
    CHECK(top.balls[0].y > CONTACT);
}
void belowAndRisingMiss() {
    for (bool moving : {false, true}) {
        Game below = field();
        below.paddleX = moving ? 100 : 200;
        ball(below, 200, BOTTOM + BALL_RADIUS + 0.2f);
        below.step(1.0f / 120, moving ? 300 : 200);
        missed(below);
        Game rising = field();
        rising.paddleX = moving ? 100 : 200;
        ball(rising, 200, PADDLE_Y, 0, -126);
        rising.step(1.0f / 120, moving ? 300 : 200);
        CHECK(rising.balls[0].vy == -126);
        CHECK(!(rising.sounds & S_PADDLE));
        CHECK(near(rising.balls[0].y, PADDLE_Y - 126.0f / 120));
    }
    // A ball below the whole paddle remains lost even if the player follows it.
    Game lost = field();
    ball(lost, 200, FLOOR + BALL_RADIUS + 1);
    lost.step(1.0f / 240, 200);
    CHECK(lost.lives == 2 && lost.scene == Scene::Ready);
    CHECK(lost.sounds & S_LOST);
    CHECK(!(lost.sounds & S_PADDLE));
}
void powerWidthsAndSpeed() {
    for (bool easy : {false, true}) for (bool wide : {false, true}) {
        Game edge = field(easy, wide);
        const float half = edge.paddleWidth() / 2;
        ball(edge, 200 + half + BALL_RADIUS - 0.1f, PADDLE_Y);
        edge.step(1.0f / 240, 200);
        caught(edge);
        Game outside = field(easy, wide);
        ball(outside, 200 + half + BALL_RADIUS + 0.2f, PADDLE_Y);
        outside.step(1.0f / 240, 200);
        missed(outside);
    }
    // The extended width actually changes a would-be miss into a catch.
    for (int mode = 0; mode < 3; ++mode) {
        Game game = field(mode == 1, mode == 2);
        ball(game, 237, PADDLE_Y);
        game.step(1.0f / 240, 200);
        if (mode == 0) missed(game); else caught(game);
    }
    for (bool easy : {false, true}) {
        Game last = field(easy, false, true, LEVEL_COUNT - 1);
        ball(last, 200, CONTACT + 0.2f);
        last.step(1.0f / 240, 200);
        caught(last);
        CHECK(expectedSpeed(last) <= (easy ? 132.51f : 157.51f));
    }
}
void slowTiming() {
    Game normal = field(), slow = field(false, false, true);
    ball(normal, 200, 212.7f);
    ball(slow, 200, 212.7f);
    normal.step(1.0f / 240, 200);
    slow.step(1.0f / 240, 200);
    caught(normal);
    missed(slow);
    slow.step(1.0f / 240, 200);
    caught(slow);
    Game travel = field(false, false, true);
    ball(travel, 280, 180);
    travel.step(1.0f / 60, 300);
    CHECK(near(travel.paddleX, 300));
    CHECK(near(travel.balls[0].y, 180 + 126.0f / 60 * 0.65f));
}
void aimAndBounds() {
    for (float offset : {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f}) {
        Game game = field();
        ball(game, 200 + offset * game.paddleWidth() / 2, 212);
        game.step(1.0f / 60, 200);
        caught(game);
        if (offset != 0) CHECK(game.balls[0].vx * offset > 0);
        else CHECK(std::fabs(game.balls[0].vx) < 10);
    }
    for (bool wide : {false, true}) for (bool right : {false, true}) {
        Game game = field(false, wide);
        const float half = game.paddleWidth() / 2;
        game.paddleX = right ? RIGHT - half : LEFT + half;
        ball(game, right ? RIGHT - BALL_RADIUS : LEFT + BALL_RADIUS, 214);
        game.step(1.0f / 60, right ? 1000 : -1000);
        caught(game);
        CHECK(near(game.paddleX, right ? RIGHT - half : LEFT + half));
        CHECK(game.balls[0].x >= LEFT + BALL_RADIUS);
        CHECK(game.balls[0].x <= RIGHT - BALL_RADIUS);
    }
}
void frameSubdivision() {
    for (bool slow : {false, true}) {
        Game one = field(false, false, slow);
        one.paddleX = 140;
        ball(one, 200, 216);
        Game four = one, eight = one;
        one.step(1.0f / 30, 260);
        for (int i = 1; i <= 4; ++i) four.step(1.0f / 120, 140 + 120.0f * i / 4);
        for (int i = 1; i <= 8; ++i) eight.step(1.0f / 240, 140 + 120.0f * i / 8);
        caught(one); caught(four); caught(eight);
        for (const Game* partitioned : {&four, &eight}) {
            CHECK(near(one.balls[0].x, partitioned->balls[0].x));
            CHECK(near(one.balls[0].y, partitioned->balls[0].y));
            CHECK(near(one.balls[0].vx, partitioned->balls[0].vx));
            CHECK(near(one.balls[0].vy, partitioned->balls[0].vy));
            CHECK(near(one.paddleX, partitioned->paddleX));
        }
    }
}
void allBalls() {
    Game game = field();
    for (int i = 0; i < MAX_BALLS; ++i) {
        const float x = 170.0f + (i % 31) * 2;
        const float y = CONTACT + 0.1f + (i % 8);
        game.balls[i] = {true, x, y, float(i % 3 - 1) * 25, 126, x, y};
    }
    game.step(1.0f / 240, 200);
    CHECK(game.ballCount() == MAX_BALLS);
    for (int i = 0; i < MAX_BALLS; ++i) caught(game, i);
    CHECK(game.score == 0 && game.destroyed == 0 && game.lives == 3);
}
struct Case { const char* name; void (*run)(); };
const Case tests[] = {
    {"late-overlap", lateOverlap},
    {"reported-late-drag", reportedLateDrag},
    {"reported-oblique", reportedOblique},
    {"both-side-entries", bothSideEntries},
    {"sweep-between-endpoints", sweepBetweenEndpoints},
    {"moving-away-misses", movingAwayMisses},
    {"below-and-rising-miss", belowAndRisingMiss},
    {"power-widths-and-speed", powerWidthsAndSpeed},
    {"slow-timing", slowTiming},
    {"aim-and-bounds", aimAndBounds},
    {"frame-subdivision", frameSubdivision},
    {"all-256-balls", allBalls},
};
}
int main(int argc, char** argv) {
    if (argc > 2) { std::cerr << "Usage: test-paddle [case-name]\n"; return 2; }
    for (const Case& test : tests) {
        if (argc == 2 && std::string(argv[1]) != test.name) continue;
        currentCase = test.name;
        ++cases;
        test.run();
    }
    if (cases == 0) { std::cerr << "Unknown paddle test case\n"; return 2; }
    std::cout << "Paddle regression: " << cases << " scenarios, " << assertions
              << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
