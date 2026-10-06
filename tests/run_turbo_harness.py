"""Exercise actual per-action turbo parsing and scheduling without OS input."""
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
  constexpr static float MAGIC_INSTANT_DURATION=40;
  struct BindingTurbo {float next=0, releaseAt=0; vector<Callback> pending;};
  map<size_t,BindingTurbo> _bindingTurbos;
  BindingTurbo *_capturingTurbo=nullptr;
  vector<Callback> instant;
  vector<string> events;
  void RegisterInstant(BtnEvent, Callback cb) override { if(cb) {if(_capturingTurbo) _capturingTurbo->pending.push_back(cb); else instant.push_back(cb);} }
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
  Mapping mapping("!1+{60} !2+{200} !3+");
  assert(mapping.isValid());
  Button first, second;
  for(int t=0;t<=240;t+=10) mapping.ProcessBindingTurbos(float(t),first);
  assert(count(first.events.begin(),first.events.end(),"+1")==5);
  assert(count(first.events.begin(),first.events.end(),"+2")==2);
  assert(count(first.events.begin(),first.events.end(),"+3")==0);
  mapping.ProcessEvent(BtnEvent::OnTurbo,first); first.flush();
  assert(count(first.events.begin(),first.events.end(),"+3")==1);
  mapping.ProcessEvent(BtnEvent::OnRelease,first);
  assert(count(first.events.begin(),first.events.end(),"+1")==count(first.events.begin(),first.events.end(),"-1"));
  mapping.ProcessBindingTurbos(0,second);
  assert(count(second.events.begin(),second.events.end(),"+1")==1);
  mapping.ProcessEvent(BtnEvent::OnRelease,second);
  first.events.clear();
  mapping.ProcessBindingTurbos(0,first);mapping.ProcessBindingTurbos(1000,first);
  assert(count(first.events.begin(),first.events.end(),"+1")==2);
  mapping.ProcessEvent(BtnEvent::OnRelease,first);
  Mapping normal("1+{60}"); first.events.clear();
  normal.ProcessBindingTurbos(0,first);
  normal.ProcessEvent(BtnEvent::OnRelease,first);
  assert(first.events.back()=="-1");
  for(const string &bad:{"1+{0}","1_{60}","1+{99999999999999999999999999999999999999999999}"}) assert(!Mapping(bad).isValid());
  cout << "PASS: native per-action timing, balanced release, independent controllers, legacy turbo and no catch-up burst\n";
}
'''

header = (ROOT / 'JoyShockMapper/include/Mapping.h').read_text()
header = header[header.index('class Mapping;'):]
source = (ROOT / 'JoyShockMapper/src/Mapping.cpp').read_text()
source = re.sub(r'^#include "(?:Mapping|InputHelpers)\.h"\s*$', '', source, flags=re.M)
digital = (ROOT / 'JoyShockMapper/src/DigitalButton.cpp').read_text()
methods = function(digital, 'void ApplyCycle(') + '\n' + function(digital, 'void ReleaseCycle(') + '\n' + function(digital, 'void TickBindingTurbo(') + '\n' + function(digital, 'void FinishBindingTurbo(')
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
