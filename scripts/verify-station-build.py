#!/usr/bin/env python3
"""Validate all three staged deliverables before the build wrapper exposes them."""
import hashlib
import json
import pathlib
import struct
import re
import zipfile
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
    verify_nordic(folder, t1000, 0x27000, 0xD4000)
    verify_nordic(folder, 'Heltec_t114_repeater', 0x26000, 0xED000)


def verify_nordic(folder, target, start, limit):
    uf2_name = target + '.uf2'
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
                or addr % 256 or not (start <= addr and addr + size <= limit)
                or addr in addresses):
            raise ValueError(f'UF2 is not a valid {target} application image')
        addresses.add(addr)
    if min(addresses) != start:
        raise ValueError(f'UF2 start does not match {target} linker layout')
    (folder / (uf2_name + '.sha256')).write_text(
        hashlib.sha256(data).hexdigest() + '  ' + uf2_name + '\n')


def name_release(folder, version, commit, station_version):
    """Use upstream target-version-SHA names, with the custom suffix last."""
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', version) or not re.fullmatch(r'[0-9a-f]+', commit):
        raise ValueError('Invalid version or commit')
    folder = pathlib.Path(folder)
    heltec = 'heltec_v4_companion_radio_usb'
    t1000 = 't1000e_companion_radio_ble_position'
    if not re.fullmatch(r'v[0-9]+\.[0-9]+\.[0-9]+', station_version):
        raise ValueError('Invalid meshcoreStation version')
    hname = f'{heltec}-{version}-{commit}-meshcoreStation-{station_version}'
    mname = f'{heltec}-{version}-{commit}-merged-meshcoreStation-{station_version}'
    tname = f't1000e_companion_radio_ble-{version}-{commit}-meshcoreStation-{station_version}'
    app = (folder / (heltec + '.bin')).read_bytes()
    merged = (folder / (heltec + '-merged.bin')).read_bytes()
    if not merged or merged[0] != 0xE9 or merged[0x10000:0x10000 + len(app)] != app:
        raise ValueError('Merged Heltec image does not contain the verified application')
    with zipfile.ZipFile(folder / (t1000 + '.zip')) as archive:
        if archive.testzip() is not None or 'manifest.json' not in archive.namelist():
            raise ValueError('Invalid Nordic DFU archive')
        dfu = json.loads(archive.read('manifest.json'))['manifest']['application']
        for key in ('bin_file', 'dat_file'):
            if not archive.read(dfu[key]):
                raise ValueError('Empty Nordic DFU payload')
    manifest = json.loads((folder / (heltec + '.json')).read_text())
    manifest['image'] = hname + '.bin'
    manifest['meshcoreStation_version'] = station_version
    manifest['meshcore_version'] = version
    manifest['commit'] = commit
    (folder / (heltec + '.json')).write_text(json.dumps(manifest, indent=2) + '\n')
    for source, dest in ((heltec + '.bin', hname + '.bin'),
                         (heltec + '.json', hname + '.json'),
                         (heltec + '-merged.bin', mname + '.bin'),
                         (t1000 + '.uf2', tname + '.uf2'),
                         (t1000 + '.zip', tname + '.zip')):
        (folder / source).rename(folder / dest)
    (folder / (t1000 + '.uf2.sha256')).unlink()
    for ext in ('.uf2', '.zip'):
        name = tname + ext
        (folder / (name + '.sha256')).write_text(
            hashlib.sha256((folder / name).read_bytes()).hexdigest() + '  ' + name + '\n')

    target = 'Heltec_t114_repeater'
    with zipfile.ZipFile(folder / (target + '.zip')) as archive:
        if archive.testzip() is not None:
            raise ValueError('Invalid T114 DFU archive')
        app = json.loads(archive.read('manifest.json'))['manifest']['application']
        for key in ('bin_file', 'dat_file'):
            if not archive.read(app[key]):
                raise ValueError('Empty T114 DFU payload')
    (folder / (target + '.uf2.sha256')).unlink()
    for ext in ('.uf2', '.zip'):
        name = f'{target}-{version}-{commit}-meshcoreStation-{station_version}' + ext
        (folder / (target + ext)).rename(folder / name)
        (folder / (name + '.sha256')).write_text(
            hashlib.sha256((folder / name).read_bytes()).hexdigest() + '  ' + name + '\n')


if __name__ == '__main__':
    verify(sys.argv[1])
    if len(sys.argv) == 5:
        name_release(*sys.argv[1:])
