#pragma once
#include <string>
#include <set>
#include <mutex>
#include <vector>

// Configuration values inherit device -> controller model -> shared base.
// A callback's context is thread-local; profile writes serialize with callbacks.
namespace ControllerContext {
inline std::set<std::string> isolatedScopes;
inline std::recursive_mutex mutex;
inline thread_local std::string model;
inline thread_local int handle = 0;
inline thread_local std::string writeScope;
inline std::string modelKey(int type, int vendor = 0, int product = 0) {
    if (type == 5 && vendor == 0x054c && product == 0x0df2) return "type-5-edge";
    return "type-" + std::to_string(type);
}
inline std::string deviceKey(int id) { return "device-" + std::to_string(id); }
inline std::string baseKey(int id) { return "device-base-" + std::to_string(id); }
inline bool isolated() { return handle && (isolatedScopes.count(deviceKey(handle)) || isolatedScopes.count(baseKey(handle))); }
inline std::vector<std::string> readScopes() {
    if (!writeScope.empty() && isolatedScopes.count(writeScope)) return {writeScope};
    if (handle && isolatedScopes.count(deviceKey(handle))) return {deviceKey(handle)};
    if (handle && isolatedScopes.count(baseKey(handle))) return {baseKey(handle)};
    if (!writeScope.empty()) return {writeScope, model};
    return handle ? std::vector<std::string>{deviceKey(handle), model} : std::vector<std::string>{model};
}
struct Guard {
    std::unique_lock<std::recursive_mutex> lock{mutex};
    std::string oldModel = model, oldWrite = writeScope;
    int oldHandle = handle;
    Guard(std::string nextModel, int nextHandle = 0, std::string nextWrite = {}) {
        model = std::move(nextModel); handle = nextHandle; writeScope = std::move(nextWrite);
    }
    ~Guard() { model = oldModel; handle = oldHandle; writeScope = oldWrite; }
};
inline bool matches(int type, int id, int vendor = 0, int product = 0) {
    return writeScope.empty() || writeScope == deviceKey(id) || writeScope == baseKey(id) || writeScope == modelKey(type, vendor, product);
}
}
