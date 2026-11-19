#include "target.h"

TWatchUltraBoard board;
static SPIClass radio_spi(FSPI);
RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, radio_spi);
WRAPPER_CLASS radio_driver(radio, board);
// The PCF85063A at 0x51 would be misidentified as a PCF8563 by RTC auto-discovery.
ESP32RTCClock rtc_clock;
EnvironmentSensorManager sensors;

bool radio_init() {
  rtc_clock.begin();
  if (!board.isRadioPowerReady()) {
    Serial.println("ERROR: T-Watch Ultra SX1262 initialization skipped: power/antenna unavailable");
    return false;
  }
  return radio.std_init(&radio_spi);
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);
}
