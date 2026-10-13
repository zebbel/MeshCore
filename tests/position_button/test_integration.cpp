#include "MyMesh.h"
#include <assert.h>
uint32_t now = 100;
FakeSensors sensors;
int main() {
  DataStore store;
  MyMesh m(store);
  strcpy(m.channels[1].name, "Family"); m.channels[1].channel.secret[0] = 42;
  strcpy(m.channels[2].name, "Other"); m.channels[2].channel.secret[0] = 77;
  assert(!m.setPositionSetting("position_button", "1"));
  assert(!m.setPositionSetting("position_channel", "missing"));
  assert(m.setPositionSetting("position_channel", "Family"));
  assert(m.setPositionSetting("position_button", "1"));
  MyMesh rebooted(store); rebooted.loadPositionSettings();
  assert(rebooted._position.config.enabled);
  assert(strcmp(rebooted._position.config.name, "Family") == 0);
  store.fs.fail = true;
  assert(!m.setPositionSetting("position_channel", "Other"));
  assert(strcmp(m._position.config.name, "Family") == 0);
  store.fs.fail = false;
  m.requestButtonPosition(); assert(sensors.enabled && m._position.pending);
  m.loopButtonPosition(); assert(m.sent == 0);
  sensors.gps.valid = true;
  m.loopButtonPosition(); assert(m.sent == 0);
  ++sensors.gps.timestamp; m.loopButtonPosition();
  assert(m.sent == 1 && !sensors.enabled);
  assert(m.lastText == "position 52.123456, 13.123456" && m.lastKey == 42);
  m.requestButtonPosition(); assert(!m._position.pending); // cooldown
  now += 10000;
  // Moving the selected channel cannot redirect the destination.
  std::swap(m.channels[1], m.channels[2]);
  m.requestButtonPosition(); ++sensors.gps.timestamp; m.loopButtonPosition();
  assert(m.sent == 2 && m.lastKey == 42);
  now += 10000;
  m.requestButtonPosition(); m.channels[2].channel.secret[0] = 88;
  ++sensors.gps.timestamp; m.loopButtonPosition();
  assert(m.sent == 2 && strcmp(m._position.status, "channel_missing") == 0);
  m.channels[2].channel.secret[0] = 42;
  m.requestButtonPosition(); now += 120000; m.loopButtonPosition();
  assert(m.sent == 2 && !sensors.enabled && strcmp(m._position.status, "no_fix") == 0);
  sensors.enabled = true;
  m.requestButtonPosition(); ++sensors.gps.timestamp; m.loopButtonPosition();
  assert(m.sent == 3 && sensors.enabled); // GPS was already on
  now += 10000; m.requestButtonPosition();
  assert(m.setPositionSetting("position_button", "0"));
  ++sensors.gps.timestamp; m.loopButtonPosition(); assert(m.sent == 3);
  assert(!m.setPositionSetting("position_status", "queued"));
  // Reject default Public even when renamed, hashtag names, duplicates.
  uint8_t pub[32] = {0x8b,0x33,0x87,0xe9,0xc5,0xcd,0xea,0x6a,0xc9,0xe5,0xed,0xba,0xa1,0x15,0xcd,0x72};
  memcpy(m.channels[2].channel.secret, pub, 32);
  assert(!m.setPositionSetting("position_channel", "Family"));
  m.channels[2].channel.secret[0] = 42;
  m.channels[3] = m.channels[2];
  assert(!m.setPositionSetting("position_channel", "Family"));
  strcpy(m.channels[3].name, "#public");
  assert(!m.setPositionSetting("position_channel", "#public"));
  assert(m.setPositionSetting("position_channel", ""));
  assert(!m._position.config.enabled && !m._position.config.name[0]);
  // Damaged saved records must not enable sending on reboot.
  (*store.fs.files["/position_button"])[4] ^= 1;
  MyMesh corrupt(store); corrupt.loadPositionSettings(); assert(!corrupt._position.config.enabled);
}
