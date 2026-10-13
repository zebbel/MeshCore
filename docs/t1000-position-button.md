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

## Configure from the Android app (v1.0.1 and later)

First install the custom T1000-E firmware built from meshcoreStation v1.0.1 or
later. The v1.0.0 firmware does not intercept these commands.

1. Connect the app to the T1000-E over Bluetooth.
2. Create/import your private channel on the device if needed.
3. Open that private channel's chat and send this exact command:

```text
/position-button on
```

The firmware saves that channel as the destination and enables the button in one
operation. A successful command returns the normal app success response and
plays a short acknowledgement beep if the buzzer is enabled. The app may show
the command as an outgoing message in its own history; it is **not transmitted
on LoRa** and other channel members do not receive it. No special Android settings
screen or computer Bluetooth is required. Settings survive restarting the device.

To disable the feature, send this in a channel chat on the same connected device:

```text
/position-button off
```

It disables sending and cancels a pending position request while retaining the
saved destination. To change destination, send `/position-button on` in the new
private channel. The firmware selects the actual channel slot, so duplicate names
are not ambiguous in this workflow. Received radio messages cannot configure the
button: interception happens only on local app-to-companion send commands.
Direct messages are not a configuration interface.

Commands are lowercase. Leading/trailing spaces and line breaks are accepted.
Invalid commands beginning with `/position-button` are rejected locally rather
than transmitted. Public/default-key, empty-key and hashtag channels cannot be
selected. Save failures or invalid destinations return an app error and leave
the previous configuration intact. No local chat reply is synthesized.

Press once to queue `position <latitude>, <longitude>` using the currently
available valid GPS position. It does not wait for a new fix or switch GPS on.
If no valid position is available, no message is sent (`no_fix`).

### Four-second LED blink (v1.0.2 and later)

Send `/led off` in any channel chat on the connected T1000-E to disable the
periodic status LED blink, or `/led on` to enable it again. The default is on.
The setting survives restarts and is independent of the position-button setting.
Disabling it also suppresses the longer periodic unread-message blink. The
shutdown indication remains unchanged.

These commands are handled locally and never transmitted over LoRa, including
invalid commands beginning with `/led`. They return the normal app success/error
response; no chat reply is generated. Direct messages are not supported for
configuration. Rebuild and flash this update before using the commands.

### Alternative: custom-variable interface

The firmware also exposes these variables through the companion protocol:

| Variable | Value |
| --- | --- |
| `position_channel` | Exact existing private channel name (case-sensitive) |
| `position_button` | `1` enables; `0` disables (default) |
| `position_status` | Read-only state, e.g. `waiting`, `queued`, `no_fix` |

Using this interface, set the channel first and then enable. Name-based selection
requires a unique name. Channel names must be at most 31 UTF-8 bytes and contain
no control characters, commas or colons. Empty channel name clears the selection
and disables sending. Use a genuinely private channel with a random shared key;
the firmware cannot know whether someone has otherwise published its key.

The user's Android app does not expose Sensor Settings/custom variables. Use the
chat commands above, or the following helper on a computer with Bluetooth.

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

The request uses the GPS provider's current valid position, even if its timestamp
has not advanced since the button press. GPS stays in its existing on/off state.
There is no acquisition wait or timeout. The chat message uses the current device
clock, not the cached fix timestamp. Requests already pending are ignored, and
successful sends have a 10-second cooldown.

`queued` means accepted into the local transmit queue, not confirmed delivery.
An acknowledgement chirp accompanies this event if the buzzer is enabled.
No message is sent without a valid position (`no_fix`), with a missing channel
(`channel_missing`), or on transmit allocation failure (`send_failed`).
`storage_error` indicates settings were not saved. The last status is available
through the custom settings/helper and is not retained across reboots. The app
may not show these device-originated messages as sent items in its own history.

## Build and install

The root-level `build-station.sh` builds both the Heltec and this target into
`release/`, with upstream-style versioned filenames ending in `-meshcoreStation-<custom version>`; see
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
