"""Compile the real repeater UITask with deterministic device stand-ins."""
from pathlib import Path
import tempfile, subprocess
ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
    d = Path(tmp)
    (d/'helpers/ui').mkdir(parents=True)
    for name in ('UITask.h','UITask.cpp','BatteryEstimate.h'):
        (d/name).write_bytes((ROOT/'examples/simple_repeater'/name).read_bytes())
    (d/'Arduino.h').write_text("""
#pragma once
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define PROGMEM
#define LOW 0
#define HIGH 1
extern unsigned long now;
inline unsigned long millis() { return now; }
struct SerialType { void println(const char*) {} };
extern SerialType Serial;
""")
    (d/'helpers/CommonCLI.h').write_text("""
#pragma once
#include <Arduino.h>
namespace mesh { struct MainBoard { int off=0, reads=0; bool usb=false; bool isExternalPowered(){return usb;} unsigned short getBattMilliVolts(){++reads;return 4123;} void powerOff(){++off;} }; }
struct NodePrefs { char node_name[32]="Test repeater"; float freq=869.525, bw=250; double node_lat=1,node_lon=2; int sf=9,cr=5,tx_power_dbm=22; };
""")
    # Use production width/ellipsis helpers and the actual ST7789 font tables.
    base=(ROOT/'src/helpers/ui/DisplayDriver.h').read_text().replace('DisplayDriver','BaseDisplayDriver')
    (d/'helpers/ui/Base.h').write_text(base)
    fonts=(ROOT/'src/helpers/ui/OLEDDisplayFonts.cpp').read_text()
    import re
    arrays=[]
    for size in (16,24):
        body=fonts.split(f'const uint8_t ArialMT_Plain_{size}[] PROGMEM = {{')[1].split('};')[0]
        arrays.append(f'const uint8_t font{size}[] = {{'+body+'};')
    (d/'fonts.h').write_text('\n'.join(arrays))
    (d/'helpers/ui/DisplayDriver.h').write_text("""
#pragma once
#include <helpers/CommonCLI.h>
#include "Base.h"
#include "fonts.h"
#include <vector>
#include <string>
#include <fstream>
struct DisplayDriver : BaseDisplayDriver {
 bool on=false; std::vector<std::string> text, ops;
 int x=0,y=0,size=1; ColorVal color=1;
 DisplayDriver():BaseDisplayDriver(128,64){}
 void turnOn(){on=true;} void turnOff(){on=false;} bool isOn(){return on;}
 void clear(){} void setColor(ColorVal c){color=c;} void setCursor(int a,int b){x=a;y=b;} void setTextSize(int s){size=s;}
 void drawXbm(int,int,const uint8_t*,int,int){}
 unsigned short getTextWidth(const char* s){auto f=size==2?font24:font16;int w=0;while(*s)w+=f[4+(uint8_t(*s++)-f[2])*4+3];return w/1.875;}
 void print(const char* s){text.push_back(s);ops.push_back("T "+std::to_string(x)+" "+std::to_string(y)+" "+std::to_string(size)+" "+std::to_string(color)+" "+s);}
 void rect(char kind,int a,int b,int w,int h){ops.push_back(std::string(1,kind)+" "+std::to_string(a)+" "+std::to_string(b)+" "+std::to_string(w)+" "+std::to_string(h)+" "+std::to_string(color));}
 void fillRect(int a,int b,int w,int h){rect('F',a,b,w,h);} void drawRect(int a,int b,int w,int h){rect('R',a,b,w,h);}
 void startFrame(ColorVal=0){text.clear();ops.clear();} void endFrame(){}
 void save(const char* name){std::ofstream f(name);for(auto& op:ops)f<<op<<std::endl;}
};
""")
    (d/'target.h').write_text("""
#pragma once
#define PIN_USER_BTN 1
#define DISPLAY_CLASS DisplayDriver
#define BUTTON_EVENT_CLICK 1
#define BUTTON_EVENT_LONG_PRESS 2
#define BUTTON_EVENT_DOUBLE_CLICK 3
#define BUTTON_EVENT_TRIPLE_CLICK 4
struct Button { int event=0; void begin(){} int check(){int e=event;event=0;return e;} };
extern Button user_btn;
struct GPS {
 bool enabled=true, fresh=true; bool isEnabled(){return enabled;}
 bool waitingTimeSync(){return !fresh;}
 bool hasFreshFix(uint32_t){return fresh && enabled;}
 long satellitesCount(){return 9;} long getLatitude(){return 49640019;}
 long getLongitude(){return -8633326;} long getAltitude(){return 115000;}
};
struct Sensors { GPS gps; bool available=true; GPS* getLocationProvider(){return available ? &gps : nullptr;} };
extern Sensors sensors;
""")
    (d/'MyMesh.h').write_text("""
#pragma once
#include <Arduino.h>
struct NeighbourInfo { struct {uint8_t pub_key[32]={1,2,3,4};} id; uint32_t heard_timestamp=0; int8_t snr=8; };
struct MyMesh {
 int adverts=0, count=0, retries=0;
 bool success=false; const char* status="GPS SEARCHING";
 bool gpsAcquisitionActive() const {return !success && (!strcmp(status,"GPS SEARCHING") || !strcmp(status,"GPS FIX / SYNCING"));}
 bool gpsAcquisitionSucceeded() const {return success;}
 const char* gpsAcquisitionStatus() const {return status;}
 void startGPSAcquisition(){++retries;success=false;status="GPS SEARCHING";}
 void sendButtonAdvertisement(){sendSelfAdvertisement(1500,true);}
 void sendSelfAdvertisement(int delay,bool flood){if(delay==1500 && flood) ++adverts;}
 uint64_t getUIUptimeMinutes(){return 1501;}
 int getUINeighbours(NeighbourInfo* out,int n){for(int i=0;i<count && i<n;++i)out[i]=NeighbourInfo();return count;}
};
""")
    (d/'test.cpp').write_text("""
#include "UITask.h"
#include "target.h"
#include "MyMesh.h"
#include <cassert>
ColorVal UIColor::primary_txt=1, UIColor::corp_blue=1, UIColor::window_bkg=0;
unsigned long now=1; SerialType Serial; Button user_btn; MyMesh the_mesh; Sensors sensors;
int main(){
 mesh::MainBoard board; DisplayDriver display; NodePrefs prefs;
 UITask ui(board,display); ui.begin(&prefs,"today","v1");
 auto has=[&](const char* v){for(auto& s:display.text)if(s.find(v)!=std::string::npos)return true;return false;};
 ui.loop();assert(has("meshcore.io"));
 now=5000;ui.loop();assert(has("Test repeater") && has("22 dBm") && has("1 d 01 h 01 m"));
 now=10001;ui.loop(); now=20001; user_btn.event=1;ui.loop();
 assert(has("LIVE") && has("49.640019") && has("-8.633326"));
 display.save("gps-orange.ops");
 user_btn.event=4;ui.loop();assert(the_mesh.retries==1 && the_mesh.adverts==0);
 sensors.gps.fresh=false; the_mesh.status="GPS TIMEOUT";now+=1001;ui.loop();
 assert(has("SAVED") && !has("49.640019") && has("GPS !"));display.save("gps-red.ops");
 the_mesh.success=true;the_mesh.status="FIX SAVED - GPS OFF";sensors.gps.enabled=false;
 prefs.node_lat=49.640019;prefs.node_lon=8.633326;now+=1001;ui.loop();
 assert(has("GPS OK") && has("Time:OK") && has("SAVED"));display.save("gps-green.ops");
 user_btn.event=1;ui.loop();assert(has("None heard"));
 the_mesh.count=8;now+=1001;ui.loop();assert(has("Neighbors: 8") && display.text.size()==7);display.save("neighbors.ops");
 user_btn.event=4;ui.loop();assert(the_mesh.retries==1);
 user_btn.event=3;ui.loop();assert(the_mesh.adverts==1 && has("Neighbors"));ui.loop();assert(the_mesh.adverts==1);
 user_btn.event=1;ui.loop();assert(has("Test repeater"));display.save("repeater.ops");
 now+=20001;ui.loop();assert(!display.on);
 int reads=board.reads;now+=10000;ui.loop();assert(board.reads==reads+1 && !display.on);
 board.usb=true;ui.loop();assert(display.on && has("USB Power"));display.save("usb.ops");
 user_btn.event=3;ui.loop();assert(the_mesh.adverts==2);
 assert(has("Test repeater") && !has("USB Power"));
 user_btn.event=1;ui.loop();assert(has("3 clicks"));
 now+=20001;ui.loop();assert(display.on && has("USB Power"));
 user_btn.event=4;ui.loop();assert(the_mesh.retries==1);
 now+=20001;ui.loop();board.usb=false;ui.loop();assert(!display.on);
 user_btn.event=1;ui.loop();assert(display.on && has("3 clicks"));
 user_btn.event=4;ui.loop();assert(the_mesh.retries==2 && has("GPS ..."));
 user_btn.event=2;ui.loop();now+=1001;ui.loop();assert(has("Turning OFF"));now+=2001;ui.loop();assert(board.off==1);
}
""")
    subprocess.run(['g++','-std=c++11','-DT114_GPS_ACQUIRE','-DT114_BUTTON_RGB','-DHELTEC_T114_WITH_DISPLAY','-DENV_INCLUDE_GPS=1','-I'+tmp,str(d/'UITask.cpp'),str(d/'test.cpp'),'-o',str(d/'test')],check=True)
    output=ROOT/'docs/t114-display-preview'
    output.mkdir(exist_ok=True)
    subprocess.run([str(d/'test')],cwd=output,check=True)
    subprocess.run(['g++','-std=c++11','-I'+tmp,'-c',str(d/'UITask.cpp'),'-o',str(d/'generic.o')],check=True)
    subprocess.run(['g++','-std=c++11','-DHELTEC_T114_WITH_DISPLAY','-I'+tmp,'-c',str(d/'UITask.cpp'),'-o',str(d/'no-gps.o')],check=True)
print('T114 UI runtime screens, refresh, gestures, timeout and shutdown tests passed.')

# Exercise the actual copy/sort implementation separately from rendering.
with tempfile.TemporaryDirectory() as tmp:
    d = Path(tmp)
    source = (ROOT/'examples/simple_repeater/MyMesh.cpp').read_text()
    body = source[source.index('int MyMesh::getUINeighbours('):].rsplit('#endif',1)[0]
    test = '''#include <stdint.h>
#include <cassert>
#define MAX_NEIGHBOURS 6
struct NeighbourInfo {uint32_t heard_timestamp=0;};
struct MyMesh {NeighbourInfo neighbours[6]; int getUINeighbours(NeighbourInfo*,int) const;};
''' + body + '''
int main(){
 MyMesh m; NeighbourInfo out[2];
 assert(m.getUINeighbours(out,2)==0);
 m.neighbours[0].heard_timestamp=2;m.neighbours[2].heard_timestamp=8;m.neighbours[4].heard_timestamp=5;
 assert(m.getUINeighbours(out,2)==3);
 assert(out[0].heard_timestamp==8 && out[1].heard_timestamp==5);
 assert(m.neighbours[0].heard_timestamp==2);
 assert(m.getUINeighbours(nullptr,0)==3);
}
'''
    (d/'test.cpp').write_text(test)
    subprocess.run(['g++','-std=c++11','-fsanitize=undefined',str(d/'test.cpp'),'-o',str(d/'test')],check=True)
    subprocess.run([str(d/'test')],check=True)
print('Neighbor copying, recency ordering, count and empty-table tests passed.')
