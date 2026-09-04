// Numeric proof for the "double cursor" teleport reported during a slow drag
// and a long, low-decay coast, reproducible about once a second: a value that
// genuinely stalls (not a stale HID duplicate -- see step()'s own comment) used
// to sit frozen past the 16ms deferral bound and then get "processed" anyway,
// handing the position filter a dt inflated by the whole deferred span for a
// sample where the raw position hadn't moved at all. dx read exactly zero
// either way, so the inflated dt bought nothing except a bigger alpha, which
// snapped whatever lag the filter's smoothed output had accumulated during the
// stall onto the raw value in one oversized step -- worse the longer the stall
// and the more lag had built up beforehand, which is exactly what a slow swipe
// (long stalls between quantisation-identical samples) or a long coast (plenty
// of time to watch it happen) makes visible.
//
// The fix settles the filter's state directly to the confirmed-stalled value,
// emitting nothing, instead of resolving the wait by running the normal update
// with an inflated dt. This proves two properties: a long hold emits nothing at
// all (not the trailing smear of small catch-up ticks an earlier, incomplete
// fix produced), and a newly-confirmed value's own tick delivers close to its
// own real displacement rather than something amplified by the stall.
//
// The struct under test is lifted verbatim out of JoyShockMapper/include/JoyShock.h
// at build time (see tests/run_touch_harness.py), so this exercises the committed
// code rather than a copy of it.
//
// Run with:  python3 tests/run_touch_harness.py
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

    printf("=== a long hold after real motion emits nothing, not a trailing smear ===\n");
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
            pipe.step(pos, 0.5f, TICK, MIN_CUTOFF, BETA);
        }
        // Hold perfectly still for far longer than kMaxDeferredDt (~5-6 ticks).
        int nonzeroConsumedTicks = 0;
        for (int i = 0; i < 400; ++i) // 1.2 real seconds
        {
            FloatXY d = pipe.step(pos, 0.5f, TICK, MIN_CUTOFF, BETA);
            if (pipe.sampleConsumed && (d.x() != 0.f || d.y() != 0.f))
                ++nonzeroConsumedTicks;
        }
        printf("nonzero-output ticks during a 1.2s hold: %d\n", nonzeroConsumedTicks);
        check(nonzeroConsumedTicks == 0,
              "a held-still raw position never emits output, however long lag had built up before it");
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
                FloatXY d = pipe.step(q, 0.5f, TICK, MIN_CUTOFF, BETA);
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
