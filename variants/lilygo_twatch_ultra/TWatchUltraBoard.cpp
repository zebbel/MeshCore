#include "TWatchUltraBoard.h"

namespace {
constexpr uint8_t PMU_ADDRESS = 0x34;
constexpr uint8_t EXPANDER_ADDRESS = 0x20;
// XL9555 port 1, bit 3 (logical pin 11): HIGH selects the internal antenna.
constexpr uint8_t ANTENNA_BIT = 1 << 3;
constexpr uint8_t PORT1_OUTPUT = 0x03;
constexpr uint8_t PORT1_DIRECTION = 0x07;

bool readRegister(uint8_t address, uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(address, uint8_t(1)) != 1) return false;
  const int result = Wire.read();
  if (result < 0) return false;
  value = uint8_t(result);
  return true;
}

bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}
}

bool TWatchUltraBoard::selectInternalAntenna() {
  uint8_t output, direction;
  if (!readRegister(EXPANDER_ADDRESS, PORT1_OUTPUT, output) ||
      !readRegister(EXPANDER_ADDRESS, PORT1_DIRECTION, direction)) return false;

  // Set the latch before making the pin an output. Preserve all other pins.
  if (!writeRegister(EXPANDER_ADDRESS, PORT1_OUTPUT, output | ANTENNA_BIT) ||
      !writeRegister(EXPANDER_ADDRESS, PORT1_DIRECTION, direction & ~ANTENNA_BIT)) return false;
  if (!readRegister(EXPANDER_ADDRESS, PORT1_OUTPUT, output) ||
      !readRegister(EXPANDER_ADDRESS, PORT1_DIRECTION, direction)) return false;
  return (output & ANTENNA_BIT) && !(direction & ANTENNA_BIT);
}

void TWatchUltraBoard::begin() {
  ESP32Board::begin();
  radioReady = pmuReady = false;

  // SD and NFC share the radio SPI bus. Leave both deselected.
  const uint8_t spiSelects[] = {21, 4, P_LORA_NSS};
  for (uint8_t pin : spiSelects) {
    digitalWrite(pin, HIGH);
    pinMode(pin, OUTPUT);
  }

  // XPowersLib 0.2.7 clears the TS ADC bit during init. Preserve its
  // original state so temperature sensing retains the board's policy.
  constexpr uint8_t adcControl = XPOWERS_AXP2101_ADC_CHANNEL_CTRL;
  uint8_t adcBefore, adcAfter;
  if (!readRegister(PMU_ADDRESS, adcControl, adcBefore)) {
    Serial.println("ERROR: T-Watch Ultra AXP2101 ADC configuration unreadable");
    return;
  }
  const bool identified = pmu.init();
  if (!identified) {
    Serial.println("ERROR: T-Watch Ultra AXP2101 identification failed at I2C 0x34");
    return;
  }
  if (!writeRegister(PMU_ADDRESS, adcControl, adcBefore) ||
      !readRegister(PMU_ADDRESS, adcControl, adcAfter) || adcAfter != adcBefore) {
    Serial.println("ERROR: T-Watch Ultra AXP2101 temperature configuration restore failed");
    return;
  }
  pmuReady = true;

  if (!pmu.setALDO3Voltage(3300) || !pmu.enableALDO3() ||
      !pmu.isEnableALDO3() || pmu.getALDO3Voltage() != 3300) {
    Serial.println("ERROR: T-Watch Ultra radio ALDO3 power initialization failed");
    return;
  }
  pmu.enableBattVoltageMeasure();
  pmu.enableVbusVoltageMeasure();
  if (!selectInternalAntenna()) {
    Serial.println("ERROR: T-Watch Ultra XL9555 internal antenna selection failed");
    return;
  }

  delay(10); // Let the radio rail settle before initialization.
  radioReady = true;
}
