#include <3ds.h>
#include "game.hpp"
#include "render.hpp"
#include "save.hpp"
#include "audio.hpp"
#include <cmath>
#include <iostream>
#include <string>
#include <sys/stat.h>

int bricksProgramMain();
static int checks=0;
#define CHECK(condition) do { if (!(condition)) { \
    std::cerr<<"Main-loop check failed at "<<__LINE__<<": "<<#condition<<'\n'; std::abort(); \
    } ++checks; } while(0)
struct Snapshot { bricks::Scene scene; float elapsed; bool confirm; int saves; };
static std::vector<Snapshot> snapshots;
static std::string scenario;
extern "C" int bricks_test_mkdir(const char*,mode_t) { return 0; }
namespace bricks {
LoadStatus SaveStore::load(Profile&) { sequence_=0;active_=-1;return LoadStatus::Missing; }
bool SaveStore::save(const Profile&) { ++sequence_;active_=active_==0?1:0;++mainMock::saves;return true; }
bool audioInit() { ++mainMock::audioStarts; mainMock::audioLive=true; return true; }
void audioPlay(unsigned,bool) { CHECK(mainMock::audioLive); }
void audioStop() { CHECK(mainMock::audioLive); ++mainMock::audioStops; }
void audioExit() { CHECK(mainMock::audioLive); mainMock::audioLive=false; ++mainMock::audioExits; }
void render(const Game& g,const View& v,Surface,Surface) {
    snapshots.push_back({g.scene,g.elapsed,v.quitConfirm,mainMock::saves});
    // A pending changed profile exercises the real main loop's save scheduling.
    if(scenario=="save"&&mainMock::frame==2)const_cast<Game&>(g).dirty=true;
}
}
static mainMock::Frame press(u32 key) { return {key,key}; }
static mainMock::Frame held(u32 key) { return {key,0}; }
static mainMock::Frame touch(int x,int y,bool down=true) { return {KEY_TOUCH,down?KEY_TOUCH:0,x,y}; }
int main(int argc,char** argv) {
    CHECK(argc==2); scenario=argv[1];
    using bricks::Scene;
    using mainMock::frames;
    if(scenario=="keys") {
        frames={press(KEY_A),held(KEY_A),{},press(KEY_A),press(KEY_B),{},press(KEY_B),held(KEY_B),{},press(KEY_A)};
    } else if(scenario=="touch") {
        frames={touch(30,90),touch(30,205,false),{},touch(30,205),touch(180,205,false),{},
                touch(180,205),touch(30,130,false),{},touch(30,130)};
    } else if(scenario=="home"||scenario=="sleep") {
        unsigned hooks=scenario=="home"?(1u<<APTHOOK_ONSUSPEND)|(1u<<APTHOOK_ONRESTORE):
                                            (1u<<APTHOOK_ONSLEEP)|(1u<<APTHOOK_ONWAKEUP);
        frames={press(KEY_A),{},press(KEY_A),{}, {KEY_A,KEY_A,0,0,10000,hooks},held(KEY_A),{},press(KEY_A)};
    } else if(scenario=="dt") {
        frames={press(KEY_A),{},press(KEY_A),{0,0,0,0,1000},{0,0,0,0,0},{0,0,0,0,1}};
    } else if(scenario=="quit") {
        frames={press(KEY_X),{},press(KEY_SELECT),{},press(KEY_A)};
    } else if(scenario=="save") {
        frames={press(KEY_A),{},press(KEY_A),{},press(KEY_B)};
    } else CHECK(false);
    CHECK(bricksProgramMain()==0);
    CHECK(mainMock::gfxStarts==1&&mainMock::gfxStops==1);
    CHECK(mainMock::audioStarts==1&&mainMock::audioExits==1);
    CHECK(mainMock::unhooks==1&&mainMock::hook==nullptr);
    CHECK(mainMock::swaps==int(snapshots.size()));
    if(scenario=="keys") {
        CHECK(snapshots.size()==10);
        CHECK(snapshots[0].scene==Scene::Ready&&snapshots[1].scene==Scene::Ready);
        CHECK(snapshots[3].scene==Scene::Playing);
        // A fresh different key is also gated until the preceding action releases.
        CHECK(snapshots[4].scene==Scene::Playing);
        CHECK(snapshots[6].scene==Scene::Paused&&snapshots[7].scene==Scene::Paused);
        CHECK(snapshots[9].scene==Scene::Playing);
    } else if(scenario=="touch") {
        CHECK(snapshots.size()==10);
        CHECK(snapshots[0].scene==Scene::Ready&&snapshots[1].scene==Scene::Ready);
        CHECK(snapshots[3].scene==Scene::Playing&&snapshots[4].scene==Scene::Playing);
        CHECK(snapshots[6].scene==Scene::Paused&&snapshots[7].scene==Scene::Paused);
        CHECK(snapshots[9].scene==Scene::Playing);
    } else if(scenario=="home"||scenario=="sleep") {
        CHECK(snapshots.size()==8);
        CHECK(snapshots[3].scene==Scene::Playing);
        CHECK(snapshots[4].scene==Scene::Paused&&snapshots[5].scene==Scene::Paused&&snapshots[6].scene==Scene::Paused);
        CHECK(std::fabs(snapshots[3].elapsed-snapshots[6].elapsed)<0.00001f);
        CHECK(snapshots[7].scene==Scene::Playing);
        CHECK(snapshots[7].elapsed-snapshots[6].elapsed<=0.017f);
        CHECK(mainMock::audioStops>=4);
    } else if(scenario=="dt") {
        CHECK(snapshots.size()==6);
        const float elapsed=snapshots[3].elapsed-snapshots[2].elapsed;
        CHECK(elapsed>0.04f&&elapsed<=0.0501f);
        CHECK(snapshots[4].elapsed==snapshots[3].elapsed);
        CHECK(snapshots[5].elapsed-snapshots[4].elapsed<=1.0f/120+0.0001f);
    } else if(scenario=="quit") {
        CHECK(snapshots.size()==4);
        CHECK(snapshots[2].confirm&&snapshots[3].confirm);
        CHECK(mainMock::saves==1);
    } else if(scenario=="save") {
        CHECK(snapshots.size()==5);
        CHECK(snapshots[3].scene==Scene::Playing&&snapshots[3].saves==0);
        CHECK(snapshots[4].scene==Scene::Paused&&snapshots[4].saves==1);
    }
    std::cout<<scenario<<": "<<checks<<" actual main-loop checks passed\n";
}
