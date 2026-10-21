#include "controls.hpp"
#include "save.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <vector>
#include <unistd.h>
using namespace bricks;
static int checks=0;
#define CHECK(x) do {if(!(x)){std::cerr<<"Failed at "<<__LINE__<<": "<<#x<<"\n";std::abort();}++checks;} while(0)
using Fixture=std::vector<unsigned char>;
static void word(Fixture& bytes,size_t offset,uint32_t value) {
    for(unsigned i=0;i<4;++i)bytes.at(offset+i)=static_cast<unsigned char>(value>>(i*8));
}
static void seal(Fixture& bytes) {
    uint32_t hash=2166136261u;
    for(size_t i=0;i<bytes.size()-4;++i){hash^=bytes[i];hash*=16777619u;}
    word(bytes,bytes.size()-4,hash);
}
// Build the actual on-disk formats, independently of SaveStore's encoder.
// Version 1 is permanently 172 bytes, even after LEVEL_COUNT grows.
static Fixture fixture(bool legacy,uint32_t sequence,uint32_t unlocked) {
    const size_t levels=legacy?36:108;
    Fixture bytes(24+levels*4+4);
    const char* magic=legacy?"MB3DS001":"MB3DS002";
    std::copy(magic,magic+8,bytes.begin());
    word(bytes,8,sequence);word(bytes,12,unlocked);word(bytes,16,765432);
    word(bytes,20,2); // Sound off, easy mode on.
    for(size_t i=0;i<levels;++i)word(bytes,24+i*4,static_cast<uint32_t>(1000+i*11));
    seal(bytes);return bytes;
}
static void writeFixture(const std::string& path,const Fixture& bytes) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    out.close();CHECK(!out.fail());
}
static Fixture readFixture(const std::string& path) {
    std::ifstream in(path,std::ios::binary);
    return Fixture(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
}
static bool sameProfile(const Profile& a,const Profile& b) {
    return a.unlocked==b.unlocked&&a.bestScore==b.bestScore&&a.levelBest==b.levelBest&&
           a.sound==b.sound&&a.easy==b.easy;
}
static void saveCompatibility(const std::string& root) {
    const std::string dir=root+"/migration";
    CHECK(std::filesystem::create_directory(dir));
    const std::string slot0=dir+"/progress0.bin",slot1=dir+"/progress1.bin";
    const Fixture legacy=fixture(true,41,36);
    CHECK(legacy.size()==172);
    writeFixture(slot0,legacy);
    Profile migrated;migrated.levelBest.fill(999999);
    SaveStore store(dir);CHECK(store.load(migrated)==LoadStatus::Loaded);
    CHECK(migrated.unlocked==36&&migrated.bestScore==765432&&!migrated.sound&&migrated.easy);
    for(int i=0;i<36;++i)CHECK(migrated.levelBest[i]==static_cast<uint32_t>(1000+i*11));
    for(int i=36;i<LEVEL_COUNT;++i)CHECK(migrated.levelBest[i]==0);
    CHECK(store.save(migrated));
    CHECK(readFixture(slot0)==legacy); // First migration preserves the old active slot.
    const Fixture current=readFixture(slot1);
    CHECK(current.size()==460&&std::string(current.begin(),current.begin()+8)=="MB3DS002");
    CHECK(current.at(8)==42);
    Profile loaded;SaveStore mixed(dir);CHECK(mixed.load(loaded)==LoadStatus::Loaded);
    CHECK(sameProfile(migrated,loaded));
    loaded.unlocked=LEVEL_COUNT;loaded.levelBest[36]=123456;loaded.levelBest[LEVEL_COUNT-1]=987654;
    CHECK(mixed.save(loaded));
    Profile last;SaveStore expanded(dir);CHECK(expanded.load(last)==LoadStatus::Loaded);
    CHECK(sameProfile(last,loaded));
    CHECK(readFixture(slot0).size()==460);
    // An invalid new-format slot recovers the untouched original v1 slot.
    writeFixture(slot0,legacy);Fixture damaged=current;damaged.at(100)^=0x80;
    writeFixture(slot1,damaged);SaveStore recovery(dir);
    CHECK(recovery.load(loaded)==LoadStatus::Recovered);CHECK(sameProfile(loaded,migrated));
    // Selection is by sequence, not format version or slot number.
    writeFixture(slot0,fixture(false,40,108));writeFixture(slot1,fixture(true,41,12));
    SaveStore newestLegacy(dir);CHECK(newestLegacy.load(loaded)==LoadStatus::Loaded);
    CHECK(loaded.unlocked==12&&loaded.levelBest[36]==0);
    CHECK(newestLegacy.save(loaded));CHECK(readFixture(slot1)==fixture(true,41,12));
    SaveStore newestCurrent(dir);CHECK(newestCurrent.load(loaded)==LoadStatus::Loaded);
    CHECK(loaded.unlocked==12&&readFixture(slot0).at(8)==42);
    // Sequence rollover must still choose the newer completed slot.
    writeFixture(slot0,fixture(true,0xffffffffu,20));writeFixture(slot1,fixture(false,0,108));
    SaveStore wrapped(dir);CHECK(wrapped.load(loaded)==LoadStatus::Loaded);CHECK(loaded.unlocked==108);
    CHECK(wrapped.save(loaded));CHECK(readFixture(slot0).at(8)==1);
    writeFixture(slot0,fixture(false,0,107));writeFixture(slot1,fixture(true,0xffffffffu,20));
    SaveStore reverseWrap(dir);CHECK(reverseWrap.load(loaded)==LoadStatus::Loaded);CHECK(loaded.unlocked==107);
    // Validate exact sizes, magic/version, checksum and format-specific limits.
    std::filesystem::remove(slot1);
    std::vector<Fixture> invalid;
    damaged=legacy;damaged.pop_back();invalid.push_back(damaged);
    damaged=current;damaged.push_back(0);invalid.push_back(damaged);
    damaged=legacy;damaged[7]='2';seal(damaged);invalid.push_back(damaged);
    damaged=current;damaged[7]='1';seal(damaged);invalid.push_back(damaged);
    damaged=current;damaged[7]='3';seal(damaged);invalid.push_back(damaged);
    damaged=legacy;damaged[40]^=1;invalid.push_back(damaged);
    damaged=legacy;word(damaged,12,37);seal(damaged);invalid.push_back(damaged);
    damaged=current;word(damaged,12,109);seal(damaged);invalid.push_back(damaged);
    damaged=current;word(damaged,12,0);seal(damaged);invalid.push_back(damaged);
    damaged=current;word(damaged,20,4);seal(damaged);invalid.push_back(damaged);
    Profile sentinel;sentinel.unlocked=75;sentinel.bestScore=4242;sentinel.levelBest.fill(123);
    sentinel.sound=false;sentinel.easy=true;
    for(const Fixture& bytes:invalid) {
        writeFixture(slot0,bytes);loaded=sentinel;SaveStore bad(dir);
        CHECK(bad.load(loaded)==LoadStatus::Invalid);CHECK(sameProfile(loaded,sentinel));
    }
}
static void lastPageControls() {
    Game g;View v;Controller c;
    g.profile.unlocked=LEVEL_COUNT;g.level=0;
    Input b;b.pressed=K_B;Input a;a.pressed=K_A;
    Input right;right.pressed=K_R;Input left;left.pressed=K_L;
    c.update(g,v,b,0.016f);CHECK(g.scene==Scene::Select&&v.page==0);
    const int pages=(LEVEL_COUNT+11)/12;
    for(int page=1;page<pages;++page) {
        c.update(g,v,right,0.016f);CHECK(v.page==page&&v.selected==page*12);
    }
    c.update(g,v,right,0.016f);CHECK(v.page==pages-1&&v.selected==96);
    Input step;step.pressed=K_RIGHT;
    for(int n=0;n<11;++n)c.update(g,v,step,0.016f);
    CHECK(v.selected==107&&v.page==8);
    c.update(g,v,step,0.016f);CHECK(v.selected==107);
    Input down;down.pressed=K_DOWN;c.update(g,v,down,0.016f);CHECK(v.selected==107);
    c.update(g,v,a,0.016f);CHECK(g.scene==Scene::Ready&&g.level==107);
    g.returnToTitle();c.update(g,v,b,0.016f);CHECK(v.selected==107&&v.page==8);
    Input touch;touch.touching=true;touch.touchDown=true;touch.x=250;touch.y=145;
    c.update(g,v,touch,0.016f);CHECK(g.scene==Scene::Ready&&g.level==107);
    // A locked last level can be highlighted but cannot be launched by either input.
    g.returnToTitle();g.profile.unlocked=107;c.update(g,v,b,0.016f);
    c.update(g,v,touch,0.016f);CHECK(g.scene==Scene::Select&&v.selected==107);
    c.update(g,v,a,0.016f);CHECK(g.scene==Scene::Select);
    touch.x=175;c.update(g,v,touch,0.016f);CHECK(g.scene==Scene::Ready&&g.level==106);
    // Page buttons work by touch too, including attempts to pass both ends.
    g.returnToTitle();c.update(g,v,b,0.016f);touch.x=30;touch.y=195;
    for(int n=0;n<pages+1;++n)c.update(g,v,touch,0.016f);
    CHECK(v.page==0&&v.selected==0);
    touch.x=250;
    for(int n=0;n<pages+1;++n)c.update(g,v,touch,0.016f);
    CHECK(v.page==8&&v.selected==96);
    c.update(g,v,left,0.016f);CHECK(v.page==7&&v.selected==84);
    // Stale/out-of-range view state is normalized before arithmetic or selection.
    v.page=std::numeric_limits<int>::max();v.selected=std::numeric_limits<int>::max();
    c.update(g,v,step,0.016f);CHECK(v.page==8&&v.selected==107&&g.scene==Scene::Select);
    v.page=-100;v.selected=-100;Input prev;prev.pressed=K_LEFT;
    c.update(g,v,prev,0.016f);CHECK(v.page==0&&v.selected==0);
}
int main() {
    char temp[]="/tmp/many-bricks-save-XXXXXX";CHECK(mkdtemp(temp)!=nullptr);
    const std::string dir=std::string(temp)+"/saves";
    Profile p;SaveStore store(dir);CHECK(store.load(p)==LoadStatus::Missing);
    p.unlocked=9;p.bestScore=7500;p.levelBest[8]=7000;p.sound=false;p.easy=true;
    CHECK(store.save(p));Profile loaded;SaveStore roundtrip(dir);
    CHECK(roundtrip.load(loaded)==LoadStatus::Loaded);
    CHECK(loaded.unlocked==9&&loaded.bestScore==7500&&loaded.levelBest[8]==7000&&!loaded.sound&&loaded.easy);
    p.unlocked=10;CHECK(store.save(p));p.unlocked=11;CHECK(store.save(p));
    SaveStore latest(dir);CHECK(latest.load(loaded)==LoadStatus::Loaded);CHECK(loaded.unlocked==11);
    // Truncation of the newest slot must recover the previous completed save.
    {std::ofstream bad(dir+"/progress0.bin",std::ios::trunc);bad<<"broken";}
    SaveStore recovery(dir);CHECK(recovery.load(loaded)==LoadStatus::Recovered);CHECK(loaded.unlocked==10);
    // An interrupted temporary file is never considered committed progress.
    {std::ofstream partial(dir+"/progress0.bin.tmp",std::ios::trunc);partial<<"partial";}
    CHECK(recovery.load(loaded)==LoadStatus::Recovered);CHECK(loaded.unlocked==10);
    loaded.unlocked=12;CHECK(recovery.save(loaded));SaveStore repaired(dir);
    CHECK(repaired.load(loaded)==LoadStatus::Loaded);CHECK(loaded.unlocked==12);
    loaded.unlocked=LEVEL_COUNT+1;CHECK(!repaired.save(loaded));
    loaded.unlocked=0;CHECK(!repaired.save(loaded));
    {std::ofstream bad(dir+"/progress0.bin",std::ios::trunc);bad<<"bad";}
    {std::ofstream bad(dir+"/progress1.bin",std::ios::trunc);bad<<"bad";}
    loaded=Profile{};SaveStore corrupt(dir);CHECK(corrupt.load(loaded)==LoadStatus::Invalid);CHECK(loaded.unlocked==1);
    {std::ofstream file(std::string(temp)+"/file");file<<"not a directory";}
    SaveStore blocked(std::string(temp)+"/file");CHECK(!blocked.save(Profile{}));
    saveCompatibility(temp);
    std::filesystem::remove_all(temp);

    Game g;View v;Controller c;
    Input a;a.pressed=K_A;c.update(g,v,a,0.016f);CHECK(g.scene==Scene::Ready);
    Input held;held.held=K_A;c.update(g,v,held,0.016f);CHECK(g.scene==Scene::Ready);
    c.update(g,v,a,0.016f);CHECK(g.scene==Scene::Playing);
    Input b;b.pressed=K_B;c.update(g,v,b,0.016f);CHECK(g.scene==Scene::Paused);
    c.update(g,v,a,0.016f);CHECK(g.scene==Scene::Playing);
    Input touch;touch.touching=true;touch.touchDown=true;touch.x=307;touch.y=100;
    c.update(g,v,touch,0.016f);CHECK(c.target>350&&c.target<=RIGHT-g.paddleWidth()/2);
    touch.x=15;c.update(g,v,touch,0.016f);CHECK(c.target<50);
    Input select;select.pressed=K_SELECT;c.update(g,v,select,0.016f);CHECK(v.quitConfirm&&g.scene==Scene::Paused&&!c.quit);
    c.update(g,v,b,0.016f);CHECK(!v.quitConfirm&&!c.quit);c.update(g,v,a,0.016f);CHECK(g.scene==Scene::Playing);
    g.returnToTitle();c.update(g,v,b,0.016f);CHECK(g.scene==Scene::Select);
    touch.x=90;touch.y=60;c.update(g,v,touch,0.016f);CHECK(g.scene==Scene::Select&&v.selected==1);
    touch.x=20;c.update(g,v,touch,0.016f);CHECK(g.scene==Scene::Ready);
    Input start;start.pressed=K_START;c.update(g,v,start,0.016f);CHECK(g.scene==Scene::Paused);
    c.update(g,v,a,0.016f);CHECK(g.scene==Scene::Ready);
    g.returnToTitle();Input toggle;toggle.pressed=K_X|K_Y;c.update(g,v,toggle,0.016f);
    CHECK(!g.profile.sound&&g.profile.easy&&g.dirty);
    c.update(g,v,select,0.016f);c.update(g,v,a,0.016f);CHECK(c.quit);
    lastPageControls();
    std::cout<<checks<<" persistence and input checks passed\n";
}
