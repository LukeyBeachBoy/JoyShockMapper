#pragma once
#include <algorithm>
#include <cstdint>

// DirectInput button numbers are firmware assignments, not physical key positions.
// This device-specific mask uses bits 0..31 for buttons and 32..35 for the hat.
namespace AzeronCyborg {
constexpr int type = 25;
constexpr int buttonCount = 32;
inline float axis(int value) {
    return std::clamp(value / (value < 0 ? 32768.f : 32767.f), -1.f, 1.f);
}
template<class ButtonReader>
uint64_t buttons(int count, int hat, bool supported, ButtonReader read) {
    uint64_t result = 0;
    for (int i = 0; i < std::clamp(count, 0, buttonCount); ++i)
        if (supported || read(i)) result |= uint64_t(1) << i;
    // SDL hat: up=1, right=2, down=4, left=8.
    for (int i = 0; i < 4; ++i)
        if (hat & (1 << i)) result |= uint64_t(1) << (32 + i);
    return result;
}
}
