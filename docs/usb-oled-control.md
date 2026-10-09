# USB OLED control, private extension v1

Target: Heltec V4 OLED, `heltec_v4_companion_radio_usb`.
This fork enables `ENABLE_HOST_DISPLAY=1` on that target only. It is not an
upstream MeshCore protocol allocation. No meshcoreStation changes are included.
Text baseline: a366955c; graphics extension based on 9a679c25. Build and upload using the usual PlatformIO target:

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
`y + 8*size <= 64`. No text wrapping, newlines or UTF-8 in v1. Use several TEXT
commands for multiple lines. Items render in submission order with transparent
text backgrounds. For replacement, CLEAR and resubmit the complete scene before
SHOW. Maximum 16 items per scene. Invalid commands leave the scene unchanged.
All request lengths are checked exactly, except TEXT's variable text length.

Statuses (offset 8): 0 OK, 1 BAD_ARGUMENT, 2 BAD_STATE (not acquired),
3 UNSUPPORTED (version/operation), 4 NO_DISPLAY, 5 FULL (text-item or line-segment capacity reached).
Errors have no additional data. INFO and CAPABILITIES success have additional data.

BEGIN, TEXT, LINE, POLYLINE, CLEAR, SHOW and KEEPALIVE renew the lease when successful.
INFO, CAPABILITIES, errors and malformed commands do not renew it. Timeout is checked before
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


## Graph drawing (graphics revision 1)

The envelope remains version 1. Operations 0–6 and INFO's exact response are
unchanged. After INFO, send CAPABILITIES (operation 7) to detect graph support.
The earlier text-only firmware returns UNSUPPORTED for this operation; hosts
should then display text only. CAPABILITIES works without BEGIN and does not
renew the display lease.

| Operation | Arguments after the 8-byte envelope |
| --- | --- |
| 7 CAPABILITIES | None |
| 8 LINE | `x0, y0, x1, y1`, all uint8 |
| 9 POLYLINE | `point_count u8`, then repeated `x u8, y u8` pairs |

CAPABILITIES success adds bytes at offsets 9–13:
`graphics_revision=1, flags=7, max_segments_lo=0, max_segments_hi=1, max_points=83`.
Flags bit 0 indicates LINE; bit 1 indicates POLYLINE; bit 2 indicates button reporting. Maximum line segments is
uint16 little endian (256), independent of the existing 16 text items.

LINE draws a one-pixel-wide white segment including both endpoints. Identical
endpoints draw a single pixel. POLYLINE joins consecutive points; it accepts
2–83 points and consumes point_count−1 segments. Each coordinate must be inside
x=0–127, y=0–63; coordinates outside the display are rejected, not clipped.
The whole command is validated before appending anything. Commands require an
active lease, renew it on success, and return existing statuses. Invalid
lengths, counts or coordinates return BAD_ARGUMENT; insufficient segment
capacity returns FULL without appending a partial graph.

Drawing is buffered until SHOW. BEGIN, CLEAR, RELEASE and expiry discard both
text and segments. SHOW renders all segments first, then text, and flushes the
OLED once. Text uses transparent backgrounds, so allocate a separate label area
if lines should not pass through labels. The existing visible screen stays
unchanged while building the next scene. There are no separate erasing commands;
CLEAR and redraw for the next update. Adding graphics requires approximately
1 KiB of fixed scene storage; no dynamic allocation or flash writes are used.

A full 128-column graph needs 127 segments, leaving room for axes and grid lines.
For more than 83 points, split the graph into multiple POLYLINE commands and
repeat the previous chunk's final point as the next chunk's first point. Do not
repeat an endpoint between disconnected runs (missing data): start a separate
polyline. Await each reply. An uncertain retry can duplicate segments, so CLEAR
and rebuild the scene after an ambiguous timeout.

### Example: voltage and percent above a 24-hour graph

Suggested pixel layout:

- TEXT at (0,0), size 1: current voltage and percentage, e.g. `3.92V  78%`.
- Plot interior x=1–126, y=18–52; horizontal axis y=53 and vertical axis x=0.
- TEXT at (0,56): `-24h`; TEXT at (108,56): `now`.

The host owns history, aggregation, units and scaling. The firmware receives
pixels only; it does not calculate battery percentage or store 24-hour samples.
For graph bounds left/right/top/bottom and a nonzero value range:

```python
x = left + round((timestamp - window_start) / (24 * 3600) * (right - left))
y = bottom - round((value - minimum) / (maximum - minimum) * (bottom - top))
```

Filter to the time window and clamp resulting coordinates to the plot bounds.
Handle a flat series by choosing a nonzero value range. For percentage the host
can use 0–100; for voltage it must choose and label meaningful bounds. Downsample
to the screen resolution. Use separate runs for data gaps rather than implying
continuous observations. This is integration guidance only; no meshcoreStation
implementation is included here.

Example graph payloads (wrap each in the usual USB header):

```text
F0 4D 43 4F 44 01 10 07                         # capabilities
F0 4D 43 4F 44 01 11 01 1E 00                   # begin, 30 seconds
F0 4D 43 4F 44 01 12 08 00 12 00 35             # vertical axis (0,18) to (0,53)
F0 4D 43 4F 44 01 13 08 00 35 7F 35             # horizontal axis (0,53) to (127,53)
F0 4D 43 4F 44 01 14 09 04 01 2D 2A 26 54 20 7E 18  # four points
F0 4D 43 4F 44 01 15 04                         # show
```

Graph tests cover all line octants, reversed and identical endpoints, exact
pixel output, display bounds, malformed payloads, all-or-nothing validation,
83-point chunks, the 256-segment capacity, and clearing/releasing scenes.

## Button gesture reporting

CAPABILITIES flags (offset 10) now include bit 2 (`0x04`) for button gestures;
LINE and POLYLINE bits remain unchanged. Test individual bits, not equality to
3. The response length and envelope version stay unchanged. This capability
is enabled for the Heltec V4 OLED USB build with its onboard user button.

After BEGIN, send operation 10 (`0x0A`, BUTTON_SUBSCRIBE) with exactly one byte:
1 enables reporting, 0 disables reporting and clears queued events. It uses the
normal 9-byte command response. No active display lease returns BAD_STATE;
invalid lengths/values return BAD_ARGUMENT. Successful subscription commands
renew the lease. Repeating enable is idempotent and keeps queued events.

BEGIN while already acquired and CLEAR preserve the subscription, so replacing
a screen does not disable buttons. RELEASE, lease expiry and reboot disable it
and discard queued events. The host must subscribe again after reacquiring.
Ordinary button activity never renews the lease. No polling command is needed.

Example subscription payload (request ID 0x20):

```text
F0 4D 43 4F 44 01 20 0A 01
```

Button notifications are unsolicited 16-byte payloads within normal `>` USB
frames. They are NOT command responses and contain no status byte:

| Offset | Field |
| --- | --- |
| 0–5 | `F0 4D 43 4F 44 01` (existing MCOD envelope) |
| 6 | Reserved, zero; not a request ID |
| 7 | `80` (button event) |
| 8 | Button ID: 0 = onboard user/PRG button, not reset |
| 9 | Gesture: 1 single click, 2 long press, 3 double click, 4 triple click |
| 10–11 | Event sequence, uint16 little endian |
| 12–15 | Gesture recognition uptime in milliseconds, uint32 little endian |

Sequence starts at 1 after boot, advances for every accepted gesture, wraps at
65536, and does not reset on unsubscribe/reacquire. Uptime wraps at 2^32 ms.
Neither field is a wall-clock timestamp. Host code should reset its tracking on
a new connection/boot and use modulo arithmetic. Sequence gaps can indicate
queue overflow or events discarded at subscription/lease transitions.

An eight-event RAM FIFO drops the oldest item when full, retaining recent user
input. At most one event is sent per firmware loop after normal command
processing, when the serial interface is enabled and not busy. Events remain
queued until writeFrame reports success. This is best-effort delivery without
host acknowledgements; deduplicate sequences if a failed/partial write leads
to a retry. Do not treat the events as replies to outstanding requests.

Gestures use the existing button recognizer (including its multi-click window).
There are no immediate press/release notifications. During host-controlled mode
local screen/button actions remain suppressed, whether subscribed or not;
unsubscribed gestures are discarded. Pending gestures are cancelled when
entering/leaving host mode or changing subscription, to avoid carrying a partial
gesture across modes. On RELEASE/expiry local button behaviour resumes.

meshcoreStation integration (separate project): inspect the capability bit,
BEGIN then subscribe, route operation 0x80 to an event handler before matching
command replies, and choose application actions for the four gestures. Continue
normal display heartbeats. Firmware does not choose pages or station actions.

Host tests cover all four event types, exact wire fields, queue overflow,
retention until successful write consumption, subscription validation, redraw
compatibility, lease cleanup and sequence wrap. Physical button timing and
USB delivery require on-device testing after compilation.
