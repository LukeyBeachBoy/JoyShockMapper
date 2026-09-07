#pragma once
#include <cmath>

// Reconstruct a displacement over its completed interval (clamped to 1..4 ms),
// delayed by 4 ms, then average it over a 4 ms box. Integrating this trapezoidal
// kernel over each output poll makes packets overlap continuously. Each packet
// ends within 8 ms regardless of the next packet's size, direction, or timing.
// Signed displacement is conserved, without prediction or heap allocation.
struct TouchMouseResampler
{
    static constexpr double duration = 0.008;
    struct Packet {
        double x = 0, y = 0, age = duration, span = .004;
    };
    // TICK_TIME is >= 1 ms. Spare capacity also supports faster harness clocks.
    Packet packets[32]{};

    static double integral(double age, double span)
    {
        if (age <= 0) return 0;
        if (age >= span + .004) return 1;
        auto square = [](double v) { return v > 0 ? v*v : 0; };
        return (square(age) - square(age-span) - square(age-.004)
                + square(age-span-.004)) / (2 * span * .004);
    }

    void reset() { for (auto &p : packets) p = {}; }

    void add(float x, float y, float sampleDt)
    {
        if (!std::isfinite(x) || !std::isfinite(y) || (x == 0 && y == 0)) return;
        const double span = std::fmax(.001, std::fmin(.004, sampleDt));
        for (auto &p : packets) {
            if (p.age >= duration) {
                p = {x, y, span - .004, span};
                return;
            }
        }
        // Only reachable if called >32 times without advancing the clock.
        // Merge with the newest packet, retaining a bounded deadline and total.
        auto *newest = &packets[0];
        for (auto &p : packets) if (p.age < newest->age) newest = &p;
        const double remaining = 1 - integral(newest->age, newest->span);
        newest->x = newest->x * remaining + x;
        newest->y = newest->y * remaining + y;
        newest->age = 0;
    }

    void advance(float dt, float &x, float &y)
    {
        double dx = 0, dy = 0;
        if (!std::isfinite(dt) || dt <= 0) dt = .001f;
        for (auto &p : packets) {
            if (p.age >= duration) continue;
            const double before = integral(p.age, p.span);
            p.age += dt;
            const double weight = integral(p.age, p.span) - before;
            dx += p.x * weight;
            dy += p.y * weight;
        }
        x = float(dx); y = float(dy);
    }

    // The existing lift policy ends mouse motion on release. Deliver only the
    // remaining measured displacement once; do not invent a velocity tail.
    void finish(float &x, float &y)
    {
        double dx = 0, dy = 0;
        for (auto &p : packets) {
            const double weight = 1 - integral(p.age, p.span);
            dx += p.x * weight;
            dy += p.y * weight;
        }
        reset();
        x = float(dx); y = float(dy);
    }
};
