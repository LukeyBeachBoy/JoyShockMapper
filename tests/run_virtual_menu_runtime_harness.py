"""Run real menu routing, catalog, Mapping and DigitalButton FSM without OS output.

Only hardware, OS key dispatch and the unrelated setting/button lookup are
stubbed. No mapper process is started and no attached controller is acquired.
"""
import re
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


preamble = r'''
#define NOMINMAX
#include <Windows.h>
#undef small
#include <optional>
#include <regex>
#include <vector>
#include <cassert>
#include <atomic>
#include <iostream>
#include <chrono>
#include <mutex>
#include <cmath>
#include <stdexcept>
#include "JoyShockMapper.h"
#include "PlatformDefinitions.h"
#include "Mapping.h"
#include "VirtualMenuCatalog.h"
#include "pocket_fsm.h"
#undef COUT
#undef CERR
#undef DEBUG_LOG
#define COUT std::cout
#define CERR std::cerr
#define DEBUG_LOG std::ostringstream()
#undef _ASSERT_EXPR
#define _ASSERT_EXPR(condition, message) assert(condition)
#define VK_NONAME 0xFC
std::atomic<int> g_gyroGlobalOffCount{0}, g_gyroGlobalOnCount{0};
std::atomic_bool g_hasGyroOnAllBinding{false};
std::vector<std::string> output;
uint16_t nameToKey(std::string_view key) {
 if(key=="NONE") return NO_HOLD_MAPPED;
 if(key=="X_A") return X_A;
 if(key=="GYRO_OFF") return GYRO_OFF_BIND;
 if(key=="GYRO_ON") return GYRO_ON_BIND;
 if(key=="GYRO_INV_X") return GYRO_INV_X;
 if(key=="GYRO_ON_ALL") return GYRO_ON_ALL_BIND;
 if(key=="GYRO_OFF_ALL") return GYRO_OFF_ALL_BIND;
 if(key.size()==1 && key[0]>='1' && key[0]<='9') return key[0];
 if(key.size()>2 && key.front()=='"' && key.back()=='"') return COMMAND_ACTION;
 return 0;
}
std::string parseHapticName(std::string_view) { return ""; }
std::ostream &operator<<(std::ostream &out, const KeyCode &key) {return out<<key.name;}
std::ostream &operator<<(std::ostream &out, const ButtonID &id) {return out<<int(id);}
/*INPUT_PARSER*/
int pressKey(KeyCode key,bool pressed) {output.push_back((pressed?"+":"-")+key.name);return 0;}
void WriteToConsole(std::string) {assert(false && "Menu cycle must remain native");}
void syncInvertedChordStack(std::deque<ButtonID>&) {}
void updateInvertedChord(std::deque<ButtonID>&,bool,ButtonID) {}
struct Indicator {};
class Gamepad {
 public: using Callback=std::function<void(int,int,Indicator)>;
 static Gamepad *getNew(ControllerScheme,Callback) {return new Gamepad;}
 bool isInitialized(std::string*) {return true;}
 void setButton(KeyCode key,bool down) {pressKey(key,down);}
};
class MotionIf {public: void ResetContinuousCalibration(){} void StartContinuousCalibration(){} void PauseContinuousCalibration(){} };
'''
preamble = preamble.replace('/*INPUT_PARSER*/', function((ROOT / 'JoyShockMapper/src/operators.cpp').read_text(encoding='utf-8'), 'istream &operator>>(istream &in, ButtonID &rhv)'))


lookup = r'''
struct Variable { Mapping mapping; Mapping value() const {return mapping;} };
class MapIterator {public: std::pair<ButtonID,Variable> value; const auto *operator->()const{return &value;} void operator++(){} };
class JSMButton {
 public: ButtonID _id; Mapping mapping;
 JSMButton(ButtonID id,Mapping value):_id(id),mapping(value){}
 std::optional<Mapping> chordedValue(ButtonID id)const {return id==ButtonID::NONE?std::optional<Mapping>(mapping):std::nullopt;}
 std::string getName(ButtonID)const{return "menu item";}
 bool hasSimMappings()const{return false;} bool hasDiagMappings()const{return false;}
 std::optional<std::pair<ButtonID,Mapping>> getDblPressMap()const{return std::nullopt;}
 std::string getDiagPressName(ButtonID)const{return "";}
 std::string getSimPressName(ButtonID)const{return "";}
 const Variable *atSimPress(ButtonID)const{return nullptr;}
};
namespace SettingsManager {
 template<class T> struct Value {T value(){if constexpr(std::is_same_v<T,float>) return 50;else return T{};} void set(T){} };
 template<class T> Value<T>* getV(SettingID) {static Value<T> result;return &result;}
}
'''

hardware = r'''
struct Touch {bool t0Down=false,t1Down=false;float t0X=.5f,t0Y=.5f,t1X=.5f,t1Y=.5f;};
struct Hardware {
 uint64_t buttons=0;Touch touch;float lx=0,ly=0,rx=0,ry=0,lt=0,rt=0;
 uint64_t GetButtons(int){return buttons;}
 Touch GetTouchState(int){return touch;}
 bool GetTouchDown(int,bool second){return second?touch.t1Down:touch.t0Down;}
 float GetLeftX(int){return lx;}float GetLeftY(int){return ly;}float GetRightX(int){return rx;}float GetRightY(int){return ry;}
 float GetLeftTrigger(int){return lt;}float GetRightTrigger(int){return rt;}
} hardware;
auto *jsl=&hardware;
struct JoyShock {
 int _controllerType=JS_TYPE_STEAM_CONTROLLER_2026,_handle=1,_splitType=JS_SPLIT_TYPE_FULL;
 int _leftEffect=0,_rightEffect=0,_unusedEffect=0;
 std::shared_ptr<DigitalButton::Context> _context=std::make_shared<DigitalButton::Context>(nullptr,nullptr);
 std::chrono::steady_clock::time_point _timeNow{};
 struct MenuRuntime {
    MenuRuntime() = default;
    MenuRuntime(const MenuRuntime &) = delete;
    MenuRuntime &operator=(const MenuRuntime &) = delete;
    MenuRuntime(MenuRuntime &&) = default;
    MenuRuntime &operator=(MenuRuntime &&) = default;
 VirtualMenuAttachment attachment;VirtualMenuDefinition definition;VirtualMenuRouting routing;VirtualMenuResult result;
 unsigned cancellation = 0;
 std::vector<std::unique_ptr<JSMButton>> mappings;
 std::vector<std::unique_ptr<DigitalButton>> buttons;
 };
 std::shared_ptr<const VirtualMenuCatalog> _menuCatalog;
 std::vector<MenuRuntime> _virtualMenus;
 std::array<std::optional<bool>, int(ButtonID::SIZE)> _virtualMenuInputs;
 std::map<ButtonID,bool> physical;
 template<class T=float> T getSetting(SettingID id) {if constexpr(std::is_same_v<T,float>) return id==SettingID::HOLD_PRESS_TIME?200:50;else return T{};}
 bool isPressed(ButtonID id){return physical[id];}
 void handleButtonChange(ButtonID id,bool pressed){physical[id]=pressed;}
 void handleTriggerChange(ButtonID,ButtonID,TriggerMode,float,int&) {assert(false && "Census must not mutate trigger processing");}
 void refreshVirtualMenuCatalog();bool virtualMenuIsPressed(ButtonID);
 bool virtualMenuConsumes(VirtualMenuSource);VirtualMenuOwners virtualMenuOwners();
 ButtonID virtualMenuConfirm(const VirtualMenuAttachment&)const;
 bool virtualMenuConsumesButton(ButtonID);void processVirtualMenus();
};
void updatePadClickHaptics(std::shared_ptr<JoyShock>&,bool,bool){assert(false && "Census must not fire haptics");}
'''

tests = r'''
int main(){
 Mapping::_isCommandValid=[](std::string_view){return false;};
 VirtualMenus::clear();assert(VirtualMenus::define("weapon TOUCH 4 2 .1"));
 assert(VirtualMenus::action("weapon 1 1"));assert(VirtualMenus::action("weapon 2 X_A"));
 assert(VirtualMenus::action("weapon 3 2\\ !3/"));
 assert(VirtualMenus::action("weapon 4 \"CYCLE 4|5\""));
 assert(VirtualMenus::attach("weapon RIGHT HOLD N CLICK NONE R"));
 auto jc=std::make_shared<JoyShock>();jc->_context->_vigemController=std::make_unique<Gamepad>();
 auto tick=[&](int ms){jc->_timeNow=std::chrono::steady_clock::time_point{}+std::chrono::milliseconds(ms);jc->refreshVirtualMenuCatalog();jc->_virtualMenuInputs.fill(std::nullopt);dispatchPhysicalButtons(jc,true);};
 hardware.buttons=(1ULL<<JSOFFSET_N)|(1ULL<<JSOFFSET_MISC2)|(1ULL<<JSOFFSET_LTOUCH);
 hardware.touch={false,true,.5f,.5f,.2f,.2f};tick(1);
 assert(jc->virtualMenuConsumes(VirtualMenuSource::RIGHT)); // before physical N dispatch
 assert(jc->virtualMenuConsumesButton(ButtonID::MISC2)); // same-poll confirm capture
 assert(jc->virtualMenuIsPressed(ButtonID::LTOUCH));assert(!jc->virtualMenuIsPressed(ButtonID::RTOUCH));
 assert(jc->virtualMenuIsPressed(ButtonID::TOUCH));assert(!jc->virtualMenuIsPressed(ButtonID::MISC4));
 jc->processVirtualMenus();assert(jc->_virtualMenus[0].result.selected==0);
 hardware.buttons=(1ULL<<JSOFFSET_N);tick(10);jc->processVirtualMenus();
 hardware.buttons|=(1ULL<<JSOFFSET_MISC2);tick(20);jc->processVirtualMenus();
 assert(std::find(output.begin(),output.end(),"+1")!=output.end());
 tick(30);jc->processVirtualMenus();tick(100);jc->processVirtualMenus();
 assert(std::find(output.begin(),output.end(),"-1")!=output.end());
 // A second cell dispatches a virtual gamepad button through the same real FSM.
 hardware.touch.t1X=.8f;hardware.buttons=(1ULL<<JSOFFSET_N);tick(110);jc->processVirtualMenus();
 hardware.buttons|=(1ULL<<JSOFFSET_MISC2);tick(120);jc->processVirtualMenus();tick(180);jc->processVirtualMenus();
 assert(std::find(output.begin(),output.end(),"+X_A")!=output.end());assert(std::find(output.begin(),output.end(),"-X_A")!=output.end());
 // Continuous selection holds a real native Mapping and a reload releases it.
 VirtualMenus::clear();assert(VirtualMenus::define("held TOUCH 2 2 .1"));assert(VirtualMenus::action("held 1 2\\"));
 assert(VirtualMenus::attach("held RIGHT HOLD N CONTINUOUS NONE NONE"));hardware.touch.t1X=.2f;tick(200);jc->processVirtualMenus();
 assert(output.back()=="+2");VirtualMenus::clear();tick(201);assert(output.back()=="-2");assert(jc->_virtualMenus.empty());
 // Toggle remains held after closing a menu, but belongs to that catalog.
 assert(VirtualMenus::define("toggle TOUCH 2 2 .1"));assert(VirtualMenus::action("toggle 1 ^3"));
 assert(VirtualMenus::attach("toggle RIGHT HOLD N CLICK NONE NONE"));
 hardware.buttons=(1ULL<<JSOFFSET_N)|(1ULL<<JSOFFSET_MISC2);tick(210);jc->processVirtualMenus();
 assert(output.back()=="+3");hardware.buttons=0;tick(220);jc->processVirtualMenus();tick(300);jc->processVirtualMenus();
 assert(output.back()=="+3");VirtualMenus::clear();tick(301);assert(output.back()=="-3");
 assert(jc->_context->activeTogglesQueue.empty());
 // The centre is an ordinary native Mapping, independent of segment count.
 assert(VirtualMenus::define("center RADIAL 8 8 .3"));assert(VirtualMenus::action("center 0 4"));
 assert(VirtualMenus::attach("center RIGHT HOLD N CLICK NONE NONE"));
 hardware.touch.t1X=hardware.touch.t1Y=.5f;hardware.buttons=(1ULL<<JSOFFSET_N)|(1ULL<<JSOFFSET_MISC2);
 tick(310);jc->processVirtualMenus();assert(jc->_virtualMenus[0].result.selected==8);assert(output.back()=="+4");
 hardware.buttons=(1ULL<<JSOFFSET_N);tick(320);jc->processVirtualMenus();tick(400);jc->processVirtualMenus();assert(output.back()=="-4");
 VirtualMenus::clear();tick(401);
 // Face-button hotbar navigation shares native ownership and edge processing.
 assert(VirtualMenus::define("face HOTBAR 3 3 .1"));
 assert(VirtualMenus::action("face 1 5"));assert(VirtualMenus::action("face 2 6"));assert(VirtualMenus::action("face 3 7"));
 assert(VirtualMenus::attach("face ABXY HOLD L CLICK NONE NONE"));
 hardware.buttons=(1ULL<<JSOFFSET_L)|(1ULL<<JSOFFSET_E);tick(410);jc->processVirtualMenus();
 assert(jc->virtualMenuConsumes(VirtualMenuSource::ABXY));
 for(auto id:{ButtonID::N,ButtonID::E,ButtonID::S,ButtonID::W}) assert(jc->virtualMenuConsumesButton(id));
 assert(!jc->virtualMenuConsumesButton(ButtonID::UP));
 assert(jc->_virtualMenus[0].result.selected==1);
 tick(411);jc->processVirtualMenus();assert(jc->_virtualMenus[0].result.selected==1); // held east does not repeat
 hardware.buttons=(1ULL<<JSOFFSET_L);tick(412);jc->processVirtualMenus();
 hardware.buttons|=(1ULL<<JSOFFSET_W);tick(413);jc->processVirtualMenus();assert(jc->_virtualMenus[0].result.selected==0);
 hardware.buttons=(1ULL<<JSOFFSET_L);tick(414);jc->processVirtualMenus();
 hardware.buttons|=(1ULL<<JSOFFSET_W);tick(415);jc->processVirtualMenus();assert(jc->_virtualMenus[0].result.selected==2);
 hardware.buttons=(1ULL<<JSOFFSET_L)|(1ULL<<JSOFFSET_N);tick(416);jc->processVirtualMenus();assert(output.back()=="+7");
 hardware.buttons=(1ULL<<JSOFFSET_L);tick(500);jc->processVirtualMenus();assert(output.back()=="-7");
 VirtualMenus::clear();tick(501);assert(!jc->virtualMenuConsumesButton(ButtonID::S));
 // Real Mapping callbacks retain another input's gyro action on release.
 auto context=std::make_shared<DigitalButton::Context>(nullptr,nullptr);
 // Production FSM release events must not clear a physically held Steam
 // condition when its touch/click activator policy suppresses that action.
 auto sensors=std::make_shared<DigitalButton::Context>(nullptr,nullptr);
 sensors->syncSteamConditions({false,true,false,true,false,false,false,false});
 JSMButton contactMap(ButtonID::TOUCH,Mapping("1\\"));
 DigitalButton contactButton(sensors,contactMap);
 auto sensorHeld=[&](ButtonID id){return std::find(sensors->chordStack.begin(),sensors->chordStack.end(),id)!=sensors->chordStack.end();};
 Pressed sensorPress{std::chrono::steady_clock::now(),0,150,0};
 Released sensorRelease{sensorPress.time_now+std::chrono::milliseconds(20),0,150,0};
 contactButton.sendEvent(sensorPress);
 contactButton.sendEvent(sensorRelease);
 assert(sensorHeld(ButtonID::TOUCH)&&sensorHeld(ButtonID::MISC2));
 sensors->syncSteamConditions({false,false,false,false,false,false,false,false});
 assert(!sensorHeld(ButtonID::TOUCH)&&!sensorHeld(ButtonID::MISC2));
 JSMButton firstMap(ButtonID::N,Mapping("GYRO_OFF\\")),secondMap(ButtonID::E,Mapping("GYRO_OFF\\"));
 DigitalButtonImpl first(firstMap,context),second(secondMap,context);
 firstMap.mapping.ProcessEvent(BtnEvent::OnPress,first);secondMap.mapping.ProcessEvent(BtnEvent::OnPress,second);
 assert(context->gyroActionQueue.size()==2);
 firstMap.mapping.ProcessEvent(BtnEvent::OnRelease,first);
 assert(context->gyroActionQueue.size()==1 && context->gyroActionQueue.front().first==ButtonID::E);
 secondMap.mapping.ProcessEvent(BtnEvent::OnRelease,second);assert(context->gyroActionQueue.empty());
 // Different events on one input release the output they applied, not the
 // first arbitrary queued action owned by that input.
 Mapping mixed("GYRO_OFF\\ GYRO_INV_X_");assert(mixed.isValid());
 mixed.ProcessEvent(BtnEvent::OnPress,first);mixed.ProcessEvent(BtnEvent::OnHold,first);
 mixed.ProcessEvent(BtnEvent::OnHoldRelease,first);
 assert(context->gyroActionQueue.size()==1 && context->gyroActionQueue.front().second.code==GYRO_OFF_BIND);
 mixed.ProcessEvent(BtnEvent::OnRelease,first);assert(context->gyroActionQueue.empty());
 // Per-input toggles coexist; turning one off does not clear the other's
 // queue entry or global counter. Explicit release retains its all-owner power.
 Mapping toggled("^GYRO_ON_ALL\\");assert(toggled.isValid());
 toggled.ProcessEvent(BtnEvent::OnPress,first);toggled.ProcessEvent(BtnEvent::OnPress,second);
 assert(g_gyroGlobalOnCount==2 && context->activeTogglesQueue.size()==2);
 toggled.ProcessEvent(BtnEvent::OnPress,first);
 assert(g_gyroGlobalOnCount==1 && context->activeTogglesQueue.size()==1 && context->activeTogglesQueue.front().first==ButtonID::E);
 Mapping explicitRelease("-GYRO_ON_ALL\\");assert(explicitRelease.isValid());
 explicitRelease.ProcessEvent(BtnEvent::OnPress,first);
 assert(g_gyroGlobalOnCount==0 && context->activeTogglesQueue.empty() && context->gyroActionQueue.empty());
 second.ReleaseOwnedToggles();assert(g_gyroGlobalOnCount==0);
 // Pending repeated instant actions each remove one corresponding apply.
 Mapping instant("!GYRO_OFF_ALL\\");assert(instant.isValid());
 instant.ProcessEvent(BtnEvent::OnPress,first);instant.ProcessEvent(BtnEvent::OnPress,first);
 assert(g_gyroGlobalOffCount==2);first.ReleaseInstant(BtnEvent::OnPress);
 assert(g_gyroGlobalOffCount==0 && context->gyroActionQueue.empty());
 // Released activation opens on the opening poll and commits when held again.
 VirtualMenus::clear();assert(VirtualMenus::define("released TOUCH 2 2 .1"));
 assert(VirtualMenus::action("released 1 8"));
 assert(VirtualMenus::attach("released RIGHT HOLD !L ACTIVATION_RELEASE NONE NONE"));
 hardware.buttons=0;hardware.touch={false,true,.5f,.5f,.2f,.2f};tick(1600);
 assert(jc->virtualMenuConsumes(VirtualMenuSource::RIGHT));
 jc->processVirtualMenus();assert(jc->_virtualMenus[0].result.open);
 hardware.buttons=(1ULL<<JSOFFSET_L);tick(1610);jc->processVirtualMenus();
 assert(!jc->_virtualMenus[0].result.open);assert(output.back()=="+8");
 tick(1700);jc->processVirtualMenus();assert(output.back()=="-8");
 // A released activation must work without another mapping registering !L.
 jc->_virtualMenuInputs[int(ButtonID::L)] = false;
 assert(jc->virtualMenuIsPressed(invertedChordOf(ButtonID::L)));
 jc->_virtualMenuInputs[int(ButtonID::L)] = true;
 assert(!jc->virtualMenuIsPressed(invertedChordOf(ButtonID::L)));

 // Regular quoted commands use the real Mapping callback and controller context.
 VirtualMenus::clear();assert(VirtualMenus::define("commands TOUCH 2 2 .1"));
 assert(VirtualMenus::action("commands 1 9"));
 assert(VirtualMenus::attach("commands RIGHT COMMAND NONE ACTIVATION_RELEASE NONE R"));
 tick(1800);hardware.buttons=0;hardware.touch={false,true,.5f,.5f,.2f,.2f};
 JSMButton openerOne(ButtonID::L,Mapping("\"MENU_HOLD commands\"\\"));
 JSMButton openerTwo(ButtonID::E,Mapping("\"MENU_HOLD commands\"\\"));
 assert(openerOne.mapping.isValid() && openerTwo.mapping.isValid());
 DigitalButtonImpl holdOne(openerOne,jc->_context), holdTwo(openerTwo,jc->_context);
 openerOne.mapping.ProcessEvent(BtnEvent::OnPress,holdOne);
 openerTwo.mapping.ProcessEvent(BtnEvent::OnPress,holdTwo);
 assert(jc->virtualMenuConsumes(VirtualMenuSource::RIGHT));
 tick(1810);jc->processVirtualMenus();assert(jc->_virtualMenus[0].result.open);
 openerOne.mapping.ProcessEvent(BtnEvent::OnRelease,holdOne);
 tick(1820);jc->processVirtualMenus();assert(jc->_virtualMenus[0].result.open);
 openerTwo.mapping.ProcessEvent(BtnEvent::OnRelease,holdTwo);
 tick(1830);jc->processVirtualMenus();assert(!jc->_virtualMenus[0].result.open && output.back()=="+9");
 tick(1900);jc->processVirtualMenus();assert(output.back()=="-9");
 Mapping openMenu("\"MENU_OPEN commands\"\\"), closeMenu("\"MENU_CLOSE commands\"\\"), toggleMenu("\"MENU_TOGGLE commands\"\\");
 assert(openMenu.isValid() && closeMenu.isValid() && toggleMenu.isValid());
 openMenu.ProcessEvent(BtnEvent::OnPress,holdOne);tick(1910);jc->processVirtualMenus();
 assert(jc->_virtualMenus[0].result.open);
 openMenu.ProcessEvent(BtnEvent::OnRelease,holdOne);tick(1920);jc->processVirtualMenus();assert(jc->_virtualMenus[0].result.open);
 auto beforeClose=output.size();closeMenu.ProcessEvent(BtnEvent::OnPress,holdOne);tick(1930);jc->processVirtualMenus();
 assert(!jc->_virtualMenus[0].result.open && output.size()==beforeClose);
 toggleMenu.ProcessEvent(BtnEvent::OnPress,holdOne);tick(1940);jc->processVirtualMenus();assert(jc->_virtualMenus[0].result.open);
 toggleMenu.ProcessEvent(BtnEvent::OnPress,holdOne);tick(1950);jc->processVirtualMenus();assert(!jc->_virtualMenus[0].result.open);
 openerOne.mapping.ProcessEvent(BtnEvent::OnPress,holdOne);tick(1960);jc->processVirtualMenus();assert(jc->_virtualMenus[0].result.open);
 hardware.buttons=1ULL<<JSOFFSET_R;tick(1970);jc->processVirtualMenus();assert(!jc->_virtualMenus[0].result.open);
 hardware.buttons=0;tick(1980);jc->processVirtualMenus();assert(!jc->_virtualMenus[0].result.open);
 openerOne.mapping.ProcessEvent(BtnEvent::OnRelease,holdOne);
 openerOne.mapping.ProcessEvent(BtnEvent::OnPress,holdOne);tick(1990);jc->processVirtualMenus();assert(jc->_virtualMenus[0].result.open);
 VirtualMenus::clear();tick(2000);assert(jc->_context->menuCommands.empty());
 assert(!Mapping("\"MENU_HOLD bad.id\"").isValid());
 // Cursor navigation uses real stick input and native action dispatch.
 VirtualMenus::clear();assert(VirtualMenus::define("cursor RADIAL 25 25 .2"));
 assert(VirtualMenus::action("cursor 1 1"));
 assert(VirtualMenus::attach("cursor RSTICK HOLD L ACTIVATION_RELEASE NONE NONE JOYSTICK_CURSOR"));
 hardware.buttons=1ULL<<JSOFFSET_L;hardware.rx=0;hardware.ry=.8f;tick(2100);jc->processVirtualMenus();
 auto cursor=jc->_virtualMenus[0].result;
 assert(cursor.cursor && cursor.selected==0 && std::abs(cursor.y-.1f)<.001f);
 auto beforeNeutral=output.size();
 hardware.rx=.01f;hardware.ry=.01f;tick(2110);jc->processVirtualMenus();
 cursor=jc->_virtualMenus[0].result;
 assert(cursor.selected==-1 && cursor.x==.5f && cursor.y==.5f && output.size()==beforeNeutral);
 hardware.buttons=0;tick(2120);jc->processVirtualMenus();assert(output.size()==beforeNeutral);
 // Deflect, then neutral plus opener release on one poll: no stale action.
 hardware.buttons=1ULL<<JSOFFSET_L;hardware.ry=.8f;tick(2130);jc->processVirtualMenus();
 hardware.buttons=0;hardware.ry=0;tick(2140);jc->processVirtualMenus();
 assert(output.size()==beforeNeutral);
 // Release while deflected still fires the real Mapping exactly once.
 hardware.buttons=1ULL<<JSOFFSET_L;hardware.rx=0;hardware.ry=.8f;tick(2150);jc->processVirtualMenus();
 hardware.buttons=0;tick(2160);jc->processVirtualMenus();assert(output.back()=="+1");
 tick(2230);jc->processVirtualMenus();assert(output.back()=="-1");
 // Native cursor data never escapes the circular menu, even at diagonals.
 hardware.buttons=1ULL<<JSOFFSET_L;hardware.rx=hardware.ry=1;tick(2240);jc->processVirtualMenus();
 cursor=jc->_virtualMenus[0].result;assert(std::hypot(cursor.x-.5f,cursor.y-.5f)<=.50001f);
 hardware.rx=hardware.ry=0;
 VirtualMenus::clear();tick(2250);
 assert(VirtualMenus::define("cursor RADIAL 8 8 .2"));assert(VirtualMenus::action("cursor 1 1"));
 assert(VirtualMenus::attach("cursor LSTICK COMMAND NONE ACTIVATION_RELEASE NONE NONE JOYSTICK_CURSOR"));
 hardware.lx=0;hardware.ly=.8f;tick(2260);
 jc->_context->menuCommands["cursor"].apply("HOLD",int(ButtonID::L));
 jc->processVirtualMenus();assert(jc->_virtualMenus[0].result.selected==0);
 auto beforeCommandRelease=output.size();hardware.ly=0;
 jc->_context->menuCommands["cursor"].apply("HOLD",int(ButtonID::L),true);
 tick(2270);jc->processVirtualMenus();assert(!jc->_virtualMenus[0].result.open && output.size()==beforeCommandRelease);
 std::cout<<"PASS: joystick cursor position, neutral noise, same-poll neutral/release cancellation, held command release and real selected Mapping output\n";
 std::cout<<"PASS: native OPEN/CLOSE/TOGGLE/HOLD bindings, overlapping hold owners, release selection, cancel/reopen and reload reset\n";
 std::cout<<"PASS: actual gyro Mapping callbacks preserve overlapping holds, event-specific outputs, independent toggles, explicit release and balanced instant counters\n";
 std::cout<<"PASS: actual native menu routing/catalog/Mapping/DigitalButton FSM, same-poll source/confirm ownership, independent stick touch, keyboard/gamepad dispatch and reload release\n";
}
'''

main = (ROOT / 'JoyShockMapper/src/main.cpp').read_text(encoding='utf-8')
joy = (ROOT / 'JoyShockMapper/src/JoyShock.cpp').read_text(encoding='utf-8')
wrapper = (INCLUDE / 'JslWrapper.h').read_text(encoding='utf-8')
constants = '\n'.join(line for line in wrapper.splitlines() if line.startswith('#define JS'))
header = (INCLUDE / 'DigitalButton.h').read_text(encoding='utf-8')
header = '\n'.join(line for line in header.splitlines() if not line.startswith('#include') and not line.startswith('#pragma'))
fsm = (ROOT / 'JoyShockMapper/src/DigitalButton.cpp').read_text(encoding='utf-8')
fsm = re.sub(r'^#include .*$', '', fsm, flags=re.M)
mapping = (ROOT / 'JoyShockMapper/src/Mapping.cpp').read_text(encoding='utf-8')
mapping = re.sub(r'^#include .*$', '', mapping, flags=re.M)
catalog = (ROOT / 'JoyShockMapper/src/VirtualMenuCatalog.cpp').read_text(encoding='utf-8')
catalog = re.sub(r'^#include .*$', '', catalog, flags=re.M)
methods = '\n'.join(function(joy, signature) for signature in [
 'bool JoyShock::virtualMenuConsumes(', 'VirtualMenuOwners JoyShock::virtualMenuOwners(',
 'ButtonID JoyShock::virtualMenuConfirm(', 'bool JoyShock::virtualMenuConsumesButton(',
 'void JoyShock::refreshVirtualMenuCatalog(', 'bool JoyShock::virtualMenuIsPressed(', 'void JoyShock::processVirtualMenus('])
with tempfile.TemporaryDirectory(prefix='jsm-menu-runtime-') as temporary:
 folder=Path(temporary);cpp=folder/'runtime.cpp';binary=folder/'runtime.exe'
 cpp.write_text(preamble+lookup+header+mapping+fsm+catalog+constants+hardware+methods+function(main,'static void dispatchPhysicalButtons(')+tests,encoding='utf-8')
 vcvars=Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat')
 batch=folder/'build.cmd'
 deps=REPO/'build-jsm-sdl/_deps'
 batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /utf-8 /EHsc /std:c++20 /I"{INCLUDE}" /I"{deps / "magic_enum-src/include"}" /I"{deps / "pocket_fsm-src/include"}" "{cpp}" /Fe:"{binary}"\n',encoding='utf-8')
 result=subprocess.run(['cmd.exe','/d','/c',str(batch)],cwd=folder,capture_output=True,text=True)
 if result.returncode:
  diagnostic=REPO/'tmp/parity-verification/menu-runtime-generated.cpp'
  diagnostic.parent.mkdir(parents=True,exist_ok=True)
  diagnostic.write_text(cpp.read_text(encoding='utf-8'),encoding='utf-8')
  print(result.stdout,result.stderr);raise SystemExit(result.returncode)
 raise SystemExit(subprocess.run([str(binary)]).returncode)
