#ifdef ENABLE_POSITION_BUTTON
#include "MyMesh.h"

bool MyMesh::handleCustomCLI(const char* command, char* reply) {
  // Fits the CLI's 160-byte response payload, including an optional prefix.
  if (strcmp(command, "?") == 0) {
    strcpy(reply, "set/get led: blink; set/get position-button: enable; get position-status: result; get position-target: destination; ? position: configure");
    return true;
  }
  if (strcmp(command, "? position") == 0) {
    strcpy(reply, "set position-target channel|room none|<32hex-scope> <name>: save destination + scope and enable; unique existing private channel/room required");
    return true;
  }
  if (strcmp(command, "get led") == 0) {
    strcpy(reply, _statusLedEnabled ? "on" : "off"); return true;
  }
  if (strncmp(command, "set led ", 8) == 0) {
    char text[32];
    snprintf(text, sizeof(text), "/led %s", command + 8);
    strcpy(reply, handlePositionButtonCommand(0, text, strlen(text)) == 1 ? "OK" : "Error: use on/off");
    return true;
  }
  if (strcmp(command, "get position-button") == 0) {
    strcpy(reply, _position.config.enabled ? "on" : "off"); return true;
  }
  if (strcmp(command, "get position-status") == 0) {
    snprintf(reply, 160, "%s", _position.status); return true;
  }
  if (strcmp(command, "get position-target") == 0) {
    char scope[33];
    for (int n = 0; n < 16; ++n) snprintf(scope + n * 2, 3, "%02x", _position.config.scopeKey[n]);
    snprintf(reply, 160, "%s %s %s", _position.config.destinationType == POSITION_DEST_ROOM ? "room" : "channel",
             scope, _position.config.name);
    return true;
  }
  if (strncmp(command, "set position-button ", 20) == 0) {
    const char* value = command + 20;
    bool ok = (!strcmp(value, "on") || !strcmp(value, "off"))
        && setPositionSetting("position_button", !strcmp(value, "on") ? "1" : "0");
    strcpy(reply, ok ? "OK" : "Error: invalid destination or on/off"); return true;
  }
  // One atomic selection supplies destination AND scope, with no implicit fallback.
  if (strncmp(command, "set position-target ", 20) == 0) {
    char type[8] = {}, scope[33] = {}; int used = 0;
    const char* args = command + 20;
    bool ok = sscanf(args, "%7s %32s %n", type, scope, &used) == 2 && used > 0;
    const char* name = args + used;
    uint8_t key[16] = {};
    if (ok && strcmp(scope, "none")) {
      ok = strlen(scope) == 32;
      for (int n = 0; ok && n < 32; ++n) {
        char c = scope[n];
        int digit = c >= '0' && c <= '9' ? c - '0' :
                    c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                    c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (digit < 0) ok = false;
        else key[n / 2] |= digit << (n % 2 ? 0 : 4);
      }
    }
    int matches = 0, selected = -1;
    ContactInfo room, candidate;
    if (ok && PositionButtonState::validName(name)) {
      if (!strcmp(type, "channel")) {
        for (int n = 0; n < MAX_GROUP_CHANNELS; ++n) {
          ChannelDetails ch;
          if (getChannel(n, ch) && !strncmp(ch.name, name, sizeof(ch.name))) {
            ++matches; if (privatePositionChannel(ch)) selected = n;
          }
        }
      } else if (!strcmp(type, "room")) {
        for (int n = 0; getContactByIdx(n, candidate); ++n) {
          if (candidate.type == ADV_TYPE_ROOM && !strncmp(candidate.name, name, sizeof(candidate.name))) {
            ++matches; room = candidate;
          }
        }
      }
    }
    if (matches != 1 || (!strcmp(type, "channel") && selected < 0)) ok = false;
    if (ok) {
      TransportKey previous = send_scope; bool unscoped = send_unscoped;
      memcpy(send_scope.key, key, 16); send_unscoped = !strcmp(scope, "none");
      // A zero key is explicitly unscoped, never an implicit default scope.
      if (send_scope.isNull()) send_unscoped = true;
      ok = handlePositionButtonCommand(selected < 0 ? 0 : selected, "/position-button on", 19,
                                      !strcmp(type, "room") ? &room : nullptr) == 1;
      send_scope = previous; send_unscoped = unscoped;
    }
    strcpy(reply, ok ? "OK: destination and scope saved; enabled" : "Error: invalid/ambiguous target or scope");
    return true;
  }
  return false;
}
#endif
