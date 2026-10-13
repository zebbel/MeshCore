"""Exercise the repository-root dual wrapper with simulated compilers, real packagers."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='dual build ') as directory:
    parent = Path(directory)
    repo = parent / 'MeshCore'
    (repo / 'scripts').mkdir(parents=True)
    (repo / 'platformio.ini').write_text('')
    shutil.copy(ROOT / 'build-station.sh', repo / 'build-station.sh')
    (repo / 'meshcoreStation.version').write_text('v1.0.0\n')
    (repo / 'examples/companion_radio').mkdir(parents=True)
    shutil.copy(ROOT / 'examples/companion_radio/MyMesh.h', repo / 'examples/companion_radio/MyMesh.h')
    subprocess.run(['git', 'init', '-q', str(repo)], check=True)
    subprocess.run(['git', '-C', str(repo), '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                    'commit', '--allow-empty', '-qm', 'Fixture'], check=True)
    remote = parent / 'remote.git'
    subprocess.run(['git', 'init', '--bare', '-q', str(remote)], check=True)
    subprocess.run(['git', '-C', str(repo), 'remote', 'add', 'origin', str(remote)], check=True)
    subprocess.run(['git', '-C', str(repo), 'push', '-qu', 'origin', 'HEAD'], check=True)
    commit = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', '--short', 'HEAD'], text=True).strip()
    for name in ('station_release.py', 'verify-station-build.py'):
        shutil.copy(ROOT / 'scripts' / name, repo / 'scripts' / name)
    shutil.copytree(ROOT / 'bin' / 'uf2conv', repo / 'bin' / 'uf2conv')
    records = [
        (1, 2, 0x9000, 0x5000, b'nvs'), (1, 0, 0xe000, 0x2000, b'otadata'),
        (0, 16, 0x10000, 0x140000, b'app0'), (0, 17, 0x150000, 0x140000, b'app1'),
        (1, 130, 0x290000, 0x160000, b'spiffs'), (1, 3, 0x3f0000, 0x10000, b'coredump'),
    ]
    partitions = b''.join(struct.pack('<HBBII16sI', 0x50aa, *rec, 0) for rec in records)
    partitions += b'\xeb\xeb' + b'\xff' * 14 + hashlib.md5(partitions).digest()
    partitions += b'\xff' * (3072 - len(partitions))
    (repo / 'fixture-partitions.bin').write_bytes(partitions)
    compiler = parent / 'fake pio'
    compiler.write_text('''#!/usr/bin/env python3
import os, pathlib, shutil, sys, zipfile, json
args = sys.argv
name = args[args.index('-e') + 1]
if os.environ.get('FAIL_T1000') and name.startswith('t1000'):
    sys.exit(7)
build = pathlib.Path('.pio/build') / name
if 'clean' in args:
    shutil.rmtree(build, ignore_errors=True)
    sys.exit(0)
build.mkdir(parents=True, exist_ok=True)
if name.startswith('heltec'):
    (build / 'firmware.bin').write_bytes(bytes([0xe9]) + bytes(1023))
    shutil.copy('fixture-partitions.bin', build / 'partitions.bin')
    if 'mergebin' in args:
        (build / 'firmware-merged.bin').write_bytes(bytes([0xe9]) + bytes(65535) + (build / 'firmware.bin').read_bytes())
else:
    with zipfile.ZipFile(build / 'firmware.zip', 'w') as archive:
        archive.writestr('application.bin', bytes(range(16)))
        archive.writestr('application.dat', b'dfu metadata')
        archive.writestr('manifest.json', json.dumps({'manifest': {'application': {'bin_file': 'application.bin', 'dat_file': 'application.dat'}}}))
    (build / 'firmware.hex').write_text(':020000040002F8\\n:10700000000102030405060708090A0B0C0D0E0F08\\n:00000001FF\\n')
''')
    compiler.chmod(0o755)
    env = dict(os.environ, STATION_PIO=str(compiler), FIRMWARE_VERSION='v1.17.1')
    stale = repo / 'release' / 'old-version'
    stale.mkdir(parents=True)
    (stale / 'old-firmware.bin').write_bytes(b'old')
    (repo / 'release' / '.old-manifest.json').write_text('{}')
    subprocess.run(['bash', str(repo / 'build-station.sh')], check=True, env=env)
    release = repo / 'release'
    hname = f'heltec_v4_companion_radio_usb-v1.17.1-{commit}-meshcoreStation-v1.0.0'
    mname = f'heltec_v4_companion_radio_usb-v1.17.1-{commit}-merged-meshcoreStation-v1.0.0'
    tname = f't1000e_companion_radio_ble-v1.17.1-{commit}-meshcoreStation-v1.0.0'
    outputs = [hname + '.bin', hname + '.json', mname + '.bin', tname + '.uf2',
               tname + '.zip', tname + '.uf2.sha256', tname + '.zip.sha256']
    assert sorted(p.name for p in release.iterdir()) == sorted(outputs)
    manifest = json.loads((release / (hname + '.json')).read_text())
    assert manifest['image'] == hname + '.bin'
    assert manifest['target'] == 'heltec_v4_companion_radio_usb'
    assert manifest['sha256'] == hashlib.sha256((release / manifest['image']).read_bytes()).hexdigest()
    for ext in ('.uf2', '.zip'):
        name = tname + ext
        assert (release / (name + '.sha256')).read_text() == hashlib.sha256((release / name).read_bytes()).hexdigest() + '  ' + name + '\n'
    # Failed pull must stop before compilation or deleting existing artifacts.
    subprocess.run(['git', '-C', str(repo), 'remote', 'set-url', 'origin', str(parent / 'missing.git')], check=True)
    failed_pull = subprocess.run(['bash', str(repo / 'build-station.sh')], env=env, capture_output=True)
    assert failed_pull.returncode != 0
    assert b'Building Heltec' not in failed_pull.stdout
    assert all((release / name).exists() for name in outputs)
    subprocess.run(['git', '-C', str(repo), 'remote', 'set-url', 'origin', str(remote)], check=True)
    # A failed rebuild must leave no current or older release artifacts.
    (release / 'old-firmware.bin').write_text('remove')
    result = subprocess.run(['bash', str(repo / 'build-station.sh')],
                            env=dict(env, FAIL_T1000='1'), capture_output=True)
    assert result.returncode != 0
    assert not any((release / name).exists() for name in outputs)
    assert list(release.iterdir()) == []
print('Root build wrapper, upstream naming, manifests, checksums and failure cleanup passed.')
