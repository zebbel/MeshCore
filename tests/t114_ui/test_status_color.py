"""Exercise the production SPI tint selection and color-only refresh bounds."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'src/helpers/ui/ST7789Spi.h').read_text()
a=s.index('      uint16_t pixelRGB(');b=s.index('  public:',a)
method=s[a:b]
a=s.index('    void setStatusColor(');b=s.index('\n#endif',a)
setter=s[a:b]
a=s.index('       if (_statusColor != _lastStatusColor)');b=s.index('\n#endif',a)
refresh=s[a:b]
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.cpp'
 p.write_text('''#include <stdint.h>
#include <cassert>
#include <algorithm>
using std::min; using std::max;
#define HELTEC_T114_WITH_DISPLAY
struct Driver { uint16_t _RGB=0xffff,_statusColor=0xffff,_lastStatusColor=0xffff;
'''+method+setter+'''
void check(){uint16_t minBoundX=UINT16_MAX,minBoundY=UINT16_MAX,maxBoundX=0,maxBoundY=0,displayWidth=240;
'''+refresh+'''
assert(minBoundX==138 && maxBoundX==239 && minBoundY==0 && maxBoundY==2);}
};
int main(){Driver d; for(uint16_t c:{0x07e0,0xfd20,0xf800}){d.setStatusColor(c);d.check();
assert(d.pixelRGB(137,0)==0xffff);assert(d.pixelRGB(138,22)==uint16_t((c>>8)|(c<<8)));
assert(d.pixelRGB(200,23)==0xffff);}}
''')
 subprocess.run(['g++','-std=c++11',str(p),'-o',tmp+'/test'],check=True)
 subprocess.run([tmp+'/test'],check=True)
print('Production SPI GPS region, RGB565 byte order and color-only refresh bounds passed.')
