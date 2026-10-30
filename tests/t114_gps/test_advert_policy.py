"""Exercise production advertisement policy and flooded/button tagging methods."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
cli=(root/'src/helpers/CommonCLI.cpp').read_text()
policy=cli[cli.index('uint8_t CommonCLI::buildAdvertData('):cli.index('\nvoid CommonCLI::handleCommand(')]
s=(root/'examples/simple_repeater/MyMesh.cpp').read_text()
send=s[s.index('void MyMesh::sendSelfAdvertisement('):s.index('\nvoid MyMesh::updateAdvertTimer(')]
source='''#include <stdint.h>
#include <cassert>
#define T114_BUTTON_RGB
#define MESH_DEBUG_PRINTLN(x)
enum {ADVERT_LOC_NONE,ADVERT_LOC_SHARE,ADVERT_LOC_PREFS};
struct Prefs {int advert_loc_policy=0,path_hash_mode=1;const char* node_name="repeater";double node_lat=49.640019,node_lon=8.633326;};
struct Sensors {double node_lat=49.640019,node_lon=8.633326;};
bool hasLocation;double latitude,longitude;
struct AdvertDataBuilder {
 AdvertDataBuilder(uint8_t,const char*){hasLocation=false;}
 AdvertDataBuilder(uint8_t,const char*,double lat,double lon){hasLocation=true;latitude=lat;longitude=lon;}
 uint8_t encodeTo(uint8_t*){return 1;}
};
struct CommonCLI {Prefs* _prefs;Sensors* _sensors;uint8_t buildAdvertData(uint8_t,uint8_t*);};
'''+policy+'''
namespace mesh {struct Packet {bool button_advert=false;};}
struct MyMesh {Prefs _prefs;Sensors sensors;bool _button_advert_request=false,alloc=true;mesh::Packet packet;
 int default_scope=7, floods=0, zero=0;
 mesh::Packet* createSelfAdvert(){if(!alloc)return nullptr;CommonCLI cli{&_prefs,&sensors};uint8_t data;cli.buildAdvertData(1,&data);return &packet;}
 void sendFloodScoped(int scope,mesh::Packet*,int delay,int hash){assert(scope==7 && delay==1500 && hash==2);++floods;}
 void sendZeroHop(mesh::Packet*,int){++zero;}
 void sendSelfAdvertisement(int,bool);
};
'''+send+'''
int main(){MyMesh m;
 for(int policy:{ADVERT_LOC_NONE,ADVERT_LOC_SHARE,ADVERT_LOC_PREFS}){
 m._prefs.advert_loc_policy=policy;m.sendSelfAdvertisement(1500,true);
 assert(!m.packet.button_advert && m.zero==0);
 assert(hasLocation==(policy!=ADVERT_LOC_NONE));
 if(hasLocation)assert(latitude==49.640019 && longitude==8.633326);
 }
 assert(m.floods==3);
 m._button_advert_request=true;m.sendSelfAdvertisement(1500,true);assert(m.packet.button_advert);
 m._button_advert_request=false;m.sendSelfAdvertisement(1500,true);assert(!m.packet.button_advert);
 m.alloc=false;m.sendSelfAdvertisement(1500,true);assert(m.floods==5);
}
'''
source='#include <initializer_list>\n'+source
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.cpp';p.write_text(source)
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror',str(p),'-o',tmp+'/test'],check=True)
 subprocess.run([tmp+'/test'],check=True)
print('Production advert policy: none/share/prefs; flooded scope; independent button tagging; allocation failure passed.')
