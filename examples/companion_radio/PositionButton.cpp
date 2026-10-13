#ifdef ENABLE_POSITION_BUTTON
#include "MyMesh.h"

namespace {
const char* const SETTINGS_FILE = "/position_button";
const char* const SETTINGS_TEMP = "/position_button.tmp";
void fingerprint(const ChannelDetails& ch, uint8_t* dest) {
  mesh::Utils::sha256(dest, 32, ch.channel.secret, sizeof(ch.channel.secret));
}
}

bool MyMesh::privatePositionChannel(const ChannelDetails& ch) {
  // Reject empty/deleted channels and the default Public key even if renamed.
  static const uint8_t publicKey[32] = {
    0x8b,0x33,0x87,0xe9,0xc5,0xcd,0xea,0x6a,0xc9,0xe5,0xed,0xba,0xa1,0x15,0xcd,0x72
  };
  static const uint8_t emptyKey[32] = {};
  return ch.name[0] && ch.name[0] != '#'
      && memcmp(ch.channel.secret, publicKey, 32) != 0
      && memcmp(ch.channel.secret, emptyKey, 32) != 0;
}

bool MyMesh::findPositionChannel(ChannelDetails& ch) {
  if (!_position.config.name[0]) return false;
  for (int i = 0; i < MAX_GROUP_CHANNELS; ++i) {
    uint8_t digest[32];
    if (!getChannel(i, ch) || !privatePositionChannel(ch)) continue;
    fingerprint(ch, digest);
    if (memcmp(digest, _position.config.fingerprint, 32) == 0) return true;
  }
  return false;
}

void MyMesh::loadPositionSettings() {
  // Independent record keeps existing position settings compatible.
  auto led = _store->openRead("/status_led");
  uint8_t record[5];
  if (led && led.size() == sizeof(record)
      && led.read(record, sizeof(record)) == sizeof(record)
      && memcmp(record, "LED1", 4) == 0 && record[4] <= 1) {
    _statusLedEnabled = record[4] != 0;
  }
  led.close();
  auto f = _store->openRead(SETTINGS_FILE);
  if (!f) return;
  PositionButtonConfig c;
  uint8_t stored[32], digest[32];
  bool ok = f.size() == sizeof(c) + sizeof(stored)
      && f.read((uint8_t*)&c, sizeof(c)) == sizeof(c)
      && f.read(stored, sizeof(stored)) == sizeof(stored);
  f.close();
  mesh::Utils::sha256(digest, sizeof(digest), (uint8_t*)&c, sizeof(c));
  if (!ok || memcmp(digest, stored, 32) || memcmp(c.magic, "PB01", 4)
      || c.enabled > 1 || c.name[31] != 0
      || (c.name[0] && !PositionButtonState::validName(c.name))) return;
  _position.config = c;
  _position.status = c.enabled ? "idle" : "disabled";
}

bool MyMesh::savePositionSettings(const PositionButtonConfig& c) {
  auto fs = _store->getPrimaryFS();
  fs->remove(SETTINGS_TEMP);
  auto f = fs->open(SETTINGS_TEMP, FILE_O_WRITE);
  if (!f) return false;
  uint8_t digest[32];
  mesh::Utils::sha256(digest, sizeof(digest), (const uint8_t*)&c, sizeof(c));
  bool ok = f.write((const uint8_t*)&c, sizeof(c)) == sizeof(c)
      && f.write(digest, sizeof(digest)) == sizeof(digest);
  f.close();
  // LittleFS rename replaces the old record atomically.
  if (ok) ok = fs->rename(SETTINGS_TEMP, SETTINGS_FILE);
  if (!ok) fs->remove(SETTINGS_TEMP);
  return ok;
}

// 0: ordinary chat, 1: saved locally, -1: local command rejected.
int MyMesh::handlePositionButtonCommand(uint8_t channel_idx, const char* text, size_t len) {
  auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  while (len && space(*text)) { ++text; --len; }
  while (len && space(text[len - 1])) --len;
  // App-to-device commands only: consume even malformed LED commands locally.
  if (len >= 4 && memcmp(text, "/led", 4) == 0) {
    if (len == 4 || !space(text[4])) return -1;
    text += 4; len -= 4;
    while (len && space(*text)) { ++text; --len; }
    bool enabled;
    if (len == 2 && memcmp(text, "on", 2) == 0) enabled = true;
    else if (len == 3 && memcmp(text, "off", 3) == 0) enabled = false;
    else return -1;
    auto fs = _store->getPrimaryFS();
    fs->remove("/status_led.tmp");
    auto f = fs->open("/status_led.tmp", FILE_O_WRITE);
    if (!f) return -1;
    const uint8_t record[] = {'L', 'E', 'D', '1', uint8_t(enabled)};
    bool ok = f.write(record, sizeof(record)) == sizeof(record);
    f.close();
    if (ok) ok = fs->rename("/status_led.tmp", "/status_led");
    if (!ok) { fs->remove("/status_led.tmp"); return -1; }
    _statusLedEnabled = enabled;
    return 1;
  }
  static const char prefix[] = "/position-button";
  const size_t prefix_len = sizeof(prefix) - 1;
  if (len < prefix_len || memcmp(text, prefix, prefix_len) != 0) return 0;
  if (len == prefix_len || !space(text[prefix_len])) return -1;
  text += prefix_len;
  len -= prefix_len;
  while (len && space(*text)) { ++text; --len; }

  PositionButtonConfig next = _position.config;
  if (len == 3 && memcmp(text, "off", 3) == 0) {
    next.enabled = 0;
  } else if (len == 2 && memcmp(text, "on", 2) == 0) {
    ChannelDetails ch;
    if (!getChannel(channel_idx, ch) || !privatePositionChannel(ch)
        || !memchr(ch.name, 0, sizeof(ch.name))
        || !PositionButtonState::validName(ch.name)) return -1;
    // Select by the app's channel slot, so duplicate names are unambiguous.
    // Persist destination and enable together: no partially applied setting.
    memset(next.name, 0, sizeof(next.name));
    strcpy(next.name, ch.name);
    fingerprint(ch, next.fingerprint);
    next.enabled = 1;
  } else {
    return -1;
  }
  if (!savePositionSettings(next)) {
    _position.status = "storage_error";
    return -1;
  }
  finishButtonPosition(next.enabled ? "idle" : "disabled");
  _position.config = next;
  if (_ui) _ui->notify(UIEventType::ack);
  return 1;
}

bool MyMesh::setPositionSetting(const char* name, const char* value) {
  PositionButtonConfig next = _position.config;
  if (strcmp(name, "position_button") == 0) {
    if (strcmp(value, "0") && strcmp(value, "1")) return false;
    ChannelDetails ch;
    if (value[0] == '1' && !findPositionChannel(ch)) return false;
    next.enabled = value[0] == '1';
  } else if (strcmp(name, "position_channel") == 0) {
    if (!value[0]) {
      next = PositionButtonConfig();  // clear destination and disable
    } else {
      if (!PositionButtonState::validName(value)) return false;
      int matches = 0;
      ChannelDetails selected;
      for (int i = 0; i < MAX_GROUP_CHANNELS; ++i) {
        ChannelDetails ch;
        if (getChannel(i, ch) && strncmp(ch.name, value, sizeof(ch.name)) == 0) {
          if (!privatePositionChannel(ch)) return false;
          selected = ch;
          ++matches;
        }
      }
      if (matches != 1) return false; // no match or ambiguous name
      memset(next.name, 0, sizeof(next.name));
      strcpy(next.name, value);
      fingerprint(selected, next.fingerprint);
    }
  } else {
    return false; // position_status is read-only
  }
  if (!savePositionSettings(next)) {
    _position.status = "storage_error";
    return false;
  }
  finishButtonPosition(next.enabled ? "idle" : "disabled");
  _position.config = next;
  return true;
}

void MyMesh::finishButtonPosition(const char* status) {
  _position.pending = false;
  _position.status = status;
}

void MyMesh::requestButtonPosition() {
  if (!_position.config.enabled || _position.pending || _position.coolingDown(millis())) return;
  ChannelDetails ch;
  if (!findPositionChannel(ch)) {
    _position.status = "channel_missing";
    return;
  }
  _position.pending = true;
  _position.status = "waiting";
}

void MyMesh::loopButtonPosition() {
  if (!_position.pending) return;
  if (_cli_rescue) { finishButtonPosition("cancelled"); return; }
  ChannelDetails ch;
  if (!findPositionChannel(ch)) { finishButtonPosition("channel_missing"); return; }
  char text[64];
  if (!PositionButtonState::formatStored(text, sizeof(text), sensors.node_lat, sensors.node_lon)) {
    finishButtonPosition("no_fix");
    return;
  }
  // Timestamp the message now, even when the position has not changed.
  bool sent = sendGroupMessage(getRTCClock()->getCurrentTimeUnique(), ch.channel,
                               _prefs.node_name, text, strlen(text));
  if (sent) {
    _position.hasSent = true;
    _position.lastSent = millis();
    if (_ui) _ui->notify(UIEventType::ack); // queued locally, not a delivery receipt
  }
  finishButtonPosition(sent ? "queued" : "send_failed");
}
#endif
