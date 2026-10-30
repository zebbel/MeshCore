#include "UITask.h"
#include "target.h"
#ifdef HELTEC_T114_WITH_DISPLAY
#include "MyMesh.h"
extern MyMesh the_mesh;
#endif
#include <Arduino.h>
#include <helpers/CommonCLI.h>

#ifndef USER_BTN_PRESSED
#define USER_BTN_PRESSED LOW
#endif

#define AUTO_OFF_MILLIS      20000  // 20 seconds
#define BOOT_SCREEN_MILLIS   4000   // 4 seconds

#define POWEROFF_DELAY 3000

// 'meshcore', 128x13px
static const uint8_t meshcore_logo [] PROGMEM = {
    0x3c, 0x01, 0xe3, 0xff, 0xc7, 0xff, 0x8f, 0x03, 0x87, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 
    0x3c, 0x03, 0xe3, 0xff, 0xc7, 0xff, 0x8e, 0x03, 0x8f, 0xfe, 0x3f, 0xfe, 0x1f, 0xff, 0x1f, 0xfe, 
    0x3e, 0x03, 0xc3, 0xff, 0x8f, 0xff, 0x0e, 0x07, 0x8f, 0xfe, 0x7f, 0xfe, 0x1f, 0xff, 0x1f, 0xfc, 
    0x3e, 0x07, 0xc7, 0x80, 0x0e, 0x00, 0x0e, 0x07, 0x9e, 0x00, 0x78, 0x0e, 0x3c, 0x0f, 0x1c, 0x00, 
    0x3e, 0x0f, 0xc7, 0x80, 0x1e, 0x00, 0x0e, 0x07, 0x1e, 0x00, 0x70, 0x0e, 0x38, 0x0f, 0x3c, 0x00, 
    0x7f, 0x0f, 0xc7, 0xfe, 0x1f, 0xfc, 0x1f, 0xff, 0x1c, 0x00, 0x70, 0x0e, 0x38, 0x0e, 0x3f, 0xf8, 
    0x7f, 0x1f, 0xc7, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0x1c, 0x00, 0xf0, 0x0e, 0x38, 0x0e, 0x3f, 0xf8, 
    0x7f, 0x3f, 0xc7, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0x1c, 0x00, 0xf0, 0x1e, 0x3f, 0xfe, 0x3f, 0xf0, 
    0x77, 0x3b, 0x87, 0x00, 0x00, 0x07, 0x1c, 0x0f, 0x3c, 0x00, 0xe0, 0x1c, 0x7f, 0xfc, 0x38, 0x00, 
    0x77, 0xfb, 0x8f, 0x00, 0x00, 0x07, 0x1c, 0x0f, 0x3c, 0x00, 0xe0, 0x1c, 0x7f, 0xf8, 0x38, 0x00, 
    0x73, 0xf3, 0x8f, 0xff, 0x0f, 0xff, 0x1c, 0x0e, 0x3f, 0xf8, 0xff, 0xfc, 0x70, 0x78, 0x7f, 0xf8, 
    0xe3, 0xe3, 0x8f, 0xff, 0x1f, 0xfe, 0x3c, 0x0e, 0x3f, 0xf8, 0xff, 0xfc, 0x70, 0x3c, 0x7f, 0xf8, 
    0xe3, 0xe3, 0x8f, 0xff, 0x1f, 0xfc, 0x3c, 0x0e, 0x1f, 0xf8, 0xff, 0xf8, 0x70, 0x3c, 0x7f, 0xf8, 
};

void UITask::begin(NodePrefs* node_prefs, const char* build_date, const char* firmware_version) {
  _prevBtnState = HIGH;
  _auto_off = millis() + AUTO_OFF_MILLIS;
  _started_at = millis();
  _node_prefs = node_prefs;
  _display->turnOn();

#if defined(PIN_USER_BTN) && defined(DISPLAY_CLASS)
  user_btn.begin();
#endif

  // strip off dash and commit hash by changing dash to null terminator
  // e.g: v1.2.3-abcdef -> v1.2.3
  char *version = strdup(firmware_version);
  char *dash = strchr(version, '-');
  if(dash){
    *dash = 0;
  }

  // v1.2.3 (1 Jan 2025)
  snprintf(_version_info, sizeof(_version_info), "%s (%s)", version, build_date);
  free(version);
}

void UITask::renderCurrScreen() {
  char tmp[80];
  if (millis() < _started_at + BOOT_SCREEN_MILLIS) { // boot screen
    // meshcore logo
    _display->setColor(UIColor::corp_blue);
    int logoWidth = 128;
    _display->drawXbm((_display->width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // meshcore website
    const char* website = "https://meshcore.io";
    _display->setColor(UIColor::primary_txt);
    _display->setTextSize(1);
    _display->drawTextCentered(_display->width() / 2, 22, website);

    // version info
    _display->setTextSize(1);
    _display->drawTextCentered(_display->width() / 2, 35, _version_info);

    // node type
    const char* node_type = "< Repeater >";
    _display->drawTextCentered(_display->width() / 2, 48, node_type);
  } else if (_powering_off_at > 0) {
    // meshcore logo
    _display->setColor(UIColor::corp_blue);
    int logoWidth = 128;
    _display->drawXbm((_display->width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // meshcore website
    const char* website = "https://meshcore.io";
    _display->setColor(UIColor::primary_txt);
    _display->setTextSize(1);
    _display->drawTextCentered(_display->width()/ 2, 22, website);

    // Powering off
    const char* poweroff_string = "Turning OFF";
    uint16_t poffWidth = _display->getTextWidth(poweroff_string);
    _display->setCursor((_display->width() - poffWidth) / 2, 48);
    _display->drawTextCentered(_display->width()/2, 48, poweroff_string);
  } else {
#ifdef HELTEC_T114_WITH_DISPLAY
    _display->setTextSize(1);
    _display->setColor(UIColor::primary_txt);
    if (_usbIdle) {
      _display->drawTextCentered(64, 5, "USB Power");
      _display->setTextSize(2);
      if (_battery.percent() < 0) snprintf(tmp, sizeof(tmp), "--%%");
      else snprintf(tmp, sizeof(tmp), "~%d%%", _battery.percent());
      _display->drawTextCentered(64, 23, tmp);
      drawBattery(28, 44, 70, 13);
      return;
    }
    drawBanner();
    if (_screen == 1) {
      const char* status = "GPS UNAVAILABLE";
      bool success = false, live = false;
#ifdef T114_GPS_ACQUIRE
      status = the_mesh.gpsAcquisitionStatus();
      success = the_mesh.gpsAcquisitionSucceeded();
#endif
      double lat = _node_prefs->node_lat, lon = _node_prefs->node_lon;
#if ENV_INCLUDE_GPS
      auto* gps = sensors.getLocationProvider();
      live = gps && gps->hasFreshFix(5000);
      if (live) { lat=gps->getLatitude()/1000000.0; lon=gps->getLongitude()/1000000.0; }
#endif
      _display->drawTextEllipsized(0, 13, 128, status);
      snprintf(tmp, sizeof(tmp), "%s  Time:%s", live ? "LIVE" : "SAVED", success ? "OK" : "WAIT");
      _display->setCursor(0, 23); _display->print(tmp);
      snprintf(tmp, sizeof(tmp), "Lat: %.6f", lat);
      _display->setCursor(0, 33); _display->print(tmp);
      snprintf(tmp, sizeof(tmp), "Lon: %.6f", lon);
      _display->setCursor(0, 43); _display->print(tmp);
      _display->setCursor(0, 53); _display->print("3 clicks: Retry GPS");
      return;
    }
    if (_screen == 2) {
      NeighbourInfo rows[4];
      int capacity = (_display->height() - 23) / 10;
      if (capacity > 4) capacity = 4;
      int count = the_mesh.getUINeighbours(rows, capacity);
      snprintf(tmp, sizeof(tmp), "Neighbors: %d   SNR", count);
      _display->setCursor(0, 13); _display->print(tmp);
      if (!count) { _display->setCursor(0, 33); _display->print("None heard yet"); }
      for (int i=0; i<count && i<capacity; ++i) {
        const auto* key=rows[i].id.pub_key;
        snprintf(tmp, sizeof(tmp), "%02X%02X%02X%02X %+.2f", key[0],key[1],key[2],key[3],rows[i].snr/4.0);
        _display->setCursor(0, 23+i*10); _display->print(tmp);
      }
      return;
    }
    _display->drawTextEllipsized(0, 13, 128, _node_prefs->node_name);
    snprintf(tmp, sizeof(tmp), "%.3f MHz  SF%d", _node_prefs->freq, _node_prefs->sf);
    _display->setCursor(0, 23); _display->print(tmp);
    snprintf(tmp, sizeof(tmp), "BW %.2f  CR %d", _node_prefs->bw, _node_prefs->cr);
    _display->setCursor(0, 33); _display->print(tmp);
    snprintf(tmp, sizeof(tmp), "TX %d dBm", _node_prefs->tx_power_dbm);
    _display->setCursor(0, 43); _display->print(tmp);
    uint64_t minutes=the_mesh.getUIUptimeMinutes();
    snprintf(tmp, sizeof(tmp), "Up %lu d %02lu h %02lu m", (unsigned long)(minutes/1440),
             (unsigned long)(minutes/60%24), (unsigned long)(minutes%60));
    _display->drawTextEllipsized(0, 53, 128, tmp);
    return;
#endif
    _display->setCursor(0, 0);
    _display->setTextSize(1);
    _display->setColor(UIColor::primary_txt);
    _display->print(_node_prefs->node_name);

    // freq / sf
    _display->setCursor(0, 20);
    sprintf(tmp, "FREQ: %06.3f SF%d", _node_prefs->freq, _node_prefs->sf);
    _display->print(tmp);

    // bw / cr
    _display->setCursor(0, 30);
    sprintf(tmp, "BW: %03.2f CR: %d", _node_prefs->bw, _node_prefs->cr);
    _display->print(tmp);

  }
}

void UITask::loop() {
#ifdef HELTEC_T114_WITH_DISPLAY
  if (_battery.due(millis()))
    _battery.sample(millis(), _board->getBattMilliVolts());
#endif
#if defined(PIN_USER_BTN) && defined(DISPLAY_CLASS)
  int ev = user_btn.check();
#ifdef HELTEC_T114_WITH_DISPLAY
  if (ev) _auto_off = millis() + AUTO_OFF_MILLIS;
#endif
  if (ev == BUTTON_EVENT_CLICK) {
    if (_display->isOn()) {
#ifdef HELTEC_T114_WITH_DISPLAY
      if (_usbIdle) _usbIdle = false;
      else _screen = (_screen + 1) % 3;
#endif
    } else {
      _display->turnOn();
    }
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
#ifdef HELTEC_T114_WITH_DISPLAY
    _next_refresh = 0;
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
#ifdef T114_BUTTON_RGB
    the_mesh.sendButtonAdvertisement();
#else
    the_mesh.sendSelfAdvertisement(1500, true);
#endif
    _auto_off = millis() + AUTO_OFF_MILLIS;
    _next_refresh = 0;
#endif
#ifdef T114_GPS_ACQUIRE
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK && _screen == 1 && !_usbIdle) {
    the_mesh.startGPSAcquisition();
    _next_refresh = 0;
#endif
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      _display->turnOn();
      Serial.println("Powering Off");
#ifdef HELTEC_T114_WITH_DISPLAY
      _auto_off = millis() + AUTO_OFF_MILLIS;
#endif
      _powering_off_at = millis() + POWEROFF_DELAY; 
  }
#endif

#ifdef HELTEC_T114_WITH_DISPLAY
  // VBUS alone determines USB idle mode; solar is not inferred.
  bool usb = _board->isExternalPowered();
  bool idle = !_powering_off_at && int32_t(uint32_t(millis()) - uint32_t(_auto_off)) >= 0;
  bool usbIdle = usb && idle;
  if (_usbIdle != usbIdle) { _usbIdle=usbIdle; _next_refresh=0; }
  if (usb && !_display->isOn()) { _display->turnOn(); _next_refresh=0; }
  if (!usb && idle && _display->isOn()) _display->turnOff();
#endif
  if (_display->isOn()) {
    if (millis() >= _next_refresh) {
      _display->startFrame();
      renderCurrScreen();
      _display->endFrame();

      _next_refresh = millis() + 1000;   // refresh every second
    }
#ifndef HELTEC_T114_WITH_DISPLAY
    if (millis() > _auto_off) _display->turnOff();
#endif
  }

  if (_powering_off_at > 0) { // power off timer armed
#if defined(LED_PIN) && defined(LED_STATE_ON)
    digitalWrite(LED_PIN, LED_STATE_ON); // switch on the led until poweroff
#ifndef HELTEC_T114_WITH_DISPLAY
    delay(1000);
#endif
    digitalWrite(LED_PIN, !LED_STATE_ON); // off LED
#endif
    if (millis() > _powering_off_at) {
      _board->powerOff();  // should not return
    }
  }
}

#ifdef HELTEC_T114_WITH_DISPLAY
void UITask::drawBattery(int x, int y, int w, int h) {
  _display->drawRect(x, y, w, h);
  _display->fillRect(x+w, y+h/3, 2, h/3);
  int percent=_battery.percent();
  if (percent > 0) _display->fillRect(x+2, y+2, (w-4)*percent/100, h-4);
}

void UITask::drawBanner() {
  char text[16];
  drawBattery(1, 1, 17, 8);
  if (_battery.percent() < 0) snprintf(text,sizeof(text),"--%%");
  else snprintf(text,sizeof(text),"~%d%%",_battery.percent());
  _display->setCursor(23,0); _display->print(text);
  ColorVal color=0xF800; // RGB565 red
  const char* label="GPS !";
#ifdef T114_GPS_ACQUIRE
  if (the_mesh.gpsAcquisitionSucceeded()) { color=0x07E0; label="GPS OK"; }
  else if (the_mesh.gpsAcquisitionActive()) {
    color=0xFD20; label="GPS ...";
  }
#endif
  _display->setColor(color);
  // Satellite: two solar panels, central body and antenna.
  // The text state keeps the indication readable without relying on color.
  _display->drawRect(74,2,3,6);
  _display->drawRect(82,2,3,6);
  _display->fillRect(77,4,5,1);
  _display->fillRect(78,3,3,4);
  _display->fillRect(79,0,1,3);
  _display->fillRect(78,0,3,1);
  _display->setCursor(86,0); _display->print(label);
  _display->setColor(UIColor::primary_txt);
  _display->fillRect(0,11,128,1);
}
#endif
