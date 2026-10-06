"""Production gyro angular pulse routing with a recording haptic transport."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'JoyShockMapper/src/JoyShock.cpp').read_text()
start = source.index('void JoyShock::updateGyroHaptics(')
opening = source.index('{', start)
depth, end = 1, opening + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
native = source[start:end]
main = (ROOT / 'JoyShockMapper/src/main.cpp').read_text()
poll = main[main.index('void joyShockPollCallback('):]
assert poll.index('jc->updateGyroHaptics(') < poll.index('float decay = exp2f(')
assert '!blockGyro && !trackball_x_pressed && !trackball_y_pressed' in poll
assert 'updateGyroHaptics(gyroX * gyro_x_sign_to_use, gyroY * gyro_y_sign_to_use' in poll, 'Disabled aim axes must not produce rotation feedback'
assert poll.index('jc->updateGyroHaptics(') < poll.index('gyroXVelocity *= appliedSensX')
program = r'''
#include <cassert>
#include <limits>
#include <map>
#include <vector>
#include <iostream>
#include "GyroAngularHaptics.h"
#include "JoyShockMapper.h"
constexpr int JS_TYPE_STEAM_CONTROLLER_2026=24;
struct Pulse { int side,effect,gain; };
struct JoyShock {
 int _controllerType=24; bool held=false;
 GyroAngularHaptics gyroAngularHaptics;
 std::vector<Pulse> pulses;
 std::map<SettingID,float> base{{SettingID::GYRO_HAPTIC_INTENSITY,30},{SettingID::GYRO_HAPTIC_INTERVAL,10},
  {SettingID::GYRO_HAPTIC_EFFECT,float(HapticEffect::TICK)},{SettingID::GYRO_HAPTIC_SIDE,3}};
 std::map<SettingID,float> shifted{{SettingID::GYRO_HAPTIC_INTENSITY,60},{SettingID::GYRO_HAPTIC_SIDE,2}};
 float getSetting(SettingID key) { return held && shifted.count(key)?shifted[key]:base[key]; }
 template<class T> T getSetting(SettingID key) { return static_cast<T>(getSetting(key)); }
 void fireHaptic(int side,int effect,int gain) { pulses.push_back({side,effect,gain}); }
 void updateGyroHaptics(float,float,float,bool);
};
NATIVE_UPDATE
int main() {
 JoyShock js;
 for(int i=0;i<9;++i)js.updateGyroHaptics(i%2?2.f:-2.f,0,.5f,true);
 assert(js.pulses.empty()); // accumulated physical travel, not net angle
 js.updateGyroHaptics(2,0,.5f,true);assert(js.pulses.size()==1);
 assert(js.pulses.back().side==3&&js.pulses.back().gain==hapticGainDb(30));
 js.held=true;js.updateGyroHaptics(0,20,.5f,true);
 assert(js.pulses.size()==2&&js.pulses.back().side==2&&js.pulses.back().gain==hapticGainDb(60));
 js.held=false;js.updateGyroHaptics(20,0,.5f,true);assert(js.pulses.back().side==3);
 // Diagonal travel uses radial angular distance, not the sum of both axes.
 js.gyroAngularHaptics.reset();auto count=js.pulses.size();
 js.updateGyroHaptics(6,8,.5f,true);assert(js.pulses.size()==count);
 js.updateGyroHaptics(6,8,.5f,true);assert(js.pulses.size()==count+1);
 // A fast turn coalesces crossings and retains only the fractional remainder.
 count=js.pulses.size();js.updateGyroHaptics(205,0,1,true);
 assert(js.pulses.size()==count+1&&js.gyroAngularHaptics.travel==5);
 js.updateGyroHaptics(10,0,.5f,false);assert(js.gyroAngularHaptics.travel==0);
 js.updateGyroHaptics(0,0,.5f,true);assert(js.pulses.size()==count+1);
 js.base[SettingID::GYRO_HAPTIC_INTENSITY]=0;js.updateGyroHaptics(100,0,1,true);
 assert(js.pulses.size()==count+1&&js.gyroAngularHaptics.travel==0);
 js.base[SettingID::GYRO_HAPTIC_INTENSITY]=30;
 js.base[SettingID::GYRO_HAPTIC_EFFECT]=float(HapticEffect::OFF);js.updateGyroHaptics(100,0,1,true);
 assert(js.pulses.size()==count+1);
 js.base[SettingID::GYRO_HAPTIC_EFFECT]=float(HapticEffect::CLICK);
 js.updateGyroHaptics(2,0,.5f,true);js.base[SettingID::GYRO_HAPTIC_INTERVAL]=.5f;
 js.updateGyroHaptics(0,0,.5f,true);assert(js.gyroAngularHaptics.travel==0);
 js.updateGyroHaptics(std::numeric_limits<float>::infinity(),0,.5f,true);
 js.updateGyroHaptics(20,0,0,true);assert(js.pulses.size()==count+1);
 js._controllerType=5;js.updateGyroHaptics(100,0,1,true);assert(js.pulses.size()==count+1);
 JoyShock independent;independent.updateGyroHaptics(0,0,.5f,true);assert(independent.pulses.empty());
 std::cout << "PASS: angular gyro distance, diagonal/reverse motion, held strength/side, release, coalescing, reset, invalid samples and hardware gating\n";
}
'''.replace('NATIVE_UPDATE', native)
compiler = shutil.which('g++') or shutil.which('clang++')
vcvars = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
with tempfile.TemporaryDirectory() as directory:
    folder = Path(directory)
    cpp, binary = folder / 'gyro-haptics.cpp', folder / ('gyro-haptics.exe' if os.name == 'nt' else 'gyro-haptics')
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
