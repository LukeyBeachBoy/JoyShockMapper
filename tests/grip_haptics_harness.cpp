// The runner inserts the production updateGripHaptics function below.
// Stub only the device/settings transport so the actual routing is exercised.
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <atomic>
#include "ControllerContext.h"
#include "TritonGripSettings.h"

enum class Switch { OFF, ON };
enum class HapticEffect { CLICK, TICK };
enum class SettingID {
    GRIP_HAPTIC_INTENSITY, GRIP_HAPTIC_EFFECT,
    GRIP_RELEASE_HAPTIC_INTENSITY, GRIP_RELEASE_HAPTIC_EFFECT,
    LEFT_GRIP_HAPTICS, RIGHT_GRIP_HAPTICS,
    GRIP_SENSOR_RANGE, GRIP_FLICKER_GUARD
};
template <typename T> struct Setting {
    T setting{};
    std::map<std::string, T> scoped;
    T value() const {
        const auto scopes = ControllerContext::readScopes();
        for (const auto& scope : scopes) {
            const auto found = scoped.find(scope);
            if (found != scoped.end()) return found->second;
        }
        return ControllerContext::isolated() ? T{} : setting;
    }
};
struct SettingsManager {
    template <typename T> static Setting<T>* get(SettingID id) {
        static std::map<SettingID, Setting<T>> values;
        return &values[id];
    }
};
constexpr int JS_TYPE_STEAM_CONTROLLER_2026 = 1;
constexpr int SDL_GAMEPAD_CAPSENSE_LEFT_GRIP = 0;
constexpr int SDL_GAMEPAD_CAPSENSE_RIGHT_GRIP = 1;
struct SDL_Gamepad { bool left = false, right = false; bool pads[2] = {}; };
uint64_t ticks = 0;
uint64_t SDL_GetTicks() { return ticks; }
int SDL_GetNumGamepadTouchpads(SDL_Gamepad*) { return 2; }
bool SDL_GetGamepadTouchpadFinger(SDL_Gamepad* pad, int side, int, bool* down, float*, float*, float*) {
    *down = pad->pads[side]; return true;
}
bool SDL_GetGamepadCapSense(SDL_Gamepad* device, int side) {
    return side == SDL_GAMEPAD_CAPSENSE_LEFT_GRIP ? device->left : device->right;
}
struct ControllerDevice {
    SDL_Gamepad* _sdlController;
    int _ctrlr_type = JS_TYPE_STEAM_CONTROLLER_2026;
    bool _leftGripWasOn = false, _rightGripWasOn = false;
    int _vendorId = 0, _productId = 0;
    int _appliedGripRange = -1, _appliedGripRelease = -1;
    triton_grip::WriteGate _gripWriteGate;
};
struct Pulse { bool right; float intensity; HapticEffect effect; };
std::vector<Pulse> pulses;
void sendGripHaptic(SDL_Gamepad*, bool right, float intensity, HapticEffect effect) {
    pulses.push_back({right, intensity, effect});
}

// UPDATE_GRIP_HAPTICS

bool transportWorks = true;
std::vector<std::pair<uint8_t, uint16_t>> writes;
bool sendTritonSettings(SDL_Gamepad*, const std::vector<std::pair<uint8_t, uint16_t>>& pending) {
    writes.insert(writes.end(), pending.begin(), pending.end());
    return transportWorks;
}
void applyTritonSettings(ControllerDevice* device) {
    // APPLY_GRIP_SETTINGS
}
// UPDATE_CONTROLLER_SETTINGS

int main() {
    SettingsManager::get<float>(SettingID::GRIP_HAPTIC_INTENSITY)->setting = 60;
    SettingsManager::get<float>(SettingID::GRIP_RELEASE_HAPTIC_INTENSITY)->setting = 30;
    SettingsManager::get<HapticEffect>(SettingID::GRIP_HAPTIC_EFFECT)->setting = HapticEffect::CLICK;
    SettingsManager::get<HapticEffect>(SettingID::GRIP_RELEASE_HAPTIC_EFFECT)->setting = HapticEffect::TICK;
    // Every gate combination, with each sensor toggling independently and both
    // toggling together. A disabled noisy grip must never affect the other side.
    for (int mask = 0; mask < 4; ++mask) {
        SettingsManager::get<Switch>(SettingID::LEFT_GRIP_HAPTICS)->setting = mask & 1 ? Switch::ON : Switch::OFF;
        SettingsManager::get<Switch>(SettingID::RIGHT_GRIP_HAPTICS)->setting = mask & 2 ? Switch::ON : Switch::OFF;
        SDL_Gamepad pad;
        ControllerDevice device{&pad};
        for (int state : {0, 1, 3, 2, 3, 1, 0, 3, 3, 0}) {
            const bool left = state & 1, right = state & 2;
            const bool previousLeft = pad.left, previousRight = pad.right;
            pad.left = left; pad.right = right;
            pulses.clear();
            updateGripHaptics(&device);
            size_t next = 0;
            if ((mask & 1) && left != previousLeft) {
                const auto& pulse = pulses.at(next++);
                assert(!pulse.right);
                assert(pulse.intensity == (left ? 60 : 30));
                assert(pulse.effect == (left ? HapticEffect::CLICK : HapticEffect::TICK));
            }
            if ((mask & 2) && right != previousRight) {
                const auto& pulse = pulses.at(next++);
                assert(pulse.right);
                assert(pulse.intensity == (right ? 60 : 30));
                assert(pulse.effect == (right ? HapticEffect::CLICK : HapticEffect::TICK));
            }
            assert(pulses.size() == next);
            assert(device._leftGripWasOn == left && device._rightGripWasOn == right);
        }
    }
    // Turning feedback on with a hand already resting must not synthesize touch.
    SDL_Gamepad pad{true, true};
    ControllerDevice device{&pad};
    for (auto id : {SettingID::LEFT_GRIP_HAPTICS, SettingID::RIGHT_GRIP_HAPTICS})
        SettingsManager::get<Switch>(id)->setting = Switch::OFF;
    updateGripHaptics(&device);
    pulses.clear();
    for (auto id : {SettingID::LEFT_GRIP_HAPTICS, SettingID::RIGHT_GRIP_HAPTICS})
        SettingsManager::get<Switch>(id)->setting = Switch::ON;
    updateGripHaptics(&device);
    assert(pulses.empty());
    // State is per device, so a second controller still produces its own edges.
    ControllerDevice second{&pad};
    updateGripHaptics(&second);
    assert(pulses.size() == 2);
    pulses.clear();
    updateGripHaptics(nullptr);
    ControllerDevice missing{nullptr};
    updateGripHaptics(&missing);
    device._ctrlr_type = 0;
    updateGripHaptics(&device);
    assert(pulses.empty());
    // The SDL maintenance path must resolve the model and the active isolated
    // chord before it reads either hardware thresholds or haptic preferences.
    device._ctrlr_type = JS_TYPE_STEAM_CONTROLLER_2026;
    pad.left = pad.right = false;
    device._leftGripWasOn = device._rightGripWasOn = false;
    const auto model = ControllerContext::modelKey(device._ctrlr_type);
    const auto chord = ControllerContext::deviceKey(7);
    auto range = SettingsManager::get<float>(SettingID::GRIP_SENSOR_RANGE);
    auto guard = SettingsManager::get<float>(SettingID::GRIP_FLICKER_GUARD);
    auto intensity = SettingsManager::get<float>(SettingID::GRIP_HAPTIC_INTENSITY);
    auto left = SettingsManager::get<Switch>(SettingID::LEFT_GRIP_HAPTICS);
    auto right = SettingsManager::get<Switch>(SettingID::RIGHT_GRIP_HAPTICS);
    range->setting = guard->setting = -1;
    range->scoped[model] = 200; guard->scoped[model] = 80;
    left->scoped[model] = right->scoped[model] = Switch::ON;
    updateControllerSettings(7, &device); // first startup poll, no editor write
    assert(device._appliedGripRange == 200 && device._appliedGripRelease == 80);
    writes.clear();
    updateControllerSettings(7, &device);
    assert(writes.empty()); // no feature report every poll

    range->scoped[chord] = 400; guard->scoped[chord] = 95;
    left->scoped[chord] = Switch::OFF; right->scoped[chord] = Switch::ON;
    intensity->scoped[chord] = 75;
    SettingsManager::get<HapticEffect>(SettingID::GRIP_HAPTIC_EFFECT)->scoped[chord] = HapticEffect::TICK;
    ControllerContext::isolatedScopes.insert(chord);
    pad.left = pad.right = true;
    updateControllerSettings(7, &device); // first held poll, no save/apply
    assert(device._appliedGripRange == 400 && device._appliedGripRelease == 95);
    assert(pulses.size() == 1 && pulses[0].right && pulses[0].intensity == 75 && pulses[0].effect == HapticEffect::TICK);
    assert(ControllerContext::model.empty() && ControllerContext::handle == 0);
    pulses.clear(); writes.clear();
    updateControllerSettings(7, &device);
    assert(pulses.empty() && writes.empty()); // held contact is one pulse

    ControllerDevice other{&pad};
    updateControllerSettings(8, &other); // chord must not leak to another pad
    assert(other._appliedGripRange == 200 && other._appliedGripRelease == 80);
    assert(pulses.size() == 2 && pulses[0].intensity == 60);
    pulses.clear(); writes.clear();
    ControllerDevice reconnected{&pad};
    updateControllerSettings(7, &reconnected);
    assert(reconnected._appliedGripRange == 400 && reconnected._appliedGripRelease == 95);
    assert(writes.size() == 2);
    ControllerContext::isolatedScopes.erase(chord); // release restores model
    range->scoped.erase(chord); guard->scoped.erase(chord);
    left->scoped.erase(chord); right->scoped.erase(chord);
    intensity->scoped.erase(chord);
    SettingsManager::get<HapticEffect>(SettingID::GRIP_HAPTIC_EFFECT)->scoped.erase(chord);
    transportWorks = false; writes.clear();
    updateControllerSettings(7, &device);
    assert(device._appliedGripRange == 400 && device._appliedGripRelease == 95);
    assert(writes.size() == 2);
    transportWorks = true;
    updateControllerSettings(7, &device);
    assert(device._appliedGripRange == 200 && device._appliedGripRelease == 80);

    // Commands can wait for SDL while holding the config lock. The hardware
    // poll must give way without consuming the sensor edge or sending defaults.
    std::atomic<bool> locked{false}, finished{false};
    std::thread command([&] {
        std::unique_lock<std::recursive_timed_mutex> lock(ControllerContext::mutex);
        locked = true;
        while (!finished) std::this_thread::yield();
    });
    while (!locked) std::this_thread::yield();
    pad.left = pad.right = false; pulses.clear(); writes.clear();
    updateControllerSettings(7, &device);
    finished = true; command.join();
    assert(pulses.empty() && writes.empty());
    updateControllerSettings(7, &device);
    assert(pulses.size() == 2); // release edge survives lock contention
    // Exercise the production write gate with real scope changes. Both pads
    // must be released; a held thumb must not let a profile change interrupt it.
    writes.clear(); pad.pads[1] = true; ticks = 1000;
    range->scoped[model] = 393; guard->scoped[model] = 99;
    updateControllerSettings(7, &device);
    assert(writes.empty() && device._appliedGripRange == 200);
    pad.pads[0] = true; pad.pads[1] = false; ticks = 1100;
    updateControllerSettings(7, &device);
    assert(writes.empty());
    pad.pads[0] = false; ticks = 1199;
    updateControllerSettings(7, &device);
    assert(writes.empty());
    ticks = 1200;
    updateControllerSettings(7, &device);
    assert(writes.size() == 2 && device._appliedGripRange == 393 && device._appliedGripRelease == 99);
    std::cout << "PASS: per-grip contact/release routing, noisy disabled side, live enable, independent devices\n";
    std::cout << "PASS: startup/model/chord/release/reconnect settings, scoped haptics, retry and bounded locking\n";
}
