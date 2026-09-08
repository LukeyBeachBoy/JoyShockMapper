#pragma once
#include <algorithm>
#include <cmath>

// Pressure is normalized by the driver, but its usable range is device-specific.
// Use a relative drop, never an assumed pressure scale or decimal precision.
struct TouchLiftGuard
{
  float peak = 0.f;
  float scale = 1.f;
  void reset() { peak = 0.f; scale = 1.f; }
  float update(float pressure, float speed, float maxSpeed)
  {
    if (!std::isfinite(pressure) || !std::isfinite(speed) || maxSpeed <= 0.f || speed > maxSpeed)
    {
      reset();
      peak = std::isfinite(pressure) ? std::max(0.f, pressure) : 0.f;
      return scale;
    }
    peak = std::max(peak, pressure);
    if (peak <= 0.f) return scale = 1.f; // no analog pressure channel
    const float drop = 1.f - std::clamp(pressure / peak, 0.f, 1.f);
    // Ignore small fluctuations; fully stop after unloading 35% of the force.
    scale = 1.f - std::clamp((drop - 0.12f) / 0.23f, 0.f, 1.f);
    return scale;
  }
};

inline bool digitalTriggerPressed(bool wasPressed, float position, float threshold, float hysteresis)
{
  if (!std::isfinite(position)) return false;
  const float release = std::max(0.f, threshold - std::max(0.f, hysteresis));
  return position > (wasPressed ? release : threshold);
}
