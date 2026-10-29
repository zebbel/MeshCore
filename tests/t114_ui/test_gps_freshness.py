"""Check the production MicroNMEA freshness predicate, including timer wrap."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'src/helpers/sensors/MicroNMEALocationProvider.h').read_text()
start=source.index('    bool hasFreshFix(')
end=source.index('\n    }',start)+6
method=source[start:end].replace(' override','')
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)
 (p/'test.cpp').write_text('#include <stdint.h>\n#include <cassert>\nstruct GPS {\n'
 'bool enabled=true, valid=true, _fix_seen=true; uint32_t now=10000, _last_fix_ms=5000;\n'
 'bool isEnabled(){return enabled;} bool isValid(){return valid;} uint32_t ageMs(uint32_t t){return now-t;}\n'
 +method+'\n};\nint main(){GPS g;assert(g.hasFreshFix(5000));g.now++;assert(!g.hasFreshFix(5000));'
 'g.now=5001;g.enabled=false;assert(!g.hasFreshFix(5000));g.enabled=true;g.valid=false;assert(!g.hasFreshFix(5000));'
 'g.valid=true;g._fix_seen=false;assert(!g.hasFreshFix(5000));g._fix_seen=true;g._last_fix_ms=0xfffffff0;g.now=20;assert(g.hasFreshFix(5000));}\n')
 subprocess.run(['g++','-std=c++11','-Wall','-Werror',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('MicroNMEA freshness: age boundary, disabled, invalid, unseen and rollover passed.')
