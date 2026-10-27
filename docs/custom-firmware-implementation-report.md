# Custom firmware implementation report

Base: zebbel/MeshCore meshcoreStation, e18ae33a (custom v1.0.8).
Custom version: v1.0.9. Source publication authorized by the user. No binary release created.

## Implemented

- T114 default-on, persistent active-low GPIO 35 TX LED control. Existing
  preferences serialize tx_led; repeater startup restores it. CommonCLI handles
  set led on/off, get led and ?. The existing remote admin gate remains intact;
  USB serial continues through the same CLI. Disabling drives the pin high.
- T1000-E CLI delegates to a new CustomCLI.cpp through MyMesh::handleCommand.
  Existing CMD_RUN_CLI_COMMAND integration remains in use. LED and position
  enable/status commands reuse existing implementation and PB03 storage.
  Destination and explicit scope are configured atomically with
  set position-target channel|room none|<32hex-scope> <exact name>.
  Matching rejects missing/ambiguous destinations and malformed scope.
  Help is split across ? and ? position within the response budget.
- Three-target build wrapper adds the display Heltec_t114_repeater target.
  Eleven versioned artifacts retain the existing seven outputs and add T114
  UF2, DFU ZIP and two SHA-256 files. Existing update/staging behavior remains.
  Release validation checks target presence and Nordic checksums before upload.
- T114 layout checked against the existing board JSON and linker script:
  nRF52840, S140 6.1.1 / FWID B6, application 0x26000..0xED000,
  bootloader settings 0xFF000. UF2 family 0xADA52840.
  T1000-E retains its independent 0x27000..0xD4000 bounds.

## Changed source and tooling

variants/heltec_t114/T114Board.h
src/helpers/CommonCLI.h and CommonCLI.cpp
examples/simple_repeater/MyMesh.cpp
examples/companion_radio/MyMesh.h, MyMesh.cpp and new CustomCLI.cpp
build-station.sh
scripts/verify-station-build.py
scripts/publish-station-release.py
tests/position_button/run.sh, stubs/MyMesh.h, test_build.py, test_publish.py,
new test_cli.cpp, test_nordic_layout.py, test_t114_led.py
docs/custom-cli.md and this report.

## Verification

Passed:
- Existing position state, firmware integration and local chat help host tests.
- New CLI host tests: LED persistence, destination/scope binding, ambiguous
  destination rejection, room selection, enable/disable, reboot and response size.
- Real T114 board-header TX callbacks with simulated GPIO: polarity, default,
  immediate disable and re-enable.
- OLED host display regression test with undefined-behavior sanitizer.
- Three-target packaging using simulated compilers and real packaging tools.
  Validates filenames, manifests, checksums, update failure and empty release
  output after failure of either Nordic target.
- Nordic UF2 layout and protected-address rejection tests.
- Mocked release preflight, draft/upload verification, failure handling and
  invalid-checksum rejection. No GitHub release was created by these tests.
- Shell syntax and git diff whitespace checks.

Incomplete:
- All three real PlatformIO builds were attempted. V4 and T1000-E timed out
  waiting for platform downloads; T114 installed nordicnrf52 but stalled on
  toolchain-gccarmnoneeabi, and was interrupted. No compile success, size result,
  or flashable binary is claimed.
- Real preference persistence, USB/remote CLI, physical LED, encrypted/scoped
  radio delivery, GPS, buzzer and button gestures require hardware verification.
  Remote authentication was inspected in source; not exercised on hardware.

Artifacts: no real firmware images generated. Packaging fixtures were temporary.
See custom-cli.md for configuration and build instructions.
