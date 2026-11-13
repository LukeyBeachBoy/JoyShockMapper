#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

struct StickCalibration {
  bool enabled = false;
  // Clockwise corrections relative to the common rotation. Bounding each
  // correction to 40 degrees leaves at least 10 degrees between anchors.
  std::array<float, 4> angles{0, 0, 0, 0};
  std::array<float, 4> ranges{1, 1, 1, 1};
};

inline std::pair<float, float> calibrateStick(float x, float y, const StickCalibration& calibration)
{
  if (!calibration.enabled) return {x, y};
  const float radius = std::hypot(x, y);
  if (!std::isfinite(radius) || radius == 0.f) return {0.f, 0.f};
  std::array<float, 5> anchors{};
  std::array<float, 4> ranges{};
  for (int i = 0; i < 4; ++i) {
    const auto correction = calibration.angles[i];
    anchors[i] = i * 90.f - (std::isfinite(correction) ? std::clamp(correction, -40.f, 40.f) : 0.f);
    ranges[i] = std::isfinite(calibration.ranges[i]) ? std::clamp(calibration.ranges[i], .1f, 1.f) : 1.f;
  }
  anchors[4] = anchors[0] + 360.f;
  float angle = std::atan2(x, y) * 57.295779513f;
  if (angle < anchors[0]) angle += 360.f;
  if (angle >= anchors[4]) angle -= 360.f;
  int segment = 0;
  while (segment < 3 && angle >= anchors[segment + 1]) ++segment;
  const float t = (angle - anchors[segment]) / (anchors[segment + 1] - anchors[segment]);
  const float mappedAngle = (segment + t) * 1.570796326795f;
  const float travel = ranges[segment] + t * (ranges[(segment + 1) % 4] - ranges[segment]);
  const float mappedRadius = std::min(1.f, radius / travel);
  return {std::sin(mappedAngle) * mappedRadius, std::cos(mappedAngle) * mappedRadius};
}

// Sensor coordinates are up-positive. Rotate clockwise, then expand the ring's
// reachable radius to full travel. Run before deadzones and mode processing.
inline std::pair<float, float> transformStickGeometry(float x, float y, float degrees,
  bool restrictor, float range, const StickCalibration& calibration = {})
{
  if (!std::isfinite(x) || !std::isfinite(y)) return {0.f, 0.f};
  degrees = std::isfinite(degrees) ? std::clamp(degrees, -180.f, 180.f) : 0.f;
  if (!restrictor && degrees == 0.f && !calibration.enabled) return {x, y}; // Preserve legacy input exactly.
  const float angle = degrees * 0.017453292519943295f;
  const float c = std::cos(angle), s = std::sin(angle);
  const float radius = restrictor ? (std::isfinite(range) ? std::clamp(range, .1f, 1.f) : .7f) : 1.f;
  const float rotatedX = (x * c + y * s) / radius;
  const float rotatedY = (y * c - x * s) / radius;
  // Radial saturation preserves direction, including diagonals after rotation.
  const float scale = std::max(1.f, std::hypot(rotatedX, rotatedY));
  return calibrateStick(rotatedX / scale, rotatedY / scale, calibration);
}
