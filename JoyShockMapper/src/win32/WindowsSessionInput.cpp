#include "WindowsSessionInput.h"
#include "LockScreenBindings.h"
#include "VirtualHidInput.h"
#include "WindowsSignInConsent.h"
#include <filesystem>
#include <wtsapi32.h>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <thread>

namespace {
std::atomic<bool> sessionLocked{false};
LRESULT CALLBACK sessionWindow(HWND window,UINT message,WPARAM event,LPARAM session) {
    if(message==WM_WTSSESSION_CHANGE) {
        if(event==WTS_SESSION_LOCK)sessionLocked=true;
        if(event==WTS_SESSION_UNLOCK)sessionLocked=false;
    }
    return DefWindowProcW(window,message,event,session);
}
void startSessionNotifications() {
    std::thread([] {
        WNDCLASSW type{};type.lpfnWndProc=sessionWindow;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"JSME.SessionLock";
        if(!RegisterClassW(&type))return;
        const auto window=CreateWindowExW(0,type.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,type.hInstance,nullptr);
        if(!window)return;
        if(!WTSRegisterSessionNotification(window,NOTIFY_FOR_THIS_SESSION)){DestroyWindow(window);return;}
        MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
        WTSUnRegisterSessionNotification(window);DestroyWindow(window);
    }).detach();
}
}

bool windowsSessionLocked() {
    static std::once_flag initialized;std::call_once(initialized,startSessionNotifications);
    static std::mutex mutex;static ULONGLONG next=0;
    std::lock_guard<std::mutex> guard(mutex);
    const auto now=GetTickCount64();if(now<next)return sessionLocked.load();next=now+100;
    DWORD session=0;if(!ProcessIdToSessionId(GetCurrentProcessId(),&session))return sessionLocked.load();
    LPWSTR data=nullptr;DWORD bytes=0;
    if(WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,session,WTSSessionInfoEx,&data,&bytes)) {
        auto info=reinterpret_cast<WTSINFOEXW *>(data);
        if(bytes>=sizeof(WTSINFOEXW) && info->Level==1)sessionLocked=info->Data.WTSInfoExLevel1.SessionFlags==WTS_SESSIONSTATE_LOCK;
        WTSFreeMemory(data);
    }
    return sessionLocked.load();
}
namespace {
struct Controller {LockScreenCapture capture;LockScreenBindings bindings;LockScreenPowerShortcut power;};
std::mutex captureMutex;
std::unordered_map<int,Controller> controllers;
}
bool captureWindowsSignIn(int handle,uint64_t buttons,const std::string &profile,const std::function<void()> &releaseNormal,const std::function<void()> &requestPowerOff) {
    const bool locked=windowsSessionLocked();
    std::lock_guard<std::mutex> guard(captureMutex);
    static ULONGLONG nextConsentCheck=0;
    static bool enabled=false;
    const auto now=GetTickCount64();
    if(now>=nextConsentCheck) {
        nextConsentCheck=now+100;
        const bool next=readWindowsSignInConsent(
            (std::filesystem::path(profile).parent_path().parent_path()/"windows-sign-in-consent").string());
        if(next!=enabled) {
            // Re-enabling while locked must not type a button already held.
            // Revoking consent cancels pending power shortcuts and key state.
            for(auto &[id,state]:controllers)state={};
            releaseNormal();VirtualHidInput::instance().releaseAll();
        }
        enabled=next;
    }
    auto &controller=controllers[handle];
    if(enabled && locked && !controller.capture.active) {
        std::ifstream file(profile);std::string text((std::istreambuf_iterator<char>(file)),{});
        controller.bindings.read(text);
    }
    const auto power=controller.power.update(enabled && locked,buttons);
    // Even with sign-in disabled, suppress ordinary mappings on the locked
    // desktop. Otherwise an installed virtual HID driver could still type.
    const auto update=controller.capture.update(locked,buttons);
    if(update.entering || update.leaving) {
        releaseNormal();VirtualHidInput::instance().releaseAll();
    }
    if(power.powerOff && requestPowerOff)requestPowerOff();
    if(enabled && locked && !power.suppressKeys && VirtualHidInput::instance().available()) {
        for(auto [bit,key]:controller.bindings.keys)if(update.pressed & (uint64_t(1)<<bit)) {
            // No PIN or button sequence is logged or sent to Studio telemetry.
            if(key>='0' && key<='9')VirtualHidInput::instance().character(wchar_t(key));
            else {VirtualHidInput::instance().key(key,true);VirtualHidInput::instance().key(key,false);}
        }
    }
    return update.capture;
}
bool windowsSignInCapturing(int handle) {
    std::lock_guard<std::mutex> guard(captureMutex);
    auto it=controllers.find(handle);return it!=controllers.end() && (it->second.capture.active || it->second.capture.waitingForRelease);
}
void forgetWindowsSignInController(int handle) {
    std::lock_guard<std::mutex> guard(captureMutex);controllers.erase(handle);
}
