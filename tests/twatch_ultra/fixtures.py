"""Synthetic ESP32-S3 packaging fixtures. Never flash these images."""
import hashlib
import struct

TARGETS = tuple('lilygo_twatch_ultra_' + role for role in
                ('companion_radio_usb', 'companion_radio_ble', 'repeater'))


def write_watch_fixture(build):
    records = [
        (1, 2, 0x9000, 0x5000, b'nvs'),
        (1, 0, 0xe000, 0x2000, b'otadata'),
        (0, 16, 0x10000, 0x400000, b'app0'),
        (0, 17, 0x410000, 0x400000, b'app1'),
        (1, 130, 0x810000, 0x7e0000, b'spiffs'),
        (1, 3, 0xff0000, 0x10000, b'coredump'),
    ]
    entries = b''.join(struct.pack('<HBBII16sI', 0x50aa, *row, 0) for row in records)
    partitions = (entries + b'\xeb\xeb' + b'\xff'*14 + hashlib.md5(entries).digest()).ljust(3072, b'\xff')
    app = bytearray(32)
    app[0], app[3], app[12] = 0xe9, 0x40, 9
    merged = bytearray(b'\xff' * 0x10000) + app
    merged[:len(app)] = app
    merged[0x8000:0x8c00] = partitions
    for name, data in [('firmware.bin', app), ('partitions.bin', partitions),
                       ('firmware-merged.bin', merged)]:
        (build / name).write_bytes(data)
