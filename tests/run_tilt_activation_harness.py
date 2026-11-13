"""Run the production tilt gate and cleanup with synthetic inputs; no hardware."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def balanced(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

header = (ROOT / 'JoyShockMapper/include/JoyShockMapper.h').read_text()
main = (ROOT / 'JoyShockMapper/src/main.cpp').read_text()
joy = (ROOT / 'JoyShockMapper/src/JoyShock.cpp').read_text()
stick = (ROOT / 'JoyShockMapper/include/Stick.h').read_text()
variables = (ROOT / 'JoyShockMapper/include/JSMVariable.hpp').read_text()
operators = (ROOT / 'JoyShockMapper/src/operators.cpp').read_text()
default_start = main.index('  GyroSettings tilt_defaults;')
defaults = main[default_start:main.index('  tilt_settings->setFilter', default_start)]
variable_types = variables[variables.index('// Global ID generator'):variables.index('// A combo map')]
types = '\n'.join(balanced(header, name) + ';' for name in [
    'enum class ButtonID', 'enum class SettingID', 'enum class StickMode',
    'enum class GyroIgnoreMode', 'enum class JoyconMask', 'struct GyroSettings'])
cleanup = balanced(joy, 'void JoyShock::stopTilt()')
start = main.index('  // Clear the previous tilt target BEFORE')
gate = main[start:main.index('\t// sticks!', start)]
cpp = r'''
#include <cassert>
#include <cmath>
#include <chrono>
#include <array>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include <type_traits>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include "@@INCLUDE@@/StickDirections.h"
#include "@@INCLUDE@@/ControllerContext.h"
using namespace std;
@@TYPES@@
@@EQUALITY@@
bool operator!=(const GyroSettings &lhs, const GyroSettings &rhs) { return !(lhs == rhs); }
@@VARIABLES@@
struct ScrollAxis {
 bool initialized = false;
 int leftovers = 0;
 void reset(chrono::steady_clock::time_point) { leftovers = 0; }
 void init(int&, int&) { initialized = true; }
};
@@STICK@@
struct Virtual {
 float left = .75f, right = .5f;
 void setStick(float x, float, bool isLeft) { (isLeft ? left : right) = x; }
};
struct Context { shared_ptr<Virtual> _vigemController = make_shared<Virtual>(); };
constexpr int JS_SPLIT_TYPE_FULL = 3;
struct JoyShock {
 GyroSettings activation;
 StickMode mode = StickMode::LEFT_STEER_X;
 JoyconMask mask = JoyconMask::USE_BOTH;
 int _splitType = JS_SPLIT_TYPE_FULL, _handle = 0;
 bool _tiltWasActive = true;
 StickMode _tiltOutputMode = StickMode::LEFT_STEER_X;
 shared_ptr<Context> _context = make_shared<Context>();
 chrono::steady_clock::time_point _timeNow;
 vector<int> _buttons = vector<int>(512);
 map<ButtonID, bool> pressed;
 float _windingAngleLeft = 0, _windingAngleRight = 0;
 Stick _motionStick{SettingID::MOTION_DEADZONE_INNER, SettingID::MOTION_DEADZONE_OUTER,
   SettingID::MOTION_RING_MODE, SettingID::MOTION_STICK_MODE, ButtonID::MRING,
   ButtonID::MLEFT, ButtonID::MRIGHT, ButtonID::MUP, ButtonID::MDOWN};
 bool isPressed(ButtonID b) { return pressed[b]; }
 pair<float, float> getStickInput(bool left);
 void handleButtonChange(ButtonID b, bool down) { pressed[b] = down; }
 void stopTilt();
 template<class T> T getSetting(SettingID id) {
  if constexpr(is_same_v<T, GyroSettings>) return activation;
  else if constexpr(is_same_v<T, JoyconMask>) return mask;
  else return id == SettingID::MOTION_STICK_MODE ? mode : StickMode::AIM;
 }
 float getSetting(SettingID) { return .15f; }
};
struct Inputs {
 float left = 0, right = 0;
 float GetLeftX(int) { return left; } float GetLeftY(int) { return 0; }
 float GetRightX(int) { return right; } float GetRightY(int) { return 0; }
};
auto jsl = make_shared<Inputs>();
pair<float, float> JoyShock::getStickInput(bool left) { return {left ? jsl->left : jsl->right, 0.f}; }
@@CLEANUP@@
bool gate(shared_ptr<JoyShock> jc) {
 @@GATE@@
 return tiltEnabled;
}
int main() {
 @@DEFAULTS@@
 auto jc = make_shared<JoyShock>();
 jc->activation = tilt_settings->value();
 assert(!gate(jc)); // Production registration starts with tilt off.
 tilt_settings->set(GyroSettings()); // Explicit TILT_OFF = NONE still enables it.
 jc->activation = tilt_settings->value();
 assert(gate(jc));
 tilt_settings->reset(); // The real JSMSetting reset restores the off default.
 jc->activation = tilt_settings->value();
 assert(!gate(jc));
 delete tilt_settings;
 jc->_tiltWasActive = true;
 jc->activation.always_off = true; // TILT_ON = NONE
 jc->pressed[ButtonID::MLEFT] = jc->pressed[ButtonID::MRING] = jc->pressed[ButtonID::LEAN_LEFT] = true;
 jc->pressed[ButtonID::S] = true; // An unrelated physical action stays held.
 jc->_motionStick.lastX = 1; jc->_motionStick.is_flicking = true;
 jc->_motionStick.acceleration = 10; jc->_motionStick.edgePushAmount = 3;
 assert(!gate(jc));
 assert(jc->_context->_vigemController->left == 0);
 assert(jc->_context->_vigemController->right == .5f);
 assert(!jc->pressed[ButtonID::MLEFT] && !jc->pressed[ButtonID::MRING] && !jc->pressed[ButtonID::LEAN_LEFT]);
 assert(jc->pressed[ButtonID::S]);
 assert(!jc->_tiltWasActive && !jc->_motionStick.is_flicking && jc->_motionStick.lastX == 0);
 assert(jc->_motionStick.acceleration == 1 && jc->_motionStick.edgePushAmount == 0);
 assert(jc->_motionStick.flick_percent_done == 1 && jc->_motionStick.scroll.isInitialized());
 jc->_context->_vigemController->left = .25f; assert(!gate(jc));
 assert(jc->_context->_vigemController->left == .25f); // No repeated clearing of other sources.
 jc->activation.button = ButtonID::R3;
 assert(!gate(jc)); jc->pressed[ButtonID::R3] = true; assert(gate(jc));
 jc->activation.always_off = false; assert(!gate(jc));
 jc->pressed[ButtonID::R3] = false; assert(gate(jc));
 jc->activation.always_off = true;
 jc->activation.conditions = {{ButtonID::MISC5, false}, {ButtonID::MISC6, true}};
 jc->activation.require_all = true;
 jc->pressed[ButtonID::MISC5] = true; assert(gate(jc));
 jc->pressed[ButtonID::MISC6] = true; assert(!gate(jc));
 jc->activation.ignore_mode = GyroIgnoreMode::LEFT_STICK;
 jsl->left = 1; assert(gate(jc)); jsl->left = 0; assert(!gate(jc));
 jc->activation = GyroSettings();
 jc->_tiltWasActive = true; jc->_tiltOutputMode = StickMode::LEFT_WIND_X;
 jc->_windingAngleLeft = 90; jc->mode = StickMode::RIGHT_STEER_X;
 assert(gate(jc)); assert(jc->_windingAngleLeft == 0 && !jc->_tiltWasActive);
 jc->_splitType = 1; jc->mask = JoyconMask::IGNORE_LEFT; assert(!gate(jc));
 cout << "PASS: production tilt defaults/reset off, explicit enable, hold on/off, Any/All, stick input, mask, target changes, owned releases, flick/hybrid reset and independent output cleanup\n";
}
'''.replace('@@TYPES@@', types).replace('@@STICK@@', balanced(stick, 'struct Stick') + ';')
cpp = cpp.replace('@@CLEANUP@@', cleanup).replace('@@GATE@@', gate)
cpp = cpp.replace('@@DEFAULTS@@', defaults).replace('@@VARIABLES@@', variable_types)
cpp = cpp.replace('@@EQUALITY@@', balanced(operators, 'bool operator==(const GyroSettings'))
cpp = cpp.replace('@@INCLUDE@@', (ROOT / 'JoyShockMapper/include').as_posix())
# The actual Stick's helper only needs initialized() on its ScrollAxis.
cpp = cpp.replace(' void init(int&, int&) { initialized = true; }', ' void init(int&, int&) { initialized = true; } bool isInitialized() const { return initialized; }')
vcvars = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
with tempfile.TemporaryDirectory() as folder:
    folder = Path(folder)
    source, binary = folder / 'tilt.cpp', folder / 'tilt.exe'
    source.write_text(cpp)
    batch = folder / 'build.bat'
    batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /EHsc /std:c++17 "{source}" /Fe:"{binary}"\n')
    env = {key.upper(): value for key, value in os.environ.items()}
    result = subprocess.run(['cmd.exe', '/d', '/c', str(batch)], cwd=folder, env=env, capture_output=True, text=True)
    if result.returncode:
        print(result.stdout, result.stderr)
        raise SystemExit(result.returncode)
    result = subprocess.run([str(binary)], env=env)
    if result.returncode:
        for index, line in enumerate(cpp.splitlines(), 1):
            if index >= len(cpp.splitlines()) - 48:
                print(f'{index}: {line}')
    raise SystemExit(result.returncode)
