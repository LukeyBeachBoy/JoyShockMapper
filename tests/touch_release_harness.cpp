#include "touch_mouse_test_support.h"
static void tick(shared_ptr<JoyShock>& js, float x, float y, int pad = 1) {
    TOUCH_POINT point{x, y};
    processTouchMouse(js, pad, point, pad, {1920, 1920}, {1, 1}, .003f);
}
int main() {
    auto js = std::make_shared<JoyShock>();
    tick(js, .3f, .3f);
    for (int i = 1; i < 20; ++i) tick(js, .3f + i * .01f, .3f + i * .01f);
    assert(totalX > 0 && totalY > 0);
    double beforeX = totalX, beforeY = totalY;
    auto queued = js->touchPipelines[1].output;
    float owedX, owedY;
    queued.finish(owedX, owedY);
    tick(js, -1, -1); // One final measured remainder on the release tick.
    assert(std::abs(totalX-beforeX-owedX) < .001);
    assert(std::abs(totalY-beforeY-owedY) < .001);
    beforeX = totalX; beforeY = totalY;
    for (int i = 0; i < 500; ++i) tick(js, -1, -1);
    assert(totalX == beforeX && totalY == beforeY); // No default post-release motion.
    tick(js, .9f, .1f);
    assert(totalX == beforeX && totalY == beforeY); // Re-touch never teleports.
    tick(js, .8f, .2f);
    assert(totalX < beforeX && totalY > beforeY); // Both axes remain usable.

    js->settings[SettingID::TOUCHPAD_TRACKBALL_DECAY] = 30;
    for (int i = 0; i < 20; ++i) tick(js, .3f + i * .01f, .3f);
    tick(js, -1, -1);
    beforeX = totalX; beforeY = totalY;
    tick(js, .1f, .9f); // Grabbing an opt-in coast cancels its velocity.
    assert(totalX == beforeX && totalY == beforeY);
    for (int i = 0; i < 30; ++i) tick(js, .1f, .9f);
    assert(totalX == beforeX && totalY == beforeY);

    auto &pipe = js->touchPipelines[1];
    pipe.step(std::numeric_limits<float>::quiet_NaN(), .5f, .003f, 6, .6f, 15);
    assert(!pipe.initialized && !pipe.sampleConsumed);
    tick(js, .5f, .5f);
    beforeX = totalX; beforeY = totalY;
    tick(js, .6f, .6f);
    assert(totalX > beforeX && totalY > beforeY);

    motion = {};
    motion.add(std::numeric_limits<float>::quiet_NaN(), 3);
    assert(MouseMotionAccumulator::consume(motion.x) == 0);
    assert(MouseMotionAccumulator::consume(motion.y) == 3);
    motion.add(std::numeric_limits<float>::max(), -2);
    assert(MouseMotionAccumulator::consume(motion.x) == 0);
    assert(MouseMotionAccumulator::consume(motion.y) == -2);
    motion.add(4, 5);
    assert(MouseMotionAccumulator::consume(motion.x) == 4);
    assert(MouseMotionAccumulator::consume(motion.y) == 5);
    motion.add(.25f, -.25f);
    assert(MouseMotionAccumulator::consume(motion.x) == 0);
    motion.add(.75f, -.75f);
    assert(MouseMotionAccumulator::consume(motion.x) == 1);
    assert(MouseMotionAccumulator::consume(motion.y) == -1);
    puts("PASS: release stops, re-touch preserves both axes, coast grab, invalid-value recovery, fractional motion");
}
