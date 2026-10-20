"""Exercise the production shutdown method with live-button and fake-clock SDL stubs."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'JoyShockMapper/src/SDLWrapper.cpp').read_text()
a=s.index('bool TurnOffController(int deviceId)');b=s.index('\n\tbool PlayHapticScript',a)
method=s[a:b].replace(' override','')
stub=r'''
#include <cassert>
#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <sstream>
#include <iostream>
int nowMs=100, sentAt=-1, sentCount=0; bool sendOK=true;
std::function<void()> onSleep;
namespace chrono {
using milliseconds=std::chrono::milliseconds;
using std::chrono::duration_cast;
struct steady_clock { using time_point=std::chrono::steady_clock::time_point;
 static time_point now() {return time_point(milliseconds(nowMs));} };
}
namespace this_thread {void sleep_for(chrono::milliseconds d){nowMs+=int(d.count());if(onSleep)onSleep();}}
using SDL_JoystickID=unsigned;
struct SDL_Gamepad { unsigned id=1; bool connected=true,guide=false,qam=false,y=false; };
constexpr int JS_TYPE_STEAM_CONTROLLER_2026=24,SDL_GAMEPAD_BUTTON_GUIDE=1,SDL_GAMEPAD_BUTTON_NORTH=2;
struct ControllerDevice {SDL_Gamepad *_sdlController;int _ctrlr_type=24;};
unsigned SDL_GetGamepadID(SDL_Gamepad*p){return p->id;}
bool SDL_GamepadConnected(SDL_Gamepad*p){return p->connected;}
bool SDL_GetGamepadButton(SDL_Gamepad*p,int button){return button==1?p->guide:p->y;}
SDL_Gamepad *SDL_GetGamepadJoystick(SDL_Gamepad*p){return p;}
bool SDL_GetJoystickButton(SDL_Gamepad*p,int index){assert(index==11);return p->qam;}
const char *SDL_GetError(){return "simulated error";}
bool sendTurnOffController(SDL_Gamepad*p){assert(!p->guide&&!p->qam&&!p->y);sentAt=nowMs;++sentCount;return sendOK;}
std::ostringstream messages;
#define CERR messages
#define COUT_INFO messages
struct Backend {std::mutex controller_lock;std::map<int,ControllerDevice*> _controllerMap;
'''
tests=r'''
};
int main(){
 SDL_Gamepad pad; ControllerDevice device{&pad}; Backend backend;
 auto reset=[&]{pad=SDL_Gamepad{};device._ctrlr_type=24;backend._controllerMap.clear();backend._controllerMap[1]=&device;nowMs=100;sentAt=-1;sentCount=0;sendOK=true;onSleep={};messages.str("");};
 reset();assert(backend.TurnOffController(1));assert(sentAt>=175&&sentAt<=190&&sentCount==1);
 for(int button=0;button<3;++button){
  reset();pad.guide=button==0;pad.qam=button==1;pad.y=button==2;
  onSleep=[&]{assert(backend.controller_lock.try_lock());backend.controller_lock.unlock();if(nowMs>=400){pad.guide=pad.qam=pad.y=false;}};
  assert(backend.TurnOffController(1));assert(sentAt>=475&&sentAt<=490&&sentCount==1);
 }
 // A bouncing release restarts the quiet interval.
 reset();pad.qam=true;onSleep=[&]{pad.qam=nowMs<200||(nowMs>=240&&nowMs<300);};
 assert(backend.TurnOffController(1));assert(sentAt>=375);
 // A controller disappearing or being replaced must never power off a new one.
 reset();pad.qam=true;onSleep=[&]{pad.connected=false;};assert(!backend.TurnOffController(1));assert(sentCount==0);
 reset();pad.qam=true;onSleep=[&]{pad.id=2;};assert(!backend.TurnOffController(1));assert(sentCount==0);
 reset();pad.qam=true;assert(!backend.TurnOffController(1));assert(nowMs==10100&&sentCount==0);
 reset();backend._controllerMap.clear();assert(!backend.TurnOffController(1));assert(backend._controllerMap.empty());
 reset();device._ctrlr_type=5;assert(!backend.TurnOffController(1));assert(sentCount==0);
 reset();sendOK=false;assert(!backend.TurnOffController(1));assert(sentCount==1);
 std::cout<<"PASS: live Guide/QAM/Y release, debounce, polling lock released, disconnect/replacement, timeout, unsupported and write-failure paths\n";
}
'''
with tempfile.TemporaryDirectory(prefix='jsm-shutdown-') as d:
 p=Path(d);cpp=p/'shutdown.cpp';exe=p/'shutdown.exe';cpp.write_text(stub+method+tests)
 vcvars='C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
 batch=p/'build.cmd';batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /EHsc /std:c++17 "{cpp}" /Fe:"{exe}"\n')
 r=subprocess.run(['cmd.exe','/d','/c',str(batch)],cwd=p,capture_output=True,text=True)
 assert r.returncode==0,r.stdout+r.stderr
 r=subprocess.run([str(exe)],capture_output=True,text=True);assert r.returncode==0,r.stdout+r.stderr
 print(r.stdout.strip())
