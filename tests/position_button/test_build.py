"""Exercise the installed dual wrapper with simulated compilers, real packagers."""
import hashlib
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
    for name in ('build-station.sh', 'station_release.py', 'verify-station-build.py'):
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
import os, pathlib, shutil, sys
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
else:
    (build / 'firmware.hex').write_text(':020000040002F8\\n:10700000000102030405060708090A0B0C0D0E0F08\\n:00000001FF\\n')
''')
    compiler.chmod(0o755)
    env = dict(os.environ, STATION_PIO=str(compiler))
    subprocess.run(['bash', str(repo / 'scripts/build-station.sh'), '--install'], check=True, env=env)
    subprocess.run(['bash', str(parent / 'build-station.sh')], check=True, env=env)
    outputs = ['heltec_v4_companion_radio_usb.bin', 'heltec_v4_companion_radio_usb.json',
               't1000e_companion_radio_ble_position.uf2', 't1000e_companion_radio_ble_position.uf2.sha256']
    assert all((parent / name).is_file() for name in outputs)
    # Verifier rejects an image redirected into bootloader/SoftDevice space.
    uf2 = parent / outputs[2]
    original = uf2.read_bytes()
    bad = bytearray(original)
    struct.pack_into('<I', bad, 12, 0)
    uf2.write_bytes(bad)
    result = subprocess.run(['python3', str(repo / 'scripts/verify-station-build.py'), str(parent)],
                            capture_output=True)
    assert result.returncode != 0
    uf2.write_bytes(original)
    # A failed second build must expose neither new nor old deliverables.
    result = subprocess.run(['bash', str(parent / 'build-station.sh')],
                            env=dict(env, FAIL_T1000='1'), capture_output=True)
    assert result.returncode != 0
    assert not any((parent / name).exists() for name in outputs)
print('Dual build wrapper, UF2 verification and failure cleanup passed.')
