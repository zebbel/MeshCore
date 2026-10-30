#include "examples/simple_repeater/BatteryEstimate.h"
#include <cassert>
int main(){
 assert(BatteryEstimate::estimate(2500)==0 && BatteryEstimate::estimate(4500)==100);
 int prev=0;for(int v=2500;v<=4500;v++){int p=BatteryEstimate::estimate(v);assert(p>=prev && p<=100);prev=p;}
 BatteryEstimate b; assert(b.percent()==-1);
 b.sample(0,4000);b.sample(10000,4400);assert(b.percent()==-1);b.sample(20000,4000);assert(b.percent()==80);
 for(int i=3;i<30;i++)b.sample(i*10000,4000+(i%2?3:-3));
 assert(b.percent()==80);
 for(int i=30;i<90;i++)b.sample(i*10000,3700);
 assert(b.percent()>=30 && b.percent()<=32);
 b.sample(900000,0);assert(b.percent()==-1);
 b.sample(910000,4100);b.sample(920000,4100);b.sample(930000,4100);assert(b.percent()==90);
 b.sample(1000000,4100);assert(b.percent()==-1);
 BatteryEstimate r;r.sample(0xfffffff0,4000);assert(!r.due(100));assert(r.due(10000));
}
