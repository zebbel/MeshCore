from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp)
 (d/'Arduino.h').write_text("""
#pragma once
#include <stdint.h>
#include <stddef.h>
#define NEOPIXEL_NUM 2
#define PIN_NEOPIXEL 14
#define OUTPUT 1
#define LOW 0
#define PWM_DECODER_LOAD_Common 0
#define PWM_DECODER_LOAD_Pos 0
#define PWM_SHORTS_SEQEND0_STOP_Msk 1
struct PWM { uint32_t ENABLE=0; struct {uint32_t OUT[4]={0xffffffff,0xffffffff,0xffffffff,0xffffffff};} PSEL;
 uint32_t MODE=0,PRESCALER=0,COUNTERTOP=0,LOOP=0,DECODER=0,INTENCLR=0,SHORTS=0,EVENTS_STOPPED=0,EVENTS_SEQEND[2]={},TASKS_SEQSTART[2]={};
 struct {uintptr_t PTR=0;uint32_t CNT=0,REFRESH=0,ENDDELAY=0;} SEQ[2];
};
using NRF_PWM_Type=PWM;
extern PWM hardware[4];
#define NRF_PWM0 (&hardware[0])
#define NRF_PWM1 (&hardware[1])
#define NRF_PWM2 (&hardware[2])
#define NRF_PWM3 (&hardware[3])
extern uint32_t now;
inline uint32_t millis(){return now;}
inline void pinMode(int,int){}
inline void digitalWrite(int,int){}
static uint32_t g_ADigitalPinMap[15]={};
""")
 source=(ROOT/'variants/heltec_t114/T114AdvertPixels.h').read_text()
 (d/'pixels.h').write_text(source.replace('class T114AdvertPixels {','class T114AdvertPixels {\npublic:'))
 (d/'test.cpp').write_text("""
#include "pixels.h"
#include <cassert>
PWM hardware[4]; uint32_t now=0;
int main(){
 T114AdvertPixels p;p.begin();assert(p._pwm);
 hardware[0].EVENTS_STOPPED=1;p.loop();assert(!p._pwm);
 p.flash(true);assert(p._active);
 for(int led=0;led<2;led++){
  for(int bit=0;bit<16;bit++)assert(p._wave[led*24+bit]==(0x8000|5));
  assert(p._wave[led*24+18]==(0x8000|10));
 }
 hardware[0].EVENTS_STOPPED=1;p.loop();
 now=299;p.loop();assert(p._active && !p._pwm);
 now=300;p.loop();assert(!p._active && p._pwm);
 for(int bit=0;bit<48;bit++)assert(p._wave[bit]==(0x8000|5));
 hardware[0].EVENTS_STOPPED=1;p.loop();
 now=0xfffffff0;p.flash(false);
 assert(p._wave[10]==(0x8000|10) && p._wave[18]==(0x8000|10));
 hardware[0].EVENTS_STOPPED=1;p.loop();now=284;p.loop();assert(!p._active);
}
""")
 subprocess.run(['g++','-std=c++11','-Wall','-Werror','-I'+tmp,str(d/'test.cpp'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
print('RGB driver: both colors, both LEDs, 300 ms off and timer wrap passed.')

# Compile the actual packet-marking and success/failure callback prefixes.
source=(ROOT/'examples/simple_repeater/MyMesh.cpp').read_text()
send=source[source.index('void MyMesh::sendSelfAdvertisement('):source.index('void MyMesh::updateAdvertTimer(')]
success=source[source.index('void MyMesh::logTx('):source.index('#ifdef WITH_BRIDGE',source.index('void MyMesh::logTx('))]+'}\n'
failure=source[source.index('void MyMesh::logTxFail('):source.index('  if (_logging)',source.index('void MyMesh::logTxFail('))]+'}\n'
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp)
 (d/'test.cpp').write_text('''
#include <cassert>
#define T114_BUTTON_RGB
#define ENV_INCLUDE_GPS 1
#define MESH_DEBUG_PRINTLN(x)
namespace mesh {struct Packet {bool button_advert=false;};}
struct GPS {bool fresh=false;bool hasFreshFix(int age){assert(age==5000);return fresh;}} gps;
struct Sensors {GPS* getLocationProvider(){return &gps;}} sensors;
struct Board {int flashes=0;bool blue=false;void flashButtonAdvert(bool f){++flashes;blue=f;}} board;
class MyMesh {public:
 bool _gpsSuccess=false;bool _button_advert_request=false;mesh::Packet* next=nullptr;int default_scope=0;
 struct {int path_hash_mode=0;} _prefs;
 mesh::Packet* createSelfAdvert(){return next;}
 void sendFloodScoped(int,mesh::Packet*,int delay,int){assert(delay==1500);}
 void sendZeroHop(mesh::Packet*,int){}
 void sendSelfAdvertisement(int,bool);
 void logTx(mesh::Packet*,int);void logTxFail(mesh::Packet*,int);
};
'''+send+success+failure+'''
int main(){
 MyMesh m;mesh::Packet button,ordinary,failed;
 m.next=&button;m._button_advert_request=true;m.sendSelfAdvertisement(1500,true);
 assert(button.button_advert && board.flashes==0);
 m._button_advert_request=false;m.next=&ordinary;m.sendSelfAdvertisement(1500,true);
 m.logTx(&ordinary,0);assert(board.flashes==0);
 gps.fresh=true;m.logTx(&button,0);assert(board.flashes==1 && board.blue);
 m.logTx(&button,0);assert(board.flashes==1);
 failed.button_advert=true;m.logTxFail(&failed,0);m.logTx(&failed,0);assert(board.flashes==1);
 button.button_advert=true;gps.fresh=false;m.logTx(&button,0);assert(board.flashes==2 && !board.blue);
 m.next=nullptr;m._button_advert_request=true;m.sendSelfAdvertisement(1500,true);assert(board.flashes==2);
}
''')
 subprocess.run(['g++','-std=c++11',str(d/'test.cpp'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
 (d/'test.cpp').write_text((d/'test.cpp').read_text().replace('gps.fresh=true;m.logTx', 'gps.fresh=false;m._gpsSuccess=true;m.logTx').replace('button.button_advert=true;gps.fresh=false;', 'button.button_advert=true;m._gpsSuccess=false;gps.fresh=true;'))
 subprocess.run(['g++','-std=c++11','-DT114_GPS_ACQUIRE',str(d/'test.cpp'),'-o',str(d/'session')],check=True)
 subprocess.run([str(d/'session')],check=True)
print('Production callbacks: marked packet only, fresh GPS at completion, failure and duplicate suppression passed.')
