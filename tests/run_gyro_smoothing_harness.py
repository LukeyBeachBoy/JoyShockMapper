"""Exercise actual legacy and speed-decay gyro smoothing; no hardware or OS."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'JoyShockMapper/src/JoyShock.cpp').read_text()

def balanced(signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

functions = '\n'.join(balanced(signature) for signature in [
    'void JoyShock::getSmoothedGyro(', 'void JoyShock::applyGyroDecaySmoothing(',
    'void JoyShock::disableGyroDecaySmoothing()'])
header = (ROOT / 'JoyShockMapper/include/JoyShock.h').read_text()
assert 'static constexpr int MAX_GYRO_SAMPLES = 256;' in header
main = (ROOT / 'JoyShockMapper/src/main.cpp').read_text()
assert 'clamp(numGyroSamples, 1.f, float(JoyShock::MAX_GYRO_SAMPLES))' in main
cutoff_start = main.index('\tauto speed = jc->getSetting(SettingID::GYRO_CUTOFF_SPEED);')
cutoff_end = main.index('\n\t// Handle _buttons before GYRO', cutoff_start)
cutoff = main[cutoff_start:cutoff_end]
program = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include "JoyShockMapper.h"
#include "GyroSteadying.h"
#include <type_traits>
using namespace std;
struct JoyShock {
 static constexpr int MAX_GYRO_SAMPLES=256;
 array<FloatXY,MAX_GYRO_SAMPLES> _gyroSamples{}; int _frontGyroSample=0;
 float _gyroDecayX=0,_gyroDecayY=0; bool _gyroDecayInit=false,_gyroDecayEnabledLast=false;
 void getSmoothedGyro(float,float,float,float,float,int,float&,float&);
 void applyGyroDecaySmoothing(float,float,float,float,float,float&,float&);
 void disableGyroDecaySmoothing();
};
NATIVE
struct CutoffSettings {
 float speed=0,recovery=0;
 float getSetting(SettingID key) {return key==SettingID::GYRO_CUTOFF_SPEED?speed:recovery;}
 template<class T> T getSetting(SettingID) {
  if constexpr(is_same_v<T, FloatXY>) return FloatXY(0,0); // exercise legacy floor default
  else if constexpr(is_same_v<T, Switch>) return Switch::OFF;
  else return GyroOutput::MOUSE;
 }
};
void applyCutoff(CutoffSettings* jc,float &gyroX,float &gyroY) {
 float gyroLength=sqrt(gyroX*gyroX+gyroY*gyroY);
 NATIVE_CUTOFF
}
void near(float a,float b) {assert(isfinite(a) && abs(a-b)<.00001f);}
int main() {
 CutoffSettings cutoff;
 float cx=3,cy=4;applyCutoff(&cutoff,cx,cy);near(cx,3);near(cy,4);
 cutoff.speed=4;cutoff.recovery=12;
 cx=0;cy=3;applyCutoff(&cutoff,cx,cy);near(cx,0);near(cy,0);
 cx=0;cy=8;applyCutoff(&cutoff,cx,cy);near(cy,4); // halfway recovery
 cx=12;cy=0;applyCutoff(&cutoff,cx,cy);near(cx,12);
 cutoff.recovery=4;cx=3;cy=0;applyCutoff(&cutoff,cx,cy);near(cx,0);
 cx=4;cy=0;applyCutoff(&cutoff,cx,cy);near(cx,4); // hard cutoff includes boundary
 cutoff.speed=0;cutoff.recovery=10;cx=3;cy=4;applyCutoff(&cutoff,cx,cy);near(cx,1.5f);near(cy,2);
 float x,y; JoyShock legacy;
 legacy.getSmoothedGyro(1,2,2.24f,5,10,4,x,y);near(x,.25f);near(y,.5f);
 for(int i=0;i<3;i++)legacy.getSmoothedGyro(1,2,2.24f,5,10,4,x,y);
 near(x,1);near(y,2);
 JoyShock bounded;bounded.getSmoothedGyro(1,0,1,5,10,1000000,x,y);near(x,1.f/256);
 JoyShock minimum;minimum.getSmoothedGyro(1,2,2.24f,5,10,0,x,y);near(x,1);near(y,2);
 JoyShock disabled;disabled.getSmoothedGyro(3,-4,5,0,0,32,x,y);near(x,3);near(y,-4);
 for(int samples : {50,100,200}) {
  JoyShock decay;decay.applyGyroDecaySmoothing(0,0,.01f,.125f,100,x,y);
  for(int i=0;i<samples;i++)decay.applyGyroDecaySmoothing(10,0,1.f/samples,.125f,100,x,y);
  near(x,8.1f*(1.f-expf(-1.f/(.125f*.81f)))+1.9f);near(y,0);
  decay.applyGyroDecaySmoothing(100,0,.01f,.125f,100,x,y);near(x,100);
  decay.disableGyroDecaySmoothing();decay.applyGyroDecaySmoothing(3,-4,.01f,.125f,100,x,y);near(x,3);near(y,-4);
  decay.applyGyroDecaySmoothing(2,1,.01f,.125f,0,x,y);near(x,2);near(y,1);
 }
 cout<<"PASS: actual cutoff/recovery boundaries, legacy averaging, bounded ring history, disabled thresholds, speed-decay EMA, reset and sample-rate invariance\n";
}
'''.replace('NATIVE_CUTOFF', cutoff).replace('NATIVE', functions)
compiler = shutil.which('g++') or shutil.which('clang++')
vcvars = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
with tempfile.TemporaryDirectory() as directory:
    folder = Path(directory)
    cpp, binary = folder / 'smoothing.cpp', folder / ('smoothing.exe' if os.name == 'nt' else 'smoothing')
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
