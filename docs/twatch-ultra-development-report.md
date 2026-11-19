# LilyGO T-Watch Ultra development report

Repository: `zebbel/MeshCore`, branch `meshcoreStation`, base
`14ce25854cffd00a3e281a5e81a3e0a93433b60d`. The development builds used `v1.0.20`. The source update sets
`meshcoreStation.version` to `v1.0.21` for subsequent local builds.

| Area | Files |
| --- | --- |
| Watch variant | `variants/lilygo_twatch_ultra/platformio.ini`, `partitions.csv`, `target.h`, `target.cpp`, `TWatchUltraBoard.h`, `TWatchUltraBoard.cpp` |
| Station builds | `build-station.sh`, `scripts/verify-station-build.py`, `scripts/publish-station-release.py` |
| Tests | `tests/twatch_ultra/`, `tests/position_button/test_build.py`, `tests/position_button/test_publish.py` |
| Documentation | `docs/lilygo-twatch-ultra.md`, `docs/station-firmware-release.md`, this report |

The shared board initializes AXP2101 ALDO3 at 3.3 V and selects the internal
antenna through XL9555 before starting the existing SX1262 driver. The board
preserves the charger settings, temperature ADC policy and other power rails.
The three environments reuse standard MeshCore USB companion, BLE companion
and headless repeater code. See the [hardware and build guide](lilygo-twatch-ultra.md)
for the reference pinout, partition layout and flashing instructions.

## Verification

The 19 Python checks and the position-button integration script passed.
Coverage includes watch power failures, preservation of PMU and antenna
registers, radio startup gating, resolved role configurations, station artifact
validation, staged failure cleanup, simulated publishing, Nordic flash layouts,
T114 GPS/UI/RGB and TX LED, T1000-E charging LED and XIAO neighbor SNR.
Espressif's partition generator and the station verifier accepted the watch
layout. The station tests use synthetic compiler output and a mock GitHub CLI.

| PlatformIO environment | Result | Application image | Merged image |
| --- | --- | ---: | ---: |
| `lilygo_twatch_ultra_companion_radio_usb` | Passed | 553,488 bytes | 619,024 bytes |
| `lilygo_twatch_ultra_companion_radio_ble` | Passed | 1,185,584 bytes | 1,251,120 bytes |
| `lilygo_twatch_ultra_repeater` | Passed | 1,082,016 bytes | 1,147,552 bytes |

All three merged images passed staging and validation. Their boot and application
headers declare an ESP32-S3 and 16 MB flash. The partition table starts at
`0x8000` and the application at `0x10000`. Esptool kept the original
hash-protected bootloader flash mode during merging.

The first repeater build pulled in a TCP library declared for Raspberry Pi by
a multi-platform dependency. Strict compatibility in the watch repeater
environment resolved it.

Fresh PlatformIO builds also passed for `heltec_v4_companion_radio_usb`,
`Heltec_t114_repeater`, `t1000e_companion_radio_ble_position` and
`Xiao_nrf52_repeater`. The first Heltec V4 test picked up an nRF52-only
CustomLFS dependency copied into the temporary build cache. Removing that
cache entry fixed the test without changing the Heltec target.

The station verifier accepted and revalidated the 30 versioned artifacts
assembled from the seven real builds, including all checksums and manifests.
The build wrapper's update and failure paths were tested with simulated
compilers. GitHub publication was tested with a mock only. No tag or firmware release was created during validation.

## Physical verification

The hardware checks require an SX1262 T-Watch Ultra. Verify cold boot from
battery and USB, 8 MB PSRAM detection, charging and temperature protection,
ALDO3 voltage, internal antenna selection, SX1262 bring-up and LoRa RX/TX.
Then verify native USB framing, companion BLE pairing and reconnect, repeater
forwarding, persistence and operation through power cycles. Factory data
migration, the external RTC, display, touch, GNSS and optimized sleep are
outside this milestone.
