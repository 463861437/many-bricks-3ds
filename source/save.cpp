#include "save.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
namespace bricks {
namespace {
constexpr int LEGACY_LEVELS=36, CURRENT_LEVELS=108;
static_assert(LEVEL_COUNT==CURRENT_LEVELS,"Update the save format when changing the level count");
constexpr size_t LEGACY_SIZE=24+LEGACY_LEVELS*4+4;
constexpr size_t SIZE=24+CURRENT_LEVELS*4+4;
using Bytes=std::array<unsigned char,SIZE>;
void put(Bytes& b,size_t off,uint32_t n) {for(unsigned i=0;i<4;++i)b[off+i]=static_cast<unsigned char>(n>>(i*8));}
uint32_t get(const Bytes& b,size_t off) {uint32_t n=0;for(unsigned i=0;i<4;++i)n|=uint32_t(b[off+i])<<(i*8);return n;}
uint32_t checksum(const Bytes& b,size_t size) {uint32_t v=2166136261u;for(size_t i=0;i<size-4;++i){v^=b[i];v*=16777619u;}return v;}
int readSlot(const std::string& path,Profile& profile,uint32_t& sequence) {
    FILE* f=std::fopen(path.c_str(),"rb");
    if(!f)return errno==ENOENT?0:-1;
    Bytes b{};
    const size_t read=std::fread(b.data(),1,b.size(),f);
    const int tail=std::fgetc(f);const bool failed=std::ferror(f)!=0;
    const int closed=std::fclose(f);
    if(tail!=EOF||failed||closed)return -1;
    int levels=0;
    if(read==LEGACY_SIZE&&std::memcmp(b.data(),"MB3DS001",8)==0)levels=LEGACY_LEVELS;
    else if(read==SIZE&&std::memcmp(b.data(),"MB3DS002",8)==0)levels=CURRENT_LEVELS;
    else return -1;
    if(get(b,read-4)!=checksum(b,read))return -1;
    const uint32_t unlocked=get(b,12), flags=get(b,20);
    if(unlocked<1||unlocked>static_cast<uint32_t>(levels)||flags>3)return -1;
    // A legacy save contains only 36 scores. Decode into a fresh profile so
    // newly added levels are zeroed and invalid files never partially apply.
    Profile decoded;
    decoded.unlocked=static_cast<int>(unlocked);decoded.bestScore=get(b,16);
    decoded.sound=(flags&1)!=0;decoded.easy=(flags&2)!=0;
    for(int i=0;i<levels;++i)decoded.levelBest[i]=get(b,24+i*4);
    profile=decoded;sequence=get(b,8);
    return 1;
}
bool directory(const std::string& path) {
    if(mkdir(path.c_str(),0777)==0)return true;
    struct stat s{};return errno==EEXIST&&stat(path.c_str(),&s)==0&&S_ISDIR(s.st_mode);
}
}
LoadStatus SaveStore::load(Profile& p) {
    Profile candidates[2];uint32_t seq[2]{};int state[2];
    for(int i=0;i<2;++i)state[i]=readSlot(directory_+"/progress"+char('0'+i)+".bin",candidates[i],seq[i]);
    active_=-1;sequence_=0;
    if(state[0]==1)active_=0;
    const uint32_t distance=seq[1]-seq[0];
    if(state[1]==1&&(active_<0||(distance!=0&&distance<0x80000000u)))active_=1;
    if(active_>=0) {p=candidates[active_];sequence_=seq[active_];return state[1-active_]<0?LoadStatus::Recovered:LoadStatus::Loaded;}
    return state[0]<0||state[1]<0?LoadStatus::Invalid:LoadStatus::Missing;
}
bool SaveStore::save(const Profile& p) {
    if(p.unlocked<1||p.unlocked>LEVEL_COUNT)return false;
    if(!directory(directory_))return false;
    Bytes b{};std::memcpy(b.data(),"MB3DS002",8);
    put(b,8,sequence_+1);put(b,12,static_cast<uint32_t>(p.unlocked));put(b,16,p.bestScore);
    put(b,20,(p.sound?1u:0u)|(p.easy?2u:0u));
    for(int i=0;i<LEVEL_COUNT;++i)put(b,24+i*4,p.levelBest[i]);
    put(b,SIZE-4,checksum(b,SIZE));
    const int next=active_==0?1:0;
    const std::string path=directory_+"/progress"+char('0'+next)+".bin",temp=path+".tmp";
    FILE* f=std::fopen(temp.c_str(),"wb");if(!f)return false;
    const bool written=std::fwrite(b.data(),1,b.size(),f)==b.size();
    const bool flushed=std::fflush(f)==0;const bool closed=std::fclose(f)==0;
    if(!written||!flushed||!closed){std::remove(temp.c_str());return false;}
    // The other slot remains intact even if power fails during replacement.
    if(std::remove(path.c_str())!=0&&errno!=ENOENT){std::remove(temp.c_str());return false;}
    if(std::rename(temp.c_str(),path.c_str())!=0){std::remove(temp.c_str());return false;}
    sequence_++;active_=next;return true;
}
}
