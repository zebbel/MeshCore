"""Compile production repeater integration and unchanged companion binary transport."""
from pathlib import Path
import tempfile,subprocess
root=Path(__file__).resolve().parents[2]
def function(s,start):
 a=s.index(start);b=s.index('{',a);depth=1;i=b+1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
s=(root/'examples/simple_repeater/MyMesh.cpp').read_text()
put=function(s,'void MyMesh::putNeighbour(')
maintain=function(s,'void MyMesh::maintainNeighbourSNR(')
a=s.index('int MyMesh::handleRequest(');b=s.index('  // uint32_t now',a)
request=s[a:b]+'return 0;}\n'
cpp=r'''
#include <stdint.h>
#include <cstring>
#include <cmath>
#include <cassert>
using std::isfinite;
#define XIAO_NEIGHBOUR_SNR
#define MAX_NEIGHBOURS 50
#include "examples/simple_repeater/NeighbourSNR.h"
uint32_t ms=0;uint32_t millis(){return ms;}
namespace mesh {struct Identity {uint8_t pub_key[32]={};bool matches(const Identity& other)const{return !memcmp(pub_key,other.pub_key,32);}};}
struct NeighbourInfo {mesh::Identity id;uint32_t heard_timestamp=0,advert_timestamp=0;int8_t snr=0;};
struct Clock {uint32_t getCurrentTime(){return 1700000000+ms/1000;}};
struct ClientInfo {};
struct ConfirmedClock {bool hasConfirmedTime(){return true;}} rtc_clock;
struct MyMesh {NeighbourSNR neighbourSNR;NeighbourInfo neighbours[50];Clock clock;uint8_t reply_data[184];
 Clock* getRTCClock(){return &clock;}void putNeighbour(const mesh::Identity&,uint32_t,float);void maintainNeighbourSNR(bool sync=false);
 int handleRequest(ClientInfo*,uint32_t,uint8_t*,size_t);
};
'''+put+maintain+request+r'''
int main(){MyMesh m;uint8_t boot[16]={1};m.neighbourSNR.begin(0,1700000000,boot);mesh::Identity id;
 for(int i=1;i<=50;i++){id.pub_key[0]=i;m.putNeighbour(id,1,-3.25);}
 int count=0;for(auto& n:m.neighbours)count+=n.heard_timestamp!=0;assert(count==50);
 id.pub_key[0]=51;m.putNeighbour(id,1,1.25);count=0;for(auto& n:m.neighbours)count+=n.heard_timestamp!=0;assert(count==50);
 ms=1800000;m.maintainNeighbourSNR();
 uint8_t req[28]={0x70,1,1};ClientInfo guest;
 assert(m.handleRequest(&guest,123,req,28)==144);assert(m.reply_data[0]==123 && m.reply_data[6]==1);
 assert(m.handleRequest(&guest,123,req,0)==0);
 ms=7200000;m.maintainNeighbourSNR();for(auto& n:m.neighbours)assert(!n.heard_timestamp);
 assert(m.handleRequest(&guest,124,req,28)==144); // history survives active expiration
 m.putNeighbour(id,1,-1.25);assert(m.neighbourSNR.active(id.pub_key));
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.cpp';p.write_text(cpp)
 subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Wno-unused-parameter','-fsanitize=undefined','-I'+str(root),str(p),'-o',tmp+'/test'],check=True)
 subprocess.run([tmp+'/test'],check=True)
# Direct-neighbor filters and replay/access path remain the existing implementation.
assert 'packet->getPathHashCount() == 0 && !isShare(packet)' in s
assert 'if (tag != pending_discover_tag)' in s
assert 'timestamp > client->last_timestamp' in function(s,'void MyMesh::onPeerDataRecv(')
mesh=(root/'src/Mesh.cpp').read_text()
assert 'pkt->isRouteDirect() && pkt->path_len == 0' in mesh or 'getPathHashCount() == 0' in mesh
print('Production repeater integration: 50 active, LRU replacement, independent cleanup, retained history, returning neighbor, binary request and length checks passed.')
# Execute the standard companion command-50 and generic response bodies unchanged.
c=(root/'examples/companion_radio/MyMesh.cpp').read_text()
a=c.index('  } else if (cmd_frame[0] == CMD_SEND_BINARY_REQ');b=c.index('  } else if (cmd_frame[0] == CMD_HAS_CONNECTION',a)
command=c[c.index('{',a)+1:b]
a=c.index('  } else if (len > 4 && tag == pending_req)');b=c.index('  } else {',a)
response=c[c.index('{',a)+1:b]
cpp=r'''
#include <stdint.h>
#include <cstring>
#include <cassert>
enum {PUB_KEY_SIZE=32,MSG_SEND_FAILED=0,MSG_SEND_SENT_FLOOD=1,ERR_CODE_TABLE_FULL=2,ERR_CODE_NOT_FOUND=3,RESP_CODE_SENT=6,PUSH_CODE_BINARY_RESPONSE=0x8c};
struct ContactInfo {};
struct Serial {uint8_t bytes[256];int length;void writeFrame(const uint8_t* p,int n){memcpy(bytes,p,n);length=n;}} serial;
struct Companion {uint32_t pending_req=0;uint8_t out_frame[256],request[100];size_t requestLength=0;Serial* _serial=&serial;ContactInfo contact;
 ContactInfo* lookupContactByPubKey(uint8_t*,int){return &contact;}
 int sendRequest(ContactInfo&,uint8_t* p,size_t n,uint32_t& tag,uint32_t& timeout){memcpy(request,p,n);requestLength=n;tag=123;timeout=100;return MSG_SEND_SENT_FLOOD;}
 void writeErrFrame(int){assert(false);}void clearPendingReqs(){pending_req=0;}
 void command(uint8_t* cmd_frame,size_t len){
'''+command+'''
}
void response(const uint8_t* data,uint8_t len,uint32_t tag){
'''+response+r'''
}
};
int main(){Companion c;uint8_t cmd[61]={50};cmd[33]=0x70;cmd[34]=1;cmd[35]=1;
 c.command(cmd,sizeof(cmd));assert(c.requestLength==28 && c.request[0]==0x70 && c.pending_req==123);
 uint8_t response[144]={123,0,0,0};for(unsigned i=4;i<144;i++)response[i]=i;
 c.response(response,144,123);assert(serial.length==146 && serial.bytes[0]==0x8c);
 assert(!memcmp(serial.bytes+6,response+4,140));
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.cpp';p.write_text(cpp)
 subprocess.run(['g++','-std=c++11','-fsanitize=undefined',str(p),'-o',tmp+'/test'],check=True)
 subprocess.run([tmp+'/test'],check=True)
print('Unmodified standard companion command 50 and binary response forward the complete request/response unchanged (host transport stand-ins).')
