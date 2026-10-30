# T114 startup GPS acquisition — v1.0.13

Only the Heltec_t114_repeater target defines T114_GPS_ACQUIRE.

Each boot starts the existing GPS sensor and requests time synchronization.
The loop waits for hasFreshFix(5000), a valid calendar date/time, completion of
the existing GPS time-sync request, and RTC readback within two seconds of GPS.
Only then does it save the acquired coordinates in node_lat/node_lon.

T114 preference writes use /prefs.tmp, verify the serialized location and rename
over /prefs.json. Failed writes leave previous preferences intact; RAM coordinates
are rolled back. Existing gps advert policies are unchanged. Both the persistent
and sensor location are updated after success.

After saving, a RAM-only success flag becomes true and GPS is stopped through
the existing sensor setting interface. Automatic stops never modify or save
gps_enabled. Next boot starts acquisition regardless of that stored setting.
A ten-minute elapsed-time limit stops unsuccessful acquisition. Save failure
stops GPS with GPS SAVE FAILED; old coordinates remain available.

Triple press on GPS page retries, clearing session success. Repeated triple
presses while already acquiring are ignored, preventing overlap and timeout
extension. Single, double, long press and display timeout retain their behavior.
Manual CLI off cancels active acquisition. CLI on outside acquisition remains
manual and is not automatically stopped.

The display shows acquisition status, satellites, time status, live or explicitly
Saved coordinates, altitude when live, and "3 clicks: Retry GPS".
RGB feedback now reads session success at actual successful button-advert TX:
blue remains after GPS automatically stops; retry/failure/timeout is purple.

Changed production files:
- examples/simple_repeater/GPSAcquisition.cpp (new), MyMesh.cpp/.h, UITask.cpp, main.cpp
- src/helpers/CommonCLI.cpp (T114-only preference write path)
- src/helpers/sensors/LocationProvider.h and MicroNMEALocationProvider.h
- variants/heltec_t114/platformio.ini
- meshcoreStation.version

Automated verification passed:
- Production acquisition code with device/filesystem stand-ins: boot, fresh fix,
  date/sync prerequisites, RTC mismatch, save failure, timeout, retry, CLI off,
  retained location and reset after restart.
- Production date validation: invalid/unset dates, leap-year day, invalid seconds.
- UI: triple only on GPS page, last-known labeling, four pages, wake, timeout,
  advertisement and shutdown.
- RGB: session success controls blue even without live fix; cleared success
  controls purple even with a live fix.
- Existing position, GPS freshness, LED and preference-access host tests.

Full firmware build was attempted but timed out downloading the ARM toolchain.
No binary produced. Device validation remains required for acquisition and RTC
sync, power draw after shutdown, persistent filesystem writes/failure recovery,
manual CLI interactions, screen layout, button gestures and LoRa coexistence.
Host tests simulate storage failures; they are not physical flash fault tests.
