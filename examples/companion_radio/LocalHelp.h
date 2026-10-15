#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Local-only help frames use standard companion message receive packets so
// existing clients can show them in the chat without changes or LoRa traffic.
namespace LocalHelp {
static const char kHelpText[] =
    "meshcoreStation: Available commands\n"
    "/? - Show this help\n"
    "/position-button on - Send GPS to this chat\n"
    "/position-button off - Disable GPS button\n"
    "/led on - Enable LED blink\n"
    "/led off - Disable LED blink";

inline bool isRequest(const char* text, size_t len) {
  if (!text) return false;
  auto whitespace = [](char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  };
  while (len && whitespace(*text)) { ++text; --len; }
  while (len && whitespace(text[len - 1])) --len;
  return len == 2 && text[0] == '/' && text[1] == '?';
}

// room_key is null for channels, otherwise it identifies a room-server contact.
// Returns zero rather than writing a truncated packet. No radio packet is made.
inline size_t buildFrame(uint8_t* frame, size_t capacity, uint8_t app_version,
                         uint8_t channel_idx, const uint8_t* room_key,
                         uint32_t timestamp) {
  if (!frame) return 0;
  const bool room = room_key != nullptr;
  const bool v3 = app_version >= 3;
  const char* body = kHelpText;
  const size_t text_size = strlen(body);
  const size_t header_size = (v3 ? 4 : 1) + (room ? 6 : 1) + 1 + 1 + 4;
  if (header_size + text_size > capacity) return 0;

  size_t i = 0;
  frame[i++] = room ? (v3 ? 16 : 7) : (v3 ? 17 : 8);
  if (v3) {
    frame[i++] = 0; // SNR not available: this was generated locally
    frame[i++] = 0; // reserved
    frame[i++] = 0; // reserved
  }
  if (room) {
    memcpy(&frame[i], room_key, 6);
    i += 6;
  } else {
    frame[i++] = channel_idx;
  }
  frame[i++] = 0xFF; // no over-the-air flood route
  frame[i++] = 0;    // TXT_TYPE_PLAIN
  for (int k = 0; k < 4; ++k) {
    frame[i++] = static_cast<uint8_t>(timestamp >> (8 * k));
  }
  memcpy(&frame[i], body, text_size);
  i += text_size;
  return i;
}
} // namespace LocalHelp
