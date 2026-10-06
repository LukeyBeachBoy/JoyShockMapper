#pragma once
#include <algorithm>
#include <cmath>
#include <utility>

// Relative selected-axis angular travel, independent of camera sensitivity.
// Capture the neutral on activation. Locking extents discards overshoot so
// reversing direction responds immediately after reaching the stick limit.
class GyroDeflection
{
public:
	void reset() { _active = false; _x = _y = 0.f; }
	std::pair<float, float> step(float velocityX, float velocityY, float seconds,
	  float rangeX, float rangeY, bool lockExtents, bool enabled, int target)
	{
		if (!enabled || !std::isfinite(seconds) || seconds <= 0.f || seconds > .25f ||
		    !std::isfinite(velocityX) || !std::isfinite(velocityY) ||
		    !std::isfinite(rangeX) || !std::isfinite(rangeY) || rangeX <= 0.f || rangeY <= 0.f)
		{
			reset();
			return {};
		}
		if (!_active || _target != target)
		{
			reset();
			_active = true;
			_target = target;
			return {};
		}
		_x += velocityX * seconds;
		_y += velocityY * seconds;
		if (!std::isfinite(_x) || !std::isfinite(_y)) { reset(); return {}; }
		if (lockExtents)
		{
			_x = std::clamp(_x, -rangeX, rangeX);
			_y = std::clamp(_y, -rangeY, rangeY);
		}
		return { std::clamp(_x / rangeX, -1.f, 1.f), std::clamp(_y / rangeY, -1.f, 1.f) };
	}
private:
	bool _active = false;
	int _target = 0;
	float _x = 0.f, _y = 0.f;
};
