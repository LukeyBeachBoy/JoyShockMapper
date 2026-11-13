#include <windows.h>
#include <string>
#include "SDL3/SDL_hidapi.h"
#include "AzeronNativeProtocol.h"
#include "AzeronVendorApp.h"

// Deliberately outside the GUI's kill-on-close job. Owns no mappings, emits no
// input, and exits after restoring onboard mode on release, parent death, or
// loss of the input worker heartbeat. The device claim lives until restoration.
int wmain(int argc,wchar_t** argv) {
    if(argc!=5) return 2;
    wchar_t* end=nullptr; auto pid=wcstoul(argv[1],&end,10);
    if(!pid || *end) return 2;
    const std::wstring pathWide=argv[2], token=argv[4];
    if(!pathWide.starts_with(L"\\\\?\\HID#VID_16D0&PID_12F7&MI_04")) return 2;
    std::string path(pathWide.begin(),pathWide.end());
    HANDLE parent=OpenProcess(SYNCHRONIZE,FALSE,pid);
    HANDLE claim=OpenMutexW(SYNCHRONIZE,FALSE,argv[3]);
    HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,(token+L".ready").c_str());
    HANDLE release=OpenEventW(SYNCHRONIZE,FALSE,(token+L".release").c_str());
    HANDLE pulse=OpenEventW(SYNCHRONIZE,FALSE,(token+L".pulse").c_str());
    auto hid=SDL_hid_open_path(path.c_str());
    if(!parent||!claim||!ready||!release||!pulse||!hid) return 3;
    SetEvent(ready);
    HANDLE waits[]={parent,release,pulse};
    auto lastPulse=GetTickCount64(), lastAppCheck=lastPulse;
    while(true) {
        const auto result=WaitForMultipleObjects(3,waits,FALSE,500);
        const auto now=GetTickCount64();
        if(result==WAIT_OBJECT_0+2) lastPulse=now;
        else if(result!=WAIT_TIMEOUT) break;
        if(now-lastPulse>=3000) break;
        // Keep process enumeration off the latency-sensitive HID reader. The
        // helper also restores onboard mode when the vendor app takes over.
        if(now-lastAppCheck>=500) {
            lastAppCheck=now;
            if(AzeronNative::vendorAppRunning()) break;
        }
    }
    const uint8_t off[]={0}, on[]={1};
    auto rawOff=AzeronNative::command(15,1,off);
    auto onboard=AzeronNative::command(6,2,on);
    SDL_hid_write(hid,rawOff.data(),rawOff.size());
    SDL_hid_write(hid,onboard.data(),onboard.size());
    // Wait for the mode acknowledgement so a replacement mapper cannot take
    // over before the final restoration report has reached the firmware.
    bool restored=false;
    auto start=GetTickCount64();
    while(GetTickCount64()-start<750) {
        unsigned char data[128]; int n=SDL_hid_read_timeout(hid,data,sizeof(data),25);
        if(n>=8 && data[0]==1 && data[1]==0 && data[2]==6 && data[6]==1 && data[7]==1) { restored=true; break; }
        if(n<0) break;
    }
    SDL_hid_close(hid);
    for(HANDLE h:{parent,ready,release,pulse,claim}) CloseHandle(h);
    return restored?0:4;
}
