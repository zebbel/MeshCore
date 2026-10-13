# Building firmware updates for meshcoreStation

The `meshcoreStation` branch contains all custom companion firmware: Heltec V4
OLED USB and T1000-E BLE position-button support, plus the shared root-level `build-station.sh`.

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
git fetch origin
git switch meshcoreStation
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

## One-command build inside MeshCore

The build entry point is `MeshCore/build-station.sh`. No installation/copy step
is needed. Run it from your checkout:

```sh
git fetch origin
git switch meshcoreStation
git pull --ff-only
bash build-station.sh
```

It creates `MeshCore/release/` and puts both custom firmware releases there. It
also works when invoked by absolute path from another working directory.
The previously installed script above MeshCore is obsolete; use this one now.

Names follow upstream `build.sh`: `<target>-<version>-<short Git SHA>`, followed
by `-meshcoreStation-<custom version>` before the extension. The T1000-E filename uses upstream's
`t1000e_companion_radio_ble` name; the build environment remains
`t1000e_companion_radio_ble_position` to enable our custom feature.

For example (the actual SHA comes from your checkout):

```text
release/heltec_v4_companion_radio_usb-v1.17.1-abcdef12-meshcoreStation-v1.0.0.bin
release/heltec_v4_companion_radio_usb-v1.17.1-abcdef12-meshcoreStation-v1.0.0.json
release/heltec_v4_companion_radio_usb-v1.17.1-abcdef12-merged-meshcoreStation-v1.0.0.bin
release/t1000e_companion_radio_ble-v1.17.1-abcdef12-meshcoreStation-v1.0.0.uf2
release/t1000e_companion_radio_ble-v1.17.1-abcdef12-meshcoreStation-v1.0.0.zip
release/t1000e_companion_radio_ble-v1.17.1-abcdef12-meshcoreStation-v1.0.0.uf2.sha256
release/t1000e_companion_radio_ble-v1.17.1-abcdef12-meshcoreStation-v1.0.0.zip.sha256
```

The Heltec application `.bin` and matching `.json` are the application-update
pair for meshcoreStation. The manifest's `image` references the renamed file;
its device `target`, partition hash and image hash remain unchanged. Downloaders
must use these versioned asset names/the manifest's `image`, not the previous
fixed filename. The separate `-merged-meshcoreStation-v1.0.0.bin` is a full-flash image
including bootloader and partition table, not an application-update image.

The T1000-E outputs match upstream formats: UF2 for its compatible UF2 bootloader
and ZIP for Nordic DFU. They are not esptool images. See
[T1000-E configuration and behavior](t1000-position-button.md).

By default the firmware version is read from the companion source. Set
`FIRMWARE_VERSION` to override it, just as with upstream's build script:

```sh
FIRMWARE_VERSION=v1.17.1 bash build-station.sh
```

The custom version is tracked separately in `meshcoreStation.version`, initially
`v1.0.0`. Increment it for each new custom release (for example `v1.0.1` for a fix,
`v1.1.0` for a feature, `v2.0.0` for a breaking change), then commit and push it.
It does not change the upstream MeshCore version. The manifest also records
`meshcoreStation_version`, `meshcore_version` and the short Git `commit`.

The companion protocol only has 20 bytes for its version field, so the device
reports a compact combined version, e.g. `v1.17.1-MS1.0.0`. The full Git SHA is
recorded in release notes and its short form in filenames. Build date uses
upstream's date format.

The wrapper finds `pio` on PATH or at `~/.platformio/penv/bin/pio`; alternatively
set `STATION_PIO` to its full executable path. It clean-builds both environments,
validates the Heltec application/partitions and merged image, validates the UF2
family/address bounds and DFU archive, updates names/manifests, and writes
checksums. All files are staged until both builds and validation succeed.

Previous files for the same version/SHA are removed when building starts; other
versions and unrelated files are retained. Builds in the same release directory
are locked. Generated `release/` files are ignored by Git. By default it does not pull Git,
change branches, upload releases or flash either device.

## Build and publish a GitHub release automatically

Install GitHub CLI on Debian and sign in once with an account that can write to
`zebbel/MeshCore`:

```sh
sudo apt install gh
gh auth login
```

Then use:

```sh
./build-station.sh --release
```

This builds both firmwares and publishes the seven files from that build to
`https://github.com/zebbel/MeshCore/releases/tag/meshcoreStation-v1.0.0` (the tag
uses the custom version file). Plain `./build-station.sh` still only builds.

Release mode requires a clean committed checkout, with that exact commit pushed
to the `meshcoreStation` branch. Local `platformio.local.ini` and extra
`PLATFORMIO_BUILD_FLAGS` overrides are rejected for release builds so an ignored
configuration cannot silently change the published firmware. `FIRMWARE_VERSION`
may still select the upstream version label. GitHub credentials stay in GitHub
CLI's normal credential store, not in the repository.

The script creates a draft release targeting the exact built commit, uploads all
seven files, checks their names and sizes, then publishes it as the latest
release. Build or validation failures never create a release. Upload/verification
failures leave a draft, not a partially populated public update. An existing
release/tag is never overwritten. If an upload fails, inspect and remove the
unfinished draft (and its tag if GitHub created one) before retrying the same
version, or commit a new custom version. Source commits are not pushed by this
script.

Host tests for this flow (simulated compiler and GitHub CLI, no public release):

```sh
python3 tests/position_button/test_build.py
python3 tests/position_button/test_publish.py
```
