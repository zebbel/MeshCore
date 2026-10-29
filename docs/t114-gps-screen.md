# T114 GPS screen

The display repeater cycles through Repeater Information, GPS Information,
Battery & Uptime, and Neighbors. Existing wake, advertisement, shutdown,
refresh and timeout behavior is retained.

The GPS page reads the existing sensor manager's LocationProvider. No GPS
initialization, pin, configuration, power or build flag is changed. The target
inherits ENV_INCLUDE_GPS=1 from sensor_base.

Status is FIX only when enabled, valid and the MicroNMEA provider has observed a
valid GGA/RMC fix within five seconds. NO FIX covers searching, invalid and
expired fixes; OFF indicates disabled GPS; UNAVAILABLE indicates no provider.
Coordinates and altitude are blank placeholders unless the fix is current.
The new read-only freshness method uses the existing observation timestamps;
other providers conservatively return false unless they implement freshness.

Latitude/longitude divide microdegrees by 1,000,000 and show six decimals.
Altitude divides millimeters by 1,000 and shows one decimal in meters.
Six rows use logical y positions 0,10,20,30,40,50. The existing ST7789 scale maps
these to the 240x135 panel with a 16-pixel font; the last row fits below y=122.

Changed production files: UITask.cpp, LocationProvider.h,
MicroNMEALocationProvider.h.
Tests: tests/t114_ui/test_ui.py and test_gps_freshness.py.
Host tests cover four-page order, GPS state transitions, negative coordinate and
altitude conversion, stale-coordinate suppression, gestures, timeout, neighbors,
and generic/no-GPS UI compilation. Freshness tests include timestamp rollover.
Position, LED, preferences access and OLED host regressions also pass.

Hardware verification remains required for physical layout, actual GPS
acquisition/loss/power transitions, buttons and concurrent LoRa operation.
Full T114 firmware compilation is not verified: the build attempt stalled
downloading platformio/toolchain-gccarmnoneeabi. No firmware artifact is provided.
