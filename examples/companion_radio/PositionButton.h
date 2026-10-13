#pragma once
#include <stdint.h>
#include <math.h>
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
  bool hasSent = false;
  uint32_t lastSent = 0;

  bool coolingDown(uint32_t now) const {
    return hasSent && uint32_t(now - lastSent) < 10000;
  }
  static bool validName(const char* name) {
    size_t n = strlen(name);
    if (n == 0 || n > 31) return false;
    for (size_t i = 0; i < n; i++) {
      if ((uint8_t)name[i] < 32 || name[i] == ',' || name[i] == ':') return false;
    }
    return true;
  }
  static bool formatStored(char* dest, size_t size, double lat, double lon) {
    // Same position used by adverts. Zero/zero is the unset default.
    // Ordered bounds also reject NaN before any integer conversion.
    if (!(lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0)
        || (lat == 0.0 && lon == 0.0)) return false;
    return format(dest, size, (int32_t)lround(lat * 1000000.0),
                  (int32_t)lround(lon * 1000000.0));
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
