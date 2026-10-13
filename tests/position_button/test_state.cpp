#include "examples/companion_radio/PositionButton.h"
#include <assert.h>
#include <limits.h>

int main() {
  char text[64];
  assert(PositionButtonState::format(text, sizeof(text), 52123456, 13123456));
  assert(strcmp(text, "position 52.123456, 13.123456") == 0);
  assert(PositionButtonState::format(text, sizeof(text), -1, -180000000));
  assert(strcmp(text, "position -0.000001, -180.000000") == 0);
  assert(PositionButtonState::format(text, sizeof(text), 0, 0));
  assert(strcmp(text, "position 0.000000, 0.000000") == 0);
  assert(!PositionButtonState::format(text, sizeof(text), INT32_MIN, 0));
  assert(!PositionButtonState::format(text, sizeof(text), 0, 180000001));
  assert(!PositionButtonState::format(text, 10, 0, 0));
  assert(PositionButtonState::validName("Family"));
  assert(!PositionButtonState::validName(""));
  assert(!PositionButtonState::validName("Family,Public"));
  assert(!PositionButtonState::validName("Family:1"));
  assert(!PositionButtonState::validName("12345678901234567890123456789012"));
  assert(PositionButtonState::formatStored(text, sizeof(text), 52.123456, 13.123456));
  assert(strcmp(text, "position 52.123456, 13.123456") == 0);
  assert(PositionButtonState::formatStored(text, sizeof(text), -0.000001, -180.0));
  assert(strcmp(text, "position -0.000001, -180.000000") == 0);
  assert(PositionButtonState::formatStored(text, sizeof(text), 0.0, 13.0));
  assert(PositionButtonState::formatStored(text, sizeof(text), 52.0, 0.0));
  assert(!PositionButtonState::formatStored(text, sizeof(text), 0.0, 0.0));
  assert(!PositionButtonState::formatStored(text, sizeof(text), NAN, 13.0));
  assert(!PositionButtonState::formatStored(text, sizeof(text), 52.0, INFINITY));
  assert(!PositionButtonState::formatStored(text, sizeof(text), 91.0, 13.0));
  PositionButtonState state;
  assert(!state.config.enabled && !state.pending);
  state.hasSent = true;
  state.lastSent = UINT32_MAX - 5000;
  assert(state.coolingDown(4998));
  assert(!state.coolingDown(4999));
}
