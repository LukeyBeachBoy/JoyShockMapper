#pragma once
#include "AzeronNativeProtocol.h"
#include "AzeronInputQueue.h"
#include "AzeronVendorApp.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include "SDL3/SDL.h"
#include "SDL3/SDL_hidapi.h"
#ifdef _WIN32
#include <windows.h>
#endif

namespace AzeronNative {
class Session {
public:
    explicit Session(std::string serial): serial_(std::move(serial)), worker_([this]{ run(); }) {}
    ~Session() { stop_=true; if(worker_.joinable()) worker_.join(); }
    Session(const Session&)=delete;
    size_t take(InputQueue::Batch& batch) { return queue_.take(batch); }
private:
    void publish(const char* status, const Input* input=nullptr) {
        Snapshot next; next.status=status;
        if(input) { next.buttons=input->buttons; next.x=input->x; next.y=input->y; }
        queue_.publish(next, input != nullptr);
    }
#ifdef _WIN32
    void release() {
        if(recovery_) {
            SetEvent(release_);
            // Recovery also runs on process death or a stalled input worker.
            WaitForSingleObject(recovery_,2000);
            CloseHandle(recovery_); recovery_=nullptr;
        }
        for(HANDLE* handle:{&ready_,&release_,&pulse_,&claim_})
            if(*handle) { CloseHandle(*handle); *handle=nullptr; }
        if(hid_) { SDL_hid_close(hid_); hid_=nullptr; }
        input_=Input{};
    }
    bool send(uint8_t type, std::initializer_list<uint8_t> payload={}) {
        auto packet=command(type,++sequence_,std::span(payload.begin(),payload.size()));
        return SDL_hid_write(hid_,packet.data(),packet.size())==int(packet.size());
    }
    bool startRecovery(const std::string& path) {
        std::wstring claimName=L"Local\\JSM.Azeron."+std::wstring(serial_.begin(),serial_.end());
        claim_=CreateMutexW(nullptr,FALSE,claimName.c_str());
        if(!claim_ || GetLastError()==ERROR_ALREADY_EXISTS) return false;
        token_=claimName+L"."+std::to_wstring(GetCurrentProcessId());
        ready_=CreateEventW(nullptr,TRUE,FALSE,(token_+L".ready").c_str());
        release_=CreateEventW(nullptr,TRUE,FALSE,(token_+L".release").c_str());
        pulse_=CreateEventW(nullptr,FALSE,FALSE,(token_+L".pulse").c_str());
        if(!ready_||!release_||!pulse_) return false;
        wchar_t exe[32768]{}; DWORD length=GetModuleFileNameW(nullptr,exe,DWORD(std::size(exe)));
        if(!length || length==std::size(exe)) return false;
        std::wstring helper(exe,length); helper.resize(helper.find_last_of(L"\\/")+1); helper+=L"AzeronRecovery.exe";
        std::wstring args=L"\""+helper+L"\" "+std::to_wstring(GetCurrentProcessId())+L" \""+
            std::wstring(path.begin(),path.end())+L"\" \""+claimName+L"\" \""+token_+L"\"";
        STARTUPINFOW startup{}; startup.cb=sizeof(startup); PROCESS_INFORMATION process{};
        BOOL inJob=FALSE; IsProcessInJob(GetCurrentProcess(),nullptr,&inJob);
        const DWORD flags=CREATE_NO_WINDOW|(inJob?CREATE_BREAKAWAY_FROM_JOB:0);
        if(!CreateProcessW(helper.c_str(),args.data(),nullptr,nullptr,FALSE,flags,nullptr,nullptr,&startup,&process)) return false;
        CloseHandle(process.hThread); recovery_=process.hProcess;
        HANDLE waits[]={ready_,recovery_};
        return WaitForMultipleObjects(2,waits,FALSE,1500)==WAIT_OBJECT_0;
    }
    bool connect() {
        if(vendorAppRunning()) { publish("vendor-app-open"); return false; }
        SDL_SetHint(SDL_HINT_HIDAPI_ENUMERATE_ONLY_CONTROLLERS,"0");
        auto list=SDL_hid_enumerate(vendor,product); std::string path;
        for(auto d=list; d; d=d->next) {
            if(d->usage_page!=0xff01 || d->usage!=0x101) continue;
            if(!serial_.empty() && (!d->serial_number || std::wstring(d->serial_number)!=std::wstring(serial_.begin(),serial_.end()))) continue;
            // Ambiguous hardware must not accidentally claim a different Cyborg.
            if(!path.empty()) { path.clear(); break; }
            path=d->path;
        }
        SDL_hid_free_enumeration(list);
        if(path.empty()) { publish("interface-unavailable"); return false; }
        hid_=SDL_hid_open_path(path.c_str());
        if(!hid_) { publish("interface-unavailable"); return false; }
        if(!startRecovery(path)) { publish("recovery-unavailable"); release(); return false; }
        // Recovery is armed before the first mode change.
        // Serialize queries: this firmware can drop adjacent requests. Retry a
        // missing response without misreporting a transport timeout as bad firmware.
        const auto query=[&](uint8_t type,auto received) {
            for(int attempt=0;attempt<3&&!stop_;++attempt) {
                if(!send(type)) return false;
                const auto deadline=SDL_GetTicks()+500;
                while(!stop_ && SDL_GetTicks()<deadline) {
                    SetEvent(pulse_);
                    unsigned char report[128]; int count=SDL_hid_read_timeout(hid_,report,sizeof(report),20);
                    if(count<0) return false;
                    if(count>0) input_.report(std::span(report,size_t(count)));
                    if(received()) return true;
                }
            }
            return false;
        };
        if(!query(5,[&]{return input_.firmware!=0;})) { release(); publish("interface-unavailable"); return false; }
        if(input_.firmware<91) { release(); publish("unsupported-firmware"); return false; }
        if(!query(2,[&]{return input_.calibration.valid;})) { release(); publish("calibration-unavailable"); return false; }
        if(!send(6,{0}) || !send(15,{1})) { release(); publish("interface-unavailable"); return false; }
        lastRaw_=lastPing_=lastPulse_=SDL_GetTicks();
        SetEvent(pulse_);
        return true;
    }
    void run() {
        // Like the mapper, favour input over background work, without real-time
        // scheduling or busy-spinning. HID's wait returns as soon as data arrives.
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        while(!stop_) {
            if(!hid_) {
                if(!connect()) { for(int i=0;i<100&&!stop_;++i) SDL_Delay(20); continue; }
            }
            bool failed=WaitForSingleObject(recovery_,0)==WAIT_OBJECT_0;
            if(!failed) {
                unsigned char report[128];
                const int n=SDL_hid_read_timeout(hid_,report,sizeof(report),10);
                if(n<0) failed=true;
                else if(n>0) input_.report(std::span(report,size_t(n)),[&](const Input& input, std::string_view message) {
                    if(message.starts_with("PJOY_24_")) lastRaw_=SDL_GetTicks();
                    if(input.software && input.rawSeen) publish("active",&input);
                    else publish("connecting");
                });
            }
            const auto now=SDL_GetTicks();
            // The recovery helper checks for Azeron Software. Process enumeration
            // can stall for milliseconds and must not run on the input thread.
            if(now-lastPulse_>=500) { lastPulse_=now; SetEvent(pulse_); }
            if(now-lastPing_>=1000) { lastPing_=now; if(!send(18)) failed=true; }
            if(now-lastRaw_>1000) failed=true;
            if(failed) { publish("connection-lost"); release(); continue; }
        }
        publish("released"); release();
    }
    SDL_hid_device* hid_=nullptr;
    HANDLE claim_=nullptr,ready_=nullptr,release_=nullptr,pulse_=nullptr,recovery_=nullptr;
    std::wstring token_;
    Input input_;
    uint8_t sequence_=0;
    uint64_t lastRaw_=0,lastPing_=0,lastPulse_=0;
#else
    void run() { publish("platform-unsupported"); }
#endif
    std::string serial_;
    InputQueue queue_;
    std::atomic_bool stop_=false;
    std::thread worker_;
};
}
