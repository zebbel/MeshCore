
#include "MyMesh.h"
#include <cassert>
uint32_t now = 0;
FakeSensors sensors;
int main() {
  DataStore store; MyMesh m(store); char reply[166];
  strcpy(m.channels[1].name, "Family"); m.channels[1].channel.secret[0] = 9;
  auto command = [&](const char* c) { memset(reply, 0x55, sizeof(reply)); assert(m.handleCustomCLI(c, reply)); assert(strlen(reply) < 160); };
  command("?"); assert(strstr(reply, "position-target")); command("? position");
  command("set led off"); assert(!m.isStatusLedEnabled());
  command("set led potato"); assert(!m.isStatusLedEnabled());
  command("set position-target channel 0102030405060708090a0b0c0d0e0f10 Family");
  assert(m._position.config.enabled && m._position.config.scopeKey[0] == 1);
  PositionButtonConfig saved = m._position.config;
  assert(!m.handleCustomCLI("set position-target", reply));
  command("set position-target channel invalid Family");
  assert(!memcmp(&saved, &m._position.config, sizeof(saved)));
  strcpy(m.channels[2].name, "Family"); m.channels[2].channel.secret[0] = 8;
  command("set position-target channel none Family");
  assert(!memcmp(&saved, &m._position.config, sizeof(saved)));
  m.channels[2].name[0] = 0;
  command("set position-button off"); assert(!m._position.config.enabled);
  command("set position-button on"); assert(m._position.config.enabled);
  MyMesh reboot(store); reboot.channels[1] = m.channels[1]; reboot.loadPositionSettings();
  assert(reboot._position.config.enabled && reboot._position.config.scopeKey[0] == 1 && !reboot.isStatusLedEnabled());
  m.hasRoom = true; strcpy(m.roomContact.name, "Room"); m.roomContact.type = ADV_TYPE_ROOM; m.roomContact.id.pub_key[0] = 7;
  command("set position-target room none Room"); assert(m._position.config.destinationType == POSITION_DEST_ROOM);
  command("get position-target"); assert(strstr(reply, "Room"));
  command("get position-status"); command("get led"); command("get position-button");
  return 0;
}
