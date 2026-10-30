#include "examples/simple_repeater/BatteryTrend.h"
#include <cassert>
#include <cstring>

int main() {
  BatteryTrend rising, falling, stable, spike;
  assert(!strcmp(rising.trend(), "Collecting"));
  for (uint32_t i=0;i<36;++i) {
    rising.sample(i*10000, 3800+i*2);
    falling.sample(i*10000, 4200-i*2);
    stable.sample(i*10000, 4000+(int(i%3)-1)*5);
    spike.sample(i*10000, i%3==1 ? 4400 : 4000);
    if(i<32) assert(!strcmp(rising.trend(),"Collecting"));
  }
  assert(!strcmp(rising.trend(),"Rising"));
  assert(!strcmp(falling.trend(),"Falling"));
  assert(!strcmp(stable.trend(),"Stable"));
  assert(!strcmp(spike.trend(),"Stable"));
  assert(!rising.due(350001));
  rising.sample(350001,0); // too soon: no extra ADC sample accepted
  assert(rising.milliVolts()!=0);
  rising.sample(360000,0);
  assert(!strcmp(rising.trend(),"Collecting") && rising.milliVolts()==0);
  falling.sample(400000,4100); // gap invalidates comparison
  assert(!strcmp(falling.trend(),"Collecting"));
  BatteryTrend rollover;
  rollover.sample(0xfffffff0,4000);
  assert(!rollover.due(100));
  assert(rollover.due(10000));
  rollover.sample(10000,4001);
  assert(rollover.milliVolts()==4001);
  for(uint32_t i=36;i<72;++i) stable.sample(i*10000,4000+(i%2?12:0));
  assert(!strcmp(stable.trend(),"Stable"));
  BatteryTrend reversal;
  for(uint32_t i=0;i<100;++i)
    reversal.sample(i*10000, i<40 ? 3800+i*3 : 3920-(i-40)*3);
  assert(!strcmp(reversal.trend(),"Falling"));
}
