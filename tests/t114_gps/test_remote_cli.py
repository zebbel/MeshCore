"""Host execution of production remote authorization/replay gate and CLI dispatch.
Crypto/radio transport is mocked; this is not an over-the-air security test.
"""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'examples/simple_repeater/MyMesh.cpp').read_text()
peer=s[s.index('void MyMesh::onPeerDataRecv('):s.index('\nbool MyMesh::onPeerPathRecv(')]
start=s.index('  while (*command ==',s.index('void MyMesh::handleCommand('))
end=s.index('  // handle ACL related commands',start)
dispatch=s[start:end]
source=r'''
#include <stdint.h>
#include <cstring>
#include <cassert>
#include <cstdio>
#define T114_GPS_ACQUIRE
#define MESH_DEBUG_PRINTLN(...)
enum {PAYLOAD_TYPE_REQ=1,PAYLOAD_TYPE_TXT_MSG=2,PAYLOAD_TYPE_RESPONSE=3,TXT_TYPE_PLAIN=0,TXT_TYPE_CLI_DATA=1,TXT_TYPE_CLI_COMMAND=2,OUT_PATH_UNKNOWN=255,PUB_KEY_SIZE=32,SERVER_RESPONSE_DELAY=1,TXT_ACK_DELAY=1,CLI_REPLY_DELAY_MILLIS=1};
namespace mesh {
struct Packet {uint8_t path[1]={},path_len=0;bool isRouteFlood(){return false;}int getPathHashSize(){return 1;}};
struct Utils {template<class... T>static void sha256(T...) {}};
}
struct Identity {uint8_t pub_key[32]={};};
struct ClientInfo {bool admin=true;uint32_t last_timestamp=0,last_activity=0;Identity id;uint8_t out_path[1]={},out_path_len=255;bool isAdmin(){return admin;}};
struct ACL {ClientInfo client;int getNumClients(){return 1;}ClientInfo* getClientByIdx(int){return &client;}};
struct Clock {uint32_t getCurrentTime(){return 100;}uint32_t getCurrentTimeUnique(){return 101;}};
struct MyMesh {
 int matching_peer_indexes[1]={0},starts=0,calls=0,fallbacks=0;
 bool active=false;const char* status="GPS OFF";const char* result="GPS update started";
 ACL acl;Clock clock;uint8_t reply_data[1]={};char lastReply[166]={};
 Clock* getRTCClock(){return &clock;}
 const char* startGPSAcquisition(){++calls;if(active)return "GPS update already running";
 if(strcmp(result,"GPS update started"))return result;
 ++starts;active=true;status="GPS SEARCHING";return result;}
 const char* gpsAcquisitionStatus(){return status;}
 int handleRequest(ClientInfo*,uint32_t,uint8_t*,size_t){return 0;}
 template<class... T>mesh::Packet* createPathReturn(T...){return nullptr;}
 template<class... T>mesh::Packet* createAck(T...){return nullptr;}
 template<class... T>void sendFloodReply(T...){}
 template<class... T>void sendDirect(T...){}
 mesh::Packet* createDatagram(int,Identity,const uint8_t*,uint8_t* data,int len){
 assert(len>=5 && len<166);memcpy(lastReply,data+5,len-5);lastReply[len-5]=0;return nullptr;}
 void handleCommand(uint32_t,char*,char*);
 void onPeerDataRecv(mesh::Packet*,uint8_t,int,const uint8_t*,uint8_t*,size_t);
};
'''+peer+'''
void MyMesh::handleCommand(uint32_t, char* command, char* reply){
'''+dispatch+'''
 ++fallbacks;strcpy(reply,"fallback");
}
'''+r'''
void remote(MyMesh& m,uint32_t time,const char* command,int flags=TXT_TYPE_CLI_COMMAND){
 mesh::Packet p;uint8_t data[180]={};memcpy(data,&time,4);data[4]=flags<<2;
 strcpy((char*)data+5,command);m.lastReply[0]=0;
 m.onPeerDataRecv(&p,PAYLOAD_TYPE_TXT_MSG,0,nullptr,data,5+strlen(command));
}
int main(){
 MyMesh m;m.matching_peer_indexes[0]=-1;remote(m,10,"gps update");assert(m.calls==0);
 m.matching_peer_indexes[0]=0;m.acl.client.admin=false;
 remote(m,10,"gps update");assert(m.calls==0 && m.acl.client.last_timestamp==0);
 m.acl.client.admin=true;remote(m,10,"gps update",31);assert(m.calls==0);
 remote(m,10,"gps update");assert(m.starts==1 && !strcmp(m.lastReply,"GPS update started"));
 remote(m,11,"gps update");assert(m.starts==1 && !strcmp(m.lastReply,"GPS update already running"));
 remote(m,11,"gps update");assert(m.calls==2 && !m.lastReply[0]);
 remote(m,9,"gps update");assert(m.calls==2);
 m.active=false;m.status="FIX SAVED - GPS OFF";
 remote(m,11,"gps update");assert(m.starts==1); // replay after completed acquisition
 remote(m,12,"gps update");assert(m.starts==2);
 const char* states[]={"GPS SEARCHING","GPS FIX / SYNCING","FIX SAVED - GPS OFF","GPS TIMEOUT","GPS SAVE FAILED","GPS UNAVAILABLE","GPS OFF"};
 uint32_t time=13;
 for(auto state:states){m.status=state;remote(m,time++,"gps status");assert(!strcmp(m.lastReply,state));}
 char reply[170]={},cmd[64];
 for(auto result:{"ERR: GPS unavailable","ERR: GPS start failed"}){
 m.active=false;m.result=result;strcpy(cmd,"gps update");m.handleCommand(0,cmd,reply);assert(!strcmp(reply,result));}
 m.result="GPS update started";strcpy(cmd,"  ab|gps update");m.handleCommand(0,cmd,reply);
 assert(!strcmp(reply,"ab|GPS update started") && m.starts==3);
 for(auto command:{"gps on","gps off","gps sync","gps diag","gps advert","gps update extra"}){
 strcpy(cmd,command);m.handleCommand(0,cmd,reply);assert(!strcmp(reply,"fallback"));}
 assert(m.fallbacks==6);
}
'''
source='#include <initializer_list>\n'+source
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.cpp';p.write_text(source)
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror',str(p),'-o',tmp+'/test'],check=True)
 subprocess.run([tmp+'/test'],check=True)
# Verify the entry points continue sharing the production acquisition function.
main=(root/'examples/simple_repeater/main.cpp').read_text()
ui=(root/'examples/simple_repeater/UITask.cpp').read_text()
assert 'the_mesh.handleCommand(0, command, reply)' in main
assert 'the_mesh.startGPSAcquisition();' in ui
print('Production peer gate/dispatch: admin, guest, unknown peer, replay, duplicate after completion, USB, prefixes, all statuses and generic GPS fallback passed.')
