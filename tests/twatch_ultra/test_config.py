"""Check resolved role configuration and independence of existing targets."""
from pathlib import Path

from platformio.project.config import ProjectConfig
from fixtures import TARGETS

root = Path(__file__).resolve().parents[2]
config = ProjectConfig(str(root / 'platformio.ini'))

for target in TARGETS:
    section = 'env:' + target
    flags = ' '.join(config.get(section, 'build_flags'))
    sources = ' '.join(config.get(section, 'build_src_filter'))
    assert config.get(section, 'board_upload.flash_size') == '16MB'
    assert config.get(section, 'board_build.arduino.memory_type') == 'qio_qspi'
    assert ('ENABLE_USB_INTERFACE' in flags) == target.endswith('_usb')
    assert ('BLE_PIN_CODE' in flags) == target.endswith('_ble')
    assert ('simple_repeater' in sources) == target.endswith('_repeater')
    assert 'DISPLAY_CLASS' not in flags and 'ENV_INCLUDE_GPS' not in flags
    assert 'variants/lilygo_twatch_ultra' in sources
    if target.endswith('_repeater'):
        assert config.get(section, 'lib_compat_mode') == 'strict'

for target in ('heltec_v4_companion_radio_usb', 't1000e_companion_radio_ble_position',
               'Heltec_t114_repeater', 'Xiao_nrf52_repeater'):
    section = 'env:' + target
    assert 'lilygo_twatch_ultra' not in ' '.join(config.get(section, 'build_src_filter'))
print('Watch USB, BLE, repeater, flash layout and existing-target isolation passed.')
