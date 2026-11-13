#pragma once
#include <string>
#include <set>
#include <mutex>
#include <chrono>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <array>
#include <algorithm>
#include <initializer_list>

// Configuration values inherit device -> controller model -> shared base.
// A callback's context is thread-local; profile writes serialize with callbacks.
namespace ControllerContext {
inline std::set<std::string> isolatedScopes;
// Timed, so the controller thread can give up waiting rather than deadlock
// against a command that is itself waiting for the controller thread.
inline std::recursive_timed_mutex mutex;
// How long the controller thread waits for a running command before it lets
// that one reading go. Ordinary commands finish well inside it, so no reading
// is lost (gyro movement adds up reading by reading); a command that is itself
// waiting for the controller thread (RECONNECT_CONTROLLERS, tones, power-off)
// can no longer deadlock against it.
inline constexpr std::chrono::milliseconds kPollLockWait{ 25 };
// Take the configuration lock for one controller reading; false: skip it.
inline bool lockForPoll(std::unique_lock<std::recursive_timed_mutex> &lock) {
    lock = std::unique_lock<std::recursive_timed_mutex>(mutex, std::defer_lock);
    return lock.try_lock_for(kPollLockWait);
}
inline thread_local std::string model;
inline thread_local int handle = 0;
inline thread_local std::string writeScope;
// Persistent hardware identity, independent of enumeration order or player slot.
inline std::map<int, std::string> instanceScopes;
inline std::string instanceKey(int id) {
    const auto found = instanceScopes.find(id);
    return found == instanceScopes.end() ? std::string{} : found->second;
}
inline std::string layoutKey(const std::string &model, const std::string &hardware) {
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : hardware) { hash ^= c; hash *= 1099511628211ULL; }
    std::ostringstream out; out << model << "-device-" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}
inline std::string modelKey(int type, int vendor = 0, int product = 0) {
    if (type == 5 && vendor == 0x054c && product == 0x0df2) return "type-5-edge";
    return "type-" + std::to_string(type);
}
inline std::string deviceKey(int id) { return "device-" + std::to_string(id); }
inline std::string baseKey(int id) { return "device-base-" + std::to_string(id); }
struct HandleKeys {
    int id = 0;
    std::string device, base;
};
inline const HandleKeys& handleKeys() {
    thread_local HandleKeys keys;
    if (keys.id != handle) {
        keys.id = handle;
        keys.device = deviceKey(handle);
        keys.base = baseKey(handle);
    }
    return keys;
}
inline bool isolated() {
    if (!handle) return false;
    const auto& keys = handleKeys();
    return isolatedScopes.count(keys.device) || isolatedScopes.count(keys.base);
}
// A transient range of references, valid inside the current Guard. Settings
// are looked up hundreds of times per poll: rebuilding vectors and copying
// persistent device IDs here used to allocate for every single lookup.
struct ScopeRefs {
    std::array<const std::string*, 4> scopes{};
    size_t count = 0;
    ScopeRefs(std::initializer_list<const std::string*> values) : count(values.size()) {
        std::copy(values.begin(), values.end(), scopes.begin());
    }
    struct Iterator {
        const std::string* const* value;
        const std::string& operator*() const { return **value; }
        Iterator& operator++() { ++value; return *this; }
        bool operator!=(const Iterator& other) const { return value != other.value; }
    };
    Iterator begin() const { return {scopes.data()}; }
    Iterator end() const { return {scopes.data() + count}; }
};
inline ScopeRefs readScopes() {
    if (!writeScope.empty() && isolatedScopes.count(writeScope)) return {&writeScope};
    const auto& keys = handleKeys();
    if (handle && isolatedScopes.count(keys.device)) return {&keys.device};
    if (handle && isolatedScopes.count(keys.base)) return {&keys.base};
    if (!writeScope.empty()) return {&writeScope, &model};
    if (!handle) return {&model};
    const auto instance = instanceScopes.find(handle);
    static const std::string empty;
    return {&keys.device, &keys.base, instance == instanceScopes.end() ? &empty : &instance->second, &model};
}
struct Guard {
    std::unique_lock<std::recursive_timed_mutex> lock{mutex};
    std::string oldModel = model, oldWrite = writeScope;
    int oldHandle = handle;
    Guard(std::string nextModel, int nextHandle = 0, std::string nextWrite = {}) {
        model = std::move(nextModel); handle = nextHandle; writeScope = std::move(nextWrite);
    }
    ~Guard() { model = oldModel; handle = oldHandle; writeScope = oldWrite; }
};
inline bool matches(int type, int id, int vendor = 0, int product = 0) {
    return writeScope.empty() || writeScope == deviceKey(id) || writeScope == baseKey(id) || writeScope == instanceKey(id) || writeScope == modelKey(type, vendor, product);
}
}
