#define private public
#include "examples/simple_repeater/NeighbourSNR.h"
#undef private
#include <cassert>
#include <cstdio>
static uint32_t get(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
static void put(uint8_t* p,uint32_t n){for(int i=0;i<4;++i){p[i]=n;n>>=8;}}
int main(){
 uint8_t boot[16]={9},key[32]={1},req[28]={0x70,1,1},out[140];NeighbourSNR s;
 s.begin(0,1700000000,boot);
 for(int period=0;period<5;++period){
   for(int n=1;n<=50;++n){key[0]=n;for(int j=0;j<10;++j)s.observe(key,int8_t(-20+n%20+j));}
   s.tick((period+1)*1800000,1700000000+(period+1)*1800);
 }
 s.reply(req,28,out,sizeof(out));assert(get(out+36)==50);assert(out[50]==200);assert(get(out+56)==1 && get(out+60)==4);
 memcpy(req+4,boot,16);put(req+20,get(out+20));
 unsigned count=0;
 do {
   s.reply(req,28,out,sizeof(out));assert(!out[1]);if(!out[2])break;
   assert(get(out+128)==10 && out[134]==1);++count;put(req+24,get(out+24));
 }while(out[3]);
 assert(count==200);
 // Force integer limits to verify no count/sum wrap and loss is visible.
 auto& r=s.slots[0].current;r.count=UINT32_MAX;r.sum=int64_t(UINT32_MAX)*127;r.minimum=r.maximum=127;
 key[0]=1;s.observe(key,127);assert(r.count==UINT32_MAX && r.sum==int64_t(UINT32_MAX)*127 && (r.flags&4));
 assert(s.dropped==1);
 s.serial=UINT32_MAX;s.tick(10800000,1700010800);assert(s.serial==UINT32_MAX && s.dropped==2);
 // Bad max count, invalid cursor and output capacity return errors, no mutation.
 memset(req+4,0,24);req[2]=2;s.reply(req,28,out,sizeof(out));assert(out[1]==2);req[2]=1;
 memcpy(req+4,boot,16);put(req+20,2);put(req+24,3);s.reply(req,28,out,sizeof(out));assert(out[1]==2);
 NeighbourSNR t;t.begin(0,1700000000,boot);key[0]=2;t.observe(key,-5);t.tick(1800000,1700001800);
 memset(req+4,0,24);t.reply(req,28,out,64);assert(out[1]==4);
 // Clock backwards invalidates only that interval; later stable interval recovers.
 t.observe(key,0);t.tick(1801000,1699999999);t.tick(3600000,1700001798);
 t.observe(key,0);t.tick(5400000,1700003598);
 assert(t.slots[0].history[1].flags&2);assert(!t.slots[0].history[1].utc);
 assert(t.slots[0].history[2].flags&1);
 uint8_t newBoot[16]={10};t.begin(0,0,newBoot);memcpy(req+4,boot,16);t.reply(req,28,out,sizeof(out));assert(out[1]==3);
 // Entropy failure disables aggregate export while preserving active neighbors.
 t.begin(0,0,newBoot,false);t.observe(key,-5);assert(t.active(key));
 t.tick(1800000,0);t.reply(req,28,out,sizeof(out));assert(out[1]==5 && !out[2]);
 assert(t.serial==0 && t.dropped==1);t.tick(7200000,0);assert(!t.active(key));
 puts("50-neighbor four-window retention, pagination, integer limits, clock recovery and reboot tests passed.");
}
