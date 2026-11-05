#include "examples/simple_repeater/NeighbourSNR.h"
#include <cassert>
#include <cstdio>
static uint32_t u32(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
static void put(uint8_t* p,uint32_t n){for(int i=0;i<4;i++){p[i]=n;n>>=8;}}
static uint8_t boot[16]={1},key[32]={2},req[28]={0x70,1,1},out[160];
int main(){
 NeighbourSNR s;s.begin(0,1700000000,boot);
 s.observe(key,-13);s.observe(key,6);s.tick(1799999,1700001799);
 assert(s.reply(req,28,out,160)==64 && out[2]==0);
 s.tick(1800000,1700001800);assert(s.reply(req,28,out,160)==140 && out[2]==1);
 assert(u32(out+64+64)==2 && out[132]==uint8_t(-13) && out[133]==6);
 assert(u32(out+112)==1700000000 && (out[134]&1));
 int64_t sum=0;for(int i=7;i>=0;--i)sum=(uint64_t(sum)<<8)|out[120+i];assert(sum==-7);
 uint8_t saved[76];memcpy(saved,out+64,76);
 // fixed snapshot ignores new completion; repeats immutable record
 memcpy(req+4,boot,16);put(req+20,1);s.observe(key,12);s.tick(3600000,1700003600);
 s.reply(req,28,out,160);assert(!memcmp(saved,out+64,76) && out[3]==0);
 put(req+24,1);s.reply(req,28,out,160);assert(!out[2]);
 memset(req+4,0,24);s.reply(req,28,out,160);assert(out[3]==1);
 s.tick(9000000,1700009000);assert(!s.active(key));s.reply(req,28,out,160);assert(u32(out+36)==1 && out[2]==1);
 s.observe(key,0);assert(s.active(key));
 s.tick(18000000,1700018000);s.reply(req,28,out,160);assert(!out[2]);
 // Invalid RTC never yields fictitious UTC; backward correction flags interval.
 NeighbourSNR c;c.begin(0,0,boot);c.observe(key,-1);c.tick(1800000,0);c.reply(req,28,out,160);assert(out[134]==0 && !u32(out+112));
 c.observe(key,-2);c.tick(1801000,1700000000);c.tick(3600000,1700001799);
 memset(req+4,0,24);c.reply(req,28,out,160);memcpy(req+4,boot,16);put(req+20,u32(out+20));put(req+24,u32(out+24));c.reply(req,28,out,160);assert(out[134]&2);
 // 50 active max, retained identities survive admission churn; deterministic bounded eviction.
 NeighbourSNR many;many.begin(0,1700000000,boot);for(int i=0;i<50;i++){key[0]=i;many.observe(key,-i);}
 many.tick(1800000,1700001800);memset(req+4,0,24);many.reply(req,28,out,160);assert(out[48]==50 && out[50]==50);
 for(int i=50;i<160;i++){key[0]=i;many.observe(key,1);}
 many.reply(req,28,out,160);assert(out[48]==50 && u32(out+40)>0);
 for(int i=2;i<20;i++)many.tick(i*1800000,1700000000+i*1800);
 many.reply(req,28,out,160);assert(out[48]==0 && out[50]==0);
 // session mismatch, bad version/length/count and small output fail safely
 req[1]=2;assert(many.reply(req,28,out,160)==64 && out[1]==1);req[1]=1;
 assert(many.reply(req,1,out,160)==64 && out[1]==2);
 req[4]=99;many.reply(req,28,out,160);assert(out[1]==3);
 assert(many.reply(req,28,out,4)==0);
 NeighbourSNR wrap;wrap.begin(0xfffffff0,0,boot);wrap.tick(1000,0);assert(wrap.seconds()==1);
 printf("NeighbourSNR storage: %zu bytes; aggregation, retention, RTC, cursor, capacity and session tests passed.\n",sizeof(NeighbourSNR));
}
