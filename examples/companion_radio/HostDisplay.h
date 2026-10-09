#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <stddef.h>

// Private USB extension. No allocation or persistent writes during updates.
class HostDisplay {
public:
  enum { COMMAND = 0xF0, VERSION = 1, MAX_ITEMS = 16, MAX_TEXT = 21, MAX_SEGMENTS = 256, MAX_POINTS = 83 };
  enum Operation { INFO, BEGIN, TEXT, CLEAR, SHOW, RELEASE, KEEPALIVE, CAPABILITIES, LINE, POLYLINE, BUTTON_SUBSCRIBE };
  enum Status { OK, BAD_ARGUMENT, BAD_STATE, UNSUPPORTED, NO_DISPLAY, FULL };

private:
  struct ButtonEvent { uint8_t gesture; uint16_t sequence; uint32_t uptime; };
  ButtonEvent buttons[8];
  uint8_t button_head = 0, button_count = 0;
  uint16_t button_sequence = 0;
  bool subscribed = false;
  void stopButtons() { subscribed = false; button_head = button_count = 0; }
  struct TextItem { uint8_t x, y, size; char text[MAX_TEXT + 1]; };
  TextItem pending[MAX_ITEMS];
  struct Segment { uint8_t x0, y0, x1, y1; };
  Segment segments[MAX_SEGMENTS];
  uint16_t segment_count = 0;
  uint8_t count = 0;

  void clearScene() { count = 0; segment_count = 0; }

  static void drawLine(DisplayDriver& display, const Segment& line) {
    // Integer Bresenham, including both endpoints and all octants.
    int x = line.x0, y = line.y0;
    int dx = line.x1 > x ? line.x1 - x : x - line.x1;
    int dy = line.y1 > y ? y - line.y1 : line.y1 - y;
    int sx = x < line.x1 ? 1 : -1, sy = y < line.y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
      display.fillRect(x, y, 1, 1);
      if (x == line.x1 && y == line.y1) break;
      int twice = 2 * error;
      if (twice >= dy) { error += dy; x += sx; }
      if (twice <= dx) { error += dx; y += sy; }
    }
  }
  bool owned = false;
  uint32_t touched = 0, timeout = 30000;

public:
  bool active() const { return owned; }
  bool buttonSubscribed() const { return subscribed; }
  void recordButton(uint8_t gesture, uint32_t now) {
    if (!owned || !subscribed || gesture < 1 || gesture > 4) return;
    uint16_t sequence = ++button_sequence;
    if (button_count == 8) { button_head = (button_head + 1) % 8; --button_count; }
    ButtonEvent& event = buttons[(button_head + button_count++) % 8];
    event.gesture = gesture; event.sequence = sequence; event.uptime = now;
  }
  size_t peekButton(uint8_t* frame) const {
    if (!owned || !subscribed || !button_count) return 0;
    const ButtonEvent& event = buttons[button_head];
    frame[0] = COMMAND; memcpy(frame + 1, "MCOD", 4);
    frame[5] = VERSION; frame[6] = 0; frame[7] = 0x80;
    frame[8] = 0; // onboard user button
    frame[9] = event.gesture;
    frame[10] = event.sequence & 0xFF; frame[11] = event.sequence >> 8;
    for (unsigned i = 0; i < 4; ++i) frame[12+i] = event.uptime >> (8*i);
    return 16;
  }
  void consumeButton() {
    if (button_count) { button_head = (button_head + 1) % 8; --button_count; }
  }
  bool expire(uint32_t now) {
    if (owned && uint32_t(now - touched) >= timeout) {
      owned = false;
      stopButtons();
      clearScene();
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
    if (op > BUTTON_SUBSCRIBE) { resp[8] = UNSUPPORTED; return 9; }
    if (!display) { resp[8] = NO_DISPLAY; return 9; }
    if ((op == BUTTON_SUBSCRIBE && (len != 9 || req[8] > 1)) ||
        (op == BEGIN && len != 10) ||
        (op == TEXT && (len < 12 || len > 11 + MAX_TEXT)) ||
        (op == LINE && len != 12) ||
        (op == POLYLINE && (len < 13 || len > 9 + MAX_POINTS * 2 ||
          req[8] < 2 || req[8] > MAX_POINTS || len != size_t(9 + req[8] * 2))) ||
        (op != BEGIN && op != TEXT && op != LINE && op != POLYLINE && op != BUTTON_SUBSCRIBE && len != 8)) return 9;

    if (op == INFO) {
      resp[8] = OK;
      resp[9] = display->width();
      resp[10] = display->height();
      resp[11] = MAX_ITEMS;
      resp[12] = MAX_TEXT;
      resp[13] = owned ? 1 : 0;
      return 14;
    }
    if (op == CAPABILITIES) {
      resp[8] = OK;
      resp[9] = 1; // graphics revision
      resp[10] = 7; // bit 0: LINE, bit 1: POLYLINE, bit 2: button gestures
      resp[11] = MAX_SEGMENTS & 0xFF;
      resp[12] = MAX_SEGMENTS >> 8;
      resp[13] = MAX_POINTS;
      return 14;
    }
    if (op == RELEASE) { owned = false; stopButtons(); clearScene(); resp[8] = OK; return 9; }
    if (op == BEGIN) {
      uint16_t seconds = uint16_t(req[8]) | (uint16_t(req[9]) << 8);
      if (seconds < 2 || seconds > 300) return 9;
      if (!owned) stopButtons();
      timeout = uint32_t(seconds) * 1000;
      clearScene();
      owned = true;
    } else {
      if (!owned) { resp[8] = BAD_STATE; return 9; }
      if (op == BUTTON_SUBSCRIBE) {
        if (!req[8]) stopButtons();
        else subscribed = true;
      } else if (op == TEXT) {
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
      } else if (op == LINE || op == POLYLINE) {
        unsigned points = op == LINE ? 2 : req[8];
        const uint8_t* xy = req + (op == LINE ? 8 : 9);
        // Validate the entire operation before changing the pending scene.
        for (unsigned i = 0; i < points; i++) {
          if (xy[2*i] >= display->width() || xy[2*i+1] >= display->height()) return 9;
        }
        if (segment_count + points - 1 > MAX_SEGMENTS) { resp[8] = FULL; return 9; }
        for (unsigned i = 1; i < points; i++) {
          Segment& line = segments[segment_count++];
          line.x0 = xy[2*i-2]; line.y0 = xy[2*i-1];
          line.x1 = xy[2*i]; line.y1 = xy[2*i+1];
        }
      } else if (op == CLEAR) {
        clearScene();
      } else if (op == SHOW) {
        display->turnOn();
        display->startFrame();
        display->setColor(UIColor::primary_txt);
        for (unsigned i = 0; i < segment_count; i++) drawLine(*display, segments[i]);
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
