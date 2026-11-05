# XIAO neighbor SNR development report

Date: 2026-11-05. Branch: `meshcoreStation`. Base commit: `82f16e0c695072db43187b20885cebfc146a2554`.

## Delivery status

Source upload to `meshcoreStation` is authorized as version `v1.0.20`. The user will compile locally; no firmware binary release is part of this delivery. Full target compilation, artifact validation and hardware checks remain outstanding. No MeshcoreStation application/database code was changed.

## Implementation and modified files

- `examples/simple_repeater/NeighbourSNR.h`: allocation-free aggregation with 50 active neighbors, 100 identity slots, four completed 30-minute records per identity, bounded expiration/eviction, immutable records, diagnostics, boot-scoped sequence numbers and versioned paginated request `0x70`. Failed boot entropy generation disables aggregate export with explicit status 5 while preserving active-neighbor tracking.
- `examples/simple_repeater/MyMesh.cpp/.h`: XIAO-only reception integration, periodic maintenance and binary request dispatch through the existing authenticated request path; preserves standard neighbor request and companion transport.
- `variants/xiao_nrf52/target.cpp/.h`: confirmed-clock tracking for aggregate UTC labels, avoiding the fallback clock's fixed startup date. Existing clock behavior is retained.
- `variants/xiao_nrf52/platformio.ini`, `scripts/check-xiao-layout.py`: feature scoped to the existing XIAO repeater environment and resolved application linker-region guard.
- `build-station.sh`: fourth target, XIAO UF2 and DFU ZIP with versioned names/checksums, preserving atomic staging and source-update behavior.
- `scripts/verify-station-build.py`, `scripts/publish-station-release.py`: 15-artifact validation, Nordic layout/archive/checksum validation, and publishing safeguards.
- `tests/neighbour_snr/`: aggregation, boundary/overflow, clock, production integration, standard companion transport and layout tests.
- `tests/position_button/test_build.py`, `test_publish.py`, `test_nordic_layout.py`: four-target packaging and failure/invalid-artifact regression coverage.
- `docs/xiao-neighbour-snr.md`, `docs/station-firmware-release.md`: protocol, retention and memory limits, build outputs and flashing constraints.

## Automated verification

All of the following passed using the production code or stated host stand-ins:

- `test_snr.cpp` and `test_limits.cpp`, compiled with C++11, warnings-as-errors and undefined-behavior sanitizer: average encoding, empty windows, 50-neighbor load, four-window retention, identity churn, immutable pagination, expiration, integer limits, RTC jumps, millis wrap, reboot separation and entropy failure.
- `tests/neighbour_snr/test_integration.py`: extracted production repeater integration and unmodified standard companion command 50/response forwarding, with host transport stand-ins.
- `tests/neighbour_snr/test_clock.py`: production clock wrapper, initially untrusted fallback and explicit valid/invalid clock updates.
- `tests/neighbour_snr/test_layout.py`: resolved linker guard accepts existing application layout and rejects a changed origin; feature scope verified.
- `tests/position_button/test_build.py`, `test_publish.py`, `test_nordic_layout.py`: mocked compiler/GitHub release tests, all 15 artifacts, atomic failure cleanup, publication safeguards, invalid UF2/ZIP and incomplete output rejection. These tests did not publish anything.
- `tests/position_button/run.sh`: position-button state, help, CLI and integration regressions.
- `tests/t114_ui/test_ui.py`, `tests/t114_gps/test_acquisition.py`, `tests/t114_gps/test_remote_cli.py`, `tests/t1000_charging/test_led.py`.
- `git diff --check` and `bash -n build-station.sh`.

The aggregation object measures 20,064 bytes on the host, bounded by a 24,000-byte static assertion. This is not an ARM link result or measured runtime free RAM.

## Firmware build attempts and artifacts

PlatformIO builds were attempted for `Xiao_nrf52_repeater`, `Heltec_t114_repeater`, `t1000e_companion_radio_ble_position` and `heltec_v4_companion_radio_usb`. Dependency installation did not finish within the available command timeouts: the Nordic platform installed, but the ARM toolchain installation stalled; the ESP32 platform installation also remained incomplete. The XIAO retry remained at ARM toolchain installation.

There is no successful target compilation, ELF memory report or flashable firmware from this work. Packaging fixtures used by automated tests are synthetic and must not be flashed. Real UF2/DFU checksums and hardware bootloader compatibility remain to be validated after a complete build.

## Required follow-up verification

- Complete all four target builds with dependency access, review XIAO flash/static RAM and runtime stack/heap headroom, then verify generated UF2 and Nordic DFU ZIP on the actual board/bootloader.
- Exercise authenticated guest/admin binary requests, replay protection and pagination over a real standard companion/radio link; confirm no packet truncation or routing regression.
- Verify reception/SNR values, 50 active neighbors, turnover and inactive cleanup, multi-hour retention and loss diagnostics under actual radio load.
- Verify reboot/session changes, RTC synchronization/corrections and invalid-time labeling with the device's actual clock configuration.
- Check unchanged T114 UI/GPS/buttons/LEDs, T1000-E position/charging behavior and Heltec V4 communication on hardware.

See [protocol specification](xiao-neighbour-snr.md) for the exact request/response layout and retention limitations.

## Source delivery verification

The complete host regression suite was rerun successfully for `v1.0.20`. The user requested source upload only and will perform firmware builds locally.

During earlier build attempts, the ARM toolchain, pinned Nordic framework, CMSIS, Nordic tools and RadioLib installed. One dependency mirror returned invalid checksums; the fallback mirror passed verification without bypassing checks. Dependency installation was stopped before compilation (exit 130), with Crypto and ESP32 toolchain dependencies still pending. This is not a successful firmware build or a compiler failure. No flashable artifacts were generated or published.
