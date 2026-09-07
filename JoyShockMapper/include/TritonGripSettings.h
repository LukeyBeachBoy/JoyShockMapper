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
