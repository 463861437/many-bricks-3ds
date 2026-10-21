#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>
#include <vector>
#include <string>

using namespace bricks;
namespace {
int assertions = 0, cases = 0;
void check(bool condition, const char* expression, int line) {
    ++assertions;
    if (!condition) { std::cerr << "FAIL line " << line << ": " << expression << '\n'; std::exit(1); }
}
#define CHECK(x) check(bool(x), #x, __LINE__)
bool near(float a, float b, float tolerance = 0.03f) { return std::fabs(a-b) < tolerance; }
void emptyField(Game& game) {
    game.startLevel(0); game.launch();
    for (Brick& brick : game.bricks) brick = {};
    for (Wall& wall : game.walls) wall = {};
    // An isolated far-away brick keeps collision fixtures in Playing.
    game.bricks[0] = {1,0}; game.totalBricks = 1; game.destroyed = 0;
    for (Ball& ball : game.balls) ball = {};
    game.sounds = 0;
}
void oneBall(Game& game, float x, float y, float vx, float vy) {
    game.balls[0] = {true,x,y,vx,vy,x,y};
}
void brickAt(Game& game, int row, int col, int hp=1) {
    game.bricks[row * COLS + col] = {static_cast<uint8_t>(hp),2};
    ++game.totalBricks;
}
float launchSpeed(bool easy = false) {
    Game probe; probe.profile.easy = easy; probe.startLevel(0); probe.launch();
    return std::hypot(probe.balls[0].vx, probe.balls[0].vy);
}
float brickCenterX(int col) { return BRICK_X + (col + 0.5f) * BRICK_W; }
float brickCenterY(int row) { return BRICK_Y + (row + 0.5f) * BRICK_H; }
float brickBottom(int row) { return BRICK_Y + (row + 1) * BRICK_H - 0.5f; }
float belowBrick(int row) { return brickBottom(row) + BALL_RADIUS + 2; }
float aboveBrick(int row) { return BRICK_Y + row * BRICK_H + 0.5f - BALL_RADIUS - 2; }
constexpr float PADDLE_CONTACT = PADDLE_Y - 3 - BALL_RADIUS;
uint32_t next(uint32_t& state) { state ^= state<<13; state ^= state>>17; state ^= state<<5; return state; }
}

int main() {
    const float normalSpeed = launchSpeed();
    {
        Game game;
        CHECK(game.scene == Scene::Title); CHECK(game.totalBricks > 2500);
        game.startLevel(35); CHECK(game.level == 0); CHECK(game.scene == Scene::Ready);
        game.step(0.01f,-200); CHECK(near(game.paddleX,LEFT+game.paddleWidth()/2));
        CHECK(near(game.balls[0].x,game.paddleX)); CHECK(game.elapsed == 0);
        game.launch(); CHECK(game.scene==Scene::Playing); CHECK(game.ballCount()==1);
        CHECK(game.balls[0].vy<0); CHECK(game.sounds&S_LAUNCH); ++cases;
    }
    {
        Game game; game.profile.unlocked=-100; game.startLevel(-999);
        CHECK(game.profile.unlocked==1); CHECK(game.level==0); CHECK(game.dirty);
        game.profile.unlocked=10000; game.startLevel(10000);
        CHECK(game.profile.unlocked==LEVEL_COUNT); CHECK(game.level==LEVEL_COUNT-1);
        CHECK(std::string(levelName(-10))==levelName(0)); CHECK(std::string(levelName(999))==levelName(LEVEL_COUNT-1)); ++cases;
    }
    {
        Game game; game.profile.unlocked=LEVEL_COUNT;
        std::set<std::string> maps; int minBricks=COLS*ROWS, maxBricks=0, firstBricks=0;
        for(int level=0;level<LEVEL_COUNT;++level) {
            game.startLevel(level); int actual=0; std::string signature;
            for(const Brick& brick:game.bricks) { actual+=brick.hp>0; CHECK(brick.hp<=2); CHECK(brick.color<7); signature+=char(brick.hp>0); }
            CHECK(actual==game.totalBricks); CHECK(actual>=2500); minBricks=std::min(minBricks,actual); maxBricks=std::max(maxBricks,actual); if(level==0)firstBricks=actual; CHECK(actual<=COLS*ROWS);
            float lowestBrick=0;
            for(int row=0;row<ROWS;++row) for(int col=0;col<COLS;++col)
                if(game.bricks[row*COLS+col].hp) lowestBrick=std::max(lowestBrick,brickBottom(row));
            // Every generated level must leave a real reaction lane below its bricks.
            CHECK(PADDLE_Y-3-lowestBrick>=90);
            CHECK(game.destroyed==0); CHECK(game.remainingBricks()==actual);
            CHECK(maps.insert(signature).second);
            Game repeated; repeated.profile.unlocked=LEVEL_COUNT; repeated.startLevel(level);
            for(int i=0;i<COLS*ROWS;++i) CHECK(repeated.bricks[i].hp==game.bricks[i].hp);
        }
        CHECK(maps.size()==LEVEL_COUNT); CHECK(BRICK_W==BRICK_H); CHECK(COLS*BRICK_W==RIGHT-LEFT);
        std::cout << "Square levels: " << minBricks << "–" << maxBricks << " bricks, first=" << firstBricks << "; "; ++cases;
    }
    {
        // Ignore destructible bricks and flood the ball-centre free space. A
        // connected free region proves the permanent walls have no sealed cell.
        constexpr int width=381, height=181;
        int minWalls=MAX_WALLS, maxWalls=0;
        for(int level=0;level<LEVEL_COUNT;++level) {
            Game game; game.profile.unlocked=LEVEL_COUNT; game.startLevel(level);
            int wallCount=0;
            for(const Wall& wall:game.walls) if(wall.active) {
                ++wallCount;
                CHECK(wall.w>0&&wall.h>0); CHECK(wall.x>LEFT&&wall.x+wall.w<RIGHT);
                CHECK(wall.y>CEILING&&wall.y+wall.h<=121);
            }
            minWalls=std::min(minWalls,wallCount); maxWalls=std::max(maxWalls,wallCount);
            CHECK(wallCount>=2&&wallCount<=MAX_WALLS);
            std::vector<uint8_t> cells(width*height,0);
            for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
                for(const Wall& wall:game.walls) if(wall.active&&
                    x+10>wall.x-BALL_RADIUS&&x+10<wall.x+wall.w+BALL_RADIUS&&
                    y+33>wall.y-BALL_RADIUS&&y+33<wall.y+wall.h+BALL_RADIUS) { cells[y*width+x]=1; break; }
            }
            std::vector<int> pending{(210-33)*width+(200-10)};cells[pending[0]]=2;
            for(size_t read=0;read<pending.size();++read) {
                const int cell=pending[read],x=cell%width,y=cell/width;
                const int adjacent[]={x>0?cell-1:-1,x+1<width?cell+1:-1,y>0?cell-width:-1,y+1<height?cell+width:-1};
                for(int next:adjacent)if(next>=0&&cells[next]==0){cells[next]=2;pending.push_back(next);}
            }
            CHECK(std::find(cells.begin(),cells.end(),0)==cells.end());
            // Every brick has a reachable contact position outside all walls.
            for(int row=0;row<ROWS;++row)for(int col=0;col<COLS;++col)if(game.bricks[row*COLS+col].hp){
                const float left=BRICK_X+col*BRICK_W+0.5f-BALL_RADIUS;
                const float top=BRICK_Y+row*BRICK_H+0.5f-BALL_RADIUS;
                const float right=left+BRICK_W-1+BALL_RADIUS*2;
                const float bottom=top+BRICK_H-1+BALL_RADIUS*2;
                bool reachable=false;
                for(int y=std::max(33,int(std::ceil(top)));y<=std::min(213,int(std::floor(bottom)));++y)
                    for(int x=std::max(10,int(std::ceil(left)));x<=std::min(390,int(std::floor(right)));++x)
                        reachable|=cells[(y-33)*width+x-10]==2;
                CHECK(reachable);
            }
            game.launch();
            const float velocity=std::hypot(game.balls[0].vx,game.balls[0].vy);
            CHECK(velocity>=126&&velocity<=157.51f);
            game.profile.easy=true;game.startLevel(level);game.launch();
            CHECK(std::hypot(game.balls[0].vx,game.balls[0].vy)<=132.51f);
        }
        std::cout << "Open walls: " << minWalls << "–" << maxWalls << " segments, all 108 levels reachable; "; ++cases;
    }
    {
        // Walls reflect from above, below and both sides, including piercing
        // balls. They never grant a brick, a point, or a new combo.
        for(bool pierce:{false,true})for(int side=0;side<4;++side){
            Game game;emptyField(game);game.walls[0]={true,150,100,90,3};
            if(pierce)game.applyPower(Power::Pierce);
            if(side==0)oneBall(game,200,106,0,-normalSpeed);
            if(side==1)oneBall(game,200,97,0,normalSpeed);
            if(side==2)oneBall(game,147,101,normalSpeed,0);
            if(side==3)oneBall(game,243,101,-normalSpeed,0);
            game.step(0.03f,200);
            CHECK(game.wallFlash>0);CHECK(game.score==0);CHECK(game.combo==0);CHECK(game.destroyed==0);
            CHECK(game.remainingBricks()==1);CHECK(game.walls[0].active);CHECK(game.scene==Scene::Playing);
            CHECK(near(std::hypot(game.balls[0].vx,game.balls[0].vy),normalSpeed));
            CHECK(side==0?game.balls[0].vy>0:side==1?game.balls[0].vy<0:side==2?game.balls[0].vx<0:game.balls[0].vx>0);
            if(side<2)CHECK(std::fabs(game.balls[0].vx)>=normalSpeed*0.18f-0.01f);
        }
        ++cases;
    }
    {
        Game game;emptyField(game);game.walls[0]={true,150,100,90,3};
        game.combo=12;game.comboTimer=3;game.comboWallBudget=1.25f;
        for(int frame=0;frame<100;++frame){oneBall(game,200,106,0,-normalSpeed);game.step(0.05f,200);}
        CHECK(game.combo==0);CHECK(game.comboTimer==0);CHECK(game.comboWallBudget==0);
        CHECK(game.score==0);CHECK(game.destroyed==0);CHECK(game.lives==3);
        game.startLevel(0);CHECK(game.wallFlash==0);CHECK(game.comboWallBudget==0);++cases;
    }
    {
        // With two shelves and open ends, a vertical ball must acquire enough
        // horizontal travel to leave the pocket rather than loop forever.
        Game game;emptyField(game);
        game.walls[0]={true,100,50,100,3};game.walls[1]={true,100,100,100,3};
        oneBall(game,150,75,0,normalSpeed);bool leftPocket=false;
        for(int frame=0;frame<1200&&!leftPocket;++frame){
            game.step(1.0f/120,150);
            leftPocket=game.balls[0].x<100-BALL_RADIUS||game.balls[0].x>200+BALL_RADIUS;
        }
        CHECK(leftPocket);CHECK(game.lives==3);CHECK(game.destroyed==0);++cases;
    }
    {
        // Regression: open bowls and ramparts produced periodic angled
        // orbits, including loops at the old minimum-axis velocity thresholds.
        // Release each without moving the ball instantly or accelerating.
        float worstRelease=0;
        for(const auto orbit:std::array<std::array<float,6>,5>{{{{36,110,34,0.586f,1,0}},{{36,80,34,0.18f,1,0}},{{62,375,85,0.9f,1,0}},{{1,285,56,0.9f,1,0}},{{0,375,108,0.9f,-1,1}}}}){
            Game game;game.profile.unlocked=LEVEL_COUNT;game.profile.easy=orbit[5]!=0;game.startLevel(static_cast<int>(orbit[0]));game.launch();
            for(Brick& brick:game.bricks)brick={};
            game.totalBricks=1;game.destroyed=0;
            const float velocity=std::hypot(game.balls[0].vx,game.balls[0].vy);
            oneBall(game,orbit[1],orbit[2],velocity*orbit[3]*orbit[4],-velocity*std::sqrt(1-orbit[3]*orbit[3]));
            bool released=false;
            for(int frame=0;frame<14400&&!released;++frame){
                const float x=game.balls[0].x,y=game.balls[0].y;
                game.step(1.0f/240,200);
                CHECK(near(std::hypot(game.balls[0].vx,game.balls[0].vy),velocity));
                CHECK(std::hypot(game.balls[0].x-x,game.balls[0].y-y)<velocity/240+0.1f);
                released=game.balls[0].y>124;
            }
            CHECK(released);CHECK(game.lives==3);CHECK(game.score==0);
            worstRelease=std::max(worstRelease,game.elapsed);
        }
        std::cout << "Periodic-orbit release: " << worstRelease << " s worst of 5; ";++cases;
    }
    {
        Game game;emptyField(game);brickAt(game,10,10,2);
        oneBall(game,brickCenterX(10),belowBrick(10),0,-normalSpeed);game.balls[0].idleTime=5.9f;game.balls[0].steerCooldown=1.7f;
        game.step(0.03f,200);CHECK(game.bricks[10*COLS+10].hp==1);CHECK(game.balls[0].idleTime<0.03f);CHECK(game.balls[0].steerCooldown==0);
        oneBall(game,200,PADDLE_CONTACT-1.7f,0,normalSpeed);game.balls[0].idleTime=5.9f;game.balls[0].steerCooldown=1.7f;
        game.step(0.02f,200);CHECK(game.balls[0].idleTime<0.02f);CHECK(game.balls[0].steerCooldown==0);
        game.balls[0].idleTime=5.9f;game.balls[0].steerCooldown=1.7f;game.applyPower(Power::Split);
        CHECK(game.balls[0].idleTime==5.9f);CHECK(game.balls[1].idleTime==0);CHECK(game.balls[2].idleTime==0);
        CHECK(game.balls[1].steerCooldown==0);CHECK(game.balls[2].steerCooldown==0);
        game.pause();game.step(30,200);CHECK(game.balls[0].idleTime==5.9f);CHECK(game.balls[0].steerCooldown==1.7f);
        game.resume();game.step(0.01f,200);CHECK(game.balls[0].idleTime>5.9f);CHECK(game.balls[0].steerCooldown<1.7f);
        game.startLevel(0);CHECK(game.balls[0].idleTime==0);++cases;
    }
    {
        // A ball reflected by the lowest brick must give the player useful
        // reaction time before it reaches the paddle, in both difficulty modes.
        float standardReturn=0, easyReturn=0;
        for(bool easy:{false,true}) {
            Game game; game.profile.easy=easy; emptyField(game);
            const int row=ROWS-1, col=COLS/2, index=row*COLS+col;
            brickAt(game,row,col);
            oneBall(game,brickCenterX(col),belowBrick(row),0,-launchSpeed(easy));
            float reflectedAt=-1, caughtAt=-1;
            for(int frame=0;frame<1200;++frame) {
                game.sounds=0; game.step(1.0f/600,brickCenterX(col));
                if(reflectedAt<0 && game.bricks[index].hp==0) reflectedAt=game.elapsed;
                if(game.sounds&S_PADDLE) { caughtAt=game.elapsed; break; }
            }
            CHECK(reflectedAt>=0); CHECK(caughtAt>reflectedAt);
            CHECK(game.lives==3); CHECK(game.balls[0].vy<0);
            const float reactionTime=caughtAt-reflectedAt;
            CHECK(reactionTime>=0.70f); CHECK(reactionTime<1.2f);
            if(easy) easyReturn=reactionTime; else standardReturn=reactionTime;
        }
        CHECK(easyReturn>standardReturn);
        std::cout << "Lowest-brick return: " << standardReturn << " s standard, "
                  << easyReturn << " s easy; "; ++cases;
    }
    {
        // Exercise the new grid boundaries without relying on old 32-column
        // coordinates, including the far right column and the lowest row.
        for(int row:{0,ROWS-1}) for(int col:{0,COLS-1}) {
            Game game; emptyField(game);
            for(Brick& brick:game.bricks) brick={};
            game.totalBricks=0; brickAt(game,ROWS/2,COLS/2); brickAt(game,row,col);
            oneBall(game,brickCenterX(col),belowBrick(row),0,-normalSpeed);
            game.step(0.05f,200);
            CHECK(game.bricks[row*COLS+col].hp==0); CHECK(game.destroyed==1);
            CHECK(game.balls[0].vy>0); CHECK(game.scene==Scene::Playing);
            CHECK(game.balls[0].x>=LEFT+BALL_RADIUS);
            CHECK(game.balls[0].x<=RIGHT-BALL_RADIUS);
        }
        ++cases;
    }
    {
        // Pierce must process every thin brick in the last column while the
        // ordinary non-target brick keeps the level running.
        Game game; emptyField(game); const int col=COLS-1;
        for(int row=0;row<ROWS;++row) brickAt(game,row,col,2);
        oneBall(game,brickCenterX(col),belowBrick(ROWS-1),0,-normalSpeed);
        game.applyPower(Power::Pierce);
        for(int frame=0;frame<100 && game.destroyed<ROWS;++frame) game.step(0.01f,200);
        for(int row=0;row<ROWS;++row) CHECK(game.bricks[row*COLS+col].hp==0);
        CHECK(game.destroyed==ROWS); CHECK(game.balls[0].vy<0);
        CHECK(game.bricks[0].hp==1); CHECK(game.scene==Scene::Playing); ++cases;
    }
    {
        // Simultaneous balls across the full width must hit their own cells,
        // including columns that did not exist in the original grid.
        Game game; emptyField(game); const int row=ROWS-1;
        for(int col=0;col<COLS;++col) {
            brickAt(game,row,col);
            const float x=brickCenterX(col), y=belowBrick(row);
            game.balls[col]={true,x,y,0,-normalSpeed,x,y};
        }
        game.step(0.05f,200);
        CHECK(game.destroyed==COLS); CHECK(game.ballCount()==COLS);
        for(int col=0;col<COLS;++col) {
            CHECK(game.bricks[row*COLS+col].hp==0); CHECK(game.balls[col].vy>0);
        }
        game.applyPower(Power::Split); CHECK(game.ballCount()==std::min(MAX_BALLS,COLS*3)); ++cases;
    }
    {
        Game game; emptyField(game); oneBall(game,10,190,-140,-60);
        game.step(1.0f/120,200); CHECK(game.balls[0].vx>0); CHECK(game.balls[0].x>=LEFT+BALL_RADIUS); ++cases;
    }
    {
        Game game; emptyField(game); oneBall(game,390,190,140,-60);
        game.step(1.0f/120,200); CHECK(game.balls[0].vx<0); CHECK(game.balls[0].x<=RIGHT-BALL_RADIUS); ++cases;
    }
    {
        Game game; emptyField(game); oneBall(game,200,33,60,-140);
        game.step(1.0f/120,200); CHECK(game.balls[0].vy>0); CHECK(game.balls[0].y>=CEILING+BALL_RADIUS); ++cases;
    }
    {
        Game game; emptyField(game); oneBall(game,100,FLOOR+1,0,normalSpeed);
        game.step(0.03f,300); CHECK(game.lives==2); CHECK(game.scene==Scene::Ready); CHECK(game.sounds&S_LOST);
        CHECK(game.ballCount()==1); CHECK(game.balls[0].vx==0); CHECK(game.balls[0].vy==0); ++cases;
    }
    {
        Game game; emptyField(game); oneBall(game,200,PADDLE_CONTACT-1.7f,0,normalSpeed);
        game.step(0.02f,200); CHECK(game.balls[0].vy<0); CHECK(game.balls[0].y<PADDLE_CONTACT);
        CHECK(std::fabs(game.balls[0].vx)>1); CHECK(game.sounds&S_PADDLE); ++cases;
    }
    {
        for(int side:{-1,1}) {
            Game game; emptyField(game); oneBall(game,200+side*25,PADDLE_CONTACT-1.7f,0,normalSpeed);
            game.step(0.02f,200); CHECK(game.balls[0].vy<0); CHECK(game.balls[0].vx*side>80);
        }
        ++cases;
    }
    {
        Game game; emptyField(game); oneBall(game,300,PADDLE_CONTACT-1,0,normalSpeed);
        game.step(0.03f,100); CHECK(game.balls[0].vy>0); CHECK(!(game.sounds&S_PADDLE)); ++cases;
    }
    {
        Game game; emptyField(game); brickAt(game,10,10);
        oneBall(game,brickCenterX(10),belowBrick(10),0,-normalSpeed); game.step(0.05f,200);
        CHECK(game.bricks[10*COLS+10].hp==0); CHECK(game.destroyed==1); CHECK(game.balls[0].vy>0);
        CHECK(game.score>=10); CHECK(game.profile.bestScore==game.score); CHECK(game.profile.levelBest[0]==game.score);
        CHECK(game.dirty); CHECK(game.sounds&S_BRICK); CHECK(game.combo==1); ++cases;
    }
    {
        Game game; emptyField(game); brickAt(game,10,10);
        oneBall(game,brickCenterX(10),aboveBrick(10),0,normalSpeed); game.step(0.05f,200);
        CHECK(game.bricks[10*COLS+10].hp==0); CHECK(game.balls[0].vy<0); ++cases;
    }
    {
        for(int side:{-1,1}) {
            Game game; emptyField(game); brickAt(game,10,10);
            oneBall(game,brickCenterX(10)+side*(BRICK_W/2+BALL_RADIUS+2),brickCenterY(10),-side*normalSpeed,0); game.step(0.05f,200);
            CHECK(game.bricks[10*COLS+10].hp==0); CHECK(game.balls[0].vx*side>0);
            CHECK(std::fabs(game.balls[0].vy)>=normalSpeed*0.27f-0.01f);
        }
        ++cases;
    }
    {
        Game game; emptyField(game); brickAt(game,10,10,2);
        oneBall(game,brickCenterX(10),belowBrick(10),0,-normalSpeed); game.step(0.03f,200);
        CHECK(game.bricks[10*COLS+10].hp==1); CHECK(game.destroyed==0); CHECK(game.score==3);
        oneBall(game,brickCenterX(10),belowBrick(10),0,-normalSpeed); game.step(0.03f,200);
        CHECK(game.bricks[10*COLS+10].hp==0); CHECK(game.destroyed==1); ++cases;
    }
    {
        Game game; emptyField(game); brickAt(game,10,10,2); brickAt(game,9,10,2);
        oneBall(game,brickCenterX(10),belowBrick(10),0,-normalSpeed); game.applyPower(Power::Pierce);
        game.step(0.05f,200); game.step(0.05f,200);
        CHECK(game.bricks[10*COLS+10].hp==0); CHECK(game.bricks[9*COLS+10].hp==0);
        CHECK(game.balls[0].vy<0); CHECK(game.destroyed==2); ++cases;
    }
    {
        Game game; emptyField(game); brickAt(game,10,10);
        oneBall(game,brickCenterX(10),brickBottom(10)+BALL_RADIUS+20,0,-1200); game.step(0.05f,200);
        CHECK(game.bricks[10*COLS+10].hp==0); CHECK(game.balls[0].vy>0); ++cases;
    }
    {
        Game game; emptyField(game); game.bricks[0]={}; game.totalBricks=0; brickAt(game,10,10);
        oneBall(game,brickCenterX(10),belowBrick(10),0,-normalSpeed); game.step(0.03f,200);
        CHECK(game.scene==Scene::Cleared); CHECK(game.profile.unlocked==2); CHECK(game.remainingBricks()==0);
        CHECK(game.sounds&S_WIN); CHECK(game.score>=310); CHECK(game.profile.levelBest[0]==game.score);
        const float y=game.balls[0].y; game.step(1,0); CHECK(game.balls[0].y==y); ++cases;
    }
    {
        Game game; emptyField(game); game.level=LEVEL_COUNT-1; game.profile.unlocked=LEVEL_COUNT;
        game.bricks[0]={}; game.totalBricks=0; brickAt(game,10,10);
        oneBall(game,brickCenterX(10),belowBrick(10),0,-normalSpeed); game.step(0.03f,200);
        CHECK(game.scene==Scene::Finished); CHECK(game.profile.unlocked==LEVEL_COUNT); ++cases;
    }
    {
        Game game; emptyField(game); oneBall(game,100,FLOOR+1,0,normalSpeed);
        game.balls[1]={true,200,190,40,-140,200,190}; game.step(0.02f,300);
        CHECK(game.lives==3); CHECK(game.scene==Scene::Playing); CHECK(game.ballCount()==1);
        game.balls[1].y=FLOOR+2; game.balls[1].vy=normalSpeed; game.step(0.02f,300);
        CHECK(game.lives==2); CHECK(game.scene==Scene::Ready);
        for(int life=1;life>=0;--life) {
            game.launch(); game.balls[0].x=100; game.balls[0].y=FLOOR+2; game.balls[0].vy=normalSpeed;
            game.step(0.02f,300); CHECK(game.lives==life);
        }
        CHECK(game.scene==Scene::GameOver); CHECK(game.ballCount()==0); ++cases;
    }
    {
        Game game; emptyField(game); oneBall(game,200,190,40,-140);
        game.applyPower(Power::Split); CHECK(game.ballCount()==3);
        game.applyPower(Power::Split); CHECK(game.ballCount()==9);
        for(int i=0;i<8;++i) game.applyPower(Power::Split);
        CHECK(game.ballCount()==MAX_BALLS);
        for(const Ball& ball:game.balls) { CHECK(std::isfinite(ball.vx)); CHECK(std::fabs(ball.vy)>=normalSpeed*0.27f-0.01f); }
        for(int i=0;i<MAX_BALLS;i+=2) game.balls[i].active=false;
        CHECK(game.ballCount()==128); game.applyPower(Power::Split); CHECK(game.ballCount()==MAX_BALLS);
        CHECK(game.sounds&S_POWER); ++cases;
    }
    {
        Game game; emptyField(game); oneBall(game,200,190,40,-140);
        const float original=game.paddleWidth(); game.applyPower(Power::Wide);
        CHECK(game.paddleWidth()>original); CHECK(game.wideTimer==12);
        game.applyPower(Power::Pierce); CHECK(game.pierceTimer==8);
        game.applyPower(Power::Slow); CHECK(game.slowTimer==12);
        game.step(0.05f,0); CHECK(game.paddleX>=LEFT+game.paddleWidth()/2);
        CHECK(game.wideTimer<12); CHECK(game.slowTimer<12); CHECK(game.pierceTimer<8); ++cases;
    }
    {
        Game normal; emptyField(normal); oneBall(normal,200,190,40,-140);
        Game slow=normal; slow.applyPower(Power::Slow);
        normal.step(0.02f,200); slow.step(0.02f,200);
        CHECK(near((190-slow.balls[0].y)/(190-normal.balls[0].y),0.65f,0.005f)); ++cases;
    }
    {
        Game game; emptyField(game); oneBall(game,200,190,40,-140);
        game.drops[0]={true,200,PADDLE_Y-8,Power::Split}; game.step(0.02f,200);
        CHECK(!game.drops[0].active); CHECK(game.ballCount()==3); CHECK(game.score==25);
        game.drops[1]={true,30,FLOOR+7,Power::Wide}; game.step(0.03f,200);
        CHECK(!game.drops[1].active); CHECK(game.wideTimer==0); ++cases;
    }
    {
        for(int milestone:{6,18,36,96,192,288}) {
            Game game; emptyField(game); game.destroyed=milestone-1; game.totalBricks=milestone; brickAt(game,10,10);
            oneBall(game,brickCenterX(10),belowBrick(10),0,-normalSpeed); game.step(0.03f,200);
            CHECK(game.destroyed==milestone); bool split=false;
            for(const Drop& drop:game.drops) if(drop.active&&drop.kind==Power::Split) split=true;
            CHECK(split); CHECK(game.comboTimer>2.9f); CHECK(game.comboWallBudget==1.25f);
        }
        ++cases;
    }
    {
        Game game; game.startLevel(0); game.pause(); CHECK(game.scene==Scene::Paused);
        game.step(0.05f,40); CHECK(game.paddleX==200); game.resume(); CHECK(game.scene==Scene::Ready);
        game.launch(); game.applyPower(Power::Wide); game.pause();
        const Ball original=game.balls[0]; const float timer=game.wideTimer;
        game.step(1,40); CHECK(game.balls[0].x==original.x); CHECK(game.balls[0].y==original.y);
        CHECK(game.wideTimer==timer); game.resume(); CHECK(game.scene==Scene::Playing);
        game.returnToTitle(); CHECK(game.scene==Scene::Title); CHECK(game.sounds==0); ++cases;
    }
    {
        Game game; emptyField(game); oneBall(game,200,190,40,-140);
        const float x=game.balls[0].x; game.step(-1,200); CHECK(game.balls[0].x==x);
        game.step(std::numeric_limits<float>::infinity(),200); CHECK(game.balls[0].x==x);
        game.step(std::numeric_limits<float>::quiet_NaN(),200); CHECK(game.balls[0].x==x);
        game.step(100,200); CHECK(game.elapsed<=0.051f);
        game.step(0.01f,std::numeric_limits<float>::quiet_NaN()); CHECK(std::isfinite(game.paddleX)); ++cases;
    }
    {
        Game game; emptyField(game); game.profile.easy=true; oneBall(game,200,190,40,-140);
        CHECK(game.paddleWidth()>62); game.score=std::numeric_limits<uint32_t>::max()-1;
        game.drops[0]={true,200,PADDLE_Y-8,Power::Wide}; game.step(0.02f,200);
        CHECK(game.score==std::numeric_limits<uint32_t>::max()); CHECK(game.profile.bestScore==game.score); ++cases;
    }
    {
        Game game; emptyField(game); oneBall(game,200,190,40,-140);
        game.balls[0].x=std::numeric_limits<float>::quiet_NaN(); game.step(0.01f,200);
        CHECK(game.scene==Scene::Ready); CHECK(std::isfinite(game.balls[0].x)); ++cases;
    }
    {
        // Exercise every level with deterministic changing input and repeated
        // 256-ball bursts. Real collision code runs for 540 simulated seconds.
        uint32_t random=0x4ea15u; unsigned long long ballSteps=0;
        for(int level=0;level<LEVEL_COUNT;++level) {
            Game game; game.profile.unlocked=LEVEL_COUNT; game.startLevel(level); game.launch();
            for(int frame=0;frame<600;++frame) {
                if(game.scene==Scene::Ready) game.launch();
                if(game.scene==Scene::GameOver||game.scene==Scene::Cleared||game.scene==Scene::Finished) {
                    game.startLevel(level); game.launch();
                }
                if(frame%37==0) game.applyPower(Power::Split);
                if(frame%211==0) game.applyPower(static_cast<Power>(next(random)%4));
                float target=8+(next(random)%384);
                if(frame%3!=0) for(const Ball& ball:game.balls) if(ball.active&&ball.vy>0) { target=ball.x; break; }
                game.step(1.0f/120,target);
                CHECK(game.ballCount()<=MAX_BALLS); CHECK(game.remainingBricks()>=0);
                CHECK(game.destroyed<=game.totalBricks); CHECK(game.lives>=0);
                int actual=0; for(const Brick& brick:game.bricks) actual+=brick.hp>0;
                CHECK(actual==game.remainingBricks());
                for(const Ball& ball:game.balls) if(ball.active) {
                    ++ballSteps;
                    CHECK(std::isfinite(ball.x)); CHECK(std::isfinite(ball.y));
                    CHECK(std::isfinite(ball.vx)); CHECK(std::isfinite(ball.vy));
                    CHECK(ball.x>=LEFT+BALL_RADIUS-0.01f); CHECK(ball.x<=RIGHT-BALL_RADIUS+0.01f);
                    CHECK(ball.y>=CEILING+BALL_RADIUS-0.01f); CHECK(ball.y<=FLOOR+BALL_RADIUS+0.01f);
                }
                for(const Particle& p:game.particles) CHECK(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.life));
                game.sounds=0;
            }
        }
        CHECK(ballSteps>100000); std::cout << ballSteps << " active-ball frames; "; ++cases;
    }
    std::cout << cases << " scenarios, " << assertions << " assertions passed (ASan + UBSan).\n";
}
