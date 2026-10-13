#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Byte-only, versioned disk record. Stores a full SHA-256 fingerprint, not a PSK.
struct PositionButtonConfig {
  char magic[4] = {'P', 'B', '0', '1'};
  uint8_t enabled = 0;
  char name[32] = {};
  uint8_t fingerprint[32] = {};
};
static_assert(sizeof(PositionButtonConfig) == 69, "Position settings disk layout");

struct PositionButtonState {
  PositionButtonConfig config;
  const char* status = "disabled";
  bool pending = false;
  bool restoreGpsOff = false;
  bool hasSent = false;
  uint32_t started = 0, lastSent = 0, baseline = 0;

  bool coolingDown(uint32_t now) const {
    return hasSent && uint32_t(now - lastSent) < 10000;
  }
  bool timedOut(uint32_t now) const {
    return pending && uint32_t(now - started) >= 120000;
  }
  bool freshFix(bool valid, uint32_t timestamp) {
    if (!valid || timestamp < 1577836800UL) return false;
    // Observe advancing GPS time after this button request, never a cached fix.
    if (!baseline) { baseline = timestamp; return false; }
    if (timestamp > baseline && timestamp - baseline <= 120) return true;
    if (timestamp != baseline) baseline = timestamp;
    return false;
  }
  static bool validName(const char* name) {
    size_t n = strlen(name);
    if (n == 0 || n > 31) return false;
    for (size_t i = 0; i < n; i++) {
      if ((uint8_t)name[i] < 32 || name[i] == ',' || name[i] == ':') return false;
    }
    return true;
  }
  static bool format(char* dest, size_t size, int32_t lat, int32_t lon) {
    if (lat < -90000000 || lat > 90000000 || lon < -180000000 || lon > 180000000) return false;
    uint32_t a = lat < 0 ? -lat : lat, b = lon < 0 ? -lon : lon;
    int n = snprintf(dest, size, "position %s%lu.%06lu, %s%lu.%06lu",
                    lat < 0 ? "-" : "", (unsigned long)(a / 1000000), (unsigned long)(a % 1000000),
                    lon < 0 ? "-" : "", (unsigned long)(b / 1000000), (unsigned long)(b % 1000000));
    return n > 0 && (size_t)n < size;
  }
};
