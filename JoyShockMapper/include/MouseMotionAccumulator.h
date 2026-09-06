#pragma once
#include <cmath>
#include <limits>

struct MouseMotionAccumulator
{
    double x = 0.0, y = 0.0;

    void add(float dx, float dy)
    {
        if (std::isfinite(dx)) x += dx;
        if (std::isfinite(dy)) y += dy;
    }

    static int consume(double &value)
    {
        // Converting NaN, infinity or an out-of-range float to int is undefined.
        // Retaining it as the fractional remainder permanently breaks that axis.
        if (!std::isfinite(value) || value > std::numeric_limits<int>::max() ||
            value < std::numeric_limits<int>::min())
        {
            value = 0.0;
            return 0;
        }
        const int pixels = static_cast<int>(value);
        value -= pixels;
        return pixels;
    }
};
