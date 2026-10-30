#ifdef T114_GPS_ACQUIRE
#include "MyMesh.h"
#include "target.h"

void MyMesh::startGPSAcquisition() {
  if (_gpsAcquiring) return;
  _gpsSuccess = false;
  _gpsStatus = "GPS UNAVAILABLE";
#if ENV_INCLUDE_GPS
  auto* gps = sensors.getLocationProvider();
  if (!gps) return;
  const char* requested = sensors.getSettingByKey("gps");
  if (!requested || (strcmp(requested, "1") && !sensors.setSettingValue("gps", "1"))) return;
  // Existing provider operation clears cached NMEA and requests a new time sync.
  gps->syncTime();
  _gpsStarted = millis();
  _gpsAcquiring = true;
  _gpsStatus = "GPS SEARCHING";
#endif
}

void MyMesh::loopGPSAcquisition() {
#if ENV_INCLUDE_GPS
  auto* gps = sensors.getLocationProvider();
  if (!_gpsAcquiring) {
    // Manual CLI operation stays manual: never restart or stop it here.
    if (gps && gps->isEnabled())
      _gpsStatus = gps->hasFreshFix(5000) ? "GPS FIX / SYNCING" : "GPS SEARCHING";
    return;
  }
  if (!gps) { _gpsAcquiring = false; _gpsStatus = "GPS UNAVAILABLE"; return; }
  const char* requested = sensors.getSettingByKey("gps");
  if (!requested || strcmp(requested, "1") || !gps->isEnabled()) {
    _gpsAcquiring = false; _gpsStatus = "GPS OFF"; return; // preserve manual CLI off
  }
  if (uint32_t(millis() - _gpsStarted) >= 600000) {
    sensors.setSettingValue("gps", "0");
    _gpsAcquiring = false; _gpsStatus = "GPS TIMEOUT"; return;
  }
  if (!gps->hasFreshFix(5000)) { _gpsStatus = "GPS SEARCHING"; return; }
  _gpsStatus = "GPS FIX / SYNCING";
  if (!gps->hasValidDateTime() || gps->waitingTimeSync()) return;
  uint32_t time = gps->getTimestamp();
  int32_t difference = int32_t(getRTCClock()->getCurrentTime() - time);
  if (difference < -2 || difference > 2) return;
  double lat = gps->getLatitude() / 1000000.0;
  double lon = gps->getLongitude() / 1000000.0;
  if (!(lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180)) return;
  double oldLat = _prefs.node_lat, oldLon = _prefs.node_lon;
  _prefs.node_lat = lat; _prefs.node_lon = lon;
  if (!_cli.savePrefs(_fs)) {
    _prefs.node_lat = oldLat; _prefs.node_lon = oldLon;
    // Retain old settings and permit manual retry; do not repeatedly write flash.
    sensors.setSettingValue("gps", "0");
    _gpsAcquiring = false; _gpsStatus = "GPS SAVE FAILED"; return;
  }
  sensors.node_lat = lat; sensors.node_lon = lon;
  _gpsSuccess = true;
  // Consume this attempt before queuing through the normal advertisement path.
  // Both prefs and shared coordinates are current; location policy remains intact.
  _gpsAcquiring = false;
  sendSelfAdvertisement(1500, true); // not a button advert: no RGB feedback
  sensors.setSettingValue("gps", "0"); // transient only: never change _prefs.gps_enabled
  _gpsStatus = "FIX SAVED - GPS OFF";
#endif
}
#endif
