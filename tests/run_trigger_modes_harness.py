"""Actual dual-stage processing, with a recording button/driver boundary.

Extracts JoyShock's trigger FSM and contact tests. Mapping/OS dispatch is not
exercised here; the separate Mapping/DigitalButton runtime harness owns that.
"""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent
INCLUDE = ROOT / 'JoyShockMapper/include'


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


joy = (ROOT / 'JoyShockMapper/src/JoyShock.cpp').read_text(encoding='utf-8')
main = (ROOT / 'JoyShockMapper/src/main.cpp').read_text(encoding='utf-8')
poll = function(main, 'void joyShockPollCallback(')
assert poll.index('syncSteamPhysicalConditions(jc)') < poll.index('getSetting<GyroSettings>'), 'Hardware conditions must be fresh before gyro activation lookup'
digital = (ROOT / 'JoyShockMapper/src/DigitalButton.cpp').read_text(encoding='utf-8')
context_methods = digital[digital.index('namespace\n{\nconstexpr std::array<ButtonID, 8> steamConditionInputs'):digital.index('\nstruct Sync')]
state = function((INCLUDE / 'DigitalButton.h').read_text(encoding='utf-8'), 'enum class BtnState') + ';'
constants = '\n'.join(line for line in (INCLUDE / 'JslWrapper.h').read_text(encoding='utf-8').splitlines() if line.startswith('#define JS'))
stub = r'''
#include <cassert>
#include <chrono>
#include <limits>
#include <vector>
#include <map>
#include <iostream>
#include "JoyShockMapper.h"
#include "JslWrapper.h"
#include "InputGuards.h"
#undef COUT
#define COUT std::cout
#undef CERR
#define CERR std::cerr
std::ostream &operator<<(std::ostream &out, const ButtonID &id){return out<<int(id);}
using Time = std::chrono::steady_clock::time_point;
struct GetDuration { Time time_now; float out_duration=0; };
struct Button {
  Time start{};
  BtnState getState(){return BtnState::NoPress;}
  void sendEvent(Time now){start=now;}
  GetDuration sendEvent(GetDuration event){event.out_duration=std::chrono::duration<float,std::milli>(event.time_now-start).count();return event;}
};
struct Gamepad {float left=0,right=0;void setLeftTrigger(float v){left=v;} void setRightTrigger(float v){right=v;} };
struct DigitalButton {struct Context {
 std::unique_ptr<Gamepad> _vigemController;
 std::deque<ButtonID> chordStack{ButtonID::NONE};
 std::optional<std::array<bool,8>> steamConditions;
 void updateChordStack(bool,ButtonID);
 void syncSteamConditions(const std::array<bool,8>&);
};};
using Context=DigitalButton::Context;
namespace SettingsManager {
 template<class T> struct Value {T value(){return T(3);} };
 template<class T> Value<T>* getV(SettingID){static Value<T> value;return &value;}
 template<class T> Value<T>* get(SettingID id){return getV<T>(id);}
}
struct JoyShock {
 int _controllerType=JS_TYPE_STEAM_CONTROLLER_2026,_splitType=JS_SPLIT_TYPE_FULL,_handle=1;
 Time _timeNow{};
 Context context;Context *_context=&context;
 AdaptiveTriggerSetting _leftEffect{},_rightEffect{},_unusedEffect{};
 std::vector<Button> _buttons=std::vector<Button>(MAPPING_SIZE);
 bool _softPullDown[NUM_DUAL_STAGE_SOURCES]{},_fullPullDown[NUM_DUAL_STAGE_SOURCES]{};
 std::array<std::optional<TriggerMode>,NUM_DUAL_STAGE_SOURCES> _lastTriggerMode{};
 std::vector<DstState> _triggerState=std::vector<DstState>(NUM_DUAL_STAGE_SOURCES,DstState::NoPress);
 std::vector<std::deque<float>> _prevTriggerPosition=std::vector<std::deque<float>>(NUM_DUAL_STAGE_SOURCES,std::deque<float>(MAGIC_TRIGGER_SMOOTHING,0));
 std::array<std::optional<bool>,int(ButtonID::SIZE)> _virtualMenuInputs;
 std::map<ButtonID,bool> held;
 std::map<SettingID,float> settings{{SettingID::TRIGGER_THRESHOLD,.2f},{SettingID::TRIGGER_HYSTERESIS,.02f},{SettingID::TRIGGER_SKIP_DELAY,100.f}};
 TriggerMode leftPad=TriggerMode::NO_SKIP,rightPad=TriggerMode::NO_SKIP;
 template<class T=float> T getSetting(SettingID id) {
   if constexpr(std::is_same_v<T,TriggerMode>) return id==SettingID::LEFT_TOUCHPAD_DUAL_STAGE_MODE?leftPad:id==SettingID::RIGHT_TOUCHPAD_DUAL_STAGE_MODE?rightPad:TriggerMode::NO_SKIP;
   else return T(settings[id]);
 }
 void handleButtonChange(ButtonID id,bool value){held[id]=value;context.updateChordStack(value,id);}
 float getTriggerEffectStartPos();bool isSoftPullPressed(int,float);
 void handleTriggerChange(ButtonID,ButtonID,TriggerMode,float,AdaptiveTriggerSetting&);
};
struct Hardware {
 uint64_t buttons=0;bool left=false,right=false;
 uint64_t GetButtons(int){return buttons;}
 bool GetTouchDown(int,bool second){return second?right:left;}
 float GetLeftTrigger(int){return 0;}float GetRightTrigger(int){return 0;}
} hardware;
auto *jsl=&hardware;
void updatePadClickHaptics(std::shared_ptr<JoyShock>&,bool,bool){}
'''
tests = r'''
int main(){
 const ButtonID softs[]={ButtonID::MISC4,ButtonID::TOUCH,ButtonID::TOUCH};
 const ButtonID fulls[]={ButtonID::MISC3,ButtonID::MISC2,ButtonID::CAPTURE};
 for(int pad=0;pad<3;++pad) {
   JoyShock js;
   auto tick=[&](float position,TriggerMode mode=TriggerMode::NO_SKIP,int ms=10){js._timeNow+=std::chrono::milliseconds(ms);js.handleTriggerChange(softs[pad],fulls[pad],mode,position,js._unusedEffect);};
   // Contact must never pass the full-pull threshold or chatter as a click.
   tick(.99f);for(int i=0;i<20;++i){tick(.99f);assert(js.held[softs[pad]]);assert(!js.held[fulls[pad]]);}
   tick(1);assert(js.held[softs[pad]]&&js.held[fulls[pad]]);
   tick(.99f);assert(js.held[softs[pad]]&&!js.held[fulls[pad]]);
   tick(0);tick(0);assert(!js.held[softs[pad]]&&!js.held[fulls[pad]]);
   js.settings[SettingID::TRIGGER_THRESHOLD]=-1; // hair trigger is analog-only
   tick(.99f);assert(js.held[softs[pad]]);tick(0);assert(!js.held[softs[pad]]);
   js.settings[SettingID::TRIGGER_THRESHOLD]=1; // no contact travel threshold
   tick(1);assert(js.held[softs[pad]]&&js.held[fulls[pad]]);tick(0);tick(0);
   tick(.99f,TriggerMode::NO_SKIP_EXCLUSIVE);tick(.99f,TriggerMode::NO_SKIP_EXCLUSIVE);
   assert(js.held[softs[pad]]&&!js.held[fulls[pad]]);
   tick(1,TriggerMode::NO_SKIP_EXCLUSIVE);assert(!js.held[softs[pad]]&&js.held[fulls[pad]]);
   tick(.99f,TriggerMode::NO_SKIP_EXCLUSIVE);assert(js.held[softs[pad]]&&!js.held[fulls[pad]]);
   tick(0,TriggerMode::NO_SKIP_EXCLUSIVE);
   tick(1,TriggerMode::NO_FULL);tick(1,TriggerMode::NO_FULL);assert(js.held[softs[pad]]&&!js.held[fulls[pad]]);
   // Switching policy while the click remains held must release the old stage.
   tick(1,TriggerMode::NO_SKIP_EXCLUSIVE);assert(!js.held[softs[pad]]&&js.held[fulls[pad]]);
   tick(0,TriggerMode::NO_SKIP_EXCLUSIVE);assert(!js.held[softs[pad]]&&!js.held[fulls[pad]]);
   for(auto mode:{TriggerMode::MAY_SKIP,TriggerMode::MUST_SKIP,TriggerMode::MAY_SKIP_R,TriggerMode::MUST_SKIP_R}) {
     JoyShock skips;
     auto step=[&](float position,int ms=10){skips._timeNow+=std::chrono::milliseconds(ms);skips.handleTriggerChange(softs[pad],fulls[pad],mode,position,skips._unusedEffect);};
     const bool responsive=mode==TriggerMode::MAY_SKIP_R||mode==TriggerMode::MUST_SKIP_R;
     const bool may=mode==TriggerMode::MAY_SKIP||mode==TriggerMode::MAY_SKIP_R;
     step(.99f);assert(skips.held[softs[pad]]==responsive&&!skips.held[fulls[pad]]);
     step(1);assert(!skips.held[softs[pad]]&&skips.held[fulls[pad]]);
     step(0);step(0);assert(!skips.held[softs[pad]]&&!skips.held[fulls[pad]]);
     step(.99f);step(.99f,120);assert(skips.held[softs[pad]]&&!skips.held[fulls[pad]]);
     step(1);assert(skips.held[softs[pad]]&&skips.held[fulls[pad]]==may);
     step(0);step(0);
     step(1);assert(!skips.held[softs[pad]]&&skips.held[fulls[pad]]); // first poll already clicked
     step(0);step(0);
   }

 }
 // Two pads use distinct FSM slots and can use different behaviors.
 auto js=std::make_shared<JoyShock>();js->leftPad=TriggerMode::NO_FULL;js->rightPad=TriggerMode::NO_SKIP_EXCLUSIVE;
 hardware.left=true;hardware.right=false;hardware.buttons=0;
 dispatchPhysicalButtons(js,true);
 assert(js->_virtualMenuInputs[int(ButtonID::MISC4)]==true);
 assert(js->_virtualMenuInputs[int(ButtonID::TOUCH)]==false);
 assert(js->held.empty()); // snapshot must never mutate bindings
 dispatchPhysicalButtons(js,false);assert(js->held[ButtonID::MISC4]&&!js->held[ButtonID::TOUCH]);
 hardware.right=true;hardware.buttons=(1ULL<<JSOFFSET_MISC2)|(1ULL<<JSOFFSET_MISC3);
 dispatchPhysicalButtons(js,false);
 assert(js->held[ButtonID::MISC4]&&!js->held[ButtonID::MISC3]);
 assert(!js->held[ButtonID::TOUCH]&&js->held[ButtonID::MISC2]);
 hardware.buttons=0;dispatchPhysicalButtons(js,false);
 assert(js->held[ButtonID::MISC4]&&js->held[ButtonID::TOUCH]&&!js->held[ButtonID::MISC2]);
 hardware.left=false;hardware.right=false;dispatchPhysicalButtons(js,false);dispatchPhysicalButtons(js,false);
 assert(!js->held[ButtonID::MISC4]&&!js->held[ButtonID::TOUCH]);
 // A click-scoped touch-only policy must not erase its own physical condition.
 auto conditions=std::make_shared<JoyShock>();
 useInvertedChord(ButtonID::TOUCH);
 hardware.right=true;hardware.left=true;
 hardware.buttons=(1ULL<<JSOFFSET_MISC2)|(1ULL<<JSOFFSET_MISC3)|(1ULL<<JSOFFSET_MISC5)|(1ULL<<JSOFFSET_MISC6)|(1ULL<<JSOFFSET_LTOUCH)|(1ULL<<JSOFFSET_RTOUCH);
 auto pressed=[&](ButtonID id){return std::find(conditions->context.chordStack.begin(),conditions->context.chordStack.end(),id)!=conditions->context.chordStack.end();};
 syncSteamPhysicalConditions(conditions);
 for(auto id:steamConditionInputs) assert(pressed(id)); // fresh before binding dispatch
 assert(!pressed(invertedChordOf(ButtonID::TOUCH)));
 for(int poll=0;poll<20;++poll){
   syncSteamPhysicalConditions(conditions);
   conditions->rightPad=pressed(ButtonID::MISC2)?TriggerMode::NO_FULL:TriggerMode::NO_SKIP;
   dispatchPhysicalButtons(conditions,false);
   assert(pressed(ButtonID::MISC2)&&conditions->held[ButtonID::TOUCH]&&!conditions->held[ButtonID::MISC2]);
 }
 conditions->rightPad=TriggerMode::NO_SKIP_EXCLUSIVE;
 dispatchPhysicalButtons(conditions,false);
 assert(!conditions->held[ButtonID::TOUCH]&&conditions->held[ButtonID::MISC2]);
 assert(pressed(ButtonID::TOUCH)&&!pressed(invertedChordOf(ButtonID::TOUCH)));
 hardware.buttons=0;hardware.left=false;hardware.right=false;
 syncSteamPhysicalConditions(conditions);
 for(auto id:steamConditionInputs) assert(!pressed(id));
 assert(pressed(invertedChordOf(ButtonID::TOUCH)));
 dispatchPhysicalButtons(conditions,false);
 assert(!conditions->held[ButtonID::TOUCH]&&!conditions->held[ButtonID::MISC2]);
 // DualSense keeps its pre-existing stage-based chord behavior.
 auto other=std::make_shared<JoyShock>();other->_controllerType=JS_TYPE_DS;
 hardware.right=true;hardware.buttons=(1ULL<<JSOFFSET_MISC2);
 syncSteamPhysicalConditions(other);assert(!other->context.steamConditions);
 other->context.updateChordStack(true,ButtonID::TOUCH);
 other->context.updateChordStack(false,ButtonID::TOUCH);
 assert(std::find(other->context.chordStack.begin(),other->context.chordStack.end(),ButtonID::TOUCH)==other->context.chordStack.end());
 clearInvertedChords();
 // Physical triggers retain their existing full-pull jitter tolerance.
 JoyShock routed;routed.context._vigemController=std::make_unique<Gamepad>();
 routed.handleTriggerChange(ButtonID::ZL,ButtonID::ZLF,TriggerMode::X_RT,.6f,routed._leftEffect);
 assert(routed.context._vigemController->left==0&&routed.context._vigemController->right==.6f);
 routed.handleTriggerChange(ButtonID::ZL,ButtonID::ZLF,TriggerMode::X_LT,.4f,routed._leftEffect);
 assert(routed.context._vigemController->left==.4f&&routed.context._vigemController->right==0);
 for(bool adaptive:{false,true}) {
   JoyShock hair;hair._controllerType=JS_TYPE_DS;
   hair.settings[SettingID::TRIGGER_THRESHOLD]=-1;
   hair.settings[SettingID::ADAPTIVE_TRIGGER]=float(adaptive?Switch::ON:Switch::OFF);
   auto travel=[&](float v){hair.handleTriggerChange(ButtonID::ZL,ButtonID::ZLF,TriggerMode::NO_FULL,v,hair._leftEffect);};
   for(int n=1;n<=8;++n) travel(n*.08f);
   assert(hair.held[ButtonID::ZL]);
   for(int n=8;n>=1;--n) travel(n*.08f);
   assert(hair.held[ButtonID::ZL]==adaptive); // hair releases on falling travel; resistance uses zero threshold
   travel(0);assert(!hair.held[ButtonID::ZL]);
 }
 JoyShock analog;analog.handleTriggerChange(ButtonID::ZL,ButtonID::ZLF,TriggerMode::NO_SKIP,.999f,analog._leftEffect);
 assert(analog.held[ButtonID::ZL]&&analog.held[ButtonID::ZLF]);
 analog.handleTriggerChange(ButtonID::ZL,ButtonID::ZLF,TriggerMode::NO_SKIP,.98f,analog._leftEffect);assert(analog.held[ButtonID::ZLF]);
 analog.handleTriggerChange(ButtonID::ZL,ButtonID::ZLF,TriggerMode::NO_SKIP,.96f,analog._leftEffect);assert(!analog.held[ButtonID::ZLF]);
 std::cout<<"PASS: real dual-stage FSM/contact routing, independent pads, all seven policies, stable raw Steam conditions before dispatch, inverted touch conditions, non-Steam compatibility, held policy changes, raw menu census and analog hysteresis\n";
}
'''
methods = '\n'.join(function(joy, name) for name in ['float JoyShock::getTriggerEffectStartPos(', 'bool JoyShock::isSoftPullPressed(', 'void JoyShock::handleTriggerChange('])
with tempfile.TemporaryDirectory(prefix='jsm-trigger-modes-') as temporary:
    folder = Path(temporary)
    cpp, binary = folder / 'trigger.cpp', folder / 'trigger.exe'
    # The minimal button boundary needs its enum declared before the stub.
    insertion = stub.index('using Time')
    cpp.write_text(stub[:insertion] + state + '\n' + constants + '\n' + stub[insertion:] + context_methods + methods + function(main, 'static void syncSteamPhysicalConditions(') + function(main, 'static void dispatchPhysicalButtons(') + tests, encoding='utf-8')
    vcvars = Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat')
    batch = folder / 'build.cmd'
    batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /utf-8 /EHsc /std:c++20 /I"{INCLUDE}" /I"{REPO / "build-jsm-sdl/_deps/magic_enum-src/include"}" "{cpp}" "{ROOT / "JoyShockMapper/src/InvertedChords.cpp"}" /Fe:"{binary}"\n', encoding='utf-8')
    result = subprocess.run(['cmd.exe', '/d', '/c', str(batch)], cwd=folder, capture_output=True, text=True)
    if result.returncode:
        print(result.stdout, result.stderr)
        raise SystemExit(result.returncode)
    raise SystemExit(subprocess.run([str(binary)]).returncode)
