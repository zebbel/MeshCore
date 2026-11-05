# XIAO repeater neighbor SNR protocol v1 (unreleased)

Target: `Xiao_nrf52_repeater`, Seeed XIAO nRF52840 + Wio-SX1262. Enabled only by `XIAO_NEIGHBOUR_SNR` in that environment. No companion firmware or MeshcoreStation application changes are required.

## Observations and storage

The existing `putNeighbour` path records direct, zero-hop repeater advertisements excluding shares, and matched, unexpired repeater discovery responses. The core accepts these control responses only at zero hops. Existing latest-SNR/advertisement data and the `0x06` neighbor response format are retained. Discovery is not newly authenticated by this feature: it retains MeshCore's existing tag/type/path checks.

SNR is signed quarter-dB. Sum is signed 64-bit; sample count is unsigned 32-bit. Average in dB is `sum / (4.0 * count)`, never integer-truncated. Empty windows produce no record. Count exhaustion drops subsequent observations, sets record flag 2 and increments the saturating dropped counter. NaN/out-of-range SNR is not accepted into the table.

Windows are half-open 1800-second **boot-relative** intervals, not UTC-aligned half-hours. Window 0 starts when aggregation begins. An observation at exactly 1800 seconds belongs to window 1. Window IDs and reception times use monotonic elapsed seconds, including 32-bit millis wrap handling. A periodic task runs at most once per second without traffic; observation handling advances time before adding a sample. Cleanup runs once per minute. The firmware must continue calling its loop (as normal); delays spanning a whole millis wrap cannot be reconstructed.

At most 50 neighbors are active. New arrivals at capacity displace the least recently heard active neighbor, with slot index breaking ties. Active entries expire after 7200 seconds without a qualifying observation, within the next cleanup interval. Latest-value legacy rows are then cleared; a returning neighbor is admitted normally.

The fixed store has 100 identity slots, each holding one current accumulator and four completed records. Each identity owns its records; there are no borrowed pointers to reused legacy-neighbor slots. An inactive identity is freed only after its current and completed history is empty. Completed records expire 7200 seconds after their window end. Empty periods do not consume history, but old records still expire by age. At most four completed windows per identity survive. At identity-capacity exhaustion, the oldest inactive identity (slot-index tie break) is evicted, counting each lost current/completed record. Active identities are never reclaimed to admit a new identity. All buffers are fixed-size, and no observations are written to flash.

A completed record is immutable. Reads do not advance windows, clear data, reset counters or alter retention. Queries are bounded linear scans of at most 400 completed slots.

## UTC and boot identity

The XIAO fallback RTC starts with a fixed date, which is **not** treated as synchronized. A target-specific RTC wrapper marks time confirmed only after an explicit `setCurrentTime` operation (for example the existing administrator clock commands). A hardware RTC discovered at startup is conservatively unconfirmed until set. The underlying clock, authentication timestamps and routing clock behavior are not changed.

An interval receives UTC fields only if confirmed, plausible UTC remained consistent with monotonic time for the whole interval. Synchronization mid-window, loss of validity, or a forward/backward clock change beyond two seconds invalidates UTC for that interval and sets the discontinuity flag. The next interval uses the corrected anchor. Unconfirmed/invalid UTC is encoded as zero; never interpret it as 1970 or as the default 2024 date. Historical UTC is never rewritten. The validity flag indicates clock continuity after an explicit set, not independent proof of wall-clock accuracy.

At boot the existing initialized CC310 hardware RNG generates a 16-byte session identifier; its last bit is set so all-zero is reserved. If hardware entropy generation fails, aggregate export returns status 5 until reboot and completed aggregates are dropped; legacy active-neighbor tracking continues. This prevents exporting records under an unreliable session identifier. This provides probabilistic boot uniqueness (127 random bits), without per-reception flash wear. Record identity is `(repeater public key, session ID, sequence)`; aggregate identity is `(repeater public key, session ID, neighbor public key, window ID)`. Do not join records across session changes. Record sequences monotonically increase at completion; if the 32-bit sequence is exhausted, new completed records are dropped rather than reusing numbers. Counters saturate at `UINT32_MAX`.

## Transport, permissions and identifiers

Custom request type **0x70**; protocol version **1**. Checked against local and upstream MeshCore repeater, sensor and room-server request definitions on 2026-11-05 (existing identifiers 0x00–0x09). This is a custom identifier, not an upstream reservation; review collisions on future upstream merges. `REQ_TYPE_GET_NEIGHBOURS = 0x06` remains unchanged.

Use the ordinary authenticated binary request. Known guest clients can read aggregates, matching existing read-only neighbor statistics; no administrator privilege is added for this request. Shared-secret verification, contact lookup and strictly increasing request-timestamp replay protection remain in MeshCore. Anonymous requests do not enter this handler. A repeated page read should use a fresh outer request tag/timestamp with the same pagination fields.

Standard companion serial command: `0x32 || repeater_public_key[32] || request[28]` (`CMD_SEND_BINARY_REQ=50`). Companion `sendRequest` supplies the ordinary tag; do not embed a second tag. Response is delivered through standard `PUSH_CODE_BINARY_RESPONSE=0x8C`, reserved byte, matching 4-byte tag, then the response below. No new companion command is introduced.

The repeater prepends the existing 4-byte echoed tag to its response. A data response is 144 bytes including that tag, safely within the 184-byte packet payload after existing encryption/MAC overhead. Diagnostic/error responses can have trailing cipher padding; parse by status/count and ignore trailing padding.

All multi-byte integers below are **little-endian**. Public keys/session IDs are opaque byte arrays. Signed SNR values use two's complement.

### Request (28 bytes, excluding MeshCore tag)

| Offset | Bytes | Field |
|---|---:|---|
| 0 | 1 | Type 0x70 |
| 1 | 1 | Version 1 |
| 2 | 1 | Maximum records: 0 diagnostics only, 1 data (larger values rejected) |
| 3 | 1 | Reserved, must be zero |
| 4 | 16 | Session ID; all zero for a new snapshot |
| 20 | 4 | Snapshot ceiling sequence |
| 24 | 4 | Exclusive `after` sequence cursor |

Initial request: session, ceiling and cursor all zero. Response fixes ceiling to the latest completed sequence. Subsequent pages echo the returned session and ceiling and use the returned cursor. With a nonzero matching session, ceiling zero is a valid fixed empty snapshot. It does not reopen the snapshot.

### Response header (64 bytes, excluding MeshCore tag)

| Offset | Bytes | Field |
|---|---:|---|
| 0 | 1 | Protocol version 1 |
| 1 | 1 | Status: 0 OK, 1 unsupported version, 2 malformed length/parameters, 3 session mismatch, 4 insufficient response capacity, 5 session entropy unavailable |
| 2 | 1 | Record count: 0 or 1 |
| 3 | 1 | More retained records within snapshot after returned cursor: 0/1 |
| 4 | 16 | Current repeater session ID |
| 20 | 4 | Snapshot ceiling |
| 24 | 4 | Returned exclusive continuation cursor (last emitted sequence; unchanged if no data) |
| 28 | 4 | Oldest retained sequence, zero if none |
| 32 | 4 | Newest retained sequence, zero if none |
| 36 | 4 | Expired record count |
| 40 | 4 | Capacity-evicted record count (including incomplete accumulators) |
| 44 | 4 | Dropped observations/completions count (count/sequence/capacity exhaustion) |
| 48 | 2 | Active neighbor count (capacity 50) |
| 50 | 2 | Retained completed record count |
| 52 | 2 | Completed record capacity: 400 |
| 54 | 2 | Identity capacity: 100 |
| 56 | 4 | Oldest retained window ID, zero if none |
| 60 | 4 | Newest retained window ID, zero if none |

On nonzero status there is no record; use only status/session/diagnostics and restart as appropriate. Length/version errors never read absent fields. A snapshot excludes records completed later. New observations cannot modify its records; expiration/eviction can remove them. This is bounded retention, not a pinned snapshot: compare loss counters, cursor gaps and oldest sequence with earlier pages to detect loss. Repeated reads return identical record contents while retained; diagnostics can change. With count zero and `more=0`, traversal is finished. To poll for later windows, open a new snapshot and deduplicate by session/sequence. Requests never acknowledge or delete records.

### Record (76 bytes, after header)

| Offset | Bytes | Field |
|---|---:|---|
| 0 | 32 | Full neighbor public key |
| 32 | 4 | Record sequence |
| 36 | 4 | Boot-relative window ID |
| 40 | 4 | Window start, monotonic seconds since aggregation startup |
| 44 | 4 | Exclusive window end, same units |
| 48 | 4 | UTC start (Unix seconds), zero if invalid |
| 52 | 4 | UTC exclusive end, zero if invalid |
| 56 | 8 | Signed sum, quarter-dB units |
| 64 | 4 | Nonzero sample count |
| 68 | 1 | Signed minimum, quarter-dB |
| 69 | 1 | Signed maximum, quarter-dB |
| 70 | 1 | Flags: bit 0 UTC valid; bit 1 clock discontinuity; bit 2 sample-count overflow |
| 71 | 1 | Reserved zero |
| 72 | 4 | Last qualifying reception, monotonic seconds since aggregation startup |

If UTC-valid, last reception UTC is `UTC_start + last_monotonic - window_monotonic_start`. Otherwise no absolute reception time is claimed.

### Example

Initial data request (hex): `70 01 01 00` followed by 24 zero bytes.

For session `01` followed by 15 zero bytes, neighbor key `02` followed by 31 zero bytes, two samples -3.25 dB and +1.50 dB at monotonic second zero, completing window zero at known UTC 1700001800:

- Header: version=1, status=0, count=1, more=0; ceiling/cursor/oldest/newest sequence=1; counters=0; active=1; retained=1; record capacity=400; identity capacity=100; oldest/newest window=0.
- Record: sequence=1, window=0; monotonic [0,1800); UTC [1700000000,1700001800); sum=-7 (`f9 ff ff ff ff ff ff ff`), count=2 (`02 00 00 00`), minimum=-13 (`f3`), maximum=6 (`06`), flags=1, last=0.
- Average: `-7 / (4 * 2) = -0.875 dB`.

A continuation sends the same session, ceiling `01 00 00 00`, cursor `01 00 00 00`. It returns count=0/more=0. `tests/neighbour_snr/test_snr.cpp` validates these fields against actual encoding.

## Memory and verification status

The aggregation object occupies **20,064 bytes in the host C++ build**, with a compile-time upper bound of 24,000 bytes. This excludes the existing 50-neighbor table and all pre-existing firmware/radio allocations. It is not a measured ARM link result or runtime headroom. Stack use is bounded (individual slot resets, no full-store stack copies). A new build hook checks the resolved linker script before linking; package verification independently checks actual UF2 addresses and family.

Full XIAO compilation currently cannot run because PlatformIO's Nordic platform download is blocked. Thus compiled RAM/flash usage, free runtime heap/stack headroom, real USB/radio operation, repeated boot entropy and timing/performance are **not yet verified**. On hardware, record link size output and heap/stack high-water marks at startup, 50 active neighbors, 100 retained identities, full history, and simultaneous requests/traffic. Do not treat the host object size as evidence of sufficient runtime headroom.
