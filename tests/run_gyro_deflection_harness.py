"""Actual native angular accumulator and controller-context entry; no OS output."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'JoyShockMapper/src/JoyShock.cpp').read_text()
start = source.index('FloatXY JoyShock::updateGyroDeflection(')
opening = source.index('{', start)
depth, end = 1, opening + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
native = source[start:end]
main = (ROOT / 'JoyShockMapper/src/main.cpp').read_text()
macro_start = main.index('bool do_RECENTER_GYRO_DEFLECTION()')
macro_end = main.index('\n}', macro_start) + 2
recenter = main[macro_start:macro_end]
poll = main[main.index('void joyShockPollCallback('):main.index('bool do_SET_MOTION_STICK_NEUTRAL()')]
assert poll.index('jc->updateGyroDeflection(') < poll.index('float decay = exp2f('), 'synthetic coast must not accumulate deflection'
assert poll.index('jc->updateGyroDeflection(') < poll.index('gyroXVelocity *= appliedSensX'), 'camera sensitivity must not affect angle'
assert 'deflectionMode && !blockGyro && !trackball_x_pressed && !trackball_y_pressed' in poll
assert 'jc->gyroXVelocity = deflection.first * maximum' in poll and 'jc->gyroYVelocity = deflection.second * maximum' in poll
assert 'GyroOutput::LEFT_STICK || deflectionTarget == GyroOutput::RIGHT_STICK' in poll
program = r'''
#include <cassert>
#include <atomic>
#include <limits>
#include <iostream>
#include <map>
#include <memory>
#include "JoyShockMapper.h"
#include "GyroDeflection.h"
struct JoyShock {
 GyroDeflection gyroDeflection; std::atomic<bool> gyroDeflectionRecenterRequested{false};
 bool held=false,locked=true; FloatXY range{30,60};
 template<class T> T getSetting(SettingID key) {
  if constexpr (std::is_same_v<T,FloatXY>) { assert(key==SettingID::GYRO_DEFLECTION_RANGE); return held?FloatXY{15,30}:range; }
  else { assert(key==SettingID::GYRO_DEFLECTION_LOCK_EXTENTS); return locked?Switch::ON:Switch::OFF; }
 }
 FloatXY updateGyroDeflection(float,float,float,bool,int);
};
NATIVE_ENTRY
std::map<int,std::shared_ptr<JoyShock>> handle_to_joyshock;
NATIVE_RECENTER
void near(float actual,float expected) { assert(std::abs(actual-expected)<.0001f); }
int main() {
 auto one=std::make_shared<JoyShock>(), two=std::make_shared<JoyShock>();
 handle_to_joyshock={{1,one},{2,two}};
 assert(do_RECENTER_GYRO_DEFLECTION());
 assert(one->gyroDeflectionRecenterRequested && two->gyroDeflectionRecenterRequested);
 one->updateGyroDeflection(0,0,.01f,true,1);
 assert(!one->gyroDeflectionRecenterRequested && two->gyroDeflectionRecenterRequested);
 two->updateGyroDeflection(0,0,.01f,true,1);
 assert(!two->gyroDeflectionRecenterRequested);
 handle_to_joyshock.clear();assert(do_RECENTER_GYRO_DEFLECTION());
 JoyShock js; auto p=js.updateGyroDeflection(15,-30,.1f,true,1);near(p.first,0);near(p.second,0);
 p=js.updateGyroDeflection(15,-30,.1f,true,1);near(p.first,.05f);near(p.second,-.05f);
 p=js.updateGyroDeflection(0,0,.1f,true,1);near(p.first,.05f);near(p.second,-.05f);
 js.held=true;p=js.updateGyroDeflection(0,0,.1f,true,1);near(p.first,.1f);near(p.second,-.1f);
 js.gyroDeflectionRecenterRequested=true;p=js.updateGyroDeflection(100,100,.1f,true,1);near(p.first,0);near(p.second,0);
 p=js.updateGyroDeflection(100,100,.1f,true,2);near(p.first,0);near(p.second,0);
 js.updateGyroDeflection(100,100,.1f,false,2);p=js.updateGyroDeflection(0,0,.1f,true,2);near(p.first,0);
 JoyShock other;other.updateGyroDeflection(0,0,.01f,true,1);
 for(int i=0;i<100;i++) other.updateGyroDeflection(100,0,.1f,true,1);
 p=other.updateGyroDeflection(-30,0,.1f,true,1);near(p.first,.9f);
 other.locked=false;other.gyroDeflection.reset();other.updateGyroDeflection(0,0,.01f,true,1);
 for(int i=0;i<100;i++) other.updateGyroDeflection(100,0,.1f,true,1);
 p=other.updateGyroDeflection(-30,0,.1f,true,1);near(p.first,1);
 for(int samples: {10,100,200}) {
  GyroDeflection state;state.step(0,0,.01f,30,60,true,true,1);
  for(int i=0;i<samples;i++) { auto output=state.step(15,-30,1.f/samples,30,60,true,true,1);p={output.first,output.second}; }
  near(p.first,.5f);near(p.second,-.5f);
 }
 p=js.updateGyroDeflection(std::numeric_limits<float>::infinity(),0,.1f,true,1);near(p.first,0);
 p=js.updateGyroDeflection(20,20,0,true,1);near(p.first,0);
 p=js.updateGyroDeflection(20,20,1,true,1);near(p.first,0);
 js.range={0,30};js.held=false;p=js.updateGyroDeflection(20,20,.1f,true,1);near(p.first,0);
 std::cout<<"PASS: actual angular deflection, held ranges, neutral/target/recenter, independent controllers, locking/overshoot, sample-rate invariance and invalid samples\n";
}
'''.replace('NATIVE_ENTRY', native).replace('NATIVE_RECENTER', recenter)
compiler = shutil.which('g++') or shutil.which('clang++')
vcvars = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
with tempfile.TemporaryDirectory() as directory:
    folder = Path(directory)
    cpp, binary = folder / 'gyro-deflection.cpp', folder / ('gyro-deflection.exe' if os.name == 'nt' else 'gyro-deflection')
    cpp.write_text(program)
    include = ROOT / 'JoyShockMapper/include'
    magic = ROOT.parent / 'build-jsm-sdl/_deps/magic_enum-src/include'
    if compiler:
        command = [compiler, '-std=c++17', '-O2', '-I', str(include), '-I', str(magic), str(cpp), '-o', str(binary)]
    else:
        batch = folder / 'build.bat'
        batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /EHsc /std:c++17 /I"{include}" /I"{magic}" "{cpp}" /Fe:"{binary}"\n')
        command = ['cmd.exe', '/d', '/c', str(batch)]
    result = subprocess.run(command, cwd=folder, capture_output=True, text=True)
    if result.returncode:
        print(result.stdout, result.stderr)
        raise SystemExit(result.returncode)
    raise SystemExit(subprocess.run([str(binary)]).returncode)
