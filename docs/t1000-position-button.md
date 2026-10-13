# T1000-E private-channel position button

Custom environment: `t1000e_companion_radio_ble_position`. This is the BLE
companion, retaining the normal app connection and other button gestures.
The standard T1000-E environments are unchanged.

A single short press sends one ordinary encrypted channel text message:

```text
position 52.123456, 13.123456
```

Latitude comes first, longitude second, each with six decimal places. Negative
coordinates retain their sign. Decimal precision does not imply GPS accuracy.
The phone need not be connected when pressing the button.

## Configure

Create/import your private channel on the device using your companion app first.
The custom firmware exposes these existing-protocol custom variables:

| Variable | Value |
| --- | --- |
| `position_channel` | Exact existing private channel name (case-sensitive) |
| `position_button` | `1` enables; `0` disables (default) |
| `position_status` | Read-only state, e.g. `waiting`, `queued`, `no_fix` |

Set the channel first, then enable the button. Both settings survive reboot.
Channel names must be unique, at most 31 UTF-8 bytes, and contain no control
characters, commas or colons. Empty channel name clears the selection and disables
the feature. Public/default-key, empty-key and hashtag channels are rejected.
Use a genuinely private channel with a random shared key; the firmware cannot
know whether someone has otherwise published a channel key.

If your Android app exposes a generic Sensor Settings/custom-variable editor,
these are configured there. The menu/support in your installed app is still
unverified; no Android app modification has been made in this repository.

### BLE fallback on Debian or Raspberry Pi

Disconnect the Android app first. Use a computer with Bluetooth enabled:

```sh
python3 -m venv ~/.venvs/meshcore-position
~/.venvs/meshcore-position/bin/pip install meshcore==2.3.15

# Replace the example address with your T1000-E Bluetooth address.
~/.venvs/meshcore-position/bin/python MeshCore/scripts/configure-position.py \
  --address AA:BB:CC:DD:EE:FF --ask-pin --list

~/.venvs/meshcore-position/bin/python MeshCore/scripts/configure-position.py \
  --address AA:BB:CC:DD:EE:FF --channel 'Family' --enable

# Read current configuration and last result:
~/.venvs/meshcore-position/bin/python MeshCore/scripts/configure-position.py \
  --address AA:BB:CC:DD:EE:FF
```

Use `--disable` to stop button sending. Add `--ask-pin` when pairing requires a
PIN; it is prompted rather than placed in shell history. The helper never prints
channel keys. It only selects an existing channel and does not create channels
or send radio messages. BLE pairing and actual device operation require testing
with your T1000-E.

The firmware binds the destination to a SHA-256 fingerprint of the stored channel
key. Moving/renaming a channel does not redirect messages. The setting displays
the name used when selected; select again to update that label. Removing the
channel or changing its key prevents sending until the destination is configured
again. There is no fallback to Public. Changing configuration cancels an in-flight
GPS request. Re-importing the same key restores access to the same destination.

## Button behavior

- Single press: start one position request, when enabled/configured.
- Double: existing advert function.
- Triple: existing buzzer on/off toggle.
- Four presses: existing GPS on/off toggle.
- Long: existing shutdown / startup CLI-rescue function.

The request powers GPS on if needed and waits for advancing valid GPS time after
the press (up to 120 seconds), so an old cached fix is not transmitted. GPS returns
to off afterwards if the request turned it on. Turning GPS off during a request
cancels it. Normal radio/BLE processing continues during the wait. Presses while
waiting are ignored, and successful sends have a 10-second cooldown.

`queued` means accepted into the local transmit queue, not confirmed delivery.
An acknowledgement chirp accompanies this event if the buzzer is enabled.
No message is sent on timeout (`no_fix`), missing channel (`channel_missing`),
GPS cancellation (`gps_off`) or transmit allocation failure (`send_failed`).
`storage_error` indicates settings were not saved. The last status is available
through the custom settings/helper and is not retained across reboots. The app
may not show these device-originated messages as sent items in its own history.

## Build and install

The shared `build-station.sh` builds both the Heltec and this target; see
[the build instructions](station-firmware-release.md). For only this firmware:

```sh
pio run -e t1000e_companion_radio_ble_position
pio run -e t1000e_companion_radio_ble_position -t create_uf2
```

Use the T1000-E's existing compatible UF2 bootloader to install the generated
UF2. This is an nRF52840 image, not an ESP32 image: it cannot be installed using
esptool or the Heltec application-update manifest. MeshcoreStation's T1000-E
installation support is outside this firmware change. No bootloader or SoftDevice
is included in the application UF2.

## Host verification

```sh
bash tests/position_button/run.sh
python3 tests/position_button/test_build.py
```

The C++ tests execute the actual position-button implementation against simulated
GPS, channel storage and filesystem services, plus coordinate/time edge cases.
The build-wrapper test uses simulated compilers and the real packaging/conversion
scripts. These tests do not replace a PlatformIO build or on-device BLE/GPS tests.
