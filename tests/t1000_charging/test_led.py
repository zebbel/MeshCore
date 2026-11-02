"""Execute production single-LED handler and PWM release with hardware stand-ins."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'examples/companion_radio/ui-orig/UITask.cpp').read_text()
a=s.index('#elif defined(PIN_STATUS_LED)',s.index('void UITask::userLedHandler'))
b=s.index('\n#endif\n}',a)
body=s[a+len('#elif defined(PIN_STATUS_LED)'):b]
s=(root/'variants/t1000-e/T1000eBoard.cpp').read_text()
release=s[s.index('void T1000eBoard::stopStatusLedPWM()'):s.rindex('#endif')]
source=r'''
#include <stdint.h>
#include <cassert>
#include <vector>
#define HIGH 1
#define LOW 0
#define LED_STATE_ON HIGH
#define PIN_STATUS_LED 24
#define EXT_PWR_DETECT 5
#define EXT_CHRG_DETECT 35
#define LED_ON_MILLIS 20
#define LED_ON_MSG_MILLIS 200
#define LED_CYCLE_MILLIS 4000
#define HWPWM_MODULE_NUM 2
uint32_t now=0;int external=0,charge=1,output=0,reads=0,writes=0;
uint32_t millis(){return now;}
int digitalRead(int pin){++reads;return pin==5?external:charge;}
void digitalWrite(int,int value){output=value;}
struct PWM {bool led=false,other=false,stopped=false;
 bool removePin(int){bool result=led;led=false;return result;}
 int usedChannelCount(){return int(led)+int(other);}
 void stop(){stopped=true;}
} pwm[2];PWM* HwPWMx[]={&pwm[0],&pwm[1]};
std::vector<int> values;
void analogWrite(int,uint32_t value){assert(value<=255);pwm[0].led=true;pwm[0].stopped=false;++writes;values.push_back(value);}
struct T1000eBoard {static void stopStatusLedPWM();};
struct Mesh {bool enabled=true;bool isStatusLedEnabled(){return enabled;}} the_mesh;
struct UITask {int _msgcount=0;void userLedHandler();};
'''+release+'\nvoid UITask::userLedHandler(){\n'+body+'\n}\n'+r'''
int main(){UITask ui;
 auto tick=[&](uint32_t ms){now=ms;ui.userLedHandler();};
 tick(0);tick(100);tick(200);tick(300);tick(301);assert(output==1);tick(322);assert(output==0);
 the_mesh.enabled=false;
 external=1;charge=0;tick(400);tick(500);external=0;tick(600);external=1;tick(700);tick(800);tick(900);
 assert(writes==0);tick(1000);assert(values.back()==0);
 int before=reads;tick(1010);assert(reads==before && values.size()==1);
 for(int t=1020;t<=3000;t+=20){
   if(t==1500)the_mesh.enabled=true;
   if(t==2300)the_mesh.enabled=false;
   tick(t);
 }
 assert(!the_mesh.enabled); // indication never rewrites the preference
 assert(values.size()==101 && values[0]==0 && values[50]==255 && values[100]==0);
 for(int i=1;i<=50;i++)assert(values[i]>=values[i-1]);
 for(int i=51;i<=100;i++)assert(values[i]<=values[i-1]);
 for(int i=1;i<=100;i++)assert(values[i]-values[i-1]<=8 && values[i-1]-values[i]<=8);
 charge=1;tick(3100);tick(3200);tick(3300);tick(3400);
 assert(!pwm[0].led && pwm[0].stopped && output==0);
 int n=writes;tick(4500);assert(writes==n && output==0);
 the_mesh.enabled=true;tick(4501);assert(writes==n && output==0);
 the_mesh.enabled=false;tick(4502);assert(writes==n && output==0);
 charge=0;tick(4600);tick(4700);tick(4800);tick(4900);tick(5400);assert(pwm[0].led);
#ifdef ENABLE_POSITION_BUTTON
 tick(5401);assert(pwm[0].led && !the_mesh.enabled);
 tick(6000);assert(values.back()>0 && !the_mesh.enabled);
#endif
 external=0;tick(6100);tick(6200);tick(6300);tick(6400);assert(!pwm[0].led);
#ifdef ENABLE_POSITION_BUTTON
 assert(output==0 && !the_mesh.enabled);
 the_mesh.enabled=true;tick(6401);assert(output==1);
#else
 assert(output==1);
#endif
 tick(6422);assert(output==0);
 ui._msgcount=1;tick(10403);assert(output==1);tick(10500);assert(output==1);tick(10604);assert(output==0);
 // A shared peripheral must not be stopped when detaching the LED.
 pwm[0].led=true;pwm[0].other=true;pwm[0].stopped=false;
 T1000eBoard::stopStatusLedPWM();assert(!pwm[0].led && !pwm[0].stopped && pwm[0].other && output==0);
 // PWM phase and debounce survive millis rollover.
 external=1;charge=0;tick(0xfffffe00u);tick(0xfffffe64u);tick(0xfffffec8u);tick(0xffffff2cu);
 tick(300);assert(pwm[0].led && values.back()>0);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.cpp';p.write_text(source)
 for flags in (['-DT1000_CHARGING_LED'],['-DT1000_CHARGING_LED','-DENABLE_POSITION_BUTTON']):
  subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=undefined',*flags,str(p),'-o',tmp+'/test'],check=True)
  subprocess.run([tmp+'/test'],check=True)
 # Compile the unchanged single-LED path without the new target flag.
 subprocess.run(['g++','-std=c++11','-c',str(p),'-o',tmp+'/generic.o'],check=True)
ini=(root/'variants/t1000-e/platformio.ini').read_text()
assert ini.count('-D T1000_CHARGING_LED')==2
for name in ('usb','ble'):
 section=ini.split('[env:t1000e_companion_radio_'+name+']')[1].split('\n[')[0]
 assert '-D T1000_CHARGING_LED' in section
assert 'extends = env:t1000e_companion_radio_ble' in ini
board=(root/'variants/t1000-e/T1000eBoard.h').read_text()
assert 'stopStatusLedPWM();' in board.split('void powerOff() override')[1]
variant=(root/'variants/t1000-e/variant.cpp').read_text()
assert 'pinMode(EXT_CHRG_DETECT, INPUT);' in variant and 'pinMode(EXT_PWR_DETECT, INPUT);' in variant
print('T1000 LED: 2-second eased PWM, debounce, stop/reconnect, disabled preference, heartbeat/unread restore, PWM release/shared pins, rollover, target guards passed.')
