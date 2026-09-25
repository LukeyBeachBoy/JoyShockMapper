// TOUCHPAD_LIFT_SPEED suppresses the camera shove that a thumb makes on its way
// off the pad. Suppression is graded: a small pressure drop is meant to quiet a
// slow glide, not silence it. It used to clear the whole resampler queue on every
// poll where the scale was below 1, which at a fast poll rate erased each packet
// before its delayed delivery had even started -- so a light, partial pressure
// dip dropped the pan entirely instead of damping it. Only a fully suppressed
// lift may cancel pending output. Exercises the real processTouchMouse.
#include "touch_mouse_test_support.h"

static int failures = 0;
static void check(const char *what, bool ok) {
    std::printf("%-70s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok) ++failures;
}

// The guard only engages below TOUCHPAD_LIFT_SPEED pad pixels/second, so the
// glide has to be genuinely slow: .00005 pad widths per 1 ms tick is ~96 px/s
// against a 1920 px pad, comfortably under the 150 default.
static constexpr float kStep = .00005f;
static constexpr float kTick = .001f;

static void tick(shared_ptr<JoyShock> &js, float x, float y, float pressure) {
    TOUCH_POINT point{x, y};
    processTouchMouse(js, 1, point, 1, {1920, 1920}, {1, 1}, kTick, pressure, false);
}

// One slow glide, held at `peak` for the first half and `held` for the second,
// so the only variable between runs is what the pressure does partway through.
static double glide(shared_ptr<JoyShock> &js, float peak, float held) {
    js->touchPipelines[1].reset();
    tick(js, .3f, .3f, peak);
    const double before = totalX;
    for (int i = 1; i < 240; ++i) tick(js, .3f + i * kStep, .3f, peak);
    for (int i = 240; i < 480; ++i) tick(js, .3f + i * kStep, .3f, held);
    // Drain whatever the pacer still owes, so this compares delivered distance
    // rather than how much happened to be in flight at the cutoff.
    for (int i = 0; i < 240; ++i) tick(js, -1, -1, 0.f);
    return totalX - before;
}

int main() {
    auto js = std::make_shared<JoyShock>();

    const double steady = glide(js, .50f, .50f);
    check("a steady slow glide delivers motion with lift protection on", steady > 20);

    // 10% below the reference is inside the guard's 12% ignore band: untouched.
    const double nudge = glide(js, .50f, .45f);
    check("a pressure wobble inside the ignore band changes nothing",
      std::abs(nudge - steady) < steady * .05);

    // 20% below is partial suppression -- past the 12% floor, short of the 35%
    // stop. The regression: this delivered nothing at all, because every poll
    // wiped the queue. It must be reduced, and it must not be zero.
    const double partial = glide(js, .50f, .40f);
    check("a partial pressure drop still delivers motion", partial > 0.0);
    check("a partial pressure drop is quieter than a steady glide",
      partial < steady * .95);
    check("a partial pressure drop keeps most of the glide, not a token amount",
      partial > steady * .70);
    // Analytically: a 20% drop leaves a .652 scale, so half the glide at full gain
    // plus half at .652 is .826 of a steady one. The old code delivered ~.5 here.
    check("a partial drop delivers the graded amount, near .83 of a steady glide",
      partial > steady * .78 && partial < steady * .88);

    // 40% below is past the 35% stop: fully suppressed, and cancelling the queue
    // there is the whole point of the feature.
    const double lifted = glide(js, .50f, .30f);
    check("a full unload suppresses the thumb-lift shove", lifted < steady * .70);
    check("a full unload is quieter than a partial one", lifted < partial);

    js->settings[SettingID::TOUCHPAD_LIFT_SPEED] = 0.f;
    check("lift protection off leaves the same glide alone",
      std::abs(glide(js, .50f, .40f) - steady) < steady * .05);

    std::printf("\n%s\n", failures ? "FAILURES" : "All checks passed.");
    return failures ? 1 : 0;
}
