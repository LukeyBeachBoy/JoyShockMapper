"""Run the actual native gyro-stick formula with recorded gamepad output."""
from pathlib import Path
import os
import re
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


types = (ROOT / 'JoyShockMapper/include/JoyShockMapper.h').read_text()
enums = '\n'.join(balanced(types, f'enum class {name}') + ';' for name in ['GyroOutput', 'StickMode', 'GyroAxisMask', 'Switch'])
source = (ROOT / 'JoyShockMapper/src/JoyShock.cpp').read_text()
native = balanced(source, 'bool JoyShock::processGyroStick(')
main = (ROOT / 'JoyShockMapper/src/main.cpp').read_text()
output = 'static void publishOutput(shared_ptr<JoyShock>& jc, const IMU_STATE& imu) {\n' + \
    'GyroOutput gyroOutput = jc->getSetting<GyroOutput>(SettingID::GYRO_OUTPUT);\n' + \
    balanced(main, 'if (!jc->processed_gyro_stick)') + '\n}'
stub = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include "GyroDeflection.h"
#include "magic_enum.hpp"
using namespace std;
NATIVE_ENUMS
enum class SettingID { GYRO_OUTPUT, LEFT_STICK_UNDEADZONE_INNER, LEFT_STICK_UNDEADZONE_OUTER, LEFT_STICK_UNPOWER, LEFT_STICK_VIRTUAL_SCALE,
 RIGHT_STICK_UNDEADZONE_INNER, RIGHT_STICK_UNDEADZONE_OUTER, RIGHT_STICK_UNPOWER, RIGHT_STICK_VIRTUAL_SCALE, VIRTUAL_STICK_CALIBRATION, LEFT_STICK_DEADZONE_PROBE, RIGHT_STICK_DEADZONE_PROBE };
struct IMU_STATE {float accelX,accelY,accelZ,gyroX,gyroY,gyroZ;};
struct Pad { float x=0, y=0; bool left=false; int reports=0,motionReports=0; IMU_STATE motion{};
 void setStick(float a,float b,bool side) { x=a;y=b;left=side;++reports; }
 void setGyro(int,float ax,float ay,float az,float gx,float gy,float gz) { motion={ax,ay,az,gx,gy,gz};++motionReports; }
};
struct Context { shared_ptr<Pad> _vigemController=make_shared<Pad>(); };
struct JoyShock {
 shared_ptr<Context> _context=make_shared<Context>();
 map<SettingID,float> settings{{SettingID::GYRO_OUTPUT,float(GyroOutput::RIGHT_STICK)}, {SettingID::VIRTUAL_STICK_CALIBRATION,360},
 {SettingID::RIGHT_STICK_UNDEADZONE_INNER,.2f}, {SettingID::RIGHT_STICK_UNDEADZONE_OUTER,.05f}, {SettingID::RIGHT_STICK_UNPOWER,2}, {SettingID::RIGHT_STICK_VIRTUAL_SCALE,1},
 {SettingID::LEFT_STICK_VIRTUAL_SCALE,1}, {SettingID::LEFT_STICK_DEADZONE_PROBE,float(Switch::ON)}, {SettingID::RIGHT_STICK_DEADZONE_PROBE,float(Switch::ON)}};
 template<class T=float> T getSetting(SettingID id) { return static_cast<T>(settings[id]); }
 float gyroXVelocity=0, gyroYVelocity=0; bool processed_gyro_stick=false;
 int _timeNow=0;
 bool hasVirtualController() {return bool(_context->_vigemController);}
 bool processGyroStick(float,float,float,StickMode,bool);
};
NATIVE_PROCESS
NATIVE_OUTPUT
void near(float actual,float expected) { assert(isfinite(actual) && abs(actual-expected)<.00001f); }
int main() {
 // Selected angular travel feeds the same production inverse game-stick curve.
 // Calibration is only the native velocity transport scale in deflection mode.
 for (float calibration : {180.f, 360.f, 720.f}) {
  JoyShock angleJs; GyroDeflection angle;
  angleJs.settings[SettingID::VIRTUAL_STICK_CALIBRATION]=calibration;
  angleJs.settings[SettingID::RIGHT_STICK_DEADZONE_PROBE]=float(Switch::OFF);
  angle.step(0,0,.01f,30,60,true,true,1);
  auto vector=angle.step(75,0,.1f,30,60,true,true,1);
  angleJs.gyroXVelocity=vector.first*calibration;
  angleJs.gyroYVelocity=vector.second*calibration;
  angleJs.processGyroStick(0,0,0,StickMode::RIGHT_STICK,false);
  near(angleJs._context->_vigemController->x,.575f);
  vector=angle.step(0,0,.1f,30,60,true,true,1);
  angleJs.gyroXVelocity=vector.first*calibration;
  angleJs.processGyroStick(0,0,0,StickMode::RIGHT_STICK,false);
  near(angleJs._context->_vigemController->x,.575f); // held angle, zero velocity
  vector=angle.step(0,0,.1f,30,60,true,false,1);
  angleJs.gyroXVelocity=vector.first*calibration;
  angleJs.processGyroStick(0,0,0,StickMode::RIGHT_STICK,false);
  near(angleJs._context->_vigemController->x,0); // suppression clears angle
 }
 JoyShock js; auto pad=js._context->_vigemController;
 js.processGyroStick(0,0,0,StickMode::RIGHT_STICK,false); near(pad->x,.2f); near(pad->y,0); assert(!pad->left);
 js.settings[SettingID::RIGHT_STICK_DEADZONE_PROBE]=float(Switch::OFF);
 js.processGyroStick(0,0,0,StickMode::RIGHT_STICK,false);near(pad->x,0);near(pad->y,0);
 js.gyroXVelocity=90; js.processGyroStick(0,0,0,StickMode::RIGHT_STICK,false); near(pad->x,.575f);
 js.settings[SettingID::RIGHT_STICK_VIRTUAL_SCALE]=0;
 js.processGyroStick(0,0,0,StickMode::RIGHT_STICK,false); near(pad->x,.575f); // scale never changes gyro
 js.gyroXVelocity=3600; js.processGyroStick(0,0,0,StickMode::RIGHT_STICK,false); near(pad->x,.95f);
 js.gyroXVelocity=0; js.gyroYVelocity=90; js.processGyroStick(0,0,0,StickMode::RIGHT_STICK,false); near(pad->x,0); near(pad->y,-.575f);
 js.gyroYVelocity=.001f; js.processGyroStick(0,0,0,StickMode::RIGHT_STICK,false); near(pad->x,0); near(pad->y,0); // native cutoff
 js.gyroYVelocity=0; js.settings[SettingID::GYRO_OUTPUT]=float(GyroOutput::MOUSE);
 js.settings[SettingID::RIGHT_STICK_VIRTUAL_SCALE]=1; js.gyroXVelocity=90;
 js.processGyroStick(.5f,0,.5f,StickMode::RIGHT_STICK,false); near(pad->x,.5f); // ordinary stick preserved when gyro targets mouse
 js.settings[SettingID::GYRO_OUTPUT]=float(GyroOutput::LEFT_STICK); js.settings[SettingID::LEFT_STICK_UNDEADZONE_INNER]=.1f;
 js.gyroXVelocity=180; js.processGyroStick(0,0,0,StickMode::LEFT_STICK,false); near(pad->x,.55f); assert(pad->left);
 js.gyroXVelocity=0;js.processGyroStick(0,0,0,StickMode::LEFT_STICK,false);near(pad->x,.1f); // right OFF does not affect left
 js.settings[SettingID::LEFT_STICK_DEADZONE_PROBE]=float(Switch::OFF);
 js.processGyroStick(0,0,0,StickMode::LEFT_STICK,false);near(pad->x,0);near(pad->y,0);
 js.settings[SettingID::VIRTUAL_STICK_CALIBRATION]=0; int reports=pad->reports;
 assert(!js.processGyroStick(0,0,0,StickMode::LEFT_STICK,false)); assert(pad->reports==reports);
 js.settings[SettingID::VIRTUAL_STICK_CALIBRATION]=360; js.settings[SettingID::LEFT_STICK_UNDEADZONE_OUTER]=.95f;
 assert(!js.processGyroStick(0,0,0,StickMode::LEFT_STICK,false)); assert(pad->reports==reports);
 js._context->_vigemController.reset(); js.processGyroStick(0,0,0,StickMode::LEFT_STICK,false);
 assert(magic_enum::enum_cast<GyroAxisMask>("XY")==GyroAxisMask::XY);
 assert(magic_enum::enum_cast<GyroAxisMask>("XYZ")==GyroAxisMask::XYZ);
 assert(int(GyroAxisMask::YZ)==(int(GyroAxisMask::Y)|int(GyroAxisMask::Z)));
 auto motionJs=make_shared<JoyShock>();auto motionPad=motionJs->_context->_vigemController;
 const IMU_STATE imu{1,2,3,4,5,6};motionJs->settings[SettingID::GYRO_OUTPUT]=float(GyroOutput::PS_MOTION);
 // Real physical passthrough and virtual-flick calls use LEFT/RIGHT_STICK;
 // neither claims PS_MOTION was processed. Exercise the actual poll branch.
 motionJs->processGyroStick(.5f,0,.5f,StickMode::LEFT_STICK,false);
 assert(!motionJs->processed_gyro_stick);
 publishOutput(motionJs,imu);assert(motionPad->motionReports==1);
 near(motionPad->motion.accelX,1);near(motionPad->motion.accelY,2);near(motionPad->motion.accelZ,3);
 near(motionPad->motion.gyroX,4);near(motionPad->motion.gyroY,5);near(motionPad->motion.gyroZ,6);
 int stickReports=motionPad->reports;
 motionJs->gyroXVelocity=180; // synthetic virtual-flick turn, with forceOutput
 motionJs->processGyroStick(0,0,0,StickMode::RIGHT_STICK,true);
 assert(!motionJs->processed_gyro_stick);stickReports=motionPad->reports;
 publishOutput(motionJs,imu);assert(motionPad->motionReports==2&&motionPad->reports==stickReports);
 motionJs->settings[SettingID::GYRO_OUTPUT]=float(GyroOutput::RIGHT_STICK);
 motionJs->processed_gyro_stick=true;publishOutput(motionJs,imu);assert(motionPad->motionReports==2&&motionPad->reports==stickReports);
 motionJs->processed_gyro_stick=false;publishOutput(motionJs,imu);assert(motionPad->reports==stickReports+1);
 motionJs->settings[SettingID::GYRO_OUTPUT]=float(GyroOutput::PS_MOTION);
 motionJs->processed_gyro_stick=false;motionJs->_context->_vigemController.reset();publishOutput(motionJs,imu);
 cout << "PASS: actual native anti-deadzone/unpower, velocity cap, Y direction, cutoff, target isolation, independent PS motion with virtual flick/stick, invalid geometry and axis masks\n";
}
'''
magic = ROOT.parent / 'build-jsm-sdl/_deps/magic_enum-src/include'
compiler = shutil.which('g++') or shutil.which('clang++')
vcvars = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
with tempfile.TemporaryDirectory() as folder:
    folder = Path(folder)
    cpp, binary = folder / 'gyro.cpp', folder / ('gyro.exe' if os.name == 'nt' else 'gyro')
    cpp.write_text(stub.replace('NATIVE_ENUMS', enums).replace('NATIVE_PROCESS', native).replace('NATIVE_OUTPUT', output))
    if compiler:
        cmd = [compiler, '-std=c++17', '-O2', '-I', str(magic), '-I', str(ROOT / 'JoyShockMapper/include'), str(cpp), '-o', str(binary)]
    else:
        batch = folder / 'build.bat'
        batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /EHsc /std:c++17 /I"{magic}" /I"{ROOT / "JoyShockMapper/include"}" "{cpp}" /Fe:"{binary}"\n')
        cmd = ['cmd.exe', '/d', '/c', str(batch)]
    result = subprocess.run(cmd, cwd=folder, capture_output=True, text=True)
    if result.returncode:
        print(result.stdout, result.stderr)
        raise SystemExit(result.returncode)
    raise SystemExit(subprocess.run([str(binary)]).returncode)
