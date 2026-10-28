"""Compile the production NodePrefs declaration and LED callers without Arduino."""
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / 'src/helpers/CommonCLI.h').read_text()
start = source.index('class NodePrefs :')
end = source.index('\n};', start) + 3
declaration = source[start:end]
with tempfile.TemporaryDirectory() as tmp:
    folder = Path(tmp)
    (folder / 'Arduino.h').write_text('#pragma once\n#include <stdint.h>\n#include <stddef.h>\n#include <cstring>\nclass Stream;\n')
    headers = '#include <helpers/CommonRadioPrefs.h>\n#include <helpers/DynamicConfigSerializer.h>\n'
    for t114 in (False, True):
        check = """
void check(NodePrefs& prefs) {
  prefs.tx_led_enabled = 0;
  bool enabled = prefs.tx_led_enabled != 0;
  (void)enabled;
}
""" if t114 else 'void check(NodePrefs& prefs) { prefs.tx_power_dbm = 22; }'
        path = folder / 'check.cpp'
        path.write_text(headers + declaration + check)
        command = ['g++', '-std=c++11', '-fsyntax-only', '-I' + tmp, '-I' + str(ROOT / 'src')]
        if t114:
            command.append('-DHELTEC_T114')
        subprocess.run(command + [str(path)], check=True)
print('Production NodePrefs compiles with public LED access; generic variant also passes.')
