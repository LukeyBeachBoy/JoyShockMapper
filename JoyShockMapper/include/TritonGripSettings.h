#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace triton_grip
{
// Triton settings, verified against Steam's personalization write path.
// These are NOT the TIMP entries in the older generic controller enum.
constexpr uint8_t thresholdSetting = 0x22;
constexpr uint8_t hysteresisSetting = 0x23;

// Changing the capacitive calibration pauses the firmware's pad coordinates
// even though IMU reports keep arriving. Never write it through an active
// gesture, including a brief contact dropout at the edge of the pad.
struct WriteGate {
    uint64_t lastContact = 0;
    bool waiting = false;
    bool ready(bool touching, uint64_t now) {
        if (touching) { lastContact = now; waiting = true; return false; }
        if (waiting && (now < lastContact || now - lastContact < 100)) return false;
        waiting = false;
        return true;
    }
};

inline int threshold(float value)
{
    return !std::isfinite(value) || value < 0 ? -1 :
        int(std::lround(std::clamp(value, 25.f, 400.f)));
}
inline int hysteresis(float value)
{
    return !std::isfinite(value) || value < 0 ? -1 :
        int(std::lround(std::clamp(value, 0.f, 100.f)));
}
inline std::vector<std::pair<uint8_t, uint16_t>> pending(
    int range, int guard, int previousRange, int previousGuard)
{
    std::vector<std::pair<uint8_t, uint16_t>> values;
    if (range >= 0 && range != previousRange)
        values.emplace_back(thresholdSetting, uint16_t(range));
    // Steam sends hysteresis independently, not threshold minus hysteresis.
    if (guard >= 0 && guard != previousGuard)
        values.emplace_back(hysteresisSetting, uint16_t(guard));
    return values;
}
}
