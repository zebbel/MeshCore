#include <cassert>
#include <vector>
#include <string>
#include "examples/companion_radio/HostDisplay.h"
ColorVal UIColor::window_bkg = 0;
ColorVal UIColor::primary_txt = 1;
class Screen : public DisplayDriver {
public:
  int frames = 0;
  bool on = false;
  std::string drawn;
  Screen() : DisplayDriver(128,64) {}
  bool isOn() override { return on; }
  void turnOn() override { on = true; }
  void turnOff() override { on = false; }
  void clear() override {}
  void startFrame(ColorVal) override { drawn.clear(); }
  void setTextSize(int) override {}
  void setColor(ColorVal) override {}
  void setCursor(int,int) override {}
  void print(const char* s) override { drawn += s; }
  void fillRect(int,int,int,int) override {}
  void drawRect(int,int,int,int) override {}
  void drawXbm(int,int,const uint8_t*,int,int) override {}
  uint16_t getTextWidth(const char*) override { return 0; }
  void endFrame() override { frames++; }
};
int main() {
  HostDisplay h; Screen d; uint8_t reply[176]; uint32_t now=0;
  auto send = [&](int op, std::vector<uint8_t> data = {}) {
    std::vector<uint8_t> req = {0xF0,'M','C','O','D',1,42,(uint8_t)op};
    req.insert(req.end(), data.begin(), data.end());
    auto n=h.handle(req.data(),req.size(),reply,&d,now);
    assert(n>=9 && reply[6]==42 && reply[7]==op);
    return reply[8];
  };
  assert(send(0)==HostDisplay::OK && reply[9]==128 && reply[10]==64);
  assert(send(4)==HostDisplay::BAD_STATE);
  assert(send(1,{1,0})==HostDisplay::BAD_ARGUMENT && !h.active());
  assert(send(1,{2,0})==HostDisplay::OK && h.active());
  assert(send(2,{0,0,1,'H','i'})==HostDisplay::OK);
  assert(d.frames==0);
  assert(send(2,{127,0,1,'X'})==HostDisplay::BAD_ARGUMENT);
  assert(send(2,{0,63,1,'X'})==HostDisplay::BAD_ARGUMENT);
  assert(send(2,{0,0,0,'X'})==HostDisplay::BAD_ARGUMENT);
  assert(send(2,{0,0,1,0})==HostDisplay::BAD_ARGUMENT);
  assert(send(4)==HostDisplay::OK && d.drawn=="Hi" && d.frames==1);
  assert(send(3)==HostDisplay::OK && d.drawn=="Hi");
  assert(send(4)==HostDisplay::OK && d.drawn.empty());
  for(int i=0;i<16;i++) assert(send(2,{0,0,1,'X'})==HostDisplay::OK);
  assert(send(2,{0,0,1,'X'})==HostDisplay::FULL);
  assert(!h.expire(1999)); assert(h.expire(2000));
  assert(send(4)==HostDisplay::BAD_STATE);
  now=0xFFFFFF00u;
  assert(send(1,{2,0})==HostDisplay::OK);
  assert(!h.expire(now+1999u)); assert(h.expire(now+2000u));
  now=0;
  assert(send(1,{2,0})==HostDisplay::OK);
  now=1500; assert(send(6)==HostDisplay::OK);
  assert(!h.expire(3000)); assert(h.expire(3500));
  assert(send(5)==HostDisplay::OK && send(5)==HostDisplay::OK);
  assert(send(99)==HostDisplay::UNSUPPORTED);
  for(size_t len=0;len<8;len++) {
    std::vector<uint8_t> short_req(len,0xF0);
    assert(h.handle(short_req.data(),len,reply,&d,0)==9);
    assert(reply[8]==HostDisplay::BAD_ARGUMENT);
  }
  uint8_t req[]={0xF0,'M','C','O','D',1,9,0};
  assert(h.handle(req,8,reply,nullptr,0)==9 && reply[8]==HostDisplay::NO_DISPLAY);
  // Every truncated request and every trailing byte on fixed-size operations
  // must fail without acquiring the display or flushing it.
  for (int op : {0,1,3,4,5,6}) {
    req[7]=op;
    assert(h.handle(req,7,reply,&d,0)==9 && reply[8]==HostDisplay::BAD_ARGUMENT);
  }
  assert(send(1,{2,0,0})==HostDisplay::BAD_ARGUMENT && !h.active());
  assert(send(0,{0})==HostDisplay::BAD_ARGUMENT);
  assert(send(1,{30,0})==HostDisplay::OK);
  std::vector<uint8_t> longest={0,0,1};
  longest.insert(longest.end(),21,'A');
  assert(send(2,longest)==HostDisplay::OK);
  longest.push_back('B');
  assert(send(2,longest)==HostDisplay::BAD_ARGUMENT);
  assert(send(2,{0,48,2,'O','K'})==HostDisplay::OK);
  assert(send(4)==HostDisplay::OK);
  req[7]=0;
  req[5]=2;
  assert(h.handle(req,8,reply,&d,0)==9 && reply[8]==HostDisplay::UNSUPPORTED);
}
