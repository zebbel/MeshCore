#pragma once
#include <stdint.h>

// Approximate single-cell Li-ion open-circuit curve. Load, temperature and
// USB/solar charging bias voltage: this is not measured state of charge.
class BatteryEstimate {
  uint16_t _raw[3] = {};
  uint8_t _count = 0;
  uint32_t _last = 0;
  int32_t _filtered = 0;
  int _percent = -1;
  bool _sampled = false;
public:
  bool due(uint32_t now) const { return !_sampled || uint32_t(now - _last) >= 10000; }
  int percent() const { return _percent; }
  static int estimate(uint16_t mv) {
    const uint16_t volts[] = {3300, 3500, 3600, 3700, 3750, 3800, 3850, 3900, 4000, 4100, 4200};
    if (mv <= volts[0]) return 0;
    for (int i=1; i<11; ++i)
      if (mv < volts[i]) return (i-1)*10 + (mv-volts[i-1])*10/(volts[i]-volts[i-1]);
    return 100;
  }
  void sample(uint32_t now, uint16_t mv) {
    if (!due(now)) return;
    if (_sampled && uint32_t(now - _last) > 30000) { _count=0; _filtered=0; _percent=-1; }
    _last=now; _sampled=true;
    if (mv < 2500 || mv > 4500) { _count=0; _filtered=0; _percent=-1; return; }
    _raw[_count++]=mv;
    if (_count < 3) return;
    _count=0;
    uint16_t a=_raw[0], b=_raw[1], c=_raw[2];
    if (a>b) { uint16_t t=a;a=b;b=t; }
    if (b>c) { uint16_t t=b;b=c;c=t; }
    if (a>b) { uint16_t t=a;a=b;b=t; }
    // Median rejects isolated spikes; fixed-point EMA smooths 30-second updates.
    _filtered = _filtered ? (_filtered*3 + int32_t(b)*16)/4 : int32_t(b)*16;
    int value=estimate(_filtered/16);
    // Two percentage point deadband prevents boundary chatter.
    if (_percent < 0 || value >= _percent+2 || value <= _percent-2 || value==0 || value==100)
      _percent=value;
  }
};
