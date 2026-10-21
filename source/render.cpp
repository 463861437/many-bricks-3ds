#include "render.hpp"
#include "font_data.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace bricks {
namespace {
using Color = uint32_t;
constexpr Color BG=0x0D1427, PANEL=0x141F35, RAISED=0x1C2941, LINE=0x2C3C55;
constexpr Color WHITE=0xE9F4F4, MUTED=0x879AB5, MINT=0x58E0C1, CORAL=0xFF887B, GOLD=0xF2CF75;
constexpr Color COLORS[]={MINT,0x7BBDFF,CORAL,GOLD,0xB29BED,0x76C6CF};
struct Rect {int x,y,w,h;};
class Canvas {
    Surface s;
public:
    explicit Canvas(Surface surface):s(surface){}
    void pixel(int x,int y,Color color,uint8_t alpha=255){
        if(!s.pixels||x<0||y<0||x>=s.width||y>=s.height)return;
        uint8_t* p=s.pixels+(static_cast<size_t>(x)*s.height+s.height-1-y)*3;
        const uint8_t v[]={static_cast<uint8_t>(color),static_cast<uint8_t>(color>>8),static_cast<uint8_t>(color>>16)};
        if(alpha==255){p[0]=v[0];p[1]=v[1];p[2]=v[2];return;}
        for(int i=0;i<3;++i)p[i]=static_cast<uint8_t>((v[i]*alpha+p[i]*(255-alpha)+127)/255);
    }
    void rect(int x,int y,int w,int h,Color color){
        if(!s.pixels||w<=0||h<=0)return;
        const int r=std::min(x+w,s.width),b=std::min(y+h,s.height);
        if(r<=std::max(0,x)||b<=std::max(0,y))return;
        for(int px=std::max(0,x);px<r;++px){
            uint8_t* p=s.pixels+(static_cast<size_t>(px)*s.height+s.height-b)*3;
            for(int py=std::max(0,y);py<b;++py){
                p[0]=static_cast<uint8_t>(color);p[1]=static_cast<uint8_t>(color>>8);p[2]=static_cast<uint8_t>(color>>16);p+=3;
            }
        }
    }
    void round(Rect r,int rad,Color color){
        rad=std::max(0,std::min(rad,std::min(r.w,r.h)/2));
        for(int py=0;py<r.h;++py){
            const int cy=py<rad?rad-1-py:(py>=r.h-rad?py-(r.h-rad):0);
            for(int px=0;px<r.w;++px){
                const int cx=px<rad?rad-1-px:(px>=r.w-rad?px-(r.w-rad):0);
                if(cx*cx+cy*cy<=rad*rad)pixel(r.x+px,r.y+py,color);
            }
        }
    }
    void panel(Rect r,Color fill=PANEL,Color border=LINE,int radius=6){
        round(r,radius,border);round({r.x+1,r.y+1,r.w-2,r.h-2},std::max(0,radius-1),fill);
    }
    void circle(int x,int y,int r,Color color,uint8_t alpha=255){
        for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx)
            if(dx*dx+dy*dy<=r*r)pixel(x+dx,y+dy,color,alpha);
    }
    void line(int x,int y,int ex,int ey,Color color,uint8_t alpha=255){
        const int dx=std::abs(ex-x),sx=x<ex?1:-1,dy=-std::abs(ey-y),sy=y<ey?1:-1;
        int error=dx+dy;
        for(int limit=0;limit<2048;++limit){
            pixel(x,y,color,alpha);if(x==ex&&y==ey)break;
            const int e2=2*error;if(e2>=dy){error+=dy;x+=sx;}if(e2<=dx){error+=dx;y+=sy;}
        }
    }
    void veil(Rect r,uint8_t alpha){
        for(int y=std::max(0,r.y);y<std::min(s.height,r.y+r.h);++y)
            for(int x=std::max(0,r.x);x<std::min(s.width,r.x+r.w);++x)pixel(x,y,BG,alpha);
    }
};
uint32_t decode(const char*& p){
    const uint8_t first=static_cast<uint8_t>(*p++);
    if(first<0x80)return first;
    unsigned count=0;uint32_t cp=0,min=0;
    if(first>=0xC2&&first<=0xDF){count=1;cp=first&31;min=0x80;}
    else if(first>=0xE0&&first<=0xEF){count=2;cp=first&15;min=0x800;}
    else if(first>=0xF0&&first<=0xF4){count=3;cp=first&7;min=0x10000;}
    else return '?';
    for(unsigned i=0;i<count;++i){const uint8_t v=static_cast<uint8_t>(*p);if((v&0xC0)!=0x80)return '?';cp=(cp<<6)|(v&63);++p;}
    return cp<min||cp>0x10FFFF||(cp>=0xD800&&cp<=0xDFFF)?'?':cp;
}
const FontGlyph* glyph(uint32_t cp){
    size_t lo=0,hi=FONT_COUNT;
    while(lo<hi){size_t mid=lo+(hi-lo)/2;if(FONT_GLYPHS[mid].codepoint<cp)lo=mid+1;else hi=mid;}
    if(lo<FONT_COUNT&&FONT_GLYPHS[lo].codepoint==cp)return &FONT_GLYPHS[lo];
    return cp=='?'?nullptr:glyph('?');
}
int width(const char* s,int scale=1){int w=0;while(*s){const FontGlyph* g=glyph(decode(s));if(g)w+=g->width*scale;}return w;}
void text(Canvas& c,int x,int y,const char* s,Color color=WHITE,int maxWidth=400,int scale=1){
    const int end=x+maxWidth;
    while(*s&&x<end){const FontGlyph* g=glyph(decode(s));if(!g)continue;
        if(x+g->width*scale>end)break;
        for(int gy=0;gy<14;++gy) {
            for(int gx=0;gx<g->width;++gx){const uint8_t a=g->alpha[gy*14+gx];if(!a)continue;
                for(int sy=0;sy<scale;++sy)for(int sx=0;sx<scale;++sx)c.pixel(x+gx*scale+sx,y+gy*scale+sy,color,a);
            }
        }
        x+=g->width*scale;
    }
}
void centered(Canvas& c,Rect r,const char* s,Color color=WHITE,int scale=1){text(c,r.x+std::max(0,(r.w-width(s,scale))/2),r.y+(r.h-14*scale)/2,s,color,r.w,scale);}
void button(Canvas& c,Rect r,const char* title,bool primary=false){
    c.panel(r,primary?MINT:RAISED,primary?MINT:LINE,7);centered(c,r,title,primary?BG:WHITE);
}
void dots(Canvas& c,int w,int fromY=0){
    for(int x=12;x<w;x+=16)for(int y=fromY+12;y<240;y+=16)c.pixel(x,y,LINE,70);
}
void number(Canvas& c,int x,int y,unsigned n,Color color=WHITE,int maxWidth=100){char s[32];std::snprintf(s,sizeof(s),"%u",n);text(c,x,y,s,color,maxWidth);}
void pill(Canvas& c,Rect r,const char* value,Color color=MINT){c.round(r,5,RAISED);centered(c,r,value,color);}
void footer(Canvas& c,const View& view,const char* hint){
    if(view.saveFailed)centered(c,{8,222,304,17},"进度未能保存，请检查 SD 卡",CORAL);
    else if(!view.audioAvailable)centered(c,{8,222,304,17},"当前无声运行",MUTED);
    else centered(c,{8,222,304,17},hint,MUTED);
}
void powerIcon(Canvas& c,int x,int y,Power kind){
    const Color colors[]={MINT,GOLD,CORAL,0x7BBDFF};
    const unsigned k=std::min(3u,static_cast<unsigned>(kind));
    c.round({x-6,y-6,13,13},3,colors[k]);
    if(kind==Power::Split){c.circle(x-3,y-2,1,BG);c.circle(x+3,y-2,1,BG);c.circle(x,y+3,1,BG);}
    else if(kind==Power::Wide){c.line(x-4,y,x+4,y,BG);c.line(x-4,y,x-2,y-2,BG);c.line(x-4,y,x-2,y+2,BG);c.line(x+4,y,x+2,y-2,BG);c.line(x+4,y,x+2,y+2,BG);}
    else if(kind==Power::Pierce){c.line(x-2,y+4,x+2,y-4,BG);c.line(x-2,y+4,x-3,y+1,BG);c.line(x+2,y-4,x+3,y-1,BG);}
    else{c.circle(x,y,4,BG);c.circle(x,y,3,colors[k]);c.line(x,y,x,y-2,BG);c.line(x,y,x+2,y,BG);}
}
void hero(Canvas& c,const Game& game){
    c.rect(0,0,400,240,BG);dots(c,400);
    text(c,24,13,"XIAOGUAI / ARCADE 01",MUTED,300);pill(c,{343,12,34,19},"3DS");
    text(c,24,43,"万砖齐发",WHITE,260,2);text(c,25,80,"MANY BRICKS 3DS",MINT,275);
    text(c,25,104,"一颗起手，满屏齐发。",MUTED,280);
    c.circle(331,75,23,0x182F42);c.circle(331,75,14,0x244C59);c.circle(331,75,7,MINT);
    c.circle(328,72,2,WHITE);c.circle(301,105,3,WHITE);c.circle(356,45,2,CORAL);
    c.line(300,111,288,131,MINT,130);c.line(352,51,345,59,CORAL,100);
    for(int r=0;r<12;++r)for(int col=0;col<70;++col){
        if((col>52&&r>7)||(col<12&&r>8)||((col+r*3)%23==0))continue;
        const Color fill=COLORS[(r/2+col/12)%6];
        c.rect(26+col*5,135+r*5,4,4,fill);c.pixel(26+col*5,135+r*5,WHITE,90);
    }
    for(int i=0;i<13;++i){const int x=72+(i*79)%270,y=190+(i*17)%29;c.line(x-3,y+5,x,y,WHITE,65);c.circle(x,y,1,WHITE);}
    c.round({166,225,68,6},3,MINT);c.circle(200,217,2,WHITE);
    if(game.profile.bestScore){text(c,22,221,"最高",MUTED,30);number(c,56,221,game.profile.bestScore,GOLD,100);}
}
void board(Canvas& c,const Game& g){
    c.rect(0,0,400,240,BG);dots(c,400,30);
    c.rect(0,0,400,30,PANEL);text(c,12,8,"分数",MUTED,31);number(c,46,8,g.score,WHITE,107);
    text(c,159,8,"砖块",MUTED,31);number(c,194,8,static_cast<unsigned>(std::max(0,g.remainingBricks())),MINT,49);
    char level[24];std::snprintf(level,sizeof(level),"%02d / %d",g.level+1,LEVEL_COUNT);text(c,263,8,level,MUTED,73);
    for(int i=0;i<std::min(3,std::max(0,g.lives));++i){c.circle(345+i*15,15,3,CORAL);c.circle(344+i*15,14,1,WHITE);}
    c.rect(7,31,1,209,LINE);c.rect(392,31,1,209,LINE);c.rect(8,30,384,1,LINE);
    for(int r=0;r<ROWS;++r)for(int col=0;col<COLS;++col){
        const Brick& b=g.bricks[r*COLS+col];if(!b.hp)continue;
        const int x=static_cast<int>(BRICK_X)+col*static_cast<int>(BRICK_W)+1,y=static_cast<int>(BRICK_Y)+r*static_cast<int>(BRICK_H)+1;
        const int w=static_cast<int>(BRICK_W)-1,h=static_cast<int>(BRICK_H)-1;
        const Color color=COLORS[b.color%6];c.rect(x,y,w,h,color);
        // One highlight keeps tiny square faces colourful and distinct.
        c.pixel(x,y,WHITE,80);
        if(b.hp>1)c.pixel(x+w-1,y+h-1,0x253956);
    }
    // Steel rails are solid obstacles; their openings remain visibly clear.
    for(const Wall& wall:g.walls)if(wall.active){
        const int x=static_cast<int>(wall.x),y=static_cast<int>(wall.y);
        const int w=static_cast<int>(wall.w),h=static_cast<int>(wall.h);
        c.rect(x,y,w,h,g.wallFlash>0?WHITE:0xA1AEC6);
        if(w>h){for(int px=x+3;px<x+w-1;px+=7)c.pixel(px,y+h/2,LINE);}
        else {for(int py=y+3;py<y+h-1;py+=7)c.pixel(x+w/2,py,LINE);}
    }
    if(g.combo>=8&&g.comboTimer>0){char s[32];std::snprintf(s,sizeof(s),"%d 连击",g.combo);pill(c,{297,180,86,22},s,GOLD);}
    for(const Particle& p:g.particles)if(p.life>0&&std::isfinite(p.x)&&std::isfinite(p.y))
        c.pixel(static_cast<int>(p.x),static_cast<int>(p.y),COLORS[p.color%6],static_cast<uint8_t>(std::min(255.0f,p.life*700)));
    for(const Drop& d:g.drops)if(d.active)powerIcon(c,static_cast<int>(d.x),static_cast<int>(d.y),d.kind);
    for(const Ball& b:g.balls)if(b.active){
        const int x=static_cast<int>(std::round(b.x)),y=static_cast<int>(std::round(b.y));
        const Color color=g.pierceTimer>0?CORAL:WHITE;
        c.line(static_cast<int>(b.previousX),static_cast<int>(b.previousY),x,y,color,70);c.circle(x,y,2,color);c.pixel(x-1,y-1,WHITE);
    }
    const int pw=static_cast<int>(g.paddleWidth()),px=static_cast<int>(std::round(g.paddleX))-pw/2;
    c.round({px-3,213,pw+6,10},5,0x163A42);c.round({px,215,pw,6},3,MINT);c.rect(px+5,215,std::max(0,pw-10),1,WHITE);
    c.rect(12,235,376,1,LINE);c.rect(12,235,static_cast<int>(376.0f*std::clamp(g.destroyed/static_cast<float>(std::max(1,g.totalBricks)),0.0f,1.0f)),1,MINT);
    if(g.scene==Scene::Ready){c.panel({127,177,146,27},PANEL,MINT);centered(c,{127,177,146,27},"准备好了就发球",MINT);}
}
void titleBottom(Canvas& c,const Game& g,const View& v){
    char levels[24];std::snprintf(levels,sizeof(levels),"%d 关",LEVEL_COUNT);
    text(c,20,15,"多球打砖块",WHITE,190);pill(c,{242,14,58,21},levels,GOLD);
    text(c,20,40,"方块密阵 · 回廊反弹 · 多球连击",MUTED,282);
    char title[64];if(g.profile.unlocked>1)std::snprintf(title,sizeof(title),"继续挑战  /  关卡 %02d",std::clamp(g.profile.unlocked,1,LEVEL_COUNT));
    else std::snprintf(title,sizeof(title),"开始挑战  /  关卡 01");
    button(c,{20,74,280,42},title,true);button(c,{20,124,280,36},"选择关卡");
    button(c,{20,170,134,28},g.profile.sound?"X 音效：开":"X 音效：关");
    button(c,{166,170,134,28},g.profile.easy?"Y 宽松模式":"Y 标准模式");
    footer(c,v,"A 开始   /   SELECT 退出");
}
void lock(Canvas& c,int x,int y,Color color){c.round({x,y+4,7,6},1,color);c.line(x+1,y+1,x+1,y+4,color);c.line(x+5,y+1,x+5,y+4,color);c.line(x+1,y+1,x+5,y+1,color);}
void selectBottom(Canvas& c,const Game& g,const View& v){
    text(c,14,13,"选择关卡",WHITE,140);char hint[48];std::snprintf(hint,sizeof(hint),"已解锁 %02d / %d",std::clamp(g.profile.unlocked,1,LEVEL_COUNT),LEVEL_COUNT);text(c,168,13,hint,MUTED,140);
    const int pages=(LEVEL_COUNT+11)/12,page=std::clamp(v.page,0,pages-1);
    for(int i=0;i<12;++i){const int n=page*12+i,x=14+(i%4)*74,y=46+(i/4)*43;const bool unlocked=n<g.profile.unlocked,selected=n==v.selected;
        if(n>=LEVEL_COUNT)break;
        c.panel({x,y,68,36},selected?RAISED:PANEL,selected?MINT:LINE,5);
        char num[12];std::snprintf(num,sizeof(num),"%02d",n+1);text(c,x+11,y+6,num,unlocked?WHITE:MUTED,31);
        if(!unlocked)lock(c,x+49,y+12,MUTED);
        else if(g.profile.levelBest[n]){c.circle(x+55,y+17,3,GOLD);c.circle(x+55,y+17,1,PANEL);}
        else c.line(x+52,y+14,x+56,y+18,selected?MINT:MUTED);
        if(selected)c.rect(x+10,y+29,48,2,MINT);
    }
    button(c,{14,181,90,28},"L 上一页");button(c,{112,181,96,28},"B 返回");button(c,{216,181,90,28},"R 下一页");
    char status[64];std::snprintf(status,sizeof(status),"第 %d / %d 页   /   A 进入关卡",page+1,pages);footer(c,v,status);
}
void controlBottom(Canvas& c,const Game& g,const View& v){
    char title[96];std::snprintf(title,sizeof(title),"%02d  %s",g.level+1,levelName(g.level));text(c,12,12,title,WHITE,226);
    char count[32];std::snprintf(count,sizeof(count),"%d 球",g.ballCount());pill(c,{248,10,60,22},count,MINT);
    c.rect(12,37,296,3,LINE);c.rect(12,37,static_cast<int>(296.0f*std::clamp(g.destroyed/static_cast<float>(std::max(1,g.totalBricks)),0.0f,1.0f)),3,MINT);
    c.panel({12,54,296,120},PANEL,v.touching?MINT:LINE,8);
    for(int x=32;x<296;x+=20)c.line(x,96,x,146,LINE,90);
    c.line(31,122,288,122,LINE);c.line(31,122,37,117,MUTED);c.line(31,122,37,127,MUTED);c.line(288,122,282,117,MUTED);c.line(288,122,282,127,MUTED);
    centered(c,{24,67,272,23},g.scene==Scene::Ready?"左右滑动，瞄准开场":"左右滑动，接住小球",WHITE);
    const int cx=std::clamp(static_cast<int>(g.paddleX*0.8f),41,279);
    c.round({cx-22,118,44,8},4,MINT);c.circle(cx,110,2,WHITE);
    if(v.touching){const int touch=std::clamp(static_cast<int>(v.touchX),29,291);c.circle(touch,147,5,MINT,55);c.circle(touch,147,2,MINT);}
    centered(c,{24,148,272,20},"触控板  /  摇杆与方向键也可控制",MUTED);
    button(c,{12,190,144,34},g.scene==Scene::Ready?"A 发球":"START 暂停",true);button(c,{164,190,144,34},"菜单");
    if(g.wideTimer>0||g.pierceTimer>0||g.slowTimer>0){
        const char* effect=g.pierceTimer>0?"穿透生效中":(g.wideTimer>0?"挡板加长中":"慢速生效中");footer(c,v,effect);
    }else footer(c,v,g.scene==Scene::Ready?"银色围墙会反弹，从开口进入":"接住分裂道具，让连击持续下去");
}
void statusBottom(Canvas& c,const Game& g,const View& v){
    const bool paused=g.scene==Scene::Paused,won=g.scene==Scene::Cleared,finished=g.scene==Scene::Finished;
    char complete[48];std::snprintf(complete,sizeof(complete),"%d 关全部完成！",LEVEL_COUNT);
    const char* title=paused?"休息一下":(won?"漂亮，过关了！":(finished?complete:"再来一局？"));
    const char* line=paused?"球已暂停，随时继续":(won?"下一面砖墙，等你来击破":(finished?"你让整片砖墙化成了星光":"调整落点，下一次会更好"));
    centered(c,{20,14,280,28},title,won||finished?GOLD:WHITE);centered(c,{20,44,280,22},line,MUTED);
    char stats[100];std::snprintf(stats,sizeof(stats),"得分 %u    最高连击 %d",static_cast<unsigned>(g.score),g.maxCombo);centered(c,{20,78,280,23},stats,MINT);
    if(paused){
        const char* names[]={"分裂","加长","穿透","慢速"};
        for(int i=0;i<4;++i){powerIcon(c,29+i*74,106,static_cast<Power>(i));text(c,39+i*74,99,names[i],MUTED,35);}
    }
    button(c,{20,116,280,38},paused?"A 继续游戏":(won?"A 下一关":(finished?"A 返回首页":"A 重新挑战")),true);
    button(c,{20,164,280,36},"选择关卡");footer(c,v,paused?"B 返回选关   /   SELECT 退出":"B 返回选关");
}
void quit(Canvas& c){
    c.veil({0,0,320,240},220);centered(c,{20,30,280,30},"退出万砖齐发？",WHITE);
    centered(c,{20,68,280,22},"已完成关卡的进度将保留",MUTED);
    button(c,{20,116,280,38},"A 保存并退出",true);button(c,{20,164,280,36},"B 取消");
}
void statusTop(Canvas& c,const Game& g){
    if(g.scene!=Scene::Paused&&g.scene!=Scene::Cleared&&g.scene!=Scene::GameOver&&g.scene!=Scene::Finished)return;
    c.veil({8,31,384,209},120);c.panel({78,83,244,75},PANEL,LINE,9);
    const char* title=g.scene==Scene::Paused?"游戏暂停":(g.scene==Scene::Cleared?"关卡完成":(g.scene==Scene::Finished?"万砖齐发，大获全胜":"挑战结束"));
    centered(c,{88,96,224,27},title,g.scene==Scene::Cleared||g.scene==Scene::Finished?GOLD:WHITE);
    char detail[64];std::snprintf(detail,sizeof(detail),"关卡 %02d   /   得分 %u",g.level+1,static_cast<unsigned>(g.score));centered(c,{88,127,224,20},detail,MUTED);
}
}
void render(const Game& game,const View& view,Surface top,Surface bottom){
    Canvas tc(top),bc(bottom);bc.rect(0,0,320,240,BG);
    if(game.scene==Scene::Title)hero(tc,game);else {board(tc,game);statusTop(tc,game);}
    switch(game.scene){
        case Scene::Title:titleBottom(bc,game,view);break;
        case Scene::Select:selectBottom(bc,game,view);break;
        case Scene::Ready:case Scene::Playing:controlBottom(bc,game,view);break;
        default:statusBottom(bc,game,view);break;
    }
    if(view.quitConfirm)quit(bc);
}
}
