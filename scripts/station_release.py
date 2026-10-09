"""Package a matching application/manifest pair for the Station USB target.

Used by PlatformIO as a post script, or directly:
python3 scripts/station_release.py .pio/build/heltec_v4_companion_radio_usb
"""
import hashlib
import json
from pathlib import Path
import struct

TARGET = "heltec_v4_companion_radio_usb"
PARTITION_SHA256 = "148b959cbff1c38aa8e1d5c0ba9d612c54997b945e56a63f41223eef650653a1"


def package(build_dir):
    build_dir = Path(build_dir)
    output = build_dir / "release"
    output.mkdir(parents=True, exist_ok=True)
    image_path = output / (TARGET + ".bin")
    manifest_path = output / (TARGET + ".json")
    # Never leave a previous pair looking current after a failed packaging run.
    image_path.unlink(missing_ok=True)
    manifest_path.unlink(missing_ok=True)
    partitions = (build_dir / "partitions.bin").read_bytes()
    partition_hash = hashlib.sha256(partitions).hexdigest()
    if partition_hash != PARTITION_SHA256:
        raise ValueError("Partition binary does not match the verified Station layout")
    slots = []
    for pos in range(0, len(partitions), 32):
        magic, kind, subtype, offset, size, label, flags = struct.unpack_from(
            "<HBBII16sI", partitions, pos)
        if magic != 0x50AA:
            break
        if kind == 0 and subtype == 0x10:
            slots.append((offset, size))
    if len(slots) != 1:
        raise ValueError("Expected exactly one ota_0 application partition")
    offset, limit = slots[0]
    image = (build_dir / "firmware.bin").read_bytes()
    if not image or image[0] != 0xE9 or len(image) > limit:
        raise ValueError("Invalid ESP application image or application exceeds slot size")
    manifest = {
        "schema": 1, "target": TARGET, "chip": "esp32s3",
        "offset": offset, "image": image_path.name,
        "sha256": hashlib.sha256(image).hexdigest(),
        "partition_sha256": partition_hash,
    }
    image_path.write_bytes(image)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("Station release files:", image_path, manifest_path)
    return manifest


def after_build(source, target, env):
    package(env.subst("$BUILD_DIR"))


try:
    Import("env")  # Provided by PlatformIO/SCons
except NameError:
    if __name__ == "__main__":
        import argparse
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument("build_dir")
        package(parser.parse_args().build_dir)
else:
    firmware = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    env.Depends(firmware, env.subst("$BUILD_DIR/partitions.bin"))
    env.AddPostAction(firmware, after_build)
