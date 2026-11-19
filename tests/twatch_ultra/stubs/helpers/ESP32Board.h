#pragma once

#include <cstdint>
#include <map>
#include <vector>

struct WireMock {
  std::map<unsigned, uint8_t> registers;
  std::vector<uint8_t> tx;
  uint8_t address = 0;
  uint8_t reg = 0;
  uint8_t missing = 0;
  bool failWrite = false;
  bool initialized = false;

  void beginTransmission(uint8_t v) { address = v; tx.clear(); }
  void write(uint8_t v) { tx.push_back(v); }
  int endTransmission(bool = true) {
    if (address == missing || (tx.size() == 2 && failWrite)) return 1;
    reg = tx.at(0);
    if (tx.size() == 2) registers[address*256 + reg] = tx.at(1);
    return 0;
  }
  int requestFrom(uint8_t, uint8_t) { return 1; }
  int read() { return registers[address*256 + reg]; }
};
extern WireMock Wire;

struct SerialMock {
  void println(const char*) {}
};
extern SerialMock Serial;

constexpr int HIGH = 1;
constexpr int OUTPUT = 1;
void digitalWrite(uint8_t pin, int level);
void pinMode(uint8_t pin, int mode);
void delay(unsigned ms);

class ESP32Board {
public:
  void begin() { Wire.initialized = true; }
  virtual uint16_t getBattMilliVolts() = 0;
  virtual bool isExternalPowered() = 0;
  virtual const char* getManufacturerName() const = 0;
  virtual void sleep(uint32_t) = 0;
};
