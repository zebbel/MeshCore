#!/usr/bin/env bash
# Build both custom companions into MeshCore/release: bash build-station.sh
set -euo pipefail

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ ! -f "$repo_dir/platformio.ini" || ! -f "$repo_dir/scripts/station_release.py" ]]; then
  printf 'Run the build-station.sh inside your MeshCore checkout.\n' >&2
  exit 1
fi
build_args=("$@")
publish_release=false
if [[ $# == 1 && "$1" == '--release' ]]; then
  publish_release=true
  shift
fi
if [[ $# != 0 ]]; then
  printf 'Usage: bash %s [--release]\nBuild only by default; --release also publishes to GitHub.\n' "$0" >&2
  exit 2
fi
# Keep pull/restart in one parsed function: Git can replace this script itself.
update_source() {
  cd -- "$repo_dir"
  local previous_head
  previous_head="$(git rev-parse HEAD)"
  printf 'Updating source with git pull --ff-only\n'
  git pull --ff-only || return $?
  if [[ "$(git rev-parse HEAD)" != "$previous_head" ]]; then
    # Run the newly pulled script, retaining --release when requested.
    exec bash "$repo_dir/build-station.sh" "${build_args[@]}"
  fi
}
update_source

output_dir="$repo_dir/release"
cd -- "$repo_dir"
# Match upstream build.sh: environment name, firmware version and short Git SHA.
version="${FIRMWARE_VERSION:-$(python3 - <<'VERSION'
import pathlib, re
source = pathlib.Path('examples/companion_radio/MyMesh.h').read_text()
print(re.search(r'^#define FIRMWARE_VERSION "([^"\n]+)"', source, re.M)[1])
VERSION
)}"
if [[ ! "$version" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]]; then
  printf 'FIRMWARE_VERSION must be a filename-safe version string.\n' >&2
  exit 2
fi
station_version="$(cat "$repo_dir/meshcoreStation.version")"
if [[ ! "$station_version" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  printf 'meshcoreStation.version must contain a version such as v1.0.0.\n' >&2
  exit 2
fi
commit_sha="$(git rev-parse HEAD)"
commit_hash="$(git rev-parse --short HEAD)"
if $publish_release; then
  if [[ -n "${PLATFORMIO_BUILD_FLAGS:-}" || -f "$repo_dir/platformio.local.ini" ]]; then
    printf 'Release builds require no extra PLATFORMIO_BUILD_FLAGS or platformio.local.ini overrides.\n' >&2
    exit 1
  fi
  python3 "$repo_dir/scripts/publish-station-release.py" --check "$commit_sha" "$station_version"
fi
suffix="$version-$commit_hash-meshcoreStation-$station_version"
# Companion DEVICE_INFO has only 20 bytes for its version, including NUL.
reported_version="$version-MS${station_version#v}"
if [[ ${#reported_version} -gt 19 ]]; then
  printf 'Combined firmware version exceeds the companion protocol limit.\n' >&2
  exit 2
fi
build_date="$(date '+%d-%b-%Y')"
export PLATFORMIO_BUILD_FLAGS="${PLATFORMIO_BUILD_FLAGS:-} -DFIRMWARE_BUILD_DATE='\"${build_date}\"' -DFIRMWARE_VERSION='\"${reported_version}\"'"

if [[ -n "${STATION_PIO:-}" ]]; then
  pio_cmd="$STATION_PIO"
elif command -v pio >/dev/null 2>&1; then
  pio_cmd="$(command -v pio)"
elif [[ -x "$HOME/.platformio/penv/bin/pio" ]]; then
  pio_cmd="$HOME/.platformio/penv/bin/pio"
else
  printf 'PlatformIO not found. Use the VS Code PlatformIO terminal or set STATION_PIO to its executable.\n' >&2
  exit 1
fi
command -v python3 >/dev/null
command -v flock >/dev/null
# Lock the stable repository directory: release/ is deleted below.
exec 9<"$repo_dir"
flock -n 9 || { printf 'Another Station build is running here.\n' >&2; exit 1; }

heltec=heltec_v4_companion_radio_usb
t1000=t1000e_companion_radio_ble_position
heltec_name="$heltec-$suffix"
merged_name="$heltec-$version-$commit_hash-merged-meshcoreStation-$station_version"
t1000_name="t1000e_companion_radio_ble-$suffix"
outputs=("$heltec_name.bin" "$heltec_name.json" "$merged_name.bin"
         "$t1000_name.uf2" "$t1000_name.zip" "$t1000_name.uf2.sha256" "$t1000_name.zip.sha256")
# Start each build with a completely empty release directory.
rm -rf -- "$output_dir"
mkdir -p -- "$output_dir"
staging="$(mktemp -d "$output_dir/.station-build.XXXXXX")"
trap 'rm -rf -- "$staging"' EXIT
cd -- "$repo_dir"

printf 'Building Heltec V4 OLED USB companion\n'
"$pio_cmd" run -e "$heltec" -t clean
"$pio_cmd" run -e "$heltec"
# Use a relative path: upstream's merge command does not quote its output path.
export MERGED_BIN_PATH=".pio/build/$heltec/firmware-merged.bin"
"$pio_cmd" run -e "$heltec" -t mergebin
cp -- "$repo_dir/.pio/build/$heltec/firmware-merged.bin" "$staging/$heltec-merged.bin"
python3 "$repo_dir/scripts/station_release.py" "$repo_dir/.pio/build/$heltec"
cp -- "$repo_dir/.pio/build/$heltec/release/$heltec.bin" "$staging/$heltec.bin"
cp -- "$repo_dir/.pio/build/$heltec/release/$heltec.json" "$staging/$heltec.json"

printf 'Building T1000-E BLE position companion\n'
"$pio_cmd" run -e "$t1000" -t clean
# Explicit conversion below avoids an inherited UF2_FILE_PATH redirect and
# ensures converter failures propagate to this script.
"$pio_cmd" run -e "$t1000"
python3 "$repo_dir/bin/uf2conv/uf2conv.py" -f 0xADA52840 -c \
  "$repo_dir/.pio/build/$t1000/firmware.hex" -o "$staging/$t1000.uf2"
cp -- "$repo_dir/.pio/build/$t1000/firmware.zip" "$staging/$t1000.zip"
python3 "$repo_dir/scripts/verify-station-build.py" "$staging" "$version" "$commit_hash" "$station_version"

for file in "${outputs[@]}"; do mv -- "$staging/$file" "$output_dir/$file"; done
printf '\nBoth builds complete. Release files:\n'
for file in "${outputs[@]}"; do printf '%s\n' "$output_dir/$file"; done
printf '\nHeltec: .bin + .json for meshcoreStation. T1000-E: .uf2 or DFU .zip. Merged Heltec image is for full flashing.\n'

if $publish_release; then
  assets=()
  for file in "${outputs[@]}"; do assets+=("$output_dir/$file"); done
  python3 "$repo_dir/scripts/publish-station-release.py" "$commit_sha" "$station_version" "${assets[@]}"
fi
