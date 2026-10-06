"""Exercise the actual Mapping parser and DigitalButton cycle methods without OS input."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


stub = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
using namespace std;
#define COUT cout
#define _ASSERT_EXPR(condition, text) assert(condition)
constexpr int COMMAND_ACTION=500, CALIBRATE=501, GYRO_INV_X=502, GYRO_ON_ALL_BIND=512, RUMBLE=513, HAPTIC=514;
constexpr float MAGIC_TAP_DURATION=40, MAGIC_EXTENDED_TAP_DURATION=50;
std::atomic_bool g_hasGyroOnAllBinding{false};
enum class BtnEvent { OnPress, OnRelease, OnTap, OnTapRelease, OnHold, OnHoldRelease, OnTurbo };
template<class E, enable_if_t<is_enum<E>::value, int> = 0> ostream& operator<<(ostream& out, E value) { return out << int(value); }
struct KeyCode {
  unsigned short code=0; string name;
  KeyCode()=default;
  KeyCode(string_view value): name(value) {
    if (value.size()>2 && value.front()=='"' && value.back()=='"') { code=COMMAND_ACTION; name=value.substr(1,value.size()-2); }
    else if (value=="NONE") code=1;
    else if (value=="X_A") code=600;
    else if (value=="X_B") code=601;
    else if (value.size()==1 && value[0]>='1' && value[0]<='9') code=value[0];
  }
  bool isValid() const { return code!=0; }
};
bool isControllerKey(unsigned short code) { return code>=600; }
void WriteToConsole(string value) { assert(false && "Cycle must never dispatch a console macro"); }
'''

tests = r'''
struct Button: EventActionIf {
  map<string,size_t> _cyclePositions;
  map<string,deque<Mapping>> _cycleReleases;
  vector<Callback> instant;
  vector<string> events;
  void RegisterInstant(BtnEvent, Callback cb) override { if(cb) instant.push_back(cb); }
  void flush() { auto queue=instant; instant.clear(); for(auto &callback:queue) callback(this); }
  void StudioCommand(const string &command) override { events.push_back(command); }
  void ApplyGyroAction(KeyCode) override {}
  void RemoveGyroAction(KeyCode, bool, bool) override {}
  void SetRumble(int,int) override {}
  void FireHaptic(int,int,int) override {}
  void ApplyBtnPress(KeyCode key) override { events.push_back("+"+key.name); }
  void ApplyBtnRelease(KeyCode key) override { events.push_back("-"+key.name); }
  void ApplyButtonToggle(KeyCode,Callback,Callback) override {}
  void StartCalibration() override {}
  void FinishCalibration() override {}
  const char *getDisplayName() override { return "simulated"; }
  CYCLE_METHODS
};
int main() {
  Mapping::_isCommandValid=[](string_view){return false;};
  Button commands;
  Mapping open("\"OPEN_KEYBOARD\"\\");
  assert(open.isValid());
  open.ProcessEvent(BtnEvent::OnPress,commands); commands.flush();
  open.ProcessEvent(BtnEvent::OnRelease,commands); commands.flush();
  assert((commands.events==vector<string>{"OPEN_KEYBOARD"}));
  Mapping pause("\"TOGGLE_MAPPING\"_");
  assert(pause.isValid());pause.ProcessEvent(BtnEvent::OnPress,commands);commands.flush();
  assert(commands.events.size()==1);
  pause.ProcessEvent(BtnEvent::OnHold,commands);commands.flush();
  assert(commands.events.back()=="TOGGLE_MAPPING");
  Mapping cycle("\"CYCLE 1 | 2 | 3\"\\");
  assert(cycle.isValid());
  Button first, second;
  for(int i=0;i<4;++i) { cycle.ProcessEvent(BtnEvent::OnPress,first); first.flush(); cycle.ProcessEvent(BtnEvent::OnRelease,first); }
  assert((first.events==vector<string>{"+1","-1","+2","-2","+3","-3","+1","-1"}));
  cycle.ProcessEvent(BtnEvent::OnPress,second); second.flush();
  assert((second.events==vector<string>{"+1","-1"}));
  // Outstanding taps release their own selected output, even if repeated early.
  cycle.ProcessEvent(BtnEvent::OnPress,second); cycle.ProcessEvent(BtnEvent::OnPress,second); second.flush();
  assert((second.events==vector<string>{"+1","-1","+2","+3","-2","-3"}));
  Mapping pad("\"CYCLE X_A | X_B\"_"); assert(pad.isValid() && pad.hasViGEmBtn());
  pad.ProcessEvent(BtnEvent::OnHold,first); first.flush();
  assert(first.events[first.events.size()-2]=="+X_A");
  for(const string &bad: {"\"CYCLE 1\"", "\"CYCLE 1||2\"", "\"CYCLE 1|2|\"", "\"CYCLE 1|BAD\"", "\"CYCLE 1|^2\"", "^\"CYCLE 1|2\""}) {
    Mapping invalid(bad); assert(!invalid.isValid());
  }
  string longCycle="\"CYCLE "; for(int i=0;i<32;++i) longCycle+=(i?" | ":"")+string("X_A"); longCycle+="\"\\";
  Mapping extended(longCycle); assert(extended.isValid() && extended.hasViGEmBtn());
  assert(longCycle.size()>128); // No old parser buffer truncation.
  cout << "PASS: actual cycle parser, wrap, independent controllers, balanced early repeats, hold activator, virtual detection, invalid syntax and long bindings\n";
}
'''

header = (ROOT / 'JoyShockMapper/include/Mapping.h').read_text()
header = header[header.index('class Mapping;'):]
source = (ROOT / 'JoyShockMapper/src/Mapping.cpp').read_text()
source = re.sub(r'^#include "(?:Mapping|InputHelpers)\.h"\s*$', '', source, flags=re.M)
digital = (ROOT / 'JoyShockMapper/src/DigitalButton.cpp').read_text()
methods = function(digital, 'void ApplyCycle(') + '\n' + function(digital, 'void ReleaseCycle(')
compiler = shutil.which('g++') or shutil.which('clang++')
vcvars = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
with tempfile.TemporaryDirectory() as folder:
    folder = Path(folder)
    cpp, binary = folder / 'cycle.cpp', folder / ('cycle.exe' if os.name == 'nt' else 'cycle')
    cpp.write_text(stub + header + source + tests.replace('CYCLE_METHODS', methods))
    if compiler:
        cmd = [compiler, '-std=c++17', '-O2', str(cpp), '-o', str(binary)]
    else:
        batch = folder / 'build.bat'
        batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /EHsc /std:c++17 "{cpp}" /Fe:"{binary}"\n')
        cmd = ['cmd.exe', '/d', '/c', str(batch)]
    result = subprocess.run(cmd, cwd=folder, capture_output=True, text=True)
    if result.returncode:
        print(result.stdout, result.stderr)
        raise SystemExit(result.returncode)
    raise SystemExit(subprocess.run([str(binary)]).returncode)
