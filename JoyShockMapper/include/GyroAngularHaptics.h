#pragma once
#include <cmath>

// Distance on the selected gyro aim axes, in degrees. Feedback never creates
// a queue on the poll thread: at most one pulse is emitted per sample.
struct GyroAngularHaptics
{
	double travel = 0;
	float lastInterval = 0;
	void reset() { travel = 0; lastInterval = 0; }
	bool advance(float x, float y, float dt, float interval, bool enabled)
	{
		if (!enabled || !std::isfinite(x) || !std::isfinite(y) ||
		    !std::isfinite(dt) || dt <= 0 || !std::isfinite(interval) || interval <= 0)
		{
			reset();
			return false;
		}
		if (interval != lastInterval) { travel = 0; lastInterval = interval; }
		travel += std::hypot(double(x), double(y)) * double(dt);
		if (travel < interval) return false;
		travel = std::fmod(travel, double(interval));
		return true;
	}
};
