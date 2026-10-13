# Building firmware updates for meshcoreStation

Target: Heltec V4 OLED USB companion, physical flash 16 MB, existing partitions
occupying the first 4 MB. This configuration matches the device dump verified
on 2026-10-09. It does not migrate or enlarge partitions.

The target explicitly uses `variants/heltec_v4/partitions_station.csv`:

| Name | Offset | Size |
| --- | --- | --- |
| nvs | 0x9000 | 0x5000 |
| otadata | 0xe000 | 0x2000 |
| app0 | 0x10000 | 0x140000 |
| app1 | 0x150000 | 0x140000 |
| spiffs | 0x290000 | 0x160000 |
| coredump | 0x3f0000 | 0x10000 |

Physical flash size remains 16 MB. The application limit is 1,310,720 bytes.
Other Heltec build targets keep their existing partition settings.

## Build on Debian in the PlatformIO terminal

```sh
git switch feature/usb-oled-control
git pull --ff-only
pio run -e heltec_v4_companion_radio_usb -t clean
pio run -e heltec_v4_companion_radio_usb
```

Check that local `platformio.local.ini` overrides do not replace these settings.
Only publish after a successful build and packaging step. It produces:

```text
.pio/build/heltec_v4_companion_radio_usb/release/heltec_v4_companion_radio_usb.bin
.pio/build/heltec_v4_companion_radio_usb/release/heltec_v4_companion_radio_usb.json
```

Upload these two files together to a new GitHub release targeting the commit
actually built. Use a new release tag rather than replacing a previously
installed version. Do not upload an older copied binary or a merged binary in
place of the application image.

The post-build script preserves the schema-1 manifest used by meshcoreStation:
`schema`, `target`, `chip`, `offset`, `image`, `sha256`, `partition_sha256`.
It derives the application offset from the build's partition binary and hashes
both the actual application and the actual 3072-byte partition binary. The
partition SHA-256 must be:

```text
148b959cbff1c38aa8e1d5c0ba9d612c54997b945e56a63f41223eef650653a1
```

The packaging script rejects a different table or an application that exceeds
the slot, and removes any previous release pair before validating. It does not
modify or patch the compiled application. If an incremental build is already
up to date, packaging can also be run explicitly after a successful build:

```sh
python3 scripts/station_release.py .pio/build/heltec_v4_companion_radio_usb
```

Use meshcoreStation's normal application-update flow and partition checks.
This build matches the supplied table; do not bypass a mismatch reported for a
different device or a later changed layout. No erase or filesystem relocation
is required. Actual firmware compilation and on-device validation must still
succeed before treating a release as verified.

## One-command build beside MeshCore

Install the wrapper once (or repeat to update it), from the directory that
contains your `MeshCore` checkout:

```sh
bash MeshCore/scripts/build-station.sh --install
./build-station.sh
```

The script lives one folder above MeshCore and can be called from any working
directory. It now clean-builds **both** custom firmware environments:

| Device | Environment | Files copied beside the script |
| --- | --- | --- |
| Heltec V4 OLED USB | `heltec_v4_companion_radio_usb` | `.bin` + `.json` |
| T1000-E BLE position button | `t1000e_companion_radio_ble_position` | `.uf2` + `.uf2.sha256` |

The full filenames start with the corresponding environment name. Upload the
Heltec pair together for meshcoreStation. The T1000-E UF2 and checksum can be
attached to the same GitHub release, but installation uses the T1000-E UF2
bootloader; the Heltec/esptool update flow cannot install it. See
[T1000-E configuration and behavior](t1000-position-button.md).

The wrapper finds `pio` on PATH or at `~/.platformio/penv/bin/pio`; alternatively
set `STATION_PIO` to the full executable path. It packages and validates the
Heltec application and manifest, converts the T1000-E HEX into UF2, checks the
nRF52840 family ID and application address bounds, and creates a SHA-256 checksum.
All four files are staged until both builds and validation succeed. Previous
outputs are removed at build start so failure cannot leave stale release files.
Builds sharing the output directory are locked; unrelated files are untouched.

It does not pull Git, change branches, upload releases, or flash either device.
Pull the desired branch first. **Repeat `--install` after pulling this update**
to replace the older single-target wrapper:

```sh
cd MeshCore
git switch feature/usb-oled-control
git pull --ff-only
bash scripts/build-station.sh --install
../build-station.sh
```

The tracked wrapper remains in `scripts/build-station.sh`. The first dual build
may download the Nordic toolchain in addition to the ESP32 tools.
