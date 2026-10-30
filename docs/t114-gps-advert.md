# Automatic GPS acquisition advertisement — v1.0.16

After fresh-fix, valid-time, RTC readback and persistent-coordinate save checks pass, GPSAcquisition consumes the current attempt and calls `sendSelfAdvertisement(1500, true)` once. Both persistent and shared sensor coordinates are updated before packet creation. The existing scoped flood queue handles the delay without blocking; GPS then powers off as before.

The existing `none`, `prefs` and `share` policies are unchanged. No button-advertisement wrapper is called, so automatic packets do not request RGB feedback. Startup/periodic advertisement scheduling is unchanged. Each successful manual acquisition retry creates one additional advertisement request.

A request does not guarantee over-the-air delivery: existing packet-allocation, queue and radio failure behavior applies. No automatic retry is introduced.

Changed implementation: `examples/simple_repeater/GPSAcquisition.cpp` only. Tests: extended `tests/t114_gps/test_acquisition.py`; added `tests/t114_gps/test_advert_policy.py`. Release version and this report also updated.

Passed host verification: success/retry/duplicate suppression, timeout/save failure/invalid time/RTC mismatch/manual cancellation, persistent-position retention, GPS shutdown, actual policy builder and send-method extraction (none/share/prefs, flood scope, allocation failure and independent button tagging), RGB feedback, T114 UI/neighbor regression and green TX LED tests. Hardware interfaces use test stand-ins.

Attempted PlatformIO `Heltec_t114_repeater` build: blocked installing `platformio/toolchain-gccarmnoneeabi`. No full firmware compile or flashable artifact is claimed. Physical GPS acquisition/save/RTC/shutdown, actual flooded packet reception with new coordinates, absence of automatic RGB flashes, manual RGB feedback and concurrent routing still require hardware testing.

Source publication follows the user's standing authorization to publish handovers.
