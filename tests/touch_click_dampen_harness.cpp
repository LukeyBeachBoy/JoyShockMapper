// Clicking a pad that is also driving the mouse rolls the finger across it, and
// that roll is a camera movement nobody asked for. TOUCHPAD_CLICK_DAMPEN scales
// the output down as the press comes on. Exercises the real processTouchMouse.
#include "touch_mouse_test_support.h"

static int failures = 0;
static void check(const char *what, bool ok) {
    std::printf("%-70s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok) ++failures;
}

static void tick(shared_ptr<JoyShock> &js, float x, float y, float pressure = 0.f, bool clickHeld = false) {
    TOUCH_POINT point{x, y};
    processTouchMouse(js, 1, point, 1, {1920, 1920}, {1, 1}, .003f, pressure, clickHeld);
}

// One identical swipe every time, so the only variable is the damping.
static double swipe(shared_ptr<JoyShock> &js, float pressure, bool clickHeld) {
    js->touchPipelines[1].reset();
    tick(js, .3f, .3f, pressure, clickHeld);
    const double before = totalX;
    for (int i = 1; i < 40; ++i) tick(js, .3f + i * .005f, .3f, pressure, clickHeld);
    // Drain whatever the pacer still owes, so a comparison is of delivered
    // distance rather than of how much happened to be in flight at the cutoff.
    for (int i = 0; i < 40; ++i) tick(js, -1, -1, 0.f, false);
    return totalX - before;
}

int main() {
    auto js = std::make_shared<JoyShock>();

    const double undamped = swipe(js, 0.f, false);
    check("a swipe moves the cursor with damping off", undamped > 100);

    // Off by default: neither a hard press nor a held click does anything until
    // TOUCHPAD_CLICK_DAMPEN is turned on.
    check("full pressure is ignored while the feature is off",
      std::abs(swipe(js, 1.f, false) - undamped) < undamped * .05);
    check("a held click is ignored while the feature is off",
      std::abs(swipe(js, 0.f, true) - undamped) < undamped * .05);

    js->settings[SettingID::TOUCHPAD_CLICK_DAMPEN] = 1.f;
    check("a held click stops the cursor dead at full damping", swipe(js, 0.f, true) == 0.0);
    check("an unclicked swipe is untouched with no pressure threshold set",
      std::abs(swipe(js, .9f, false) - undamped) < undamped * .05);

    js->settings[SettingID::TOUCHPAD_CLICK_DAMPEN] = .5f;
    const double half = swipe(js, 0.f, true);
    check("half damping halves the delivered distance",
      half > undamped * .40 && half < undamped * .60);

    // The analog ramp: leads the switch, so the cursor is already settling by the
    // time the click registers rather than stopping dead on it.
    js->settings[SettingID::TOUCHPAD_CLICK_DAMPEN] = 1.f;
    js->settings[SettingID::TOUCHPAD_CLICK_DAMPEN_THRESHOLD] = .5f;
    check("pressure below the threshold is undamped",
      std::abs(swipe(js, .4f, false) - undamped) < undamped * .05);
    const double middle = swipe(js, .75f, false);
    check("pressure halfway up the ramp roughly halves the distance",
      middle > undamped * .35 && middle < undamped * .65);
    check("pressure at the top of the ramp stops the cursor", swipe(js, 1.f, false) == 0.0);
    check("the ramp is monotonic", swipe(js, .6f, false) > swipe(js, .9f, false));

    // A swipe that ends in a click must not lose the tail it had already earned,
    // nor fling a coast out of the press itself.
    js->settings[SettingID::TOUCHPAD_TRACKBALL_DECAY] = 30.f;
    js->touchPipelines[1].reset();
    tick(js, .3f, .3f);
    for (int i = 1; i < 40; ++i) tick(js, .3f + i * .01f, .3f);
    const double beforeClick = totalX;
    for (int i = 0; i < 60; ++i) tick(js, .7f, .3f, 1.f, true);
    check("the pacing already in flight still drains through a click", totalX > beforeClick);
    const double afterDrain = totalX;
    for (int i = 0; i < 200; ++i) tick(js, -1, -1, 0.f, false);
    check("lifting off a fully damped press starts no coast", totalX == afterDrain);

    std::printf("\n%s (%d failures)\n", failures ? "FAILURES" : "ALL PASS", failures);
    return failures ? 1 : 0;
}
