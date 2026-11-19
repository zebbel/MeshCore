#pragma once

#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/sensors/EnvironmentSensorManager.h>
#include "TWatchUltraBoard.h"

extern TWatchUltraBoard board;
extern WRAPPER_CLASS radio_driver;
extern ESP32RTCClock rtc_clock;
extern EnvironmentSensorManager sensors;

bool radio_init();
mesh::LocalIdentity radio_new_identity();
