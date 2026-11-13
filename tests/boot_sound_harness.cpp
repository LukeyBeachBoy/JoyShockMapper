#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include "TritonBootSound.h"
enum class SettingID { BOOT_SOUND_LEVEL };
struct Setting { int level = 0; int value() const { return level; } } setting;
struct SettingsManager {
    template<class T> static Setting* get(SettingID) { return &setting; }
};
struct ControllerDevice {
    void* _sdlController = nullptr;
    int _appliedBootSoundLevel = -1;
    std::string _key = "same-physical-controller";
};
int hardwareLevel = 2, attempts = 0;
bool transportWorks = true;
bool SDL_SendGamepadEffect(void*, const void* data, int size) {
    ++attempts;
    assert(size == 64);
    auto report = static_cast<const uint8_t*>(data);
    assert(report[0] == 1 && report[1] == 0xdc && report[2] == 2 && report[3] == 1);
    if (transportWorks) hardwareLevel = report[4];
    return transportWorks;
}
void apply(ControllerDevice* device) {
    // APPLY_BOOT_SOUND
}
int main() {
    ControllerDevice first;
    apply(&first);
    assert(hardwareLevel == 0 && attempts == 1);
    apply(&first);
    assert(attempts == 1); // Never write flash every poll.
    hardwareLevel = 2; // Firmware update while JSM stays open.
    ControllerDevice reconnected;
    apply(&reconnected);
    assert(hardwareLevel == 0 && attempts == 2);
    setting.level = 1; transportWorks = false;
    apply(&reconnected);
    assert(reconnected._appliedBootSoundLevel == 0 && hardwareLevel == 0);
    transportWorks = true; apply(&reconnected);
    assert(hardwareLevel == 1 && reconnected._appliedBootSoundLevel == 1);
    setting.level = -1; auto before = attempts;
    apply(&reconnected);
    assert(attempts == before); // Unset preserves the hardware preference.
    std::cout << "PASS: boot mute reapplied after firmware reset/reconnect; unchanged skips; failures retry; unset preserves\n";
}
