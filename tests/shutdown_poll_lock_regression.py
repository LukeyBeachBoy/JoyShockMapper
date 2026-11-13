"""Production poll ordering: a busy configuration must not monopolize SDL's lock."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'JoyShockMapper/src/SDLWrapper.cpp').read_text()
start = source.index('PollingTimer(uint64_t(tick_time));', source.index('int pollDevices()'))
start += len('PollingTimer(uint64_t(tick_time));')
end = source.index('for (auto iter = _controllerMap.begin()', start)
prefix = source[start:end]
harness = r'''
#include <cassert>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <iostream>
using std::lock_guard;
std::mutex controller_lock, test_lock;
std::condition_variable changed;
bool entered = false, proceed = false, configurationAvailable = false;
int updates = 0, mapped = 0;
namespace ControllerContext {
bool lockForPoll(std::unique_lock<std::recursive_timed_mutex>&) {
    if (configurationAvailable) return true;
    std::unique_lock lock(test_lock);
    entered = true; changed.notify_all();
    changed.wait(lock, [] { return proceed; });
    return false;
}
}
void SDL_UpdateGamepads() { ++updates; }
void pollOnce() {
    for (int tick = 0; tick < 1; ++tick) {
        // POLL_PREFIX
        // Model the existing mapping callback, which also requests config.
        std::unique_lock<std::recursive_timed_mutex> callbackConfig;
        if (ControllerContext::lockForPoll(callbackConfig)) ++mapped;
    }
}
int main() {
    std::thread poll(pollOnce);
    {
        std::unique_lock lock(test_lock);
        changed.wait(lock, [] { return entered; });
    }
    // A shutdown command owns configuration and needs this lock to read
    // button release/send power-off. It must be free while polling waits.
    bool available = controller_lock.try_lock();
    if (available) controller_lock.unlock();
    { std::lock_guard lock(test_lock); proceed = true; }
    changed.notify_all(); poll.join();
    assert(available && "polling blocked shutdown behind a configuration wait");
    assert(updates == 1 && mapped == 0); // Still refresh live release state.
    configurationAvailable = true;
    pollOnce();
    assert(updates == 2 && mapped == 1); // Normal mapping resumes.
    std::cout << "PASS: shutdown can acquire SDL lock during config contention; live buttons still update; mapping resumes\n";
}
'''.replace('// POLL_PREFIX', prefix)
with tempfile.TemporaryDirectory(prefix='jsm-poll-lock-') as directory:
    work = Path(directory)
    cpp = work / 'poll.cpp'
    exe = work / 'poll.exe'
    cpp.write_text(harness)
    vcvars = 'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
    batch = work / 'build.cmd'
    batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /EHsc /std:c++17 "{cpp}" /Fe:"{exe}"\n')
    subprocess.run(['cmd.exe', '/d', '/c', str(batch)], cwd=work, check=True)
    subprocess.run([str(exe)], check=True, timeout=10)
