#pragma once
#include "DisplayDriver.h"

// Nearest-neighbour scaling of a classic 6x8 cell; blank spacing is preserved.
inline void drawMediumGlyph(DisplayDriver& display, int x, int y, const uint8_t rows[8]) {
  for (int dy = 0; dy < 12; ++dy) {
    for (int dx = 0; dx < 8; ++dx) {
      if (rows[dy * 8 / 12] & (1 << (dx * 6 / 8))) {
        display.fillRect(x + dx, y + dy, 1, 1);
      }
    }
  }
}
