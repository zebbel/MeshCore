#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <XiaoNrf52Board.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/sensors/EnvironmentSensorManager.h>

#ifdef DISPLAY_CLASS
  #include <helpers/ui/NullDisplayDriver.h>
  #include <helpers/ui/MomentaryButton.h>
  extern DISPLAY_CLASS display;
  extern MomentaryButton user_btn;
#endif

extern XiaoNrf52Board board;
extern WRAPPER_CLASS radio_driver;
#ifdef XIAO_NEIGHBOUR_SNR
// The fallback starts at a fixed date. Do not label that date as known UTC.
class XiaoRepeaterClock : public AutoDiscoverRTCClock {
  bool confirmed = false;
public:
  XiaoRepeaterClock(mesh::RTCClock& fallback) : AutoDiscoverRTCClock(fallback) {}
  void setCurrentTime(uint32_t time) override {
    AutoDiscoverRTCClock::setCurrentTime(time);
    confirmed = time >= 1577836800UL && time < 4102444800UL;
  }
  bool hasConfirmedTime() const { return confirmed; }
};
extern XiaoRepeaterClock rtc_clock;
#else
extern AutoDiscoverRTCClock rtc_clock;
#endif
extern EnvironmentSensorManager sensors;

bool radio_init();
mesh::LocalIdentity radio_new_identity();

