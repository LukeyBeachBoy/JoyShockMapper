#pragma once
#include <cstdint>
#include <mutex>
#include <unordered_map>

// Freshness comes from sensor reports, never from whether their values change.
// SDL_GetGamepadSensorData succeeds even if it is returning an old cached value.
// Keys are SDL joystick instance IDs, never the mapper's logical handles.
class GyroReportWatchdog
{
    struct State { uint64_t lastReport = 0, lastRetry = 0; bool received = false; };
    std::mutex mutex;
    std::unordered_map<int, State> devices;
public:
    void connect(int handle, uint64_t now) {
        std::lock_guard<std::mutex> lock(mutex);
        devices[handle] = {0, now, false};
    }
    void disconnect(int handle) {
        std::lock_guard<std::mutex> lock(mutex);
        devices.erase(handle);
    }
    void observe(int handle, uint64_t now) {
        std::lock_guard<std::mutex> lock(mutex);
        if (auto it = devices.find(handle); it != devices.end()) {
            it->second.lastReport = now;
            it->second.received = true;
        }
    }
    bool fresh(int handle, uint64_t now) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = devices.find(handle);
        // The event thread can publish a report after the caller reads now.
        return it != devices.end() && it->second.received &&
            (now <= it->second.lastReport || now - it->second.lastReport <= 100);
    }
    bool shouldRetry(int handle, uint64_t now) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = devices.find(handle);
        if (it == devices.end()) return false;
        auto &state = it->second;
        if ((state.received && (now <= state.lastReport || now - state.lastReport <= 100)) ||
            now <= state.lastRetry || now - state.lastRetry < 1000)
            return false;
        state.lastRetry = now;
        return true;
    }
};
