#!/usr/bin/env python3
"""Validate all seven staged deliverables before the build wrapper exposes them."""
import hashlib
import json
import pathlib
import struct
import re
import zipfile
import sys
import tempfile
import shutil


WATCH_TARGETS = (
    'lilygo_twatch_ultra_companion_radio_usb',
    'lilygo_twatch_ultra_companion_radio_ble',
    'lilygo_twatch_ultra_repeater',
)
WATCH_PARTITIONS = (
    (1, 2, 0x9000, 0x5000, b'nvs'),
    (1, 0, 0xe000, 0x2000, b'otadata'),
    (0, 16, 0x10000, 0x400000, b'app0'),
    (0, 17, 0x410000, 0x400000, b'app1'),
    (1, 130, 0x810000, 0x7e0000, b'spiffs'),
    (1, 3, 0xff0000, 0x10000, b'coredump'),
)


def validate_watch_partitions(data):
    entries = b''.join(struct.pack('<HBBII16sI', 0x50aa, *row, 0) for row in WATCH_PARTITIONS)
    expected = entries + b'\xeb\xeb' + b'\xff'*14 + hashlib.md5(entries).digest()
    if data != expected.ljust(3072, b'\xff'):
        raise ValueError('T-Watch Ultra partition table does not match the expected layout')


def watch_names(target, version, commit, station):
    app = f'{target}-{version}-{commit}-meshcoreStation-{station}'
    merged = f'{target}-{version}-{commit}-merged-meshcoreStation-{station}'
    return {target+'.bin': app+'.bin', target+'.json': app+'.json',
            target+'-merged.bin': merged+'.bin'}


def watch_manifest(target, app, merged):
    partitions = merged[0x8000:0x8c00]
    validate_watch_partitions(partitions)
    # Both image headers must identify ESP32-S3 with 16 MB flash.
    if (len(app) < 24 or len(app) > 0x400000 or app[0] != 0xe9
            or app[3] >> 4 != 4 or struct.unpack_from('<H', app, 12)[0] != 9
            or merged[0] != 0xe9 or len(merged) > 0x1000000
            or merged[3] >> 4 != 4 or struct.unpack_from('<H', merged, 12)[0] != 9
            or merged[0x10000:0x10000 + len(app)] != app):
        raise ValueError(f'Invalid T-Watch Ultra image for {target}')
    return dict(schema=1, target=target, chip='esp32s3', offset=0x10000,
                image=target+'.bin', sha256=hashlib.sha256(app).hexdigest(),
                partition_sha256=hashlib.sha256(partitions).hexdigest())


def verify_watch(folder, target):
    app = (folder / (target+'.bin')).read_bytes()
    merged = (folder / (target+'-merged.bin')).read_bytes()
    manifest = json.loads((folder / (target+'.json')).read_text())
    if manifest != watch_manifest(target, app, merged):
        raise ValueError(f'Invalid T-Watch Ultra manifest for {target}')


def stage_watch(build, folder, target):
    if target not in WATCH_TARGETS:
        raise ValueError('Unknown T-Watch Ultra target')
    build, folder = pathlib.Path(build), pathlib.Path(folder)
    app = (build / 'firmware.bin').read_bytes()
    merged = (build / 'firmware-merged.bin').read_bytes()
    if (build / 'partitions.bin').read_bytes() != merged[0x8000:0x8c00]:
        raise ValueError('Merged T-Watch Ultra partition table differs from the build')
    manifest = watch_manifest(target, app, merged)
    (folder / (target+'.bin')).write_bytes(app)
    (folder / (target+'-merged.bin')).write_bytes(merged)
    (folder / (target+'.json')).write_text(json.dumps(manifest, indent=2) + '\n')


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
    for target in WATCH_TARGETS:
        verify_watch(folder, target)
    verify_nordic(folder, t1000, 0x27000, 0xD4000)
    verify_nordic(folder, 'Heltec_t114_repeater', 0x26000, 0xED000)
    verify_nordic(folder, 'Xiao_nrf52_repeater', 0x27000, 0xED000)
    for target in (t1000, 'Heltec_t114_repeater', 'Xiao_nrf52_repeater'):
        verify_dfu(folder / (target + '.zip'))


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
    checksum = folder / (uf2_name + '.sha256')
    expected = hashlib.sha256(data).hexdigest() + '  ' + uf2_name + '\n'
    if checksum.exists() and checksum.read_text() != expected:
        raise ValueError('Incorrect UF2 checksum')
    checksum.write_text(expected)


def verify_dfu(path):
    with zipfile.ZipFile(path) as archive:
        names=archive.namelist()
        if len(names)!=len(set(names)) or archive.testzip() is not None:
            raise ValueError('Corrupt or ambiguous Nordic DFU archive')
        app=json.loads(archive.read('manifest.json'))['manifest']['application']
        files=[app['bin_file'],app['dat_file']]
        if files[0]==files[1] or not files[0].endswith('.bin') or not files[1].endswith('.dat'):
            raise ValueError('Invalid DFU application names')
        for name in files:
            if pathlib.PurePosixPath(name).name != name or not archive.read(name):
                raise ValueError('Missing or empty DFU application contents')


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

    for target in ('Heltec_t114_repeater', 'Xiao_nrf52_repeater'):
        with zipfile.ZipFile(folder / (target + '.zip')) as archive:
            if archive.testzip() is not None:
                raise ValueError('Invalid Nordic DFU archive')
            app = json.loads(archive.read('manifest.json'))['manifest']['application']
            for key in ('bin_file', 'dat_file'):
                if not archive.read(app[key]):
                    raise ValueError('Empty Nordic DFU payload')
        (folder / (target + '.uf2.sha256')).unlink()
        for ext in ('.uf2', '.zip'):
            name = f'{target}-{version}-{commit}-meshcoreStation-{station_version}' + ext
            (folder / (target + ext)).rename(folder / name)
            (folder / (name + '.sha256')).write_text(
                hashlib.sha256((folder / name).read_bytes()).hexdigest() + '  ' + name + '\n')

    for target in WATCH_TARGETS:
        mapping = watch_names(target, version, commit, station_version)
        manifest_path = folder / (target+'.json')
        watch = json.loads(manifest_path.read_text())
        watch.update(image=mapping[target+'.bin'], meshcore_version=version,
                     meshcoreStation_version=station_version, commit=commit)
        manifest_path.write_text(json.dumps(watch, indent=2) + '\n')
        for source, name in mapping.items():
            (folder / source).rename(folder / name)
            if name.endswith('.bin'):
                checksum = hashlib.sha256((folder / name).read_bytes()).hexdigest()
                (folder / (name+'.sha256')).write_text(f'{checksum}  {name}\n')


def verify_release_assets(paths):
    """Revalidate named artifacts at the publication boundary, not just hashes."""
    paths = [pathlib.Path(p) for p in paths]
    manifest_path = next(p for p in paths if p.suffix == '.json'
                         and p.name.startswith('heltec_v4_companion_radio_usb-'))
    manifest = json.loads(manifest_path.read_text())
    version, commit, station = (manifest[k] for k in ('meshcore_version', 'commit', 'meshcoreStation_version'))
    suffix = f'{version}-{commit}-meshcoreStation-{station}'
    heltec = 'heltec_v4_companion_radio_usb'
    mapping = {heltec+'.bin': f'{heltec}-{suffix}.bin', heltec+'.json': f'{heltec}-{suffix}.json',
               heltec+'-merged.bin': f'{heltec}-{version}-{commit}-merged-meshcoreStation-{station}.bin'}
    for target, public in (('t1000e_companion_radio_ble_position', 't1000e_companion_radio_ble'),
                           ('Heltec_t114_repeater', 'Heltec_t114_repeater'),
                           ('Xiao_nrf52_repeater', 'Xiao_nrf52_repeater')):
        for ext in ('.uf2', '.zip'):
            mapping[target+ext] = public+'-'+suffix+ext
    watch_checksums = set()
    for target in WATCH_TARGETS:
        watch_mapping = watch_names(target, version, commit, station)
        mapping.update(watch_mapping)
        watch_checksums.update(name+'.sha256' for name in watch_mapping.values() if name.endswith('.bin'))
    expected = set(mapping.values()) | watch_checksums
    expected |= {p+'.sha256' for p in expected if p.endswith(('.uf2','.zip'))}
    if set(p.name for p in paths) != expected:
        raise ValueError('Release artifacts do not match one complete named build')
    by_name = {p.name:p for p in paths}
    for name in expected:
        if name.endswith('.sha256'):
            original = name[:-7]
            if by_name[name].read_text().strip() != hashlib.sha256(by_name[original].read_bytes()).hexdigest()+'  '+original:
                raise ValueError('Invalid release checksum')
    with tempfile.TemporaryDirectory(prefix='station-verify-') as tmp:
        folder=pathlib.Path(tmp)
        for dest, source in mapping.items(): shutil.copyfile(by_name[source],folder/dest)
        manifest['image']=heltec+'.bin'
        (folder/(heltec+'.json')).write_text(json.dumps(manifest))
        for target in WATCH_TARGETS:
            watch_path = folder / (target+'.json')
            watch = json.loads(watch_path.read_text())
            if (watch.pop('meshcore_version', None), watch.pop('commit', None),
                    watch.pop('meshcoreStation_version', None), watch.get('image')) != (
                        version, commit, station, mapping[target+'.bin']):
                raise ValueError(f'Invalid release metadata for {target}')
            watch['image'] = target+'.bin'
            watch_path.write_text(json.dumps(watch))
        verify(folder)
        name_release(folder,version,commit,station)


if __name__ == '__main__':
    if sys.argv[1:2] == ['--stage-watch']:
        stage_watch(*sys.argv[2:])
    else:
        verify(sys.argv[1])
        if len(sys.argv) == 5:
            name_release(*sys.argv[1:])
