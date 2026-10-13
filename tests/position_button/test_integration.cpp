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
  DataStore phoneStore;
  MyMesh phone(phoneStore);
  phone.channels[1] = m.channels[1];
  strcpy(phone.channels[1].name, "Family");
  phone.channels[2] = phone.channels[1];
  phone.channels[2].channel.secret[0] = 91; // same name, different destination
  auto command = [&](uint8_t slot, const char* text) {
    return phone.handlePositionButtonCommand(slot, text, strlen(text));
  };
  assert(command(2, "/position-button on") == 1);
  assert(phone._position.config.enabled);
  ChannelDetails chosen;
  assert(phone.findPositionChannel(chosen) && chosen.channel.secret[0] == 91);
  MyMesh saved(phoneStore); saved.loadPositionSettings();
  assert(saved._position.config.enabled);
  assert(memcmp(saved._position.config.fingerprint, phone._position.config.fingerprint, 32) == 0);
  assert(command(2, "ordinary message") == 0);
  assert(command(2, "/position-button nonsense") == -1);
  assert(command(2, "/position-button") == -1);
  assert(command(2, "/position-button on extra") == -1);
  assert(command(2, "/position-button-typo on") == -1);
  assert(command(255, "/position-button on") == -1);
  assert(command(0, "/position-button on") == -1); // empty channel
  memcpy(phone.channels[3].channel.secret, pub, 32);
  strcpy(phone.channels[3].name, "Public renamed");
  assert(command(3, "/position-button on") == -1);
  assert(command(1, "/position-button on") == 1);
  auto before = phone._position.config;
  phoneStore.fs.fail = true;
  assert(command(2, "/position-button on") == -1);
  assert(memcmp(&before, &phone._position.config, sizeof(before)) == 0);
  assert(command(2, "/position-button off") == -1);
  assert(phone._position.config.enabled);
  phoneStore.fs.fail = false;
  phone.requestButtonPosition(); assert(phone._position.pending);
  assert(command(0, "  /position-button off \r\n") == 1);
  assert(!phone._position.config.enabled && !phone._position.pending);
  assert(phone.sent == 0); // configuring never queues a radio message
  // Bounded frame text: it need not be NUL terminated.
  const char bounded[] = {'/', 'p', 'o', 's', 'i', 't', 'i', 'o', 'n', '-', 'b', 'u', 't', 't', 'o', 'n', ' ', 'o', 'n', 'X'};
  assert(phone.handlePositionButtonCommand(2, bounded, sizeof(bounded) - 1) == 1);
  assert(phone.handlePositionButtonCommand(2, bounded, sizeof(bounded)) == -1);

}
