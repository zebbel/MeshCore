#include "examples/companion_radio/LocalHelp.h"
#include <assert.h>
#include <string.h>

int main() {
  assert(LocalHelp::isRequest("/?", 2));
  assert(LocalHelp::isRequest(" \t/? \r\n", 7));
  assert(!LocalHelp::isRequest("/??", 3));
  assert(!LocalHelp::isRequest("/? something", 12));
  assert(!LocalHelp::isRequest("/position-button on", 19));
  const char bounded[] = {'/', '?', '!', 'x'};
  assert(LocalHelp::isRequest(bounded, 2));
  assert(!LocalHelp::isRequest(bounded, 3));
  assert(!LocalHelp::isRequest(nullptr, 0));

  const uint32_t ts = 0x12345678;
  uint8_t frame[256] = {};
  uint8_t key[32] = {};
  for (int i = 0; i < 32; ++i) key[i] = i;
  for (int v3 = 0; v3 <= 1; ++v3) {
    size_t n = LocalHelp::buildFrame(frame, sizeof(frame), v3 ? 3 : 2, 5, nullptr, ts);
    assert(n > 10);
    assert(frame[0] == (v3 ? 17 : 8));
    const size_t base = v3 ? 4 : 1;
    if (v3) assert(frame[1] == 0 && frame[2] == 0 && frame[3] == 0);
    assert(frame[base] == 5 && frame[base + 1] == 0xFF && frame[base + 2] == 0);
    assert(frame[base + 3] == 0x78 && frame[base + 6] == 0x12);
    assert(memcmp(frame + base + 7, LocalHelp::kHelpText, strlen(LocalHelp::kHelpText)) == 0);
    assert(n == base + 7 + strlen(LocalHelp::kHelpText));

    n = LocalHelp::buildFrame(frame, sizeof(frame), v3 ? 3 : 2, 5, key, ts);
    assert(n > 15);
    assert(frame[0] == (v3 ? 16 : 7));
    assert(memcmp(frame + base, key, 6) == 0);
    assert(frame[base + 6] == 0xFF && frame[base + 7] == 0);
    assert(frame[base + 8] == 0x78 && frame[base + 11] == 0x12);
    assert(memcmp(frame + base + 12, LocalHelp::kHelpText, strlen(LocalHelp::kHelpText)) == 0);
    assert(n == base + 12 + strlen(LocalHelp::kHelpText));
  }
  // Short buffers must remain untouched.
  memset(frame, 0xAA, sizeof(frame));
  assert(LocalHelp::buildFrame(frame, 8, 3, 2, nullptr, ts) == 0);
  assert(frame[0] == 0xAA);
  assert(LocalHelp::buildFrame(nullptr, 256, 3, 2, nullptr, ts) == 0);
}
