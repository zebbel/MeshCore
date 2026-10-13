#!/usr/bin/env bash
# Install beside MeshCore: bash MeshCore/scripts/build-station.sh --install
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ -f "$script_dir/MeshCore/platformio.ini" ]]; then
  repo_dir="$script_dir/MeshCore"
  output_dir="$script_dir"
elif [[ -f "$script_dir/../platformio.ini" && -f "$script_dir/station_release.py" ]]; then
  repo_dir="$(cd -- "$script_dir/.." && pwd)"
  output_dir="$(cd -- "$repo_dir/.." && pwd)"
else
  printf 'Cannot find MeshCore. Put this script one folder above MeshCore.\n' >&2
  exit 1
fi

if [[ "${1:-}" == '--install' && $# == 1 ]]; then
  destination="$output_dir/build-station.sh"
  if [[ "$script_dir/build-station.sh" != "$destination" ]]; then
    install -m 755 -- "${BASH_SOURCE[0]}" "$destination"
  else
    chmod +x -- "$destination"
  fi
  printf 'Installed: %s\nRun: %s\n' "$destination" "$destination"
  exit 0
fi
if [[ $# != 0 ]]; then
  printf 'Usage: %s [--install]\n' "$0" >&2
  exit 2
fi

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
# Lock the output directory without leaving a lock file beside the release.
exec 9<"$output_dir"
flock -n 9 || { printf 'Another Station build is running here.\n' >&2; exit 1; }

heltec=heltec_v4_companion_radio_usb
t1000=t1000e_companion_radio_ble_position
# Remove previous deliverables so a failed build cannot look like a new release.
outputs=("$heltec.bin" "$heltec.json" "$t1000.uf2" "$t1000.uf2.sha256")
for file in "${outputs[@]}"; do rm -f -- "$output_dir/$file"; done
staging="$(mktemp -d "$output_dir/.station-build.XXXXXX")"
trap 'rm -rf -- "$staging"' EXIT
cd -- "$repo_dir"

printf 'Building Heltec V4 OLED USB companion\n'
"$pio_cmd" run -e "$heltec" -t clean
"$pio_cmd" run -e "$heltec"
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
python3 "$repo_dir/scripts/verify-station-build.py" "$staging"

for file in "${outputs[@]}"; do mv -- "$staging/$file" "$output_dir/$file"; done
printf '\nBoth builds complete. Release files:\n'
for file in "${outputs[@]}"; do printf '%s\n' "$output_dir/$file"; done
printf '\nHeltec: .bin + .json for meshcoreStation. T1000-E: .uf2 for its UF2 bootloader.\n'
