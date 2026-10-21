#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace bricks {
namespace {
constexpr float PI = 3.14159265358979323846f;
constexpr float EPSILON = 0.012f;
constexpr float PADDLE_TOP = PADDLE_Y - PADDLE_HALF_HEIGHT;
constexpr float PADDLE_BOTTOM = PADDLE_Y + PADDLE_HALF_HEIGHT;
constexpr const char* NAMES[LEVEL_COUNT] = {
    "彩虹回廊·初光", "彩虹回廊·晨曦", "彩虹回廊·涟漪", "彩虹回廊·彩波", "彩虹回廊·交织", "彩虹回廊·星芒", "彩虹回廊·流光", "彩虹回廊·映月", "彩虹回廊·虹影", "彩虹回廊·烟火", "彩虹回廊·繁星", "彩虹回廊·终章",
    "光之双塔·初光", "光之双塔·晨曦", "光之双塔·涟漪", "光之双塔·彩波", "光之双塔·交织", "光之双塔·星芒", "光之双塔·流光", "光之双塔·映月", "光之双塔·虹影", "光之双塔·烟火", "光之双塔·繁星", "光之双塔·终章",
    "星空拱门·初光", "星空拱门·晨曦", "星空拱门·涟漪", "星空拱门·彩波", "星空拱门·交织", "星空拱门·星芒", "星空拱门·流光", "星空拱门·映月", "星空拱门·虹影", "星空拱门·烟火", "星空拱门·繁星", "星空拱门·终章",
    "珊瑚双湾·初光", "珊瑚双湾·晨曦", "珊瑚双湾·涟漪", "珊瑚双湾·彩波", "珊瑚双湾·交织", "珊瑚双湾·星芒", "珊瑚双湾·流光", "珊瑚双湾·映月", "珊瑚双湾·虹影", "珊瑚双湾·烟火", "珊瑚双湾·繁星", "珊瑚双湾·终章",
    "银河阶梯·初光", "银河阶梯·晨曦", "银河阶梯·涟漪", "银河阶梯·彩波", "银河阶梯·交织", "银河阶梯·星芒", "银河阶梯·流光", "银河阶梯·映月", "银河阶梯·虹影", "银河阶梯·烟火", "银河阶梯·繁星", "银河阶梯·终章",
    "开放城堡·初光", "开放城堡·晨曦", "开放城堡·涟漪", "开放城堡·彩波", "开放城堡·交织", "开放城堡·星芒", "开放城堡·流光", "开放城堡·映月", "开放城堡·虹影", "开放城堡·烟火", "开放城堡·繁星", "开放城堡·终章",
    "霓虹迷径·初光", "霓虹迷径·晨曦", "霓虹迷径·涟漪", "霓虹迷径·彩波", "霓虹迷径·交织", "霓虹迷径·星芒", "霓虹迷径·流光", "霓虹迷径·映月", "霓虹迷径·虹影", "霓虹迷径·烟火", "霓虹迷径·繁星", "霓虹迷径·终章",
    "水晶花园·初光", "水晶花园·晨曦", "水晶花园·涟漪", "水晶花园·彩波", "水晶花园·交织", "水晶花园·星芒", "水晶花园·流光", "水晶花园·映月", "水晶花园·虹影", "水晶花园·烟火", "水晶花园·繁星", "水晶花园·终章",
    "万砖乐园·初光", "万砖乐园·晨曦", "万砖乐园·涟漪", "万砖乐园·彩波", "万砖乐园·交织", "万砖乐园·星芒", "万砖乐园·流光", "万砖乐园·映月", "万砖乐园·虹影", "万砖乐园·烟火", "万砖乐园·繁星", "万砖乐园·终章"
};

uint32_t random(Game& game) {
    uint32_t x = game.rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    game.rng = x ? x : 1;
    return game.rng;
}

float speed(const Game& game) {
    // Spread the old 36-level speed range across the larger campaign.
    const float progress = std::min(game.level / 3.0f, 35.0f);
    return game.profile.easy ? 108.0f + progress * 0.7f : 126.0f + progress * 0.9f;
}

void addScore(Game& game, uint32_t points) {
    game.score += std::min(points, std::numeric_limits<uint32_t>::max() - game.score);
    if (game.score > game.profile.bestScore) {
        game.profile.bestScore = game.score;
        game.dirty = true;
    }
    if (game.score > game.profile.levelBest[game.level]) {
        game.profile.levelBest[game.level] = game.score;
        game.dirty = true;
    }
}

void normalizeVelocity(Ball& ball, float magnitude) {
    float length = std::hypot(ball.vx, ball.vy);
    if (!(length > 0.01f) || !std::isfinite(length)) {
        ball.vx = magnitude * 0.3f;
        ball.vy = -magnitude * 0.9539392f;
        return;
    }
    ball.vx *= magnitude / length;
    ball.vy *= magnitude / length;
    // A small vertical component guarantees that nearly horizontal balls progress.
    if (std::fabs(ball.vy) < magnitude * 0.27f) {
        ball.vy = std::copysign(magnitude * 0.27f, ball.vy == 0 ? -1.0f : ball.vy);
        ball.vx = std::copysign(magnitude * std::sqrt(1 - 0.27f * 0.27f), ball.vx);
    }
}

void particles(Game& game, float x, float y, uint8_t color, int count) {
    for (Particle& particle : game.particles) {
        if (particle.life > 0) continue;
        const float angle = (random(game) % 6283) / 1000.0f;
        const float magnitude = 15.0f + random(game) % 40;
        particle = {x, y, std::cos(angle) * magnitude, std::sin(angle) * magnitude,
                    0.25f + (random(game) % 24) / 100.0f, color};
        if (--count == 0) break;
    }
}

void dropPower(Game& game, float x, float y) {
    const bool guarantee = game.destroyed == 6 || game.destroyed == 18 || game.destroyed == 36 || game.destroyed % 96 == 0;
    if (!guarantee && random(game) % 12 != 0) return;
    const unsigned choice = random(game) % 8;
    // Half of random drops split the balls; every 96 cleared bricks renews the
    // supply. The fixed drop/ball pools keep even a dense burst bounded.
    const Power kind = guarantee || choice < 4 ? Power::Split : static_cast<Power>(1 + choice % 3);
    for (Drop& drop : game.drops) {
        if (!drop.active) { drop = {true, x, y, kind}; return; }
    }
}

void hitBrick(Game& game, int index) {
    Brick& brick = game.bricks[index];
    if (!brick.hp) return;
    game.sounds |= S_BRICK;
    if (game.pierceTimer > 0) brick.hp = 0;
    else --brick.hp;
    if (brick.hp) { addScore(game, 3); return; }
    ++game.destroyed;
    ++game.combo;
    game.maxCombo = std::max(game.maxCombo, game.combo);
    game.comboTimer = 3.0f;
    game.comboWallBudget = 1.25f;
    addScore(game, 10u + static_cast<uint32_t>(std::min(game.combo / 5, 20)));
    const float x = BRICK_X + (index % COLS + 0.5f) * BRICK_W;
    const float y = BRICK_Y + (index / COLS + 0.5f) * BRICK_H;
    particles(game, x, y, brick.color, 4);
    dropPower(game, x, y);
}

struct Hit {
    float time = 2, nx = 0, ny = 0, push = 0;
    int brick = -1;
    bool paddle = false, wall = false;
};

// Sweep the ball's centre against an expanded box. Only grid cells crossed by
// this short segment are queried, so 256 balls never require a full-board scan.
void sweepBox(float x, float y, float dx, float dy, float left, float top,
              float right, float bottom, int index, Hit& hit) {
    if (x > left && x < right && y > top && y < bottom) {
        const float distances[] = {x - left, right - x, y - top, bottom - y};
        int side = 0;
        for (int i = 1; i < 4; ++i) if (distances[i] < distances[side]) side = i;
        if (hit.time > 0) {
            hit = {0, side == 0 ? -1.0f : side == 1 ? 1.0f : 0,
                      side == 2 ? -1.0f : side == 3 ? 1.0f : 0,
                      distances[side], index, false};
        }
        return;
    }
    float entryX = -1e30f, exitX = 1e30f, entryY = -1e30f, exitY = 1e30f;
    if (std::fabs(dx) < 1e-7f) { if (x < left || x > right) return; }
    else {
        entryX = (left - x) / dx; exitX = (right - x) / dx;
        if (entryX > exitX) std::swap(entryX, exitX);
    }
    if (std::fabs(dy) < 1e-7f) { if (y < top || y > bottom) return; }
    else {
        entryY = (top - y) / dy; exitY = (bottom - y) / dy;
        if (entryY > exitY) std::swap(entryY, exitY);
    }
    const float entry = std::max(entryX, entryY), exit = std::min(exitX, exitY);
    if (entry < -1e-6f || entry > 1 || entry > exit || entry >= hit.time) return;
    hit = {std::max(0.0f, entry), entryX >= entryY ? (dx > 0 ? -1.0f : 1.0f) : 0,
           entryX < entryY ? (dy > 0 ? -1.0f : 1.0f) : 0, 0, index, false};
}

void moveBall(Game& game, Ball& ball, float seconds, float paddleStart) {
    if (!std::isfinite(ball.x) || !std::isfinite(ball.y) ||
        !std::isfinite(ball.vx) || !std::isfinite(ball.vy)) { ball.active = false; return; }
    ball.idleTime += seconds;
    ball.steerCooldown = std::max(0.0f, ball.steerCooldown - seconds);
    const float slow = game.slowTimer > 0 ? 0.65f : 1.0f;
    float remaining = seconds * slow;
    const float duration = remaining;
    const float paddleTravel = game.paddleX - paddleStart;
    for (int collision = 0; collision < 8 && remaining > 0.000001f && ball.active; ++collision) {
        const float dx = ball.vx * remaining, dy = ball.vy * remaining;
        Hit hit;
        float contactPaddleX = game.paddleX;
        if (dx < 0 && ball.x + dx < LEFT + BALL_RADIUS)
            hit = {std::max(0.0f, (LEFT + BALL_RADIUS - ball.x) / dx), 1, 0, 0, -1, false};
        if (dx > 0 && ball.x + dx > RIGHT - BALL_RADIUS) {
            const float time = std::max(0.0f, (RIGHT - BALL_RADIUS - ball.x) / dx);
            if (time < hit.time) hit = {time, -1, 0, 0, -1, false};
        }
        if (dy < 0 && ball.y + dy < CEILING + BALL_RADIUS) {
            const float time = std::max(0.0f, (CEILING + BALL_RADIUS - ball.y) / dy);
            if (time < hit.time) hit = {time, 0, 1, 0, -1, false};
        }
        if (dy > 0 && ball.y <= PADDLE_BOTTOM + BALL_RADIUS) {
            // A moving paddle has sides and thickness, not just a top plane.
            // Sweep in its reference frame so late side catches and fast touch
            // drags work even when neither endpoint overlaps the ball. Slow
            // power changes ball travel only, never the paddle's time scale.
            const float paddleDx = paddleTravel * (remaining / duration);
            const float paddleX = game.paddleX - paddleDx;
            const float half = game.paddleWidth() * 0.5f + BALL_RADIUS;
            Hit paddleHit;
            sweepBox(ball.x, ball.y, dx - paddleDx, dy,
                     paddleX - half, PADDLE_TOP - BALL_RADIUS,
                     paddleX + half, PADDLE_BOTTOM + BALL_RADIUS, -1, paddleHit);
            if (paddleHit.time < hit.time) {
                hit = {paddleHit.time, 0, -1, 0, -1, true};
                contactPaddleX = paddleX + paddleDx * hit.time;
            }
        }
        for (const Wall& wall : game.walls) {
            if (!wall.active) continue;
            const float previousHit = hit.time;
            sweepBox(ball.x, ball.y, dx, dy, wall.x - BALL_RADIUS, wall.y - BALL_RADIUS,
                     wall.x + wall.w + BALL_RADIUS, wall.y + wall.h + BALL_RADIUS, -1, hit);
            if (hit.time < previousHit) hit.wall = true;
        }
        const int firstCol = std::clamp(static_cast<int>(std::floor((std::min(ball.x, ball.x + dx) - BALL_RADIUS - BRICK_X) / BRICK_W)), 0, COLS - 1);
        const int lastCol = std::clamp(static_cast<int>(std::floor((std::max(ball.x, ball.x + dx) + BALL_RADIUS - BRICK_X) / BRICK_W)), 0, COLS - 1);
        const int firstRow = std::clamp(static_cast<int>(std::floor((std::min(ball.y, ball.y + dy) - BALL_RADIUS - BRICK_Y) / BRICK_H)), 0, ROWS - 1);
        const int lastRow = std::clamp(static_cast<int>(std::floor((std::max(ball.y, ball.y + dy) + BALL_RADIUS - BRICK_Y) / BRICK_H)), 0, ROWS - 1);
        for (int row = firstRow; row <= lastRow; ++row) {
            for (int col = firstCol; col <= lastCol; ++col) {
                const int index = row * COLS + col;
                if (!game.bricks[index].hp) continue;
                const float x = BRICK_X + col * BRICK_W + 0.5f;
                const float y = BRICK_Y + row * BRICK_H + 0.5f;
                sweepBox(ball.x, ball.y, dx, dy, x - BALL_RADIUS, y - BALL_RADIUS,
                         x + BRICK_W - 1 + BALL_RADIUS, y + BRICK_H - 1 + BALL_RADIUS, index, hit);
            }
        }
        if (hit.time > 1) { ball.x += dx; ball.y += dy; break; }
        ball.x += dx * hit.time;
        ball.y += dy * hit.time;
        remaining *= 1 - hit.time;
        if (hit.brick >= 0) {
            hitBrick(game, hit.brick);
            ball.idleTime = ball.steerCooldown = 0;
            if (game.pierceTimer > 0) {
                // The destroyed cell is absent on the next iteration. Keeping
                // the remaining segment checks every later cell along the path.
                continue;
            }
        }
        if (hit.wall) {
            game.wallFlash = 0.14f;
            game.sounds |= S_PADDLE;
            // Walls sustain an existing chain briefly, but cannot create one,
            // score points, or keep a stale combo alive indefinitely.
            if (game.combo > 0 && game.comboWallBudget > 0) {
                const float extension = std::min({0.12f, game.comboWallBudget, 3.0f - game.comboTimer});
                game.comboTimer += extension;
                game.comboWallBudget -= extension;
            }
        }
        ball.x += hit.nx * (hit.push + EPSILON);
        ball.y += hit.ny * (hit.push + EPSILON);
        if (hit.paddle) {
            // Side/overlap catches are rescued to the upper face once. Balls
            // rising away or already below the whole paddle cannot be caught.
            ball.y = std::min(ball.y, PADDLE_TOP - BALL_RADIUS - EPSILON);
            ball.idleTime = ball.steerCooldown = 0;
            const float offset = std::clamp((ball.x - contactPaddleX) / (game.paddleWidth() * 0.5f), -1.0f, 1.0f);
            float angle = offset * (PI * 0.36f);
            if (std::fabs(angle) < 0.075f) angle = std::copysign(0.075f, ball.vx == 0 ? 1.0f : ball.vx);
            ball.vx = std::sin(angle) * speed(game);
            ball.vy = -std::cos(angle) * speed(game);
            game.sounds |= S_PADDLE;
            particles(game, ball.x, PADDLE_TOP, 6, 3);
        } else {
            const float dot = ball.vx * hit.nx + ball.vy * hit.ny;
            if (dot < 0) { ball.vx -= 2 * dot * hit.nx; ball.vy -= 2 * dot * hit.ny; }
            if (hit.wall && ball.idleTime >= 6.0f && ball.steerCooldown == 0) {
                // Choose a valid outgoing angle directly: subsequent velocity
                // normalization cannot collapse it onto either minimum axis.
                // Start with a small seeded change after six idle seconds; if
                // still idle at twelve, explore the full safe angle range.
                // Repeat at most every two seconds until a brick/paddle hit.
                const uint32_t turn = random(game);
                constexpr float minimumAngle = 0.42f, maximumAngle = 1.15f;
                float angle;
                if (ball.idleTime >= 12.0f) {
                    angle = minimumAngle + (turn % 731) * 0.001f;
                } else {
                    const float current = std::atan2(std::fabs(ball.vy), std::fabs(ball.vx));
                    const float delta = (0.16f + (turn % 101) * 0.001f) * (turn & 256 ? 1 : -1);
                    angle = std::clamp(current + delta, minimumAngle, maximumAngle);
                    if (std::fabs(angle - current) < 0.1f)
                        angle = std::clamp(current - delta, minimumAngle, maximumAngle);
                }
                float signX = std::signbit(ball.vx) ? -1.0f : 1.0f;
                float signY = std::signbit(ball.vy) ? -1.0f : 1.0f;
                if (ball.idleTime >= 12.0f) {
                    // A stalled ray may also change its tangential direction;
                    // the normal always points out of the surface it just hit.
                    if (hit.ny != 0) signX = turn & 512 ? 1.0f : -1.0f;
                    if (hit.nx != 0) signY = turn & 512 ? 1.0f : -1.0f;
                }
                if (hit.nx != 0) signX = hit.nx;
                if (hit.ny != 0) signY = hit.ny;
                ball.vx = signX * std::cos(angle) * speed(game);
                ball.vy = signY * std::sin(angle) * speed(game);
                ball.steerCooldown = 2.0f;
            }
            normalizeVelocity(ball, speed(game));
            // A perfectly vertical ray must eventually find an open end of a
            // shelf instead of bouncing forever between that shelf and a roof.
            if (hit.wall && hit.ny != 0 && std::fabs(ball.vx) < speed(game) * 0.18f) {
                ball.vx = std::copysign(speed(game) * 0.18f,
                                       ball.vx == 0 ? ball.x - 200.0f : ball.vx);
                ball.vy = std::copysign(speed(game) * std::sqrt(1 - 0.18f * 0.18f), ball.vy);
            }
        }
    }
    ball.x = std::clamp(ball.x, LEFT + BALL_RADIUS, RIGHT - BALL_RADIUS);
    ball.y = std::max(ball.y, CEILING + BALL_RADIUS);
    if (ball.y > FLOOR + BALL_RADIUS) ball.active = false;
}

void prepareBall(Game& game) {
    for (Ball& ball : game.balls) ball = {};
    game.balls[0] = {true, game.paddleX, PADDLE_TOP - BALL_RADIUS - 0.1f, 0, 0,
                     game.paddleX, PADDLE_TOP - BALL_RADIUS - 0.1f};
}

// Each chapter changes the actual passages as well as the colours. The
// narrow empty channels cost less than 12% of the 3,328-cell square grid.
bool filled(int row, int col, int level) {
    const int variant = level % 12;
    switch (level / 12) {
        case 0: return (row + variant) % 7 != 5 || (col + variant * 3) % 24 >= 5;
        case 1: return (row + col / 8 + variant * 2) % 26 >= 2;
        case 2: return (col + (row / 4) * (7 + variant % 3) + variant * 5) % 37 >= 3;
        case 3: return (std::abs(col * 2 - (COLS - 1)) / 3 + std::abs(row * 2 - (ROWS - 1)) + variant * 2) % 29 >= 3;
        case 4: return (row + std::abs(col - COLS / 2) / 5 + variant * 2) % 19 >= 2;
        case 5: return !((row + variant) % 7 < 2 && (col / 4 + variant * 3) % 9 == 3);
        case 6: return (col - row * (2 + variant % 3) + 256 + variant * 7) % 41 >= 4;
        case 7: return (row * 5 + col / 5 + variant * 3) % 23 >= 2;
        default: return !((row / 2 + col / 4 + variant * 3) % 17 == 0 && (row + col + variant) % 3 != 0);
    }
}

void buildWalls(Game& game) {
    for (Wall& wall : game.walls) wall = {};
    int count = 0;
    const float shift = (game.level % 4 - 1.5f) * 4.0f;
    const float rise = static_cast<float>((game.level % 12) / 4) * 3;
    auto put = [&](float x, float y, float w, float h) {
        game.walls[count++] = {true, x + shift, y - rise, w, h};
    };
    switch (game.level / 12) {
        case 0: // Open shelves: the very first level rewards balls above them.
            put(38,118,118,3); put(244,118,118,3); break;
        case 1: // Two vertical corridors, open at both ends.
            put(120,49,3,62); put(277,49,3,62); break;
        case 2: // Arch, with a completely open lower edge.
            put(116,68,168,3); put(116,68,3,37); put(281,68,3,37); break;
        case 3: // Two open bowls, separated by a broad central passage.
            put(62,59,3,48); put(62,104,110,3); put(169,59,3,48);
            put(228,59,3,48); put(228,104,110,3); put(335,59,3,48); break;
        case 4: // Staggered shelves preserve a route around every end.
            put(34,64,101,3); put(150,90,100,3); put(265,116,101,3); break;
        case 5: // Split ramparts leave a 48-pixel opening in the centre.
            put(48,118,128,3); put(224,118,128,3);
            put(104,90,3,28); put(293,90,3,28); break;
        case 6: // Alternating brackets never form a closed cell.
            put(48,65,100,3); put(145,65,3,35); put(252,65,100,3);
            put(252,65,3,35); put(176,113,48,3); break;
        case 7: // Three short gates create a garden of open pockets.
            put(72,78,3,35); put(72,78,62,3); put(166,104,68,3);
            put(266,78,62,3); put(325,78,3,35); break;
        default: // A central cross with detached outer ledges.
            put(199,62,3,50); put(161,86,79,3);
            put(38,115,92,3); put(270,115,92,3); break;
    }
}

bool clearOfWalls(const Game& game, int row, int col) {
    const float x = BRICK_X + col * BRICK_W + 0.5f;
    const float y = BRICK_Y + row * BRICK_H + 0.5f;
    constexpr float gap = 1.0f;
    for (const Wall& wall : game.walls) {
        if (wall.active && x < wall.x + wall.w + gap && x + BRICK_W - 1 > wall.x - gap &&
            y < wall.y + wall.h + gap && y + BRICK_H - 1 > wall.y - gap) return false;
    }
    return true;
}
}

Game::Game() { startLevel(0); scene = Scene::Title; }

void Game::startLevel(int index) {
    const int unlocked = std::clamp(profile.unlocked, 1, LEVEL_COUNT);
    if (profile.unlocked != unlocked) { profile.unlocked = unlocked; dirty = true; }
    level = std::clamp(index, 0, unlocked - 1);
    lives = 3; score = 0; totalBricks = 0; destroyed = 0; combo = 0; maxCombo = 0;
    paddleX = 200; wideTimer = pierceTimer = slowTimer = comboTimer = comboWallBudget = wallFlash = elapsed = 0;
    sounds = 0; rng = 0x9e3779b9u ^ (static_cast<uint32_t>(level + 1) * 0x85ebca6bu);
    for (Drop& drop : drops) drop = {};
    for (Particle& particle : particles) particle = {};
    buildWalls(*this);
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLS; ++col) {
            Brick& brick = bricks[row * COLS + col];
            brick.color = static_cast<uint8_t>((row / 2 + col / 5 + level) % 7);
            brick.hp = filled(row, col, level) && clearOfWalls(*this, row, col) ? 1 : 0;
            if (brick.hp && level >= 18 && (row * 3 + col + level) % 17 < 1 + level / 36) brick.hp = 2;
            if (brick.hp) ++totalBricks;
        }
    }
    prepareBall(*this);
    scene = Scene::Ready;
}

void Game::launch() {
    if (scene != Scene::Ready) return;
    prepareBall(*this);
    balls[0].vx = speed(*this) * 0.29f;
    balls[0].vy = -speed(*this) * std::sqrt(1 - 0.29f * 0.29f);
    scene = Scene::Playing;
    sounds |= S_LAUNCH;
}

void Game::step(float seconds, float targetX) {
    if (scene != Scene::Playing && scene != Scene::Ready) return;
    const float startX = paddleX;
    const float endX = std::clamp(std::isfinite(targetX) ? targetX : paddleX,
                                LEFT + paddleWidth() * 0.5f, RIGHT - paddleWidth() * 0.5f);
    if (scene == Scene::Ready) { paddleX = endX; prepareBall(*this); return; }
    if (!(seconds > 0) || !std::isfinite(seconds)) return;
    seconds = std::min(seconds, 0.05f);
    const int steps = std::max(1, static_cast<int>(std::ceil(seconds * 240.0f)));
    const float dt = seconds / steps;
    for (Ball& ball : balls) { ball.previousX = ball.x; ball.previousY = ball.y; }
    for (int iteration = 0; iteration < steps && scene == Scene::Playing; ++iteration) {
        const float paddleStart = paddleX;
        elapsed += dt;
        wideTimer = std::max(0.0f, wideTimer - dt);
        pierceTimer = std::max(0.0f, pierceTimer - dt);
        slowTimer = std::max(0.0f, slowTimer - dt);
        comboTimer = std::max(0.0f, comboTimer - dt);
        wallFlash = std::max(0.0f, wallFlash - dt);
        if (comboTimer == 0) { combo = 0; comboWallBudget = 0; }
        paddleX = std::clamp(startX + (endX - startX) * ((iteration + 1.0f) / steps),
                             LEFT + paddleWidth() * 0.5f, RIGHT - paddleWidth() * 0.5f);
        for (Ball& ball : balls) if (ball.active) moveBall(*this, ball, dt, paddleStart);
        for (Particle& particle : particles) {
            if (particle.life <= 0) continue;
            particle.life = std::max(0.0f, particle.life - dt);
            particle.x += particle.vx * dt; particle.y += particle.vy * dt;
            particle.vy += 55 * dt;
        }
        for (Drop& drop : drops) {
            if (!drop.active) continue;
            const float previous = drop.y;
            drop.y += 70 * dt;
            if (previous <= PADDLE_Y + 6 && drop.y >= PADDLE_TOP - 4 &&
                std::fabs(drop.x - paddleX) < paddleWidth() * 0.5f + 5) {
                drop.active = false;
                applyPower(drop.kind);
                addScore(*this, 25);
            } else if (drop.y > FLOOR + 8) drop.active = false;
        }
        if (remainingBricks() == 0) {
            addScore(*this, static_cast<uint32_t>(lives) * 100);
            if (profile.unlocked < std::min(LEVEL_COUNT, level + 2)) {
                profile.unlocked = std::min(LEVEL_COUNT, level + 2);
                dirty = true;
            }
            scene = level == LEVEL_COUNT - 1 ? Scene::Finished : Scene::Cleared;
            sounds |= S_WIN;
        } else if (ballCount() == 0) {
            --lives;
            wideTimer = pierceTimer = slowTimer = comboTimer = comboWallBudget = wallFlash = 0;
            combo = 0;
            for (Drop& drop : drops) drop.active = false;
            sounds |= S_LOST;
            scene = lives > 0 ? Scene::Ready : Scene::GameOver;
            if (scene == Scene::Ready) prepareBall(*this);
        }
    }
}

void Game::pause() { if (scene == Scene::Ready || scene == Scene::Playing) scene = Scene::Paused; }

void Game::resume() {
    if (scene != Scene::Paused) return;
    scene = Scene::Ready;
    for (const Ball& ball : balls) if (ball.active && (ball.vx != 0 || ball.vy != 0)) { scene = Scene::Playing; break; }
}

void Game::returnToTitle() { scene = Scene::Title; sounds = 0; }

void Game::applyPower(Power power) {
    if (scene != Scene::Playing) return;
    sounds |= S_POWER;
    switch (power) {
        case Power::Wide: wideTimer = 12; break;
        case Power::Pierce: pierceTimer = 8; break;
        case Power::Slow: slowTimer = 12; break;
        case Power::Split: {
            std::array<int,MAX_BALLS> originals{};
            int count = 0;
            for (int i = 0; i < MAX_BALLS; ++i) if (balls[i].active) originals[count++] = i;
            int slot = 0;
            for (int i = 0; i < count; ++i) {
                const Ball original = balls[originals[i]];
                for (int direction : {-1, 1}) {
                    while (slot < MAX_BALLS && balls[slot].active) ++slot;
                    if (slot == MAX_BALLS) return;
                    Ball child = original;
                    const float angle = direction * 0.32f;
                    child.vx = original.vx * std::cos(angle) - original.vy * std::sin(angle);
                    child.vy = original.vx * std::sin(angle) + original.vy * std::cos(angle);
                    normalizeVelocity(child, speed(*this));
                    child.previousX = child.x; child.previousY = child.y;
                    child.idleTime = child.steerCooldown = 0;
                    balls[slot++] = child;
                }
            }
            break;
        }
    }
    paddleX = std::clamp(paddleX, LEFT + paddleWidth() * 0.5f, RIGHT - paddleWidth() * 0.5f);
}

int Game::remainingBricks() const { return std::max(0, totalBricks - destroyed); }
int Game::ballCount() const { int count = 0; for (const Ball& ball : balls) count += ball.active; return count; }
float Game::paddleWidth() const { return (profile.easy ? 78.0f : 62.0f) * (wideTimer > 0 ? 1.55f : 1.0f); }
const char* levelName(int index) { return NAMES[std::clamp(index, 0, LEVEL_COUNT - 1)]; }
const char* powerName(Power power) {
    switch (power) { case Power::Split: return "三倍分裂"; case Power::Wide: return "加长挡板";
                    case Power::Pierce: return "穿透"; case Power::Slow: return "慢速"; }
    return "道具";
}
}
