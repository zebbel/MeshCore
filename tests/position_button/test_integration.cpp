#include "MyMesh.h"
#include <assert.h>
uint32_t now = 100;
FakeSensors sensors;
int main() {
  DataStore store;
  MyMesh m(store);
  FakeUI feedback; m._ui = &feedback;
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
  m.requestButtonPosition(); assert(!sensors.enabled && m._position.pending);
  m.loopButtonPosition();
  assert(m.sent == 0 && !m._position.pending && strcmp(m._position.status, "no_fix") == 0);
  assert(feedback.acks == 0); // no beep when no position is available
  sensors.node_lat = 52.123456; sensors.node_lon = 13.123456;
  // Receiver still has no valid fix: use the stored advertised coordinates.
  assert(!sensors.gps.valid);
  m.requestButtonPosition(); m.loopButtonPosition();
  assert(feedback.acks == 1); // one confirmation only after successful queueing
  assert(m.sent == 1 && !sensors.enabled); // stored position, GPS stays off
  assert(m.lastText == "position 52.123456, 13.123456" && m.lastKey == 42);
  m.requestButtonPosition(); assert(!m._position.pending); // cooldown
  assert(feedback.acks == 1);
  now += 10000;
  // Moving the selected channel cannot redirect the destination.
  std::swap(m.channels[1], m.channels[2]);
  m.requestButtonPosition(); m.loopButtonPosition();
  assert(m.sent == 2 && m.lastKey == 42);
  now += 10000;
  m.requestButtonPosition(); m.channels[2].channel.secret[0] = 88;
  m.loopButtonPosition();
  assert(m.sent == 2 && strcmp(m._position.status, "channel_missing") == 0);
  m.channels[2].channel.secret[0] = 42;
  sensors.node_lat = 91; // invalid stored position must not be transmitted
  m.requestButtonPosition(); m.loopButtonPosition();
  assert(m.sent == 2 && !sensors.enabled && strcmp(m._position.status, "no_fix") == 0);
  sensors.node_lat = 52.123456;
  sensors.enabled = true;
  m.requestButtonPosition(); m.loopButtonPosition();
  assert(m.sent == 3 && sensors.enabled); // GPS was already on
  now += 10000; m.requestButtonPosition();
  assert(m.setPositionSetting("position_button", "0"));
  m.loopButtonPosition(); assert(m.sent == 3);
  assert(!m.setPositionSetting("position_status", "queued"));
  assert(feedback.acks == 3);
  assert(m.setPositionSetting("position_button", "1"));
  m.sendOK = false;
  m.requestButtonPosition(); m.loopButtonPosition();
  assert(strcmp(m._position.status, "send_failed") == 0);
  assert(feedback.acks == 3); // failed transmission allocation must not beep
  m.sendOK = true;

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
  assert(phone.isStatusLedEnabled());
  assert(command(0, "/led off") == 1); // any channel, no destination selection
  assert(!phone.isStatusLedEnabled());
  MyMesh ledReboot(phoneStore); ledReboot.loadPositionSettings();
  assert(!ledReboot.isStatusLedEnabled()); // survives reboot without position record
  assert(command(0, "/led on extra") == -1);
  assert(command(0, "/led") == -1);
  assert(command(0, "/led-typo off") == -1);
  phoneStore.fs.fail = true;
  assert(command(0, "/led on") == -1);
  assert(!phone.isStatusLedEnabled()); // failed save leaves state intact
  phoneStore.fs.fail = false;
  assert(command(0, " \t/led on\r\n") == 1);
  assert(phone.isStatusLedEnabled());
  MyMesh ledOnReboot(phoneStore); ledOnReboot.loadPositionSettings();
  assert(ledOnReboot.isStatusLedEnabled());
  (*phoneStore.fs.files["/status_led"])[4] = 9;
  MyMesh invalidLed(phoneStore); invalidLed.loadPositionSettings();
  assert(invalidLed.isStatusLedEnabled()); // invalid record defaults to on
  assert(phone.sent == 0);
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

  // Scope is captured with the channel, survives reboot, and ignores subsequent
  // app overrides/default changes. Enqueue restores all app state, even on error.
  DataStore scopedStore;
  MyMesh scoped(scopedStore);
  scoped.channels[1] = phone.channels[1];
  scoped.send_scope.key[0] = 31;
  const char* on = "/position-button on";
  assert(scoped.handlePositionButtonCommand(1, on, strlen(on)) == 1);
  MyMesh scopedBoot(scopedStore); scopedBoot.loadPositionSettings();
  scopedBoot.channels[1] = scoped.channels[1];
  assert(scopedBoot._position.config.scopeKey[0] == 31);
  scopedBoot.send_scope.key[0] = 99;
  scopedBoot.send_unscoped = true;
  scopedBoot._prefs.default_scope_key[0] = 88;
  scopedBoot.requestButtonPosition(); scopedBoot.loopButtonPosition();
  assert(scopedBoot.sent == 1 && scopedBoot.lastScope[0] == 31);
  assert(scopedBoot.send_scope.key[0] == 99 && scopedBoot.send_unscoped);
  now += 10000;
  scopedBoot.sendOK = false;
  scopedBoot.requestButtonPosition(); scopedBoot.loopButtonPosition();
  assert(scopedBoot.send_scope.key[0] == 99 && scopedBoot.send_unscoped);
  scopedBoot.sendOK = true;
  // Explicitly unscoped remains unscoped even when a default is later set.
  assert(scopedBoot.handlePositionButtonCommand(1, on, strlen(on)) == 1);
  scopedBoot.send_unscoped = false;
  scopedBoot.requestButtonPosition(); scopedBoot.loopButtonPosition();
  uint8_t noScope[16] = {};
  assert(memcmp(scopedBoot.lastScope, noScope, 16) == 0);
  assert(!scopedBoot.send_unscoped && scopedBoot.send_scope.key[0] == 99);
  // Snapshot the effective default when there is no app override.
  now += 10000;
  memset(scopedBoot.send_scope.key, 0, 16);
  assert(scopedBoot.handlePositionButtonCommand(1, on, strlen(on)) == 1);
  scopedBoot._prefs.default_scope_key[0] = 77;
  scopedBoot.requestButtonPosition(); scopedBoot.loopButtonPosition();
  assert(scopedBoot.lastScope[0] == 88);
  assert(scopedBoot.send_scope.isNull() && !scopedBoot.send_unscoped);

  // Room-server chat is a direct contact, not a group channel. The local app
  // command saves the room's full identity; only subsequent button presses send.
  DataStore roomStore;
  MyMesh roomMesh(roomStore);
  roomMesh.hasRoom = true;
  strcpy(roomMesh.roomContact.name, "Bot room");
  roomMesh.roomContact.type = ADV_TYPE_ROOM;
  roomMesh.roomContact.id.pub_key[0] = 123;
  roomMesh.roomContact.id.pub_key[31] = 99;
  roomMesh.send_scope.key[0] = 44;
  const char* roomOn = "/position-button on";
  assert(roomMesh.handlePositionButtonCommand(0, roomOn, strlen(roomOn), &roomMesh.roomContact) == 1);
  assert(roomMesh.sent == 0);
  assert(roomMesh._position.config.enabled);
  assert(roomMesh._position.config.destinationType == POSITION_DEST_ROOM);
  assert(strcmp(roomMesh._position.config.name, "Bot room") == 0);
  assert(roomMesh._position.config.fingerprint[0] == 123);
  assert(roomMesh._position.config.fingerprint[31] == 99);
  assert(roomMesh.handlePositionButtonCommand(0, "hello", 5, &roomMesh.roomContact) == 0);
  assert(roomMesh.handlePositionButtonCommand(0, "/position-button typo", 21, &roomMesh.roomContact) == -1);
  ContactInfo notRoom = roomMesh.roomContact;
  notRoom.type = 0;
  assert(roomMesh.handlePositionButtonCommand(0, roomOn, strlen(roomOn), &notRoom) == -1);
  MyMesh restoredRoom(roomStore);
  restoredRoom.hasRoom = true;
  restoredRoom.roomContact = roomMesh.roomContact;
  restoredRoom.loadPositionSettings();
  assert(restoredRoom._position.config.enabled);
  assert(restoredRoom._position.config.destinationType == POSITION_DEST_ROOM);
  restoredRoom._prefs.default_scope_key[0] = 11;
  restoredRoom.send_scope.key[0] = 88;
  restoredRoom.requestButtonPosition(); restoredRoom.loopButtonPosition();
  assert(restoredRoom.sent == 1 && restoredRoom.lastRoomKey == 123);
  assert(restoredRoom.lastText == "position 52.123456, 13.123456");
  assert(restoredRoom.lastScope[0] == 44);
  assert(restoredRoom.send_scope.key[0] == 88 && !restoredRoom.send_unscoped);
  now += 10000;
  strcpy(restoredRoom.roomContact.name, "Renamed bot room");
  restoredRoom.requestButtonPosition(); restoredRoom.loopButtonPosition();
  assert(restoredRoom.sent == 2);  // a room rename does not change its identity
  now += 10000;
  restoredRoom.roomContact.id.pub_key[31]++;
  restoredRoom.requestButtonPosition();
  assert(!restoredRoom._position.pending && strcmp(restoredRoom._position.status, "room_missing") == 0);
  assert(!restoredRoom.setPositionSetting("position_button", "1"));
  restoredRoom.roomContact.id.pub_key[31]--;
  assert(restoredRoom.setPositionSetting("position_button", "0"));
  assert(!restoredRoom._position.config.enabled);
  restoredRoom.requestButtonPosition(); assert(!restoredRoom._position.pending);
  assert(restoredRoom.setPositionSetting("position_button", "1"));
  restoredRoom.sendOK = false;
  restoredRoom.requestButtonPosition(); restoredRoom.loopButtonPosition();
  assert(restoredRoom.sent == 2 && strcmp(restoredRoom._position.status, "send_failed") == 0);
  restoredRoom.sendOK = true;

  // A PB02 channel selection remains enabled after a PB03 firmware upgrade.
  DataStore legacyV2Store;
  MyMesh legacyV2Config(legacyV2Store);
  legacyV2Config.channels[1] = scoped.channels[1];
  assert(legacyV2Config.handlePositionButtonCommand(1, on, strlen(on)) == 1);
  auto& v2 = *legacyV2Store.fs.files["/position_button"];
  v2.erase(v2.begin() + 85);  // remove PB03 destinationType before the digest
  memcpy(v2.data(), "PB02", 4);
  uint8_t oldHash[32];
  mesh::Utils::sha256(oldHash, sizeof(oldHash), v2.data(), 85);
  memcpy(v2.data() + 85, oldHash, 32);
  MyMesh legacyV2(legacyV2Store);
  legacyV2.loadPositionSettings();
  assert(legacyV2._position.config.enabled && legacyV2._position.config.destinationType == POSITION_DEST_CHANNEL);
  assert(strcmp(legacyV2._position.config.name, "Family") == 0);

  // PB01 records lack a scope and must not silently enable unscoped sending.
  auto& oldRecord = *scopedStore.fs.files["/position_button"];
  oldRecord.resize(69 + 32); oldRecord[3] = '1';
  MyMesh legacy(scopedStore); legacy.loadPositionSettings();
  assert(!legacy._position.config.enabled);
  assert(strcmp(legacy._position.status, "configure_required") == 0);

}
