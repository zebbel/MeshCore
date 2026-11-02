#include <Arduino.h>
#include <Wire.h>

#include "T1000eBoard.h"

void T1000eBoard::begin() {
  NRF52BoardDCDC::begin();
  btn_prev_state = HIGH;

#ifdef BUTTON_PIN
  pinMode(BATTERY_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
#endif

#if defined(PIN_BOARD_SDA) && defined(PIN_BOARD_SCL)
  Wire.setPins(PIN_BOARD_SDA, PIN_BOARD_SCL);
#endif

  Wire.begin();

  delay(10);   // give sx1262 some time to power up
}
#ifdef T1000_CHARGING_LED
#include <HardwarePWM.h>
void T1000eBoard::stopStatusLedPWM() {
  // Only detach our LED; never stop a PWM peripheral with other active pins.
  digitalWrite(PIN_STATUS_LED, !LED_STATE_ON);
  for (int i=0; i<HWPWM_MODULE_NUM; ++i) {
    if (HwPWMx[i]->removePin(PIN_STATUS_LED) && HwPWMx[i]->usedChannelCount() == 0)
      HwPWMx[i]->stop();
  }
}
#endif
