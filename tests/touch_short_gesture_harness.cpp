// Numeric regression for the "preview moves smoothly, MOUSE mode doesn't move the
// cursor" bug. Root cause: the 1-euro filter's literature defaults (min_cutoff=0.8Hz,
// speed_coeff=0.015) were tuned for a desk mouse, not a touch gesture -- they deferred
// most of a short, fast swipe's displacement into a single "momentum" sample taken at
// the moment of lift, and the legacy TRACKBALL_DECAY default (1.0, shared with an
// unrelated feature) then coasted that deferred remainder out over 5+ SECONDS after
// the finger had already left the pad. A user watching the cursor during the actual
// touch would see almost nothing move.
//
// This lifts the same structs as touch_pipeline_harness.cpp (see run_touch_harness.py)
// and additionally lifts the SettingID default from main.cpp textually, so a future
// change to either can't silently drift back to the broken behaviour without failing
// this test.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

struct FloatXY {
    float _x=0.f,_y=0.f;
    FloatXY()=default;
    FloatXY(float a,float b):_x(a),_y(b){}
    float x() const {return _x;} float y() const {return _y;}
};

#include "lifted.inc"

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

static float accX = 0.f;
static long totalPx = 0;
static void moveMouse(float x) { accX += x; int ax = (int)accX; accX -= ax; totalPx += ax; }

struct GestureResult { long duringContact; long coastPx; float coastMs; };

// Mirrors processTouchMouse()'s exact contact + coast logic, including the
// decaySetting<=0 "no coast" branch added by this fix.
static GestureResult runGesture(float minCutoff, float beta, float trackballDecaySetting,
                                 float swipeFraction, float durationMs)
{
    accX = 0.f; totalPx = 0;
    const float TICK = 0.003f, tpSize = 1920.f, sens = 1.f;
    const int contactPolls = std::max(1, (int)(durationMs / 1000.f / TICK));

    TouchMousePipeline pipe; pipe.reset();
    float pos = 0.4f;
    FloatXY moved{0.f, 0.f};
    for (int i = 0; i < contactPolls; ++i)
    {
        pos = 0.4f + swipeFraction * (float(i + 1) / contactPolls);
        FloatXY d = pipe.step(pos, 0.5f, TICK, minCutoff, beta);
        moved = { d.x() * tpSize * sens, d.y() * tpSize * sens };
        moveMouse(moved.x());
    }
    long duringContact = totalPx;

    float momentumX = moved.x();
    int coastPolls = 0;
    if (trackballDecaySetting > 0.f)
    {
        while (std::fabs(momentumX) >= 0.1f && coastPolls < 200000)
        {
            momentumX *= exp2f(-TICK * trackballDecaySetting);
            moveMouse(momentumX);
            ++coastPolls;
        }
    }
    return { duringContact, totalPx - duringContact, coastPolls * TICK * 1000.f };
}

static int fails = 0;
static void check(bool ok, const char *what)
{
    printf("%-72s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok) ++fails;
}

int main()
{
    // The shipped defaults. run_touch_harness.py cross-checks these against the
    // actual registration lines in main.cpp, so this test can't silently drift
    // out of sync with what's really shipped.
    const float SHIPPED_MIN_CUTOFF = 6.0f;
    const float SHIPPED_SPEED_COEFF = 0.6f;
    const float SHIPPED_TRACKBALL_DECAY = 0.f; // 0 = coast disabled by default

    printf("=== Realistic quick swipe: 18%% of pad width in 90ms ===\n");
    GestureResult broken = runGesture(0.8f, 0.015f, 1.0f, 0.18f, 90.f); // pre-fix values
    GestureResult fixed  = runGesture(SHIPPED_MIN_CUTOFF, SHIPPED_SPEED_COEFF,
                                       SHIPPED_TRACKBALL_DECAY, 0.18f, 90.f);
    float expectedPx = 0.18f * 1920.f;

    printf("pre-fix (0.8/0.015, decay=1.0): during-contact=%ld px, coast=%ld px over %.0fms\n",
           broken.duringContact, broken.coastPx, broken.coastMs);
    printf("shipped (%.1f/%.2f, decay=off): during-contact=%ld px, coast=%ld px over %.0fms\n",
           SHIPPED_MIN_CUTOFF, SHIPPED_SPEED_COEFF, fixed.duringContact, fixed.coastPx, fixed.coastMs);

    check(broken.duringContact < expectedPx * 0.30f,
          "sanity: pre-fix defaults really did move <30% of the gesture live (confirms the bug existed)");
    check(broken.coastMs > 2000.f,
          "sanity: pre-fix defaults really did produce a multi-second post-lift coast");

    check(fixed.duringContact >= expectedPx * 0.60f,
          "shipped defaults move at least 60% of a realistic quick swipe LIVE, during contact");
    check(fixed.coastPx == 0 && fixed.coastMs == 0.f,
          "shipped defaults produce NO post-lift coast (TOUCHPAD_TRACKBALL_DECAY off by default)");

    // A fast full-pad flick is the hardest case: high displacement, short duration.
    GestureResult flick = runGesture(SHIPPED_MIN_CUTOFF, SHIPPED_SPEED_COEFF,
                                      SHIPPED_TRACKBALL_DECAY, 0.70f, 150.f);
    float flickExpected = 0.70f * 1920.f;
    printf("\nfast full-pad flick (70%% in 150ms): during-contact=%ld px (expected %.0f)\n",
           flick.duringContact, flickExpected);
    check(flick.duringContact >= flickExpected * 0.60f,
          "shipped defaults move at least 60% of a fast full-pad flick LIVE");

    // A slow deliberate pan (the ORIGINAL bug this whole feature started from) must
    // still be immune to stepping/jitter with the new, less-aggressive cutoff floor.
    {
        accX = 0.f; totalPx = 0;
        const float TICK = 0.003f, TPX = 1920.f;
        unsigned seed = 12345u;
        auto noise = [&]() { seed = seed * 1664525u + 1013904223u; return ((seed >> 8) & 0xFFFF) / 65535.f - 0.5f; };
        TouchMousePipeline pipe; pipe.reset();
        float pos = 0.2f;
        int maxStep = 0;
        for (int i = 0; i < 1000; ++i)
        {
            pos = 0.2f + 0.06f * (i + 1) * TICK;
            float p = pos + noise() * 0.0004f;
            FloatXY d = pipe.step(p, 0.5f, TICK, SHIPPED_MIN_CUTOFF, SHIPPED_SPEED_COEFF);
            long before = totalPx;
            moveMouse(d.x() * TPX);
            maxStep = std::max(maxStep, (int)std::labs(totalPx - before));
        }
        for (int i = 0; i < 400; ++i)
        {
            FloatXY d = pipe.step(pos, 0.5f, TICK, SHIPPED_MIN_CUTOFF, SHIPPED_SPEED_COEFF);
            moveMouse(d.x() * TPX);
        }
        check(maxStep <= 1, "slow pan still steps in single pixels with the new cutoff (no stepping regression)");
        check(totalPx >= 340 && totalPx <= 350, "slow pan still fully delivers its ~345px displacement");
    }

    // Stationary hold: raising the cutoff floor must not reintroduce visible jitter.
    {
        accX = 0.f; totalPx = 0;
        const float TICK = 0.003f, TPX = 1920.f;
        unsigned seed = 99u;
        auto noise = [&]() { seed = seed * 1664525u + 1013904223u; return (((seed >> 8) & 0xFFFF) / 65535.f - 0.5f) * 0.0006f; };
        TouchMousePipeline pipe; pipe.reset();
        for (int i = 0; i < 500; ++i)
        {
            float p = 0.5f + noise();
            FloatXY d = pipe.step(p, 0.5f, TICK, SHIPPED_MIN_CUTOFF, SHIPPED_SPEED_COEFF);
            moveMouse(d.x() * TPX);
        }
        check(totalPx == 0, "stationary hold with sensor noise produces zero drift at the new cutoff");
    }

    printf("\n%s (%d failure%s)\n", fails ? "FAILURES" : "ALL PASS", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
