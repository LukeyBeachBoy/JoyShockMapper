// The runner inserts the production updateGripHaptics function below.
// Stub only the device/settings transport so the actual routing is exercised.
#include <cassert>
#include <iostream>
#include <map>
#include <vector>

enum class Switch { OFF, ON };
enum class HapticEffect { CLICK, TICK };
enum class SettingID {
    GRIP_HAPTIC_INTENSITY, GRIP_HAPTIC_EFFECT,
    GRIP_RELEASE_HAPTIC_INTENSITY, GRIP_RELEASE_HAPTIC_EFFECT,
    LEFT_GRIP_HAPTICS, RIGHT_GRIP_HAPTICS
};
template <typename T> struct Setting {
    T setting{};
    T value() const { return setting; }
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
struct SDL_Gamepad { bool left = false, right = false; };
bool SDL_GetGamepadCapSense(SDL_Gamepad* device, int side) {
    return side == SDL_GAMEPAD_CAPSENSE_LEFT_GRIP ? device->left : device->right;
}
struct ControllerDevice {
    SDL_Gamepad* _sdlController;
    int _ctrlr_type = JS_TYPE_STEAM_CONTROLLER_2026;
    bool _leftGripWasOn = false, _rightGripWasOn = false;
};
struct Pulse { bool right; float intensity; HapticEffect effect; };
std::vector<Pulse> pulses;
void sendGripHaptic(SDL_Gamepad*, bool right, float intensity, HapticEffect effect) {
    pulses.push_back({right, intensity, effect});
}

// UPDATE_GRIP_HAPTICS

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
    std::cout << "PASS: per-grip contact/release routing, noisy disabled side, live enable, independent devices\n";
}
