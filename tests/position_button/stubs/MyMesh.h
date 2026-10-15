#pragma once
#include "PositionButton.h"
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#define MAX_GROUP_CHANNELS 40
#define FILE_O_WRITE 1
#define PUB_KEY_SIZE 32
#define ADV_TYPE_ROOM 3
#define MSG_SEND_FAILED 0
struct ContactInfo { struct { uint8_t pub_key[32] = {}; } id; char name[32] = {}; uint8_t type = 0; };
extern uint32_t now;
inline uint32_t millis() { return now; }
struct ChannelDetails { char name[32] = {}; struct { uint8_t secret[32] = {}; } channel; };
namespace mesh { struct Utils {
  // A deterministic test checksum; production uses MeshCore SHA-256.
  static void sha256(uint8_t* dest, size_t n, const uint8_t* data, int size) {
    memset(dest, 0, n);
    for (int i = 0; i < size; ++i) dest[i % n] = uint8_t(dest[i % n] * 33 + data[i]);
  }
}; }
struct FakeFile {
  std::shared_ptr<std::vector<uint8_t>> bytes;
  size_t pos = 0;
  explicit operator bool() const { return bool(bytes); }
  size_t size() const { return bytes ? bytes->size() : 0; }
  size_t read(uint8_t* data, size_t n) {
    n = std::min(n, size() - pos); memcpy(data, bytes->data() + pos, n); pos += n; return n;
  }
  size_t write(const uint8_t* data, size_t n) { bytes->insert(bytes->end(), data, data+n); return n; }
  void close() {}
};
struct FakeFS {
  bool fail = false;
  std::map<std::string, std::shared_ptr<std::vector<uint8_t>>> files;
  void remove(const char* name) { files.erase(name); }
  FakeFile open(const char* name, int mode = 0) {
    FakeFile f;
    if (mode) { if (fail) return f; files[name] = std::make_shared<std::vector<uint8_t>>(); }
    if (files.count(name)) f.bytes = files[name];
    return f;
  }
  bool rename(const char* a, const char* b) { if (fail) return false; files[b] = files.at(a); files.erase(a); return true; }
};
struct DataStore {
  FakeFS fs;
  FakeFS* getPrimaryFS() { return &fs; }
  FakeFile openRead(const char* name) { return fs.open(name); }
};
struct FakeGPS {
  bool valid = false; uint32_t timestamp = 1800000000; int32_t lat = 52123456, lon = 13123456;
  bool isValid() { return valid; }
  long getTimestamp() { return timestamp; }
  long getLatitude() { return lat; }
  long getLongitude() { return lon; }
};
struct FakeSensors {
  double node_lat = 0, node_lon = 0;
  bool enabled = false; FakeGPS gps;
  FakeGPS* getLocationProvider() { return &gps; }
  const char* getSettingValue(int) { return enabled ? "1" : "0"; }
  bool setSettingValue(const char*, const char* value) { enabled = value[0] == '1'; return true; }
};
extern FakeSensors sensors;
enum class UIEventType { ack };
struct FakeUI { int acks = 0; void notify(UIEventType) { ++acks; } };
struct TransportKey {
  uint8_t key[16] = {};
  bool isNull() const { uint8_t zero[16] = {}; return memcmp(key, zero, 16) == 0; }
};
class MyMesh {
public:
  bool _statusLedEnabled = true;
  PositionButtonState _position;
  DataStore* _store;
  FakeUI* _ui = nullptr;
  bool _cli_rescue = false, sendOK = true;
  struct { char node_name[32] = "test"; uint8_t default_scope_key[16] = {}; } _prefs;
  TransportKey send_scope;
  bool send_unscoped = false;
  uint8_t lastScope[16] = {};
  ChannelDetails channels[40];
  ContactInfo roomContact;
  bool hasRoom = false;
  uint8_t lastRoomKey = 0;
  struct FakeClock { uint32_t getCurrentTimeUnique() { return 1800000000 + now; } } clock;
  FakeClock* getRTCClock() { return &clock; }
  int sent = 0; std::string lastText; uint8_t lastKey = 0;
  explicit MyMesh(DataStore& store): _store(&store) {}
  bool getChannel(int i, ChannelDetails& ch) { if (i < 0 || i >= 40) return false; ch = channels[i]; return true; }
  ContactInfo* lookupContactByPubKey(const uint8_t* pub_key, int len) {
    return hasRoom && memcmp(roomContact.id.pub_key, pub_key, len) == 0 ? &roomContact : nullptr;
  }
  int sendMessage(const ContactInfo& room, uint32_t, uint8_t, const char* text, uint32_t& ack, uint32_t& timeout) {
    if (!sendOK) return MSG_SEND_FAILED;
    ack = 0;
    timeout = 0;
    memset(lastScope, 0, sizeof(lastScope));
    if (!send_unscoped) memcpy(lastScope, send_scope.isNull() ? _prefs.default_scope_key : send_scope.key, 16);
    ++sent; lastText = text; lastRoomKey = room.id.pub_key[0]; return 1;
  }
  template<class C> bool sendGroupMessage(uint32_t, const C& ch, const char*, const char* text, size_t) {
    if (!sendOK) return false;
    memset(lastScope, 0, sizeof(lastScope));
    if (!send_unscoped) memcpy(lastScope, send_scope.isNull() ? _prefs.default_scope_key : send_scope.key, 16);
    ++sent; lastText = text; lastKey = ch.secret[0]; return true;
  }
  bool privatePositionChannel(const ChannelDetails&);
  bool findPositionChannel(ChannelDetails&);
  bool findPositionRoom(ContactInfo&);
  void loadPositionSettings();
  void capturePositionScope(PositionButtonConfig&);
  bool savePositionSettings(const PositionButtonConfig&);
  bool setPositionSetting(const char*, const char*);
  void finishButtonPosition(const char*);
  bool isStatusLedEnabled() const { return _statusLedEnabled; }
  void requestButtonPosition();
  int handlePositionButtonCommand(uint8_t, const char*, size_t, const ContactInfo* room = nullptr);
  void loopButtonPosition();
};
