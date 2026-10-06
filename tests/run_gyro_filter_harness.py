"""Exercise production gyro One Euro filtering with recorded scoped settings.

The setting resolver is a controlled boundary; the filter structs, recurrence
and JoyShock entry point are extracted from the current native source.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def balanced(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


header = (ROOT / 'JoyShockMapper/include/JoyShock.h').read_text()
source = (ROOT / 'JoyShockMapper/src/JoyShock.cpp').read_text()
structs = '\n'.join(balanced(header, f'struct {name}') + ';'
                    for name in ['LowPassFilter1E', 'OneEuroFilter'])
functions = '\n'.join(balanced(source, signature) for signature in [
    'float OneEuroFilter::filter(float x, float dt, float minCutoff, float beta)',
    'void JoyShock::applyOneEuroFilter(', 'void JoyShock::resetOneEuroFilter()'])
program = r'''
#include <cassert>
#include <cmath>
#include <iostream>
NATIVE_STRUCTS
enum class SettingID { ONE_EURO_MIN_CUTOFF, ONE_EURO_SPEED_COEFF };
// A global read must not be used by the production gyro entry point.
float OneEuroFilter::filter(float, float) { assert(false); return 0; }
struct JoyShock {
 OneEuroFilter _oneEuroX, _oneEuroY;
 float baseCutoff=6, baseCoeff=.3f, heldCutoff=.5f, heldCoeff=0;
 bool held=false; int reads=0;
 float getSetting(SettingID id) {
  ++reads;
  return id==SettingID::ONE_EURO_MIN_CUTOFF ? (held?heldCutoff:baseCutoff) : (held?heldCoeff:baseCoeff);
 }
 void applyOneEuroFilter(float,float,float,float&,float&);
 void resetOneEuroFilter();
};
NATIVE_FUNCTIONS
void near(float a,float b) { assert(std::isfinite(a) && std::abs(a-b)<.00001f); }
int main() {
 JoyShock scoped, independent;
 OneEuroFilter expectedX, expectedY;
 float x,y,otherX,otherY;
 // Both axes share tuning, with independent state. Changing and releasing a
 // held override retains that state rather than snapping the output to raw.
 for(int sample=0;sample<60;++sample) {
  scoped.held=sample>=10 && sample<35;
  const float rawX=sample%2?20.f:-12.f, rawY=sample%3?7.f:-3.f;
  const float cutoff=scoped.held?scoped.heldCutoff:scoped.baseCutoff;
  const float coeff=scoped.held?scoped.heldCoeff:scoped.baseCoeff;
  scoped.applyOneEuroFilter(rawX,rawY,.004f,x,y);
  independent.applyOneEuroFilter(rawX,rawY,.004f,otherX,otherY);
  near(x,expectedX.filter(rawX,.004f,cutoff,coeff));
  near(y,expectedY.filter(rawY,.004f,cutoff,coeff));
  if(sample==20) assert(std::abs(x-otherX)>.1f);
  assert(!independent.held);
 }
 assert(scoped.reads==120); // once per setting per frame, not per axis
 scoped.resetOneEuroFilter(); scoped.held=true;
 scoped.applyOneEuroFilter(3,-7,.004f,x,y);near(x,3);near(y,-7);
 // A stronger speed coefficient makes a sudden motion more responsive.
 JoyShock slower, faster; slower.baseCoeff=0;faster.baseCoeff=1;
 slower.applyOneEuroFilter(0,0,.004f,x,y);faster.applyOneEuroFilter(0,0,.004f,otherX,otherY);
 slower.applyOneEuroFilter(100,0,.004f,x,y);faster.applyOneEuroFilter(100,0,.004f,otherX,otherY);
 assert(otherX>x && otherX<=100);
 std::cout << "PASS: production gyro filter uses scoped cutoff/speed, continuous held/release history, independent devices and reset\n";
}
'''.replace('NATIVE_STRUCTS', structs).replace('NATIVE_FUNCTIONS', functions)
compiler = shutil.which('g++') or shutil.which('clang++')
vcvars = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
with tempfile.TemporaryDirectory() as folder:
    folder = Path(folder)
    cpp, binary = folder / 'filter.cpp', folder / ('filter.exe' if os.name == 'nt' else 'filter')
    cpp.write_text(program)
    if compiler:
        command = [compiler, '-std=c++17', '-O2', str(cpp), '-o', str(binary)]
    else:
        batch = folder / 'build.bat'
        batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /EHsc /std:c++17 "{cpp}" /Fe:"{binary}"\n')
        command = ['cmd.exe', '/d', '/c', str(batch)]
    result = subprocess.run(command, cwd=folder, capture_output=True, text=True)
    if result.returncode:
        print(result.stdout, result.stderr)
        raise SystemExit(result.returncode)
    raise SystemExit(subprocess.run([str(binary)]).returncode)
