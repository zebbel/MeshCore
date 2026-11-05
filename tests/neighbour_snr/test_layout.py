from pathlib import Path
import tempfile
root=Path(__file__).resolve().parents[2]
script=(root/'scripts/check-xiao-layout.py').read_text()
class Env:
 def __init__(self,path):self.path=path
 def subst(self,s):return str(self.path) if s=='$LDSCRIPT_PATH' else str(root)
 def AddPreAction(self,*args):pass
with tempfile.TemporaryDirectory() as tmp:
 path=Path(tmp)/'layout.ld';env=Env(path);ns={'env':env,'Import':lambda *a:None}
 exec(compile(script,'check-xiao-layout.py','exec'),ns)
 path.write_bytes((root/'boards/nrf52840_s140_v7.ld').read_bytes());ns['check_layout'](None,None,env)
 path.write_text(path.read_text().replace('0x27000','0x26000'))
 try:ns['check_layout'](None,None,env);raise AssertionError('wrong layout accepted')
 except RuntimeError:pass
ini=(root/'variants/xiao_nrf52/platformio.ini').read_text()
assert ini.count('-D XIAO_NEIGHBOUR_SNR')==1
assert 'post:scripts/check-xiao-layout.py' in ini.split('[env:Xiao_nrf52_repeater]')[1].split('[env:')[0]
print('Resolved XIAO linker guard and target isolation passed.')
