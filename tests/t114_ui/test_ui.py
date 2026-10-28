"""Compile the real repeater UITask with deterministic device stand-ins."""
from pathlib import Path
import tempfile, subprocess
ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
    d = Path(tmp)
    (d/'helpers/ui').mkdir(parents=True)
    for name in ('UITask.h','UITask.cpp'):
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
namespace mesh { struct MainBoard { int off=0; unsigned short getBattMilliVolts(){return 4123;} void powerOff(){++off;} }; }
struct NodePrefs { char node_name[32]="Test repeater"; float freq=869.525, bw=250; int sf=9,cr=5,tx_power_dbm=22; };
""")
    (d/'helpers/ui/DisplayDriver.h').write_text("""
#pragma once
#include <helpers/CommonCLI.h>
#include <vector>
#include <string>
enum class UIColor {corp_blue,primary_txt};
struct DisplayDriver {
 bool on=false; std::vector<std::string> text;
 int width(){return 128;} int height(){return 64;}
 void turnOn(){on=true;} void turnOff(){on=false;} bool isOn(){return on;}
 void setColor(UIColor){} void setCursor(int,int){} void setTextSize(int){}
 void drawXbm(int,int,const uint8_t*,int,int){}
 void drawTextCentered(int,int,const char* s){text.push_back(s);}
 unsigned short getTextWidth(const char*){return 0;}
 void print(const char* s){text.push_back(s);}
 void startFrame(){text.clear();} void endFrame(){}
};
""")
    (d/'target.h').write_text("""
#pragma once
#define PIN_USER_BTN 1
#define DISPLAY_CLASS DisplayDriver
#define BUTTON_EVENT_CLICK 1
#define BUTTON_EVENT_LONG_PRESS 2
#define BUTTON_EVENT_DOUBLE_CLICK 3
struct Button { int event=0; void begin(){} int check(){int e=event;event=0;return e;} };
extern Button user_btn;
""")
    (d/'MyMesh.h').write_text("""
#pragma once
#include <Arduino.h>
struct NeighbourInfo { struct {uint8_t pub_key[32]={1,2,3,4};} id; uint32_t heard_timestamp=0; int8_t snr=8; };
struct MyMesh {
 int adverts=0, count=0;
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
unsigned long now=1; SerialType Serial; Button user_btn; MyMesh the_mesh;
int main(){
 mesh::MainBoard board; DisplayDriver display; NodePrefs prefs;
 UITask ui(board,display); ui.begin(&prefs,"today","v1");
 auto has=[&](const char* value){for(auto& s:display.text)if(s.find(value)!=std::string::npos)return true;return false;};
 ui.loop(); assert(has("meshcore.io"));
 now=5000;ui.loop();assert(has("Test repeater") && has("22 dBm"));
 user_btn.event=1;ui.loop();assert(has("4.123 V") && has("1 d 01 h 01 m"));
 user_btn.event=1;ui.loop();assert(has("None heard"));
 the_mesh.count=8;now+=1001;ui.loop();assert(has("Neighbors: 8") && has("01020304") && display.text.size()==6);
 user_btn.event=3;ui.loop();assert(the_mesh.adverts==1 && has("Neighbors"));
 ui.loop();assert(the_mesh.adverts==1);
 now+=20001;ui.loop();assert(!display.on);
 user_btn.event=3;ui.loop();assert(the_mesh.adverts==2 && !display.on);
 user_btn.event=1;ui.loop();assert(display.on && has("Neighbors"));
 user_btn.event=1;ui.loop();assert(has("Test repeater"));
 user_btn.event=2;ui.loop();now+=3001;ui.loop();assert(board.off==1);
}
""")
    subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Wno-unused-variable','-DHELTEC_T114_WITH_DISPLAY','-I'+tmp,str(d/'UITask.cpp'),str(d/'test.cpp'),'-o',str(d/'test')],check=True)
    subprocess.run([str(d/'test')],check=True)
    subprocess.run(['g++','-std=c++11','-I'+tmp,'-c',str(d/'UITask.cpp'),'-o',str(d/'generic.o')],check=True)
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
