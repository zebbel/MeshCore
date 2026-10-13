#!/usr/bin/env python3
"""Validate both staged deliverables before the build wrapper exposes them."""
import hashlib
import json
import pathlib
import struct
import sys


def verify(folder):
    folder = pathlib.Path(folder)
    heltec = 'heltec_v4_companion_radio_usb'
    t1000 = 't1000e_companion_radio_ble_position'
    manifest = json.loads((folder / (heltec + '.json')).read_text())
    image = (folder / (heltec + '.bin')).read_bytes()
    if (manifest.get('schema') != 1 or manifest.get('target') != heltec
            or manifest.get('image') != heltec + '.bin'
            or manifest.get('chip') != 'esp32s3'
            or manifest.get('offset') != 0x10000
            or manifest.get('partition_sha256') !=
            '148b959cbff1c38aa8e1d5c0ba9d612c54997b945e56a63f41223eef650653a1'
            or manifest.get('sha256') != hashlib.sha256(image).hexdigest()
            or not image or image[0] != 0xE9 or len(image) > 0x140000):
        raise ValueError('Heltec release verification failed')
    uf2_name = t1000 + '.uf2'
    data = (folder / uf2_name).read_bytes()
    if not data or len(data) % 512:
        raise ValueError('Invalid UF2 length')
    blocks = len(data) // 512
    addresses = set()
    for i in range(blocks):
        block = data[i * 512:(i + 1) * 512]
        m0, m1, flags, addr, size, number, total, family = struct.unpack_from('<8I', block)
        end, = struct.unpack_from('<I', block, 508)
        if ((m0, m1, end) != (0x0A324655, 0x9E5D5157, 0x0AB16F30)
                or flags != 0x2000 or family != 0xADA52840
                or size != 256 or number != i or total != blocks
                or addr % 256 or not 0x27000 <= addr < 0xD4000
                or addr in addresses):
            raise ValueError('UF2 is not a valid T1000-E application image')
        addresses.add(addr)
    if min(addresses) != 0x27000:
        raise ValueError('UF2 application start does not match T1000-E linker layout')
    (folder / (uf2_name + '.sha256')).write_text(
        hashlib.sha256(data).hexdigest() + '  ' + uf2_name + '\n')


if __name__ == '__main__':
    verify(sys.argv[1])
