"""Reject wrong Nordic layouts, overflow and malformed UF2 before packaging."""
import importlib.util
from pathlib import Path
import tempfile
import struct
spec = importlib.util.spec_from_file_location('verify', Path(__file__).resolve().parents[2] / 'scripts/verify-station-build.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    def block(addr):
        data = bytearray(512)
        struct.pack_into('<8I', data, 0, 0x0A324655, 0x9E5D5157, 0x2000, addr, 256, 0, 1, 0xADA52840)
        struct.pack_into('<I', data, 508, 0x0AB16F30)
        return data
    for target, start, limit in [('Heltec_t114_repeater', 0x26000, 0xED000),
                                 ('t1000e_companion_radio_ble_position', 0x27000, 0xD4000)]:
        p = root / (target + '.uf2')
        p.write_bytes(block(start))
        m.verify_nordic(root, target, start, limit)
        for addr in (start - 256, start + 256, limit, 0xFF000):
            p.write_bytes(block(addr))
            try:
                m.verify_nordic(root, target, start, limit)
                raise AssertionError('bad address accepted')
            except ValueError:
                pass
print('Nordic target layout and protected-address rejection passed.')
