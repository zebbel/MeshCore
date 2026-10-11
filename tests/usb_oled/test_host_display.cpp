#include <cassert>
#include <vector>
#include <string>
#include <set>
#include <utility>
#include "examples/companion_radio/HostDisplay.h"
#include "src/helpers/ui/MediumText.h"
ColorVal UIColor::window_bkg = 0;
ColorVal UIColor::primary_txt = 1;
class Screen : public DisplayDriver {
public:
  int frames = 0;
  bool medium = true;
  int medium_calls = 0;
  bool on = false;
  std::string drawn;
  std::set<std::pair<int,int>> pixels;
  Screen() : DisplayDriver(128,64) {}
  bool isOn() override { return on; }
  void turnOn() override { on = true; }
  void turnOff() override { on = false; }
  void clear() override {}
  void startFrame(ColorVal) override { drawn.clear(); pixels.clear(); }
  void setTextSize(int) override {}
  void setColor(ColorVal) override {}
  void setCursor(int,int) override {}
  void print(const char* s) override { drawn += s; }
  bool supportsMediumText() const override { return medium; }
  void drawMediumText(int x,int y,const char* s) override {
    assert(x>=0 && y>=0 && x+strlen(s)*8<=128 && y+12<=64);
    ++medium_calls; drawn += s;
  }
  void fillRect(int x,int y,int w,int h) override { assert(x>=0 && x<128 && y>=0 && y<64 && w==1 && h==1); pixels.insert({x,y}); }
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
  assert(send(HostDisplay::CAPABILITIES)==HostDisplay::OK);
  assert(reply[9]==1 && reply[10]==15 && reply[11]==0 && reply[12]==1 && reply[13]==83);
  assert(send(HostDisplay::CLEAR)==HostDisplay::OK);
  // All octants, reversed endpoints and degenerate point.
  for (auto end : std::vector<std::pair<uint8_t,uint8_t>>{{20,12},{12,20},{8,20},{0,12},{0,8},{8,0},{12,0},{20,8},{10,10},{10,20},{20,10}}) {
    send(HostDisplay::CLEAR);
    assert(send(HostDisplay::LINE,{10,10,end.first,end.second})==HostDisplay::OK);
    send(HostDisplay::SHOW);
    auto forward=d.pixels;
    assert(forward.count({10,10}) && forward.count({end.first,end.second}));
    send(HostDisplay::CLEAR);
    assert(send(HostDisplay::LINE,{end.first,end.second,10,10})==HostDisplay::OK);
    send(HostDisplay::SHOW);
    assert(d.pixels.size()==forward.size());
  }
  send(HostDisplay::CLEAR);
  assert(send(HostDisplay::POLYLINE,{3,0,0,2,0,2,2})==HostDisplay::OK);
  assert(send(HostDisplay::POLYLINE,{3,4,4,5,5,128,2})==HostDisplay::BAD_ARGUMENT);
  assert(send(HostDisplay::LINE,{0,0,1,64})==HostDisplay::BAD_ARGUMENT);
  assert(send(HostDisplay::LINE,{0,0,1})==HostDisplay::BAD_ARGUMENT);
  assert(send(HostDisplay::POLYLINE,{2,0,0})==HostDisplay::BAD_ARGUMENT);
  assert(send(HostDisplay::POLYLINE,{1,0,0})==HostDisplay::BAD_ARGUMENT);
  send(HostDisplay::SHOW);
  std::set<std::pair<int,int>> expected={{0,0},{1,0},{2,0},{2,1},{2,2}};
  assert(d.pixels==expected); // invalid command did not partially append
  send(HostDisplay::CLEAR);
  std::vector<uint8_t> many={83};
  for(int i=0;i<83;i++) { many.push_back(i); many.push_back(30); }
  for(int i=0;i<3;i++) assert(send(HostDisplay::POLYLINE,many)==HostDisplay::OK);
  for(int i=0;i<10;i++) assert(send(HostDisplay::LINE,{0,0,0,0})==HostDisplay::OK);
  assert(send(HostDisplay::LINE,{0,0,1,1})==HostDisplay::FULL);
  assert(send(HostDisplay::POLYLINE,{2,0,0,1,1})==HostDisplay::FULL);
  send(HostDisplay::SHOW);
  assert(d.pixels.size()==84);
  send(HostDisplay::CLEAR); send(HostDisplay::SHOW); assert(d.pixels.empty());
  assert(send(HostDisplay::LINE,{127,63,127,63})==HostDisplay::OK);
  send(HostDisplay::SHOW); assert(d.pixels.count({127,63}));
  send(HostDisplay::RELEASE);
  assert(send(HostDisplay::LINE,{0,0,1,1})==HostDisplay::BAD_STATE);
  send(HostDisplay::BEGIN,{2,0}); send(HostDisplay::SHOW); assert(d.pixels.empty());
  now=0;
  send(HostDisplay::BEGIN,{2,0});
  now=1000; send(HostDisplay::CAPABILITIES);
  assert(h.expire(2000)); // capability query must not renew lease

  uint8_t event[176], again[176];
  assert(send(HostDisplay::BUTTON_SUBSCRIBE,{1})==HostDisplay::BAD_STATE);
  send(HostDisplay::BEGIN,{2,0});
  h.recordButton(1,1); assert(h.peekButton(event)==0);
  assert(send(HostDisplay::BUTTON_SUBSCRIBE,{2})==HostDisplay::BAD_ARGUMENT);
  assert(send(HostDisplay::BUTTON_SUBSCRIBE,{1,0})==HostDisplay::BAD_ARGUMENT);
  assert(send(HostDisplay::BUTTON_SUBSCRIBE,{1})==HostDisplay::OK);
  for(int gesture=1;gesture<=4;gesture++) h.recordButton(gesture,0x12345678);
  for(int gesture=1;gesture<=4;gesture++) {
    assert(h.peekButton(event)==16 && event[7]==0x80 && event[8]==0);
    assert(event[9]==gesture && event[10]==gesture && event[11]==0);
    assert(event[12]==0x78 && event[13]==0x56 && event[14]==0x34 && event[15]==0x12);
    h.peekButton(again); assert(memcmp(event,again,16)==0); // retain on busy/failed write
    h.consumeButton();
  }
  assert(h.peekButton(event)==0);
  for(int i=0;i<10;i++) h.recordButton(1,i);
  assert(h.peekButton(event)==16 && event[10]==7); // drop oldest, visible sequence gap
  send(HostDisplay::CLEAR); send(HostDisplay::BEGIN,{2,0});
  assert(h.buttonSubscribed() && h.peekButton(event)==16); // redraw keeps subscription
  send(HostDisplay::BUTTON_SUBSCRIBE,{0}); assert(h.peekButton(event)==0);
  send(HostDisplay::BUTTON_SUBSCRIBE,{1}); assert(h.peekButton(event)==0);
  h.recordButton(0,0); h.recordButton(5,0); assert(h.peekButton(event)==0);
  h.recordButton(2,0); send(HostDisplay::RELEASE);
  assert(!h.buttonSubscribed() && h.peekButton(event)==0);
  now=0; send(HostDisplay::BEGIN,{2,0}); send(HostDisplay::BUTTON_SUBSCRIBE,{1});
  h.recordButton(1,1999); assert(h.expire(2000)); // events do not renew lease
  assert(!h.buttonSubscribed() && h.peekButton(event)==0);
  send(HostDisplay::BEGIN,{2,0}); send(HostDisplay::BUTTON_SUBSCRIBE,{1});
  // Sequence wraps modulo 65536. Every accepted event advances it even on overflow.
  for(int i=0;i<65536;i++) { h.recordButton(1,i); h.peekButton(event); h.consumeButton(); }
  assert(event[10]==16 && event[11]==0);

  now=0; send(HostDisplay::BEGIN,{30,0});
  assert(send(HostDisplay::CAPABILITIES)==HostDisplay::OK && (reply[10]&8));
  std::vector<uint8_t> medium_text={0,52,3};
  medium_text.insert(medium_text.end(),16,'M');
  assert(send(HostDisplay::TEXT,medium_text)==HostDisplay::OK);
  assert(d.medium_calls==0); // buffered until SHOW
  send(HostDisplay::SHOW); assert(d.medium_calls==1 && d.drawn==std::string(16,'M'));
  medium_text.push_back('M'); assert(send(HostDisplay::TEXT,medium_text)==HostDisplay::BAD_ARGUMENT);
  assert(send(HostDisplay::TEXT,{0,53,3,'X'})==HostDisplay::BAD_ARGUMENT);
  assert(send(HostDisplay::TEXT,{121,0,3,'X'})==HostDisplay::BAD_ARGUMENT);
  assert(send(HostDisplay::TEXT,{120,52,3,'X'})==HostDisplay::OK);
  assert(send(HostDisplay::TEXT,{0,0,4,'X'})==HostDisplay::BAD_ARGUMENT);
  d.medium=false;
  assert(send(HostDisplay::CAPABILITIES)==HostDisplay::OK && !(reply[10]&8));
  assert(send(HostDisplay::TEXT,{0,0,3,'X'})==HostDisplay::BAD_ARGUMENT);
  d.medium=true;
  // Native glyph rendering preserves exact pixels and cell bounds.
  uint8_t glyph[12]={1,0,0,0,0,0,0,0,0,0,0,64};
  d.pixels.clear(); drawMediumGlyph(d,120,52,glyph);
  std::set<std::pair<int,int>> medium_pixels={{120,52},{126,63}};
  assert(d.pixels==medium_pixels);
  memset(glyph,0,sizeof(glyph));d.pixels.clear();drawMediumGlyph(d,0,0,glyph);
  assert(d.pixels.empty());
  memset(glyph,0xFF,sizeof(glyph));drawMediumGlyph(d,0,0,glyph);
  assert(d.pixels.size()==96 && d.pixels.count({7,11}));

  for (int c=33; c<=126; ++c) {
    d.pixels.clear(); char text[]={static_cast<char>(c),0};
    drawStationMediumText(d,120,52,text); assert(!d.pixels.empty());
  }
  d.pixels.clear(); drawStationMediumText(d,0,0," "); assert(d.pixels.empty());
  d.pixels.clear(); drawStationMediumText(d,0,0,"M"); auto native_m=d.pixels;
  d.pixels.clear(); drawStationMediumText(d,8,0,"M");
  for(auto pixel:native_m) assert(d.pixels.count({pixel.first+8,pixel.second}));

}
