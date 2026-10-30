from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp)
 (d/'GPSAcquisition.cpp').write_bytes((ROOT/'examples/simple_repeater/GPSAcquisition.cpp').read_bytes())
 (d/'MyMesh.h').write_text("""
#pragma once
#include <stdint.h>
#include <cstring>
extern uint32_t now;
inline uint32_t millis(){return now;}
struct GPS {
 bool enabled=false,fresh=false,date=false,waiting=true;
 bool hasFreshFix(int){return fresh;} bool hasValidDateTime(){return date;}
 bool waitingTimeSync(){return waiting;} bool isEnabled(){return enabled;}
 void syncTime(){waiting=true;fresh=false;}
 long getTimestamp(){return 1800000000;} long getLatitude(){return 49640019;}
 long getLongitude(){return 8633326;}
};
struct Sensors {
 GPS gps;bool present=true;double node_lat=0,node_lon=0;int starts=0,stops=0;
 GPS* getLocationProvider(){return present?&gps:nullptr;}
 const char* getSettingByKey(const char*){return gps.enabled?"1":"0";}
 bool setSettingValue(const char*,const char* v){gps.enabled=v[0]=='1';if(gps.enabled)++starts;else ++stops;return present;}
};
extern Sensors sensors;
struct Clock {uint32_t time=1800000000;uint32_t getCurrentTime(){return time;}};
struct CLI {bool ok=true;int saves=0;bool savePrefs(void*){++saves;return ok;}};
struct MyMesh {
 bool _gpsSuccess=false,_gpsAcquiring=false;uint32_t _gpsStarted=0;const char* _gpsStatus="GPS OFF";
 struct {double node_lat=1,node_lon=2;int gps_enabled=0;} _prefs;
 CLI _cli;void* _fs=nullptr;Clock clock;
 Clock* getRTCClock(){return &clock;}
 void startGPSAcquisition();void loopGPSAcquisition();
};
""")
 (d/'target.h').write_text('#include "MyMesh.h"\n')
 (d/'test.cpp').write_text("""
#include "MyMesh.h"
#include <cassert>
uint32_t now=0;Sensors sensors;
void fixed(){sensors.gps.fresh=true;sensors.gps.date=true;sensors.gps.waiting=false;}
int main(){
 MyMesh m;m.startGPSAcquisition();assert(m._gpsAcquiring && sensors.starts==1 && !m._gpsSuccess);
 m.startGPSAcquisition();assert(sensors.starts==1);
 sensors.gps.fresh=true;m.loopGPSAcquisition();assert(m._cli.saves==0);
 fixed();m.clock.time=1800000010;m.loopGPSAcquisition();assert(m._cli.saves==0);
 m.clock.time=1800000000;m.loopGPSAcquisition();
 assert(m._gpsSuccess && !sensors.gps.enabled && m._prefs.gps_enabled==0);
 assert(m._prefs.node_lat==49.640019 && m._prefs.node_lon==8.633326);
 m.loopGPSAcquisition();assert(m._gpsSuccess);
 double saved=m._prefs.node_lat;
 m.startGPSAcquisition();assert(!m._gpsSuccess && sensors.gps.enabled);
 now=600000;m.loopGPSAcquisition();assert(!sensors.gps.enabled && !strcmp(m._gpsStatus,"GPS TIMEOUT"));
 assert(m._prefs.node_lat==saved);
 m.startGPSAcquisition();fixed();m._cli.ok=false;m.loopGPSAcquisition();
 assert(!m._gpsSuccess && m._prefs.node_lat==saved && !sensors.gps.enabled);
 m._cli.ok=true;m.startGPSAcquisition();sensors.setSettingValue("gps","0");m.loopGPSAcquisition();
 assert(!m._gpsAcquiring && !strcmp(m._gpsStatus,"GPS OFF"));
 sensors.present=false;m.startGPSAcquisition();assert(!m._gpsSuccess && !strcmp(m._gpsStatus,"GPS UNAVAILABLE"));
 sensors.present=true;MyMesh reboot;reboot.startGPSAcquisition();assert(!reboot._gpsSuccess && sensors.gps.enabled);
}
""")
 subprocess.run(['g++','-std=c++11','-Wall','-Werror','-DT114_GPS_ACQUIRE','-DENV_INCLUDE_GPS=1','-I'+tmp,str(d/'GPSAcquisition.cpp'),str(d/'test.cpp'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
print('Production GPS acquisition: boot, sync confirmation, save failure, timeout, retry, manual off, and restart passed.')

source=(ROOT/'src/helpers/sensors/MicroNMEALocationProvider.h').read_text()
start=source.index('    bool hasValidDateTime() override')
end=source.index('\n    long getTimestamp()',start)
method=source[start:end].replace(' override','')
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp)
 (d/'date.cpp').write_text('#include <stdint.h>\n#include <cassert>\nstruct NMEA {int year=2026,month=10,day=30,hour=12,minute=0,second=0;'
 'int getYear(){return year;}int getMonth(){return month;}int getDay(){return day;}'
 'int getHour(){return hour;}int getMinute(){return minute;}int getSecond(){return second;}};'
 'struct GPS {NMEA nmea;'+method+'};'
 'int main(){GPS g;assert(g.hasValidDateTime());g.nmea.year=0;assert(!g.hasValidDateTime());'
 'g.nmea.year=2025;g.nmea.month=2;g.nmea.day=29;assert(!g.hasValidDateTime());'
 'g.nmea.year=2024;assert(g.hasValidDateTime());g.nmea.second=60;assert(!g.hasValidDateTime());}')
 subprocess.run(['g++','-std=c++11',str(d/'date.cpp'),'-o',str(d/'date')],check=True)
 subprocess.run([str(d/'date')],check=True)
print('Production GPS date validation: valid date, unset year, leap days, invalid seconds passed.')
