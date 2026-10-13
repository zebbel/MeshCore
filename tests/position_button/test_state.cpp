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
  PositionButtonState state;
  assert(!state.config.enabled && !state.pending);
  assert(!state.freshFix(false, 1800000000));
  assert(!state.freshFix(true, 0));
  assert(!state.freshFix(true, 1800000000));
  assert(!state.freshFix(true, 1800000000)); // cached fix
  assert(state.freshFix(true, 1800000001));
  assert(!state.freshFix(true, 1800086400)); // slept overnight: rebaseline
  assert(state.freshFix(true, 1800086401));
  state.pending = true;
  state.started = UINT32_MAX - 60000;
  assert(!state.timedOut(59998));
  assert(state.timedOut(59999)); // wrap-safe 120 second timeout
  state.hasSent = true;
  state.lastSent = UINT32_MAX - 5000;
  assert(state.coolingDown(4998));
  assert(!state.coolingDown(4999));
}
