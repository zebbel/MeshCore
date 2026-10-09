#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <stddef.h>

// Private USB extension. No allocation or persistent writes during updates.
class HostDisplay {
public:
  enum { COMMAND = 0xF0, VERSION = 1, MAX_ITEMS = 16, MAX_TEXT = 21 };
  enum Operation { INFO, BEGIN, TEXT, CLEAR, SHOW, RELEASE, KEEPALIVE };
  enum Status { OK, BAD_ARGUMENT, BAD_STATE, UNSUPPORTED, NO_DISPLAY, FULL };

private:
  struct TextItem { uint8_t x, y, size; char text[MAX_TEXT + 1]; };
  TextItem pending[MAX_ITEMS];
  uint8_t count = 0;
  bool owned = false;
  uint32_t touched = 0, timeout = 30000;

public:
  bool active() const { return owned; }
  bool expire(uint32_t now) {
    if (owned && uint32_t(now - touched) >= timeout) {
      owned = false;
      count = 0;
      return true;
    }
    return false;
  }

  // Request includes command byte; response uses its own identifiable envelope.
  size_t handle(const uint8_t* req, size_t len, uint8_t* resp,
                DisplayDriver* display, uint32_t now) {
    // Never inspect fields outside a short request.
    resp[0] = COMMAND;
    memcpy(resp + 1, "MCOD", 4);
    resp[5] = VERSION;
    resp[6] = len > 6 ? req[6] : 0;
    resp[7] = len > 7 ? req[7] : 0xFF;
    resp[8] = BAD_ARGUMENT;
    if (len < 8 || req[0] != COMMAND || memcmp(req + 1, "MCOD", 4)) return 9;
    if (req[5] != VERSION) { resp[8] = UNSUPPORTED; return 9; }
    uint8_t op = req[7];
    if (op > KEEPALIVE) { resp[8] = UNSUPPORTED; return 9; }
    if (!display) { resp[8] = NO_DISPLAY; return 9; }
    if ((op == BEGIN && len != 10) ||
        (op == TEXT && (len < 12 || len > 11 + MAX_TEXT)) ||
        (op != BEGIN && op != TEXT && len != 8)) return 9;

    if (op == INFO) {
      resp[8] = OK;
      resp[9] = display->width();
      resp[10] = display->height();
      resp[11] = MAX_ITEMS;
      resp[12] = MAX_TEXT;
      resp[13] = owned ? 1 : 0;
      return 14;
    }
    if (op == RELEASE) { owned = false; count = 0; resp[8] = OK; return 9; }
    if (op == BEGIN) {
      uint16_t seconds = uint16_t(req[8]) | (uint16_t(req[9]) << 8);
      if (seconds < 2 || seconds > 300) return 9;
      timeout = uint32_t(seconds) * 1000;
      count = 0;
      owned = true;
    } else {
      if (!owned) { resp[8] = BAD_STATE; return 9; }
      if (op == TEXT) {
        unsigned n = len - 11;
        uint8_t x = req[8], y = req[9], size = req[10];
        if (size < 1 || size > 2 || x + n * 6 * size > unsigned(display->width()) ||
            y + 8 * size > display->height()) return 9;
        for (size_t i = 11; i < len; i++) if (req[i] < 32 || req[i] > 126) return 9;
        if (count == MAX_ITEMS) { resp[8] = FULL; return 9; }
        TextItem& item = pending[count++];
        item.x = x; item.y = y; item.size = size;
        memcpy(item.text, req + 11, n);
        item.text[n] = 0;
      } else if (op == CLEAR) {
        count = 0;
      } else if (op == SHOW) {
        display->turnOn();
        display->startFrame();
        display->setColor(UIColor::primary_txt);
        for (unsigned i = 0; i < count; i++) {
          display->setTextSize(pending[i].size);
          display->setCursor(pending[i].x, pending[i].y);
          display->print(pending[i].text);
        }
        display->endFrame();
      }
    }
    touched = now;
    resp[8] = OK;
    return 9;
  }
};
