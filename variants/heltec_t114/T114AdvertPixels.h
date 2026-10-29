#pragma once
#include <Arduino.h>

// Two GRB SK6812 LEDs at 800 kHz. PWM/EasyDMA owns the waveform;
// no delay, interrupt masking, allocation or polling loop is used.
class T114AdvertPixels {
  NRF_PWM_Type* _pwm = nullptr;
  uint16_t _wave[NEOPIXEL_NUM * 24 + 80]; // 100 us reset-low tail
  bool _active = false;
  bool _clear = true;
  uint32_t _started = 0;

  bool startFrame(bool fresh, bool off) {
    if (_pwm) return false;
    NRF_PWM_Type* devices[] = {NRF_PWM0, NRF_PWM1, NRF_PWM2, NRF_PWM3};
    for (auto* p : devices) {
      if (p->ENABLE == 0 && p->PSEL.OUT[0] == 0xffffffff &&
          p->PSEL.OUT[1] == 0xffffffff && p->PSEL.OUT[2] == 0xffffffff &&
          p->PSEL.OUT[3] == 0xffffffff) { _pwm = p; break; }
    }
    if (!_pwm) return false; // never steal another user's PWM peripheral
    unsigned n = 0;
    for (unsigned led = 0; led < NEOPIXEL_NUM; ++led) {
      uint8_t grb[] = {0, uint8_t(off || fresh ? 0 : 32), uint8_t(off ? 0 : 32)};
      for (uint8_t c : grb)
        for (uint8_t mask = 128; mask; mask >>= 1)
          _wave[n++] = 0x8000 | ((c & mask) ? 10 : 5);
    }
    while (n < sizeof(_wave) / sizeof(_wave[0])) _wave[n++] = 0x8000;
    _pwm->MODE = 0;
    _pwm->PRESCALER = 0; // 16 MHz
    _pwm->COUNTERTOP = 20;
    _pwm->LOOP = 0;
    _pwm->DECODER = PWM_DECODER_LOAD_Common << PWM_DECODER_LOAD_Pos;
    _pwm->SEQ[0].PTR = (uint32_t)(uintptr_t)_wave;
    _pwm->SEQ[0].CNT = sizeof(_wave) / sizeof(_wave[0]);
    _pwm->SEQ[0].REFRESH = 0;
    _pwm->SEQ[0].ENDDELAY = 0;
    _pwm->INTENCLR = 0xffffffff;
    _pwm->SHORTS = PWM_SHORTS_SEQEND0_STOP_Msk;
    _pwm->PSEL.OUT[0] = g_ADigitalPinMap[PIN_NEOPIXEL];
    _pwm->EVENTS_STOPPED = 0;
    _pwm->EVENTS_SEQEND[0] = 0;
    _pwm->ENABLE = 1;
    _pwm->TASKS_SEQSTART[0] = 1;
    return true;
  }

public:
  void begin() {
    pinMode(PIN_NEOPIXEL, OUTPUT);
    digitalWrite(PIN_NEOPIXEL, LOW);
    loop(); // clear any colors left by a previous firmware
  }
  void flash(bool fresh) {
    if (startFrame(fresh, false)) {
      _started = millis();
      _active = true;
      _clear = false;
    }
  }
  void loop() {
    if (_pwm && _pwm->EVENTS_STOPPED) {
      _pwm->ENABLE = 0;
      _pwm->SHORTS = 0;
      _pwm->PSEL.OUT[0] = 0xffffffff;
      _pwm = nullptr;
    }
    if (_active && uint32_t(millis() - _started) >= 300) {
      _active = false;
      _clear = true;
    }
    if (_clear && startFrame(false, true)) _clear = false;
  }
};
