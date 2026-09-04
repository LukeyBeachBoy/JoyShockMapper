// Numeric regression for the trackball coast, reported from a real build:
//
//   "if I swipe and release to the left, then touch the right side of the
//    trackpad while the cursor has not completely stopped, the cursor will
//    suddenly zip a large distance on screen"
//   "the cursor jumps BACK to a position it has already been in and then
//    returns to the next position in the coast trajectory"
//
// One root cause behind both. processTouchMouse used pipe.active to mean "this
// pipeline has a finger on it", but active stays true for the whole coast. So a
// contact arriving mid-coast skipped the filter reset, and the very next step()
// differentiated the gap between where the finger LIFTED and where it LANDED --
// a whole pad's width, delivered in one tick. The huge delta it recorded then
// became the momentum for the next coast, which is the "endless loop" of flings.
// A single stray capacitive sample during a coast did the same thing in
// miniature, which is what the backwards jump was.
//
// contact is now tracked separately from active. This harness lifts the real
// TouchMousePipeline (see run_touch_harness.py) and mirrors processTouchMouse's
// two branches exactly.
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

static const float MIN_CUTOFF = 6.0f, SPEED_COEFF = 0.6f;
static const float TPX = 1920.f, SENS = 1.f;

static float accX = 0.f;
static long emitted = 0;
static void moveMouse(float x) { accX += x; int ax = (int)accX; accX -= ax; emitted += ax; }
static void resetMouse() { accX = 0.f; emitted = 0; }

// Mirror of processTouchMouse for one axis. `contactFlagIsUsed` selects between
// the fixed logic and the pre-fix logic that keyed off `active`, so the harness
// can show the bug as well as its absence.
static void tick(TouchMousePipeline &pipe, bool down, float pos, float dt,
                 float decaySetting, bool contactFlagIsUsed)
{
    if (down)
    {
        const bool needsReset = contactFlagIsUsed ? !pipe.contact : !pipe.active;
        if (needsReset)
            pipe.reset();
        pipe.contact = true;
        FloatXY d = pipe.step(pos, 0.5f, dt, MIN_CUTOFF, SPEED_COEFF);
        float moved = d.x() * TPX * SENS;
        pipe.active = true;
        pipe.momentumX = moved / dt;
        moveMouse(moved);
    }
    else
    {
        pipe.contact = false;
        if (!pipe.active) return;
        if (decaySetting <= 0.f) { pipe.reset(); return; }
        float decay = exp2f(-dt * decaySetting);
        pipe.momentumX *= decay;
        if (fabsf(pipe.momentumX) < 40.f) pipe.reset();
        else moveMouse(pipe.momentumX * dt);
    }
}

static int failures = 0;
static void check(bool cond, const char *what)
{
    printf("%-72s %s\n", what, cond ? "PASS" : "FAIL");
    if (!cond) failures++;
}

// Swipe leftward, lift, coast a while, then land a finger on the far side.
// Returns the pixels emitted by that one landing tick.
static long retouchJump(bool contactFlagIsUsed)
{
    const float TICK = 0.003f, DECAY = 1.0f;
    TouchMousePipeline pipe; pipe.reset();
    resetMouse();

    for (int i = 0; i < 30; ++i)
        tick(pipe, true, 0.60f - 0.01f * i, TICK, DECAY, contactFlagIsUsed);
    for (int i = 0; i < 60; ++i)
        tick(pipe, false, 0.f, TICK, DECAY, contactFlagIsUsed);

    long before = emitted;
    tick(pipe, true, 0.95f, TICK, DECAY, contactFlagIsUsed);   // land far right
    return labs(emitted - before);
}

int main()
{
    const float TICK = 0.003f, DECAY = 1.0f;

    printf("=== re-touch during a coast ===\n");
    long buggy = retouchJump(false);
    long fixed = retouchJump(true);
    printf("landing tick emits: pre-fix=%ld px, fixed=%ld px\n", buggy, fixed);
    check(buggy > 100, "sanity: keying the reset off `active` really did zip the cursor");
    check(fixed == 0, "landing a finger mid-coast emits nothing on that tick");

    printf("\n=== a coast is stopped by the new contact, not resumed ===\n");
    {
        TouchMousePipeline pipe; pipe.reset(); resetMouse();
        for (int i = 0; i < 30; ++i) tick(pipe, true, 0.60f - 0.01f * i, TICK, DECAY, true);
        for (int i = 0; i < 20; ++i) tick(pipe, false, 0.f, TICK, DECAY, true);
        tick(pipe, true, 0.95f, TICK, DECAY, true);            // grab the trackball
        long before = emitted;
        for (int i = 0; i < 30; ++i) tick(pipe, true, 0.95f, TICK, DECAY, true);  // hold still
        check(labs(emitted - before) == 0, "holding still after grabbing the coast emits nothing");
    }

    printf("\n=== a stray sample during a coast cannot jump backwards ===\n");
    {
        TouchMousePipeline pipe; pipe.reset(); resetMouse();
        for (int i = 0; i < 30; ++i) tick(pipe, true, 0.40f + 0.01f * i, TICK, DECAY, true);
        long worst = 0;
        for (int i = 0; i < 40; ++i)
        {
            long before = emitted;
            // One stray capacitive frame in the middle of the coast, reporting a
            // position well behind where the finger actually left.
            tick(pipe, i == 20, i == 20 ? 0.20f : 0.f, TICK, DECAY, true);
            long step = emitted - before;
            if (step < worst) worst = step;
        }
        check(worst >= 0, "no tick during the coast moves the cursor backwards");
    }

    printf("\n=== coast speed does not depend on the tick length ===\n");
    {
        auto coastDistance = [&](float dt) {
            TouchMousePipeline pipe; pipe.reset(); resetMouse();
            // Same gesture in wall-clock terms regardless of tick length.
            int polls = (int)(0.090f / dt);
            for (int i = 0; i < polls; ++i)
                tick(pipe, true, 0.40f + 0.20f * (float(i + 1) / polls), dt, DECAY, true);
            long duringContact = emitted;
            for (int i = 0; i < (int)(2.0f / dt); ++i)
                tick(pipe, false, 0.f, dt, DECAY, true);
            return emitted - duringContact;
        };
        long fast = coastDistance(0.003f);
        long slow = coastDistance(0.006f);
        printf("coast at 333Hz=%ld px, at 167Hz=%ld px\n", fast, slow);
        check(fast > 0 && slow > 0, "both tick rates produce a coast");
        check(fabs(double(fast - slow)) / double(std::max(fast, slow)) < 0.25,
              "coast distance is within 25% across a 2x change in tick rate");
    }

    printf("\n%s (%d failures)\n", failures ? "FAILURES" : "ALL PASS", failures);
    return failures ? 1 : 0;
}
