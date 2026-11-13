#pragma once
#include <algorithm>
#include <cmath>

// Stateless recovery transfer for all acceleration curves; no sample history.
inline float gyroSteadyingFactor(float speed, float cutoff, float recovery)
{
 if (recovery > cutoff)
  return std::clamp((speed - cutoff) / (recovery - cutoff), 0.f, 1.f);
 return cutoff > 0.f && speed < cutoff ? 0.f : 1.f;
}
inline float gyroSteadyingSensitivity(float base, float floor, float factor)
{
 if (factor >= 1.f) return base;
 const float safeFloor = std::isfinite(floor) ? std::clamp(floor, 0.f, std::max(0.f, base)) : 0.f;
 // Quintic smootherstep: zero first/second derivatives at both ends. The
 // sensitivity joins the base curve without a recovery-threshold corner.
 const float t = std::clamp(factor, 0.f, 1.f);
 const float blend = std::clamp(t * t * t * (10.f + t * (-15.f + 6.f * t)), 0.f, 1.f);
 return safeFloor + blend * (base - safeFloor);
}
