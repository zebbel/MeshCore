"""Fail the custom XIAO link if actual resolved flash layout changes."""
from pathlib import Path
import re
Import('env')

def check_layout(source, target, env):
    path = Path(env.subst('$LDSCRIPT_PATH'))
    if not path.is_absolute():
        path = Path(env.subst('$PROJECT_DIR')) / path
    text = path.read_text()
    pattern = r'FLASH\s*\(rx\)\s*:\s*ORIGIN\s*=\s*0x27000\s*,\s*LENGTH\s*=\s*0xED000\s*-\s*0x27000'
    if not re.search(pattern, text, re.I):
        raise RuntimeError('XIAO resolved linker flash layout differs from verified UF2 bounds: '+str(path))
    print('Verified XIAO linker application region 0x27000..0xED000: '+str(path))

env.AddPreAction('$BUILD_DIR/${PROGNAME}.elf', check_layout)
