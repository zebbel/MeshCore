#pragma once
#include <stdint.h>

// Observation only: voltage direction is not a charging-status measurement.
class BatteryTrend {
  uint16_t _raw[3] = {}, _history[11] = {};
  uint8_t _rawCount = 0, _count = 0, _next = 0, _confirm = 0;
  int8_t _direction = 0, _candidate = 0;
  bool _sampled = false, _ready = false;
  uint32_t _last = 0;
  uint16_t _voltage = 0;

  void clearHistory() {
    _rawCount = _count = _next = _confirm = 0;
    _direction = _candidate = 0;
    _ready = false;
  }
public:
  bool due(uint32_t now) const { return !_sampled || uint32_t(now - _last) >= 10000; }
  uint16_t milliVolts() const { return _voltage; }
  const char* trend() const {
    if (!_ready) return "Collecting";
    return _direction > 0 ? "Rising" : _direction < 0 ? "Falling" : "Stable";
  }
  void sample(uint32_t now, uint16_t mv) {
    if (!due(now)) return;
    if (_sampled && uint32_t(now - _last) > 20000) clearHistory();
    _sampled = true;
    _last = now;
    if (mv < 2000 || mv > 5000) {
      _voltage = 0; clearHistory(); return;
    }
    _voltage = mv;
    _raw[_rawCount++] = mv;
    if (_rawCount < 3) return;
    _rawCount = 0;
    uint16_t a = _raw[0], b = _raw[1], c = _raw[2];
    if (a > b) { uint16_t t=a; a=b; b=t; }
    if (b > c) { uint16_t t=b; b=c; c=t; }
    if (a > b) { uint16_t t=a; a=b; b=t; }
    _history[_next] = b; // median of three ten-second samples
    _next = (_next + 1) % 11;
    if (_count < 11) ++_count;
    if (_count < 11) return;
    int delta = int(b) - int(_history[_next]); // filtered endpoints 5 minutes apart
    int8_t desired = _direction;
    if (delta >= 30) desired = 1;
    else if (delta <= -30) desired = -1;
    else if ((_direction > 0 && delta <= 10) ||
             (_direction < 0 && delta >= -10)) desired = 0;
    if (desired == _direction) { _ready = true; _confirm = 0; return; }
    if (desired != _candidate) { _candidate = desired; _confirm = 1; return; }
    if (++_confirm >= 2) { _direction = desired; _ready = true; _confirm = 0; }
  }
};
