#pragma once

#include <helpers/ESP32Board.h>
#include <XPowersLib.h>

class TWatchUltraBoard : public ESP32Board {
  XPowersAXP2101 pmu{Wire, PIN_BOARD_SDA, PIN_BOARD_SCL};
  bool pmuReady = false;
  bool radioReady = false;

  bool selectInternalAntenna();

public:
  void begin();
  bool isRadioPowerReady() const { return radioReady; }

  uint16_t getBattMilliVolts() override {
    if (!pmuReady || !pmu.isBatteryConnect()) return 0;
    const uint16_t mv = pmu.getBattVoltage();
    return mv >= 2000 && mv <= 5000 ? mv : 0;
  }
  bool isExternalPowered() override { return pmuReady && pmu.isVbusIn(); }
  const char* getManufacturerName() const override { return "LilyGO T-Watch Ultra SX1262"; }

  // Keep USB, BLE and the radio running during ordinary idle periods.
  void sleep(uint32_t) override { delay(1); }
};
