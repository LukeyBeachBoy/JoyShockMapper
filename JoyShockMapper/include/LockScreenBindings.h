#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <sstream>
#include <unordered_map>

// Only individual numeric, submit, erase and escape keys are allowed here.
// Lock-screen profiles never execute commands, includes or arbitrary macros.
struct LockScreenBindings {
    struct Input { const char *name; int bit; };
    static constexpr std::array<Input,23> inputs{{
        {"UP",0},{"DOWN",1},{"LEFT",2},{"RIGHT",3},{"+",4},{"-",5},
        {"L3",6},{"R3",7},{"L",8},{"R",9},{"ZL",10},{"ZR",11},
        {"S",12},{"E",13},{"W",14},{"N",15},{"HOME",16},
        {"LSL",19},{"RSR",20},{"LSR",21},{"RSL",22},{"MISC2",28},{"MISC3",29}
    }};
    std::unordered_map<int,unsigned short> keys;
    uint64_t mask() const { uint64_t result=0;for(auto [bit,key]:keys)result|=uint64_t(1)<<bit;return result; }
    void read(const std::string &text) {
        keys.clear();std::istringstream source(text);std::string line;
        while(std::getline(source,line)) {
            line=line.substr(0,line.find('#'));const auto equal=line.find('=');if(equal==std::string::npos)continue;
            auto trim=[](std::string s){const auto a=s.find_first_not_of(" \t\r");return a==std::string::npos?std::string{}:s.substr(a,s.find_last_not_of(" \t\r")-a+1);};
            const auto name=trim(line.substr(0,equal)),value=trim(line.substr(equal+1));
            unsigned short key=0;
            if(value.size()==1 && value[0]>='0' && value[0]<='9')key=value[0];
            else if(value=="ENTER")key=0x0d;else if(value=="BACKSPACE")key=0x08;else if(value=="ESC")key=0x1b;
            for(auto input:inputs)if(name==input.name){if(key)keys[input.bit]=key;else keys.erase(input.bit);break;}
        }
    }
};

struct LockScreenCapture {
    bool active=false, waitingForRelease=false;
    uint64_t previous=0;
    struct Update { bool capture=false, entering=false, leaving=false;uint64_t pressed=0; };
    Update update(bool locked,uint64_t buttons) {
        Update result;
        if(locked && !active) {active=true;waitingForRelease=buttons!=0;previous=buttons;result.entering=true;}
        if(!locked && active) {active=false;waitingForRelease=buttons!=0;previous=buttons;result.leaving=true;}
        result.capture=locked || waitingForRelease;
        if(waitingForRelease) {if(!buttons)waitingForRelease=false;previous=buttons;return result;}
        if(locked && !result.entering)result.pressed=buttons & ~previous;
        previous=buttons;return result;
    }
};

// The one non-key action allowed on the secure desktop. It is a fixed hardware
// shortcut, never a command read from the sign-in profile. Wait for both wake
// buttons to be released before asking the command thread to power down.
struct LockScreenPowerShortcut {
    static constexpr uint64_t quickAccess=uint64_t(1)<<27, north=uint64_t(1)<<15;
    static constexpr uint64_t mask=quickAccess|north;
    bool active=false, waitingForRelease=false, armed=false;
    struct Update { bool suppressKeys=false, powerOff=false; };
    Update update(bool locked,uint64_t buttons) {
        if(!locked) {active=false;waitingForRelease=false;armed=false;return {};}
        if(!active) {active=true;waitingForRelease=(buttons&mask)!=0;}
        Update result;result.suppressKeys=(buttons&quickAccess)!=0 || armed || waitingForRelease;
        if(waitingForRelease) {if(!(buttons&mask))waitingForRelease=false;return result;}
        if((buttons&mask)==mask)armed=true;
        result.suppressKeys=result.suppressKeys || armed;
        if(armed && !(buttons&mask)) {result.powerOff=true;armed=false;}
        return result;
    }
};
