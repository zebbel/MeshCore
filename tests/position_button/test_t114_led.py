"""Compile the real T114 board header with GPIO stand-ins."""
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    (root / 'helpers').mkdir()
    (root / 'MeshCore.h').write_text('#pragma once\n#include <stdint.h>\n')
    (root / 'Arduino.h').write_text("""
#pragma once
#define LOW 0
#define HIGH 1
#define OUTPUT 1
#define AR_INTERNAL_3_0 0
extern int led;
inline void digitalWrite(int pin, int value) { if (pin == 35) led = value; }
inline void pinMode(int, int) {}
inline void analogReadResolution(int) {}
inline void analogReference(int) {}
inline int analogRead(int) { return 0; }
inline void delay(int) {}
""")
    (root / 'helpers/NRF52Board.h').write_text("""
#pragma once
class NRF52Board {
public:
  NRF52Board(const char* = nullptr) {}
  virtual void onBeforeTransmit() {}
  virtual void onAfterTransmit() {}
  virtual unsigned short getBattMilliVolts() { return 0; }
  virtual const char* getManufacturerName() const { return ""; }
  virtual void powerOff() {}
};
class NRF52BoardDCDC : virtual public NRF52Board {};
""")
    source = root / 'test.cpp'
    source.write_text("""
#include "variants/heltec_t114/T114Board.h"
#include <cassert>
int led = HIGH;
int main() {
  T114Board b;
  b.onBeforeTransmit(); assert(led == LOW);
  b.onAfterTransmit(); assert(led == HIGH);
  b.onBeforeTransmit(); b.setTxLedEnabled(false); assert(led == HIGH);
  b.onBeforeTransmit(); assert(led == HIGH);
  b.onAfterTransmit(); assert(led == HIGH);
  b.setTxLedEnabled(true); b.onBeforeTransmit(); assert(led == LOW);
}
""")
    subprocess.run(['g++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-DP_LORA_TX_LED=35', '-I' + tmp, '-I' + str(ROOT),
                    str(source), '-o', str(root / 'led')], check=True)
    subprocess.run([str(root / 'led')], check=True)
print('T114 active-low TX callbacks and immediate disable passed (simulated GPIO).')
