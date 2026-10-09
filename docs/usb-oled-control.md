# USB OLED control, private extension v1

Target: Heltec V4 OLED, `heltec_v4_companion_radio_usb`.
This fork enables `ENABLE_HOST_DISPLAY=1` on that target only. It is not an
upstream MeshCore protocol allocation. No meshcoreStation changes are included.
Baseline: a366955c. Build and upload using the usual PlatformIO target:

```sh
pio run -e heltec_v4_companion_radio_usb
pio run -e heltec_v4_companion_radio_usb -t upload
```

## Transport and discovery

Use the existing companion USB connection at 115200 baud. Host frames have
`0x3C length_lo length_hi payload...`; device frames have `0x3E` instead of
`0x3C`. The length counts payload bytes only, and is little endian. Keep the
existing stream parser: reads may split frames or contain multiple frames and
normal asynchronous MeshCore notifications may arrive between replies.

Private command/response byte: `0xF0`. It is unused in the baseline command
handler, but is NOT reserved by upstream; check for collisions when rebasing.
The signature and version prevent accidental interpretation of another payload.
Do not alter the standard MeshCore protocol version or device-query response.

Request payload:

| Offset | Field |
| --- | --- |
| 0 | `F0` |
| 1–4 | ASCII `MCOD` (`4D 43 4F 44`) |
| 5 | Extension version: `01` |
| 6 | Request ID, chosen by host, echoed by device |
| 7 | Operation |
| 8 onward | Operation arguments |

Response payload: same first eight bytes, followed by status at offset 8 and
optional INFO fields. Version is always the firmware's extension version.
Malformed short requests echo available ID/operation bytes, defaulting to ID 0
and operation FF if missing. Unknown signatures receive BAD_ARGUMENT.

Probe with INFO before use. Only enable the feature after a matching `F0 MCOD`
v1 response. Standard firmware will normally return its existing unsupported
command error (`01 01`). A timeout or unexpected reply means the extension has
not been confirmed. Send the probe through the same serialized request queue as
other commands; do not open another serial connection or steal responses from
another reader. Allow one display request at a time and match its ID/operation.

## Operations

| Value | Operation | Arguments and behaviour |
| --- | --- | --- |
| 0 | INFO | No arguments. Does not acquire control or renew the timeout. |
| 1 | BEGIN | Timeout in seconds, uint16 LE, 2–300. Acquire display and clear pending scene. Existing visible content remains until SHOW. |
| 2 | TEXT | `x u8, y u8, size u8, text bytes`. Append one pending text item. |
| 3 | CLEAR | No arguments. Clear pending scene only. |
| 4 | SHOW | No arguments. Wake OLED, clear drawing buffer, render pending scene, flush once. Scene remains available for another SHOW. |
| 5 | RELEASE | No arguments. Return to normal home screen. Idempotent. |
| 6 | KEEPALIVE | No arguments. Renew timeout without drawing. |

INFO success adds five bytes at offsets 9–13:
`width=128, height=64, maximum_items=16, maximum_text_bytes=21, active=0/1`.

TEXT uses top-left pixel coordinates and built-in printable ASCII (32–126),
without a terminating NUL. Size 1 uses 6×8 cells; size 2 uses 12×16 cells.
Text must be nonempty and fit completely: `x + length*6*size <= 128`,
`y + 8*size <= 64`. No wrapping, newlines or UTF-8 in v1. Use several TEXT
commands for multiple lines. Items render in submission order with transparent
text backgrounds. For replacement, CLEAR and resubmit the complete scene before
SHOW. Maximum 16 items per scene. Invalid commands leave the scene unchanged.
All request lengths are checked exactly, except TEXT's variable text length.

Statuses (offset 8): 0 OK, 1 BAD_ARGUMENT, 2 BAD_STATE (not acquired),
3 UNSUPPORTED (version/operation), 4 NO_DISPLAY, 5 FULL (16 items already used).
Errors have no additional data. Only INFO success has additional data.

BEGIN, TEXT, CLEAR, SHOW and KEEPALIVE renew the lease when successful.
INFO, errors and malformed commands do not renew it. Timeout is checked before
commands and during the main loop, including across millis() rollover. A USB
cable disconnect cannot be reliably inferred from upstream's serial interface,
so expiry is the recovery mechanism. Suggested host lease: 30 seconds, heartbeat
at most every 10 seconds. On expiry or RELEASE the normal screen is restored
and its usual screen-off timer restarts. Reboot always starts with the normal UI.

While acquired, local screen refresh, alerts, buttons and automatic screen-off
are suppressed. Radio processing, USB commands, sensors and incoming-message
bookkeeping continue. SHOW wakes the display; BEGIN alone does not. No display
content is written to flash. Receiving an acknowledgement means the command was
processed; SHOW acknowledges after the display driver's flush returns, not after
independent verification of the physical panel.

## Byte examples

INFO with request ID 1 (complete USB frame):

```text
3C 08 00 F0 4D 43 4F 44 01 01 00
```

Response, when idle and available:

```text
3E 0E 00 F0 4D 43 4F 44 01 01 00 00 80 40 10 15 00
```

Payloads for a screen showing `Station ready` at (0,0):

```text
F0 4D 43 4F 44 01 02 01 1E 00                         # BEGIN 30 seconds
F0 4D 43 4F 44 01 03 02 00 00 01 53 74 61 74 69 6F 6E 20 72 65 61 64 79
F0 4D 43 4F 44 01 04 04                               # SHOW
F0 4D 43 4F 44 01 05 06                               # KEEPALIVE
F0 4D 43 4F 44 01 06 05                               # RELEASE
```

Each needs its own USB frame wrapper and a matching successful response before
the next command. Retrying TEXT appends another item; after an uncertain reply,
restart with BEGIN or CLEAR and rebuild the whole scene. There is no duplicate
request cache. Existing MeshCore clients can ignore this feature entirely.

## Validation

Host-side parser/state/render tests (no Arduino dependency):

```sh
g++ -std=c++11 -Wall -Wextra -Werror -Wno-unused-parameter \
  -fsanitize=undefined -I. -Isrc \
  tests/usb_oled/test_host_display.cpp -o /tmp/test-host-display
/tmp/test-host-display
```

On hardware, verify INFO, acquire/render/release, expiry after stopping the host,
normal messaging while acquired, and normal UI after reset. Host tests cannot
verify the physical OLED or radio behaviour.
