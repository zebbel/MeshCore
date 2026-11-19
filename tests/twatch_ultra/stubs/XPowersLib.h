#pragma once

#include "helpers/ESP32Board.h"

constexpr uint8_t XPOWERS_AXP2101_ADC_CHANNEL_CTRL = 0x30;
extern bool detected, railEnabled, railCanEnable, vbus, battery;
extern uint16_t railVoltage, batteryVoltage;

class XPowersAXP2101 {
public:
  XPowersAXP2101(WireMock&, int, int) {}
  bool init() {
    if (!detected) return false;
    Wire.registers[0x3430] &= ~2;
    return true;
  }
  bool setALDO3Voltage(uint16_t voltage) { railVoltage = voltage; return true; }
  bool enableALDO3() { railEnabled = railCanEnable; return railEnabled; }
  bool isEnableALDO3() { return railEnabled; }
  uint16_t getALDO3Voltage() { return railVoltage; }
  void enableBattVoltageMeasure() {}
  void enableVbusVoltageMeasure() {}
  bool isBatteryConnect() { return battery; }
  uint16_t getBattVoltage() { return batteryVoltage; }
  bool isVbusIn() { return vbus; }
};
