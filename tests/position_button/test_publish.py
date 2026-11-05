"""Exercise release ordering and failure handling without contacting GitHub."""
import importlib.util
import hashlib
import json
import struct
import zipfile
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
    names = ['heltec_v4_companion_radio_usb-v1-aaaaaaa-meshcoreStation-v1.0.0' + ext for ext in ('.bin', '.json')]
    names += ['heltec_v4_companion_radio_usb-v1-aaaaaaa-merged-meshcoreStation-v1.0.0.bin']
    for target in ('t1000e_companion_radio_ble', 'Heltec_t114_repeater', 'Xiao_nrf52_repeater'):
        names += [target + '-v1-aaaaaaa-meshcoreStation-v1.0.0' + ext for ext in ('.uf2', '.zip', '.uf2.sha256', '.zip.sha256')]
    for name in names:
        p = m.ROOT / 'release' / name
        p.write_bytes(b'firmware')
        assets.append(str(p))
    app=bytes([0xe9])+bytes(31)
    h='heltec_v4_companion_radio_usb-v1-aaaaaaa-meshcoreStation-v1.0.0'
    (m.ROOT/'release'/(h+'.bin')).write_bytes(app)
    (m.ROOT/'release'/'heltec_v4_companion_radio_usb-v1-aaaaaaa-merged-meshcoreStation-v1.0.0.bin').write_bytes(bytes([0xe9])+bytes(65535)+app)
    (m.ROOT/'release'/(h+'.json')).write_text(json.dumps(dict(schema=1,target='heltec_v4_companion_radio_usb',image=h+'.bin',chip='esp32s3',offset=0x10000,partition_sha256='148b959cbff1c38aa8e1d5c0ba9d612c54997b945e56a63f41223eef650653a1',sha256=hashlib.sha256(app).hexdigest(),meshcore_version='v1',commit='aaaaaaa',meshcoreStation_version='v1.0.0')))
    for name in names:
        p=m.ROOT/'release'/name
        if name.endswith('.uf2'):
            data=bytearray(512)
            struct.pack_into('<8I',data,0,0x0A324655,0x9E5D5157,0x2000,0x26000 if name.startswith('Heltec') else 0x27000,256,0,1,0xADA52840)
            struct.pack_into('<I',data,508,0x0AB16F30);p.write_bytes(data)
        if name.endswith('.zip'):
            with zipfile.ZipFile(p,'w') as z:
                z.writestr('manifest.json',json.dumps({'manifest':{'application':{'bin_file':'app.bin','dat_file':'app.dat'}}}))
                z.writestr('app.bin',b'app');z.writestr('app.dat',b'metadata')
    for name in names:
        if name.endswith('.sha256'):
            payload=name[:-7]
            (m.ROOT/'release'/name).write_text(hashlib.sha256((m.ROOT/'release'/payload).read_bytes()).hexdigest()+'  '+payload+'\n')
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
    # Corrupt XIAO payloads with matching replacement checksums still fail before upload.
    for extension in ('.uf2', '.zip'):
        damaged=next(Path(p) for p in assets if Path(p).name.startswith('Xiao') and p.endswith(extension))
        original=damaged.read_bytes(); sidecar=damaged.with_name(damaged.name+'.sha256');old_hash=sidecar.read_bytes()
        damaged.write_bytes(bytes(len(original)))
        sidecar.write_text(hashlib.sha256(damaged.read_bytes()).hexdigest()+'  '+damaged.name+'\n')
        calls.clear()
        try:
            m.publish(sha,'v1.0.0',assets)
            raise AssertionError('corrupt XIAO accepted')
        except (ValueError, zipfile.BadZipFile):
            pass
        assert not any(c[:3]==('gh','release','create') for c in calls)
        damaged.write_bytes(original);sidecar.write_bytes(old_hash)
    calls.clear()
    try:
        m.publish(sha,'v1.0.0',[p for p in assets if not Path(p).name.startswith('Xiao')])
        raise AssertionError('missing XIAO accepted')
    except ValueError:
        pass
    assert not any(c[:3]==('gh','release','create') for c in calls)
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
