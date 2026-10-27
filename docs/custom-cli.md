# Custom firmware CLI and three-target builds

## Heltec T114 display repeater

Use the existing authenticated admin remote CLI or USB serial CLI:

- `set led on` / `set led off`: enable/disable GPIO 35 active-low TX indication.
- `get led`: read the saved setting.
- `?`: custom command help.

Enabled by default. Saved as `tx_led` in the existing JSON preferences.
Disabling immediately drives the LED high. Radio operations are unchanged.

## T1000-E position companion

Use the companion CMD_RUN_CLI_COMMAND interface (or existing serial CLI):

- `set led on` / `set led off`, `get led`: periodic status blink.
- `set position-button on` / `set position-button off`, `get position-button`.
- `get position-status`: last result.
- `get position-target`: saved destination type/name and transport scope.
- `?` and `? position`: custom help.
- `set position-target channel|room none|<32hex-scope> <exact name>`:
  atomically validate, bind, persist, and enable a destination with explicit scope.
  Example: `set position-target channel 0102030405060708090a0b0c0d0e0f10 Family`.
  `none` explicitly chooses unscoped transport. A hexadecimal value is the
  16-byte transport scope key, not the private channel encryption key.

Channel/room names must match exactly and uniquely. Private channels must already
exist; room contacts must be existing room servers. Invalid selections preserve
the previous configuration. Destination selection enables the button; disable it
after selection if desired. Later on/off commands retain the saved scope.
Existing PB03 files, chat commands, custom variables, stored coordinates and
confirmation beeps remain in use. Channel keys are never printed.

## Building

Run `./build-station.sh` to update sources using fast-forward pull and build:

1. `heltec_v4_companion_radio_usb`
2. `t1000e_companion_radio_ble_position`
3. `Heltec_t114_repeater` (display)

The script clears release/, stages and validates all builds, then exposes eleven
versioned artifacts. The original seven files remain compatible; T114 adds UF2,
Nordic DFU ZIP, and SHA-256 files for both. `--release` retains clean-source,
exact pushed-commit, existing-release, draft-upload and size checks and verifies
Nordic checksums before uploading.

T114 uses nRF52840 S140 6.1.1 (FWID 0x00B6), the existing
`boards/nrf52840_s140_v6.ld` application range 0x26000..0xED000, and
board-defined bootloader settings at 0xFF000. UF2 validation requires nRF52840
family 0xADA52840 and excludes bootloader/filesystem addresses.
T1000-E retains its separate 0x27000..0xD4000 application bounds.
These are application updates; they do not install a replacement bootloader.

Automated tests: `bash tests/position_button/run.sh`,
`python3 tests/position_button/test_build.py`,
`python3 tests/position_button/test_publish.py`, and
`python3 tests/position_button/test_nordic_layout.py`.
Hardware acceptance still requires checking LED polarity/persistence, remote
admin and USB commands, scoped/encrypted delivery, stored GPS, beep, gestures,
and reboot behavior on real devices.
