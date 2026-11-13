#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Spatial hysteresis only: no timers, averaging, queues or delayed key presses.
// Coordinates have +Y up. Bits are up, right, down, left respectively.
namespace StickDirections {
constexpr float legacyWidth = 53.130102f; // atan(.5) * 2, the original JSM sectors
struct Options {
    float width = legacyWidth;
    float hysteresis = 0;
    float activation = .15f;
    float releaseMargin = 0;
};
class State {
public:
    void reset() { sector_ = -1; }
    uint8_t update(float x, float y, Options options) {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(options.width) ||
            !std::isfinite(options.hysteresis) || !std::isfinite(options.activation) ||
            !std::isfinite(options.releaseMargin)) { reset(); return 0; }
        options.width = std::clamp(options.width, 10.f, 90.f);
        options.activation = std::clamp(options.activation, 0.f, 1.f);
        options.releaseMargin = std::clamp(options.releaseMargin, 0.f, options.activation);
        // A stability margin cannot swallow a narrow diagonal sector entirely.
        const float narrow = options.width < 90.f ? std::min(options.width, 90.f-options.width) : 90.f;
        options.hysteresis = std::clamp(options.hysteresis, 0.f, std::min(15.f, narrow*.45f));
        if (options.width != options_.width || options.hysteresis != options_.hysteresis ||
            options.activation != options_.activation || options.releaseMargin != options_.releaseMargin) reset();
        options_ = options;
        const float radius = std::hypot(x,y);
        const float release = options.activation-options.releaseMargin;
        if (radius <= release || (sector_ < 0 && radius <= options.activation)) { reset(); return 0; }
        float angle = std::atan2(x,y)*57.295779513f;
        if (angle < 0) angle += 360.f;
        if (angle >= 360.f) angle = 0;
        const int quadrant = int(angle/90.f);
        const float within = angle-quadrant*90.f, half = options.width*.5f;
        const int candidate = within <= half ? quadrant*2
            : within >= 90.f-half ? ((quadrant+1)*2)%8 : quadrant*2+1;
        if (sector_ >= 0 && candidate != sector_) {
            float distance = std::abs(angle-sector_*45.f);
            distance = std::min(distance,360.f-distance);
            const float previousHalf = sector_%2 ? (90.f-options.width)*.5f : half;
            if (distance <= previousHalf+options.hysteresis) return mask(sector_);
        }
        // A rapid return can cross centre between two hardware reports. Never
        // arm the opposite/adjacent direction on a sub-activation spring rebound.
        if (candidate != sector_ && radius <= options.activation) { reset(); return 0; }
        sector_ = candidate;
        return mask(sector_);
    }
private:
    static uint8_t mask(int sector) {
        constexpr uint8_t masks[]{1,3,2,6,4,12,8,9};
        return masks[sector];
    }
    int sector_ = -1;
    Options options_;
};
}
