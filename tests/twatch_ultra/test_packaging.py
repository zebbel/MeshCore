"""Reject mismatched images and partition tables before packaging."""
import importlib.util
from pathlib import Path
import tempfile

from fixtures import TARGETS, write_watch_fixture

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('verifier', root / 'scripts/verify-station-build.py')
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)

with tempfile.TemporaryDirectory(prefix='watch-packaging-') as tmp:
    build = Path(tmp) / 'build'
    stage = Path(tmp) / 'stage'
    build.mkdir()
    stage.mkdir()
    target = TARGETS[0]
    write_watch_fixture(build)
    verifier.stage_watch(build, stage, target)
    verifier.verify_watch(stage, target)

    for offset, mask in ((3, 0x10), (12, 1), (0x8000, 1), (0x80d0, 1), (0x10000, 1)):
        path = stage / (target+'-merged.bin')
        merged = bytearray((build / 'firmware-merged.bin').read_bytes())
        merged[offset] ^= mask
        path.write_bytes(merged)
        try:
            verifier.verify_watch(stage, target)
        except ValueError:
            pass
        else:
            raise AssertionError(f'Corrupt image accepted at offset {offset:#x}')
print('Watch image, embedded application and partition corruption checks passed.')
