#include "TWatchUltraBoard.h"
#include <cassert>

WireMock Wire;
SerialMock Serial;
TWatchUltraBoard board;
bool detected = true, railEnabled, railCanEnable = true, vbus, battery = true;
uint16_t railVoltage, batteryVoltage = 4100;
std::map<int, int> gpio;
unsigned delayed;

void digitalWrite(uint8_t pin, int level) { gpio[pin] = level; }
void pinMode(uint8_t pin, int mode) { assert(gpio.at(pin) == HIGH && mode == OUTPUT); }
void delay(unsigned ms) { delayed += ms; }

struct TestClock { void begin() {} } rtc_clock;
struct TestSPI {} radio_spi;
struct TestRadio {
  unsigned calls = 0;
  bool result = true;
  bool std_init(TestSPI*) { ++calls; return result; }
} radio;
bool radio_init();

int main() {
  Wire.registers = {{0x3430, 0x72}, {0x2003, 0x16}, {0x2007, 0xfb}};
  assert(!radio_init() && radio.calls == 0);
  board.begin();
  assert(Wire.initialized && board.isRadioPowerReady() && railEnabled);
  assert(railVoltage == 3300 && delayed == 10);
  assert(gpio.size() == 3 && gpio.at(4) && gpio.at(21) && gpio.at(36));
  assert(Wire.registers[0x3430] == 0x72);
  assert(Wire.registers[0x2003] == 0x1e && Wire.registers[0x2007] == 0xf3);
  assert(radio_init() && radio.calls == 1);
  radio.result = false;
  assert(!radio_init() && radio.calls == 2);
  assert(board.getBattMilliVolts() == 4100 && !board.isExternalPowered());
  vbus = true;
  assert(board.isExternalPowered());
  batteryVoltage = 8191;
  assert(board.getBattMilliVolts() == 0);
  batteryVoltage = 4100;
  battery = false;
  assert(board.getBattMilliVolts() == 0);
  Wire.registers[0x3430] = 0x70;
  board.begin();
  assert(board.isRadioPowerReady() && Wire.registers[0x3430] == 0x70);
  Wire.missing = 0x20;
  board.begin();
  assert(!board.isRadioPowerReady() && !radio_init() && radio.calls == 2);
  Wire.missing = 0x34;
  board.begin();
  assert(!board.isRadioPowerReady() && !board.isExternalPowered());
  Wire.missing = 0;
  Wire.failWrite = true;
  board.begin();
  assert(!board.isRadioPowerReady());
  Wire.failWrite = false;
  detected = false;
  board.begin();
  assert(!board.isRadioPowerReady());
  detected = true;
  railCanEnable = false;
  board.begin();
  assert(!board.isRadioPowerReady());
}
