#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <helpers/CommonCLI.h>
#ifdef HELTEC_T114_WITH_DISPLAY
#include "BatteryEstimate.h"
#endif

class UITask {
  mesh::MainBoard* _board;
  DisplayDriver* _display;
  unsigned long _next_read, _next_refresh, _auto_off;
  int _prevBtnState;
#ifdef HELTEC_T114_WITH_DISPLAY
  uint8_t _screen = 0;
  BatteryEstimate _battery;
  bool _usbIdle = false;
  void drawBattery(int x, int y, int w, int h);
  void drawBanner();
#endif
  NodePrefs* _node_prefs;
  char _version_info[32];
  unsigned long _powering_off_at = 0;
  unsigned long _started_at = 0;

  void renderCurrScreen();
public:
  UITask(mesh::MainBoard& board, DisplayDriver& display) : _board(&board), _display(&display) { _next_read = _next_refresh = 0; }
  void begin(NodePrefs* node_prefs, const char* build_date, const char* firmware_version);

  void loop();
};
