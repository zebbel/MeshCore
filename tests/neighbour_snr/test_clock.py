from pathlib import Path
import os, subprocess, tempfile
root = Path(__file__).resolve().parents[2]
source = (root / "variants/xiao_nrf52/target.h").read_text()
clock = source.split("class XiaoRepeaterClock")[1].split("extern XiaoRepeaterClock")[0]
program = """#include <stdint.h>
#include <assert.h>
namespace mesh { class RTCClock {}; }
class AutoDiscoverRTCClock {
public:
 uint32_t current=1715770351;
 AutoDiscoverRTCClock(mesh::RTCClock&) {}
 virtual void setCurrentTime(uint32_t t) {current=t;}
};
class XiaoRepeaterClock""" + clock + """
int main() {
 mesh::RTCClock fallback; XiaoRepeaterClock c(fallback);
 assert(!c.hasConfirmedTime());
 c.setCurrentTime(1700000000); assert(c.hasConfirmedTime() && c.current==1700000000);
 c.setCurrentTime(0); assert(!c.hasConfirmedTime() && c.current==0);
 c.setCurrentTime(4102444800UL); assert(!c.hasConfirmedTime());
 c.setCurrentTime(1577836800); assert(c.hasConfirmedTime());
}
"""
with tempfile.TemporaryDirectory() as tmp:
 path=Path(tmp)/"clock.cpp"; path.write_text(program)
 exe=Path(tmp)/"clock"
 subprocess.run(["g++", "-std=c++11", "-Wall", "-Wextra", "-Werror", str(path), "-o", str(exe)], check=True)
 subprocess.run([str(exe)], check=True)
print("Production XIAO clock wrapper: fallback UTC untrusted, explicit valid clock updates confirmed.")
