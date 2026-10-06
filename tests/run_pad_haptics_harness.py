"""Compile production feedback selection/edge routing with a recording actuator boundary."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent
source = (ROOT / 'JoyShockMapper/src/main.cpp').read_text(encoding='utf-8')


def function(signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


stub = r'''
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
#include "JoyShockMapper.h"
struct Pulse {int side,effect,gain;};
struct JoyShock {
  bool padClickWasOn[2]{};
  std::map<SettingID,float> strengths;
  std::map<SettingID,Switch> switches;
  std::map<SettingID,HapticEffect> effects;
  std::vector<Pulse> pulses;
  float getSetting(SettingID id){return strengths[id];}
  template<class T> T getSetting(SettingID id) {
    if constexpr(std::is_same_v<T,Switch>) return switches[id];
    else return effects[id];
  }
  void fireHaptic(int side,int effect,int gain){pulses.push_back({side,effect,gain});}
};
'''
tests = r'''
int main() {
 auto js=std::make_shared<JoyShock>();
 js->strengths[SettingID::TOUCHPAD_CLICK_HAPTIC_INTENSITY]=30;
 js->strengths[SettingID::TOUCHPAD_RELEASE_HAPTIC_INTENSITY]=20;
 js->effects[SettingID::TOUCHPAD_CLICK_HAPTIC_EFFECT]=HapticEffect::CLICK;
 js->effects[SettingID::TOUCHPAD_RELEASE_HAPTIC_EFFECT]=HapticEffect::TICK;
 js->strengths[SettingID::LEFT_TOUCHPAD_CLICK_HAPTIC_INTENSITY]=70;
 js->strengths[SettingID::LEFT_TOUCHPAD_RELEASE_HAPTIC_INTENSITY]=0;
 js->effects[SettingID::LEFT_TOUCHPAD_CLICK_HAPTIC_EFFECT]=HapticEffect::SWEEP;
 js->strengths[SettingID::RIGHT_TOUCHPAD_CLICK_HAPTIC_INTENSITY]=90;
 js->effects[SettingID::RIGHT_TOUCHPAD_CLICK_HAPTIC_EFFECT]=HapticEffect::TONE;
 // Both sides retain shared feedback until they explicitly opt in.
 updatePadClickHaptics(js,true,true);assert(js->pulses.size()==2);
 assert(js->pulses[0].side==1&&js->pulses[1].side==2);
 for(auto pulse:js->pulses) assert(pulse.effect==int(HapticEffect::CLICK)&&pulse.gain==hapticGainDb(30));
 updatePadClickHaptics(js,true,true);assert(js->pulses.size()==2);
 updatePadClickHaptics(js,false,false);assert(js->pulses.size()==4);
 js->pulses.clear();js->switches[SettingID::LEFT_TOUCHPAD_HAPTICS]=Switch::ON;
 updatePadClickHaptics(js,true,true);assert(js->pulses.size()==2);
 assert(js->pulses[0].effect==int(HapticEffect::SWEEP)&&js->pulses[0].gain==hapticGainDb(70));
 assert(js->pulses[1].effect==int(HapticEffect::CLICK)&&js->pulses[1].gain==hapticGainDb(30));
 updatePadClickHaptics(js,false,false);assert(js->pulses.size()==3&&js->pulses.back().side==2);
 js->switches[SettingID::RIGHT_TOUCHPAD_HAPTICS]=Switch::ON;
 updatePadClickHaptics(js,false,true);assert(js->pulses.back().effect==int(HapticEffect::TONE)&&js->pulses.back().gain==hapticGainDb(90));
 auto count=js->pulses.size();js->switches[SettingID::RIGHT_TOUCHPAD_HAPTICS]=Switch::OFF;
 updatePadClickHaptics(js,false,true);assert(js->pulses.size()==count);
 updatePadClickHaptics(js,false,false);assert(js->pulses.back().gain==hapticGainDb(20));
 // Movement uses the identical source selector for strength, effect and spacing.
 js->strengths[SettingID::TOUCHPAD_HAPTIC_INTERVAL]=250;
 js->strengths[SettingID::LEFT_TOUCHPAD_HAPTIC_INTERVAL]=500;
 js->strengths[SettingID::RIGHT_TOUCHPAD_HAPTIC_INTERVAL]=100;
 assert(padFeedbackSetting<float>(*js,0,SettingID::TOUCHPAD_HAPTIC_INTERVAL,SettingID::LEFT_TOUCHPAD_HAPTIC_INTERVAL,SettingID::RIGHT_TOUCHPAD_HAPTIC_INTERVAL)==500);
 assert(padFeedbackSetting<float>(*js,1,SettingID::TOUCHPAD_HAPTIC_INTERVAL,SettingID::LEFT_TOUCHPAD_HAPTIC_INTERVAL,SettingID::RIGHT_TOUCHPAD_HAPTIC_INTERVAL)==250);
 js->switches[SettingID::RIGHT_TOUCHPAD_HAPTICS]=Switch::ON;
 assert(padFeedbackSetting<float>(*js,1,SettingID::TOUCHPAD_HAPTIC_INTERVAL,SettingID::LEFT_TOUCHPAD_HAPTIC_INTERVAL,SettingID::RIGHT_TOUCHPAD_HAPTIC_INTERVAL)==100);
 js->effects[SettingID::RIGHT_TOUCHPAD_CLICK_HAPTIC_EFFECT]=HapticEffect::OFF;
 count=js->pulses.size();updatePadClickHaptics(js,false,true);assert(js->pulses.size()==count);
 std::cout<<"PASS: production feedback selector and click/release routing, shared compatibility, independent gains/effects/spacing, disabled edges and held policy changes\n";
}
'''
with tempfile.TemporaryDirectory(prefix='jsm-pad-feedback-') as temporary:
    folder = Path(temporary)
    cpp, binary = folder / 'pad.cpp', folder / 'pad.exe'
    cpp.write_text(stub + 'template<typename T>\n' + function('static T padFeedbackSetting(') + function('static void updatePadClickHaptics(') + tests, encoding='utf-8')
    vcvars = Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat')
    batch = folder / 'build.cmd'
    batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /utf-8 /EHsc /std:c++20 /I"{ROOT / "JoyShockMapper/include"}" /I"{REPO / "build-jsm-sdl/_deps/magic_enum-src/include"}" "{cpp}" /Fe:"{binary}"\n', encoding='utf-8')
    result = subprocess.run(['cmd.exe', '/d', '/c', str(batch)], cwd=folder, capture_output=True, text=True)
    if result.returncode:
        print(result.stdout, result.stderr)
        raise SystemExit(result.returncode)
    raise SystemExit(subprocess.run([str(binary)]).returncode)
