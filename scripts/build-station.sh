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

target=heltec_v4_companion_radio_usb
build_dir="$repo_dir/.pio/build/$target"
# These two filenames belong to this build; remove old copies before starting.
rm -f -- "$output_dir/$target.bin" "$output_dir/$target.json"
cd -- "$repo_dir"
printf 'Building %s in %s\n' "$target" "$repo_dir"
"$pio_cmd" run -e "$target" -t clean
"$pio_cmd" run -e "$target"
# Also run explicitly: ensures packaging happens even with incremental builds.
python3 "$repo_dir/scripts/station_release.py" "$build_dir"

# Stage and validate both outputs before publishing them beside this script.
staging="$(mktemp -d "$output_dir/.station-build.XXXXXX")"
trap 'rm -rf -- "$staging"' EXIT
cp -- "$build_dir/release/$target.bin" "$staging/$target.bin"
cp -- "$build_dir/release/$target.json" "$staging/$target.json"
python3 - "$staging" "$target" <<'PY'
import hashlib, json, pathlib, sys
folder, target = pathlib.Path(sys.argv[1]), sys.argv[2]
manifest = json.loads((folder / (target + '.json')).read_text())
image = (folder / (target + '.bin')).read_bytes()
if (manifest.get('schema') != 1 or manifest.get('target') != target
        or manifest.get('image') != target + '.bin'
        or manifest.get('chip') != 'esp32s3'
        or manifest.get('sha256') != hashlib.sha256(image).hexdigest()):
    raise SystemExit('Release verification failed; nothing copied.')
PY
mv -- "$staging/$target.bin" "$output_dir/$target.bin"
mv -- "$staging/$target.json" "$output_dir/$target.json"
printf '\nBuild complete. Upload BOTH files to the same GitHub release:\n%s\n%s\n' \
  "$output_dir/$target.bin" "$output_dir/$target.json"
