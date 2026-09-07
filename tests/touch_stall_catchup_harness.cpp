// A position filter advances on every output poll, including held coordinates.
// Verify that this continuous catch-up has a bounded stop and never turns a
// long quantisation stall into one oversized sample. Uses the real pipeline.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

struct FloatXY {
    float _x=0.f,_y=0.f;
    FloatXY()=default;
    FloatXY(float a,float b):_x(a),_y(b){}
    float x() const {return _x;} float y() const {return _y;}
};

#include "lifted.inc"   // LowPassFilter1E, OneEuroFilter, TouchMousePipeline

float OneEuroFilter::filter(float x, float dt, float minCutoff, float beta)
{
    float dx = initialized ? (x - xPrev) / dt : 0.f;
    xPrev = x;
    initialized = true;
    float edx = dxFilt.filter(dx, alpha(dCutoff, dt));
    float cutoff = minCutoff + beta * std::abs(edx);
    return xFilt.filter(x, alpha(cutoff, dt));
}
float OneEuroFilter::filter(float x, float dt) { return filter(x, dt, 1.f, 0.007f); }

int main(){
    const float TICK = 0.003f;        // 333 Hz poll
    const float MIN_CUTOFF = 6.0f;    // shipped TOUCHPAD_MIN_CUTOFF
    const float BETA = 0.6f;          // shipped TOUCHPAD_SPEED_COEFF
    const float TPX = 1920.f;         // Steam Controller pad width, in device units

    int fails = 0;
    auto check = [&](bool ok, const char *what) {
        printf("%-78s %s\n", what, ok ? "PASS" : "FAIL");
        if (!ok) ++fails;
    };

    printf("=== a long hold settles for at most 16 ms, then stops ===\n");
    {
        TouchMousePipeline pipe;
        pipe.reset();
        // Warm up with genuine, ongoing motion so the filter's smoothed position
        // is realistically lagging the raw one when the hold begins -- mid-gesture
        // state, not fresh off a reset.
        float pos = 0.200f;
        for (int i = 0; i < 30; ++i)
        {
            pos += 0.00006f; // ~0.02 pad-widths/second: slow, but genuinely moving
            pipe.step(pos, 0.5f, TICK, MIN_CUTOFF, BETA, 15.0f);
        }
        // Hold perfectly still for far longer than the 16 ms stop threshold (~5-6 ticks).
        int nonzeroConsumedTicks = 0;
        float lastOutputTime = 0;
        for (int i = 0; i < 400; ++i) // 1.2 real seconds
        {
            FloatXY d = pipe.step(pos, 0.5f, TICK, MIN_CUTOFF, BETA, 15.0f);
            if (pipe.sampleConsumed && (d.x() != 0.f || d.y() != 0.f))
            {
                ++nonzeroConsumedTicks;
                lastOutputTime = (i + 1) * TICK;
            }
        }
        printf("nonzero-output ticks during a 1.2s hold: %d\n", nonzeroConsumedTicks);
        check(lastOutputTime <= .016f,
              "held position advances the filter briefly, with no output after the stop deadline");
    }

    printf("\n=== a newly-confirmed quantised step delivers close to its own size ===\n");
    {
        // Real hardware reports a fresh touch sample every poll, but the pad's
        // positional resolution is finite, so at a slow, constant swipe speed
        // many consecutive fresh reports quantise to the identical coordinate
        // before the finger's motion finally advances it by one step -- not the
        // stale-HID-duplicate case step()'s deferral was built for (report rate <
        // poll rate); every tick here is a genuinely new sample, it just happens
        // to repeat, for well beyond the 16ms deferral bound.
        const float quantStep = 1.f / 2048.f;
        float worstRatio = 0.f;
        for (float speed : { 0.005f, 0.01f, 0.02f, 0.04f, 0.08f, 0.15f })
        {
            TouchMousePipeline pipe;
            pipe.reset();
            float truePos = 0.2f;
            float lastQ = -1.f;
            float worstPxForSpeed = 0.f;
            const int N = 20000; // 60 real seconds at 333Hz
            for (int i = 0; i < N; ++i)
            {
                truePos += speed * TICK;
                float q = std::round(truePos / quantStep) * quantStep;
                FloatXY d = pipe.step(q, 0.5f, TICK, MIN_CUTOFF, BETA, 15.0f);
                if (!pipe.sampleConsumed)
                    continue;
                float px = std::fabs(d.x()) * TPX;
                if (px > worstPxForSpeed)
                    worstPxForSpeed = px;
            }
            // A confirmed step's own real size, in pixels, is the honest ceiling:
            // any single tick that meaningfully exceeds it is amplifying the step
            // rather than passing it through.
            float stepPx = quantStep * TPX;
            float ratio = worstPxForSpeed / stepPx;
            printf("speed=%.3f pad-widths/s   worst confirmed-step tick=%.3f px (%.2fx a %.3fpx step)\n",
                   speed, worstPxForSpeed, ratio, stepPx);
            if (ratio > worstRatio)
                worstRatio = ratio;
        }
        check(worstRatio <= 2.0f,
              "a confirmed step's own tick stays within 2x its real quantised size, not amplified by the stall before it");
    }

    printf("\n%s (%d failure%s)\n", fails ? "FAILURES" : "ALL PASS", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
