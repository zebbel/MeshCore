# T1000-E channel/room-server position button

Custom environment: `t1000e_companion_radio_ble_position`. This is the BLE
companion, retaining the normal app connection and other button gestures.
The standard T1000-E environments are unchanged.

A single short press sends one ordinary encrypted channel or room-server text message:

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

### Select a room-server chat as the destination

You can also open a room-server **contact** in the Android app and send
`/position-button on` in its chat. The firmware now intercepts this locally,
stores the room's full public key, and sends future button positions as encrypted
direct messages to that server. Room-server posts and normal direct messages are
not interpreted as configuration commands. The room must remain available as a
room-server contact. Removing/changing its identity yields `room_missing` until
you select it again. Commands in chats with ordinary contacts are not intercepted.

To disable the feature, send this in a channel chat on the same connected device:

```text
/position-button off
```

It disables sending and cancels a pending position request while retaining the
saved destination. To change destination, send `/position-button on` in the new
private channel or room-server chat. For channels, the firmware selects the slot, so duplicate names
are not ambiguous in this workflow. Received radio messages cannot configure the
button: interception happens only on local app-to-companion send commands.
Direct messages to ordinary contacts are not a configuration interface.

Commands are lowercase. Leading/trailing spaces and line breaks are accepted.
Invalid commands beginning with `/position-button` are rejected locally rather
than transmitted. Public/default-key, empty-key and hashtag channels cannot be
selected. Save failures or invalid destinations return an app error and leave
the previous configuration intact. No local chat reply is synthesized.

### Local command list (`/?`)

Send `/?` in a channel or room-server chat to display the available
meshcoreStation firmware commands in the same Android chat. The companion
queues a local help message using the standard message-sync protocol; it does
not transmit the command or its response over LoRa. Help works only while the
app is connected to this companion, and does not affect the saved destination.
A full offline message queue causes the help request to fail rather than
replace received radio messages. Ordinary contact chats are not intercepted.

In v1.0.3 and later, press once to queue `position <latitude>, <longitude>` using the currently
stored device position used for adverts. It does not wait for a new fix or switch
GPS on, and does not require the receiver to report a valid fix. The stored
position may be older or manually configured. Unset (0,0), non-finite or
out-of-range coordinates are not sent (`no_fix`).

### Saved transport scope (v1.0.4 and later)

Choose the intended channel scope in Android, then send `/position-button on`
in that channel. The firmware captures the effective scope supplied by the app
for this command (or the device default when no override is active), and saves
it together with the destination. Button sends use this saved scope after
restarts, independently of later app overrides or default-scope changes. An
explicitly unscoped selection stays unscoped. Normal app send settings are
restored after queueing the button message, including on queue failure.
Room-server destinations capture the app's effective scope the same way; it is
used if a room message must be flooded, while known direct routes use the usual
MeshCore direct routing.

Stored PB02 private-channel settings are migrated in memory to PB03 on reboot.

The firmware does not receive a persistent per-channel scope table from the app.
If you change the channel scope, send `/position-button on` again in that chat.
After upgrading from older firmware, repeat this command once: old position
settings without a scope are disabled (`configure_required`). LED settings remain
unchanged. With the BLE helper, selecting `position_channel` captures the current
device scope; enabling the button later preserves that captured scope.

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
| `position_channel` | Selected private channel name (empty when using a room) |
| `position_room` | Read-only name of selected room-server contact (empty when using a channel) |
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

The request uses `sensors.node_lat` / `sensors.node_lon`, the same stored position
used for adverts, regardless of the GPS receiver's current fix status. GPS stays in its existing on/off state.
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
