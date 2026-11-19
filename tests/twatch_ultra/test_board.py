"""Test production board bring-up and the radio power gate with hardware stand-ins."""
from pathlib import Path
import subprocess
import tempfile

here = Path(__file__).resolve().parent
variant = here.parents[1] / 'variants/lilygo_twatch_ultra'
target = (variant / 'target.cpp').read_text()
radio_init = target[target.index('bool radio_init()'):target.index('mesh::LocalIdentity radio_new_identity()')]
with tempfile.TemporaryDirectory(prefix='twatch-board-') as tmp:
    test = Path(tmp) / 'test.cpp'
    binary = Path(tmp) / 'test'
    test.write_text((here / 'test_board.cpp').read_text() + '\n' + radio_init)
    subprocess.run(['g++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-DPIN_BOARD_SDA=3', '-DPIN_BOARD_SCL=2',
                    '-DP_LORA_NSS=36', '-I', str(here / 'stubs'), '-I', str(variant),
                    str(test), str(variant / 'TWatchUltraBoard.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('Watch power, antenna, temperature ADC preservation and radio initialization gate passed.')
