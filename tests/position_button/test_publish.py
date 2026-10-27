"""Exercise release ordering and failure handling without contacting GitHub."""
import importlib.util
import hashlib
import json
from pathlib import Path
import tempfile

spec = importlib.util.spec_from_file_location('publish', Path(__file__).resolve().parents[2] / 'scripts/publish-station-release.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
sha = 'a' * 40
with tempfile.TemporaryDirectory() as tmp:
    m.ROOT = Path(tmp)
    (m.ROOT / 'release').mkdir()
    assets = []
    names = ['heltec_v4_companion_radio_usb-test-meshcoreStation-v1.0.0' + ext for ext in ('.bin', '.json', '.merged.bin')]
    for target in ('t1000e_companion_radio_ble', 'Heltec_t114_repeater'):
        names += [target + '-test-meshcoreStation-v1.0.0' + ext for ext in ('.uf2', '.zip', '.uf2.sha256', '.zip.sha256')]
    for name in names:
        p = m.ROOT / 'release' / name
        p.write_bytes(b'firmware')
        assets.append(str(p))
    for name in names:
        if name.endswith('.sha256'):
            payload = name[:-7]
            (m.ROOT / 'release' / name).write_text(hashlib.sha256(b'firmware').hexdigest() + '  ' + payload + '\n')
    calls = []
    mode = 'ok'
    def fake(*args):
        calls.append(args)
        if args[:3] == ('git', 'rev-parse', 'HEAD'): return sha
        if args[:2] == ('git', 'status'): return ' M file' if mode == 'dirty' else ''
        if args[:2] == ('gh', 'api'):
            if '/commits/' in args[2]: return 'b' * 40 if mode == 'unpushed' else sha
            if '/releases' in args[2]: return 'meshcoreStation-v1.0.0' if mode == 'existing' else ''
            return ''
        if args[:3] == ('gh', 'release', 'create'):
            assert '--draft' in args and args[args.index('--target') + 1] == sha
            assert Path(args[args.index('--notes-file') + 1]).exists()
            if mode == 'upload_error': raise RuntimeError('upload failed')
        if args[:3] == ('gh', 'release', 'view'):
            items = [{'name': Path(p).name, 'size': Path(p).stat().st_size} for p in assets]
            if mode == 'incomplete': items.pop()
            return json.dumps({'assets': items})
        return ''
    m.run = fake
    m.publish(sha, 'v1.0.0', assets)
    assert calls[-1][:3] == ('gh', 'release', 'edit')
    assert '--draft=false' in calls[-1]
    for mode in ('dirty', 'unpushed', 'existing', 'upload_error', 'incomplete'):
        calls.clear()
        try:
            m.publish(sha, 'v1.0.0', assets)
            raise AssertionError(mode)
        except RuntimeError:
            pass
        assert not any(c[:3] == ('gh', 'release', 'edit') for c in calls), mode
    mode = 'ok'
    checksum = next(Path(p) for p in assets if p.endswith('.sha256'))
    checksum.write_text('invalid')
    calls.clear()
    try:
        m.publish(sha, 'v1.0.0', assets)
        raise AssertionError('invalid checksum accepted')
    except ValueError:
        pass
    assert not any(c[:3] == ('gh', 'release', 'create') for c in calls)
print('Release preflight, draft upload, verification and failure tests passed.')
