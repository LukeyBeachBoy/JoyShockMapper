// Exercises the real processTouchMouse function with recorded mouse output.
// No SDL or OS mouse injection occurs in this executable.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include "MouseMotionAccumulator.h"
using std::shared_ptr;
struct FloatXY {
    float a, b;
    FloatXY(float x = 0, float y = 0): a(x), b(y) {}
    float x() const { return a; }
    float y() const { return b; }
};
#include "lifted.inc"
float OneEuroFilter::filter(float x, float dt, float cutoff, float beta) {
    const float dx = initialized ? (x - xPrev) / dt : 0;
    xPrev = x; initialized = true;
    const float speed = dxFilt.filter(dx, alpha(dCutoff, dt));
    return xFilt.filter(x, alpha(cutoff + beta * std::abs(speed), dt));
}
float OneEuroFilter::filter(float x, float dt) { return filter(x, dt, 1, .007f); }
enum class SettingID {
    TOUCHPAD_MIN_CUTOFF, TOUCHPAD_SPEED_COEFF, TOUCHPAD_D_CUTOFF,
    TOUCHPAD_ACCEL_MIN_GAIN, TOUCHPAD_ACCEL_MAX_GAIN, ACCEL_CURVE_LINK,
    TOUCHPAD_ACCELERATION, TOUCHPAD_TRACKBALL_DECAY, TOUCHPAD_TRACKBALL_MIN_VELOCITY
};
enum class AccelCurveLink { NONE, TOUCHPAD_USES_GYRO };
struct JoyShock {
    TouchMousePipeline touchPipelines[2];
    std::map<SettingID, float> settings {
        {SettingID::TOUCHPAD_MIN_CUTOFF, 6}, {SettingID::TOUCHPAD_SPEED_COEFF, .6f},
        {SettingID::TOUCHPAD_D_CUTOFF, 15}, {SettingID::TOUCHPAD_ACCEL_MIN_GAIN, 1},
        {SettingID::TOUCHPAD_ACCEL_MAX_GAIN, 1}, {SettingID::TOUCHPAD_TRACKBALL_MIN_VELOCITY, 200}
    };
    template<class T = float> T getSetting(SettingID id) { return static_cast<T>(settings[id]); }
};
struct TOUCH_POINT {
    float posX = -1, posY = -1;
    bool isDown() { return posX >= 0 && posX <= 1 && posY >= 0 && posY <= 1; }
};
struct AccelCurveShape { float minThreshold = 0; };
static AccelCurveShape readTouchpadAccelShape(JoyShock&) { return {}; }
static AccelCurveShape readGyroAccelShape(JoyShock&) { return {}; }
static float rescaleAdjustedSpeed(float x, const AccelCurveShape&, const AccelCurveShape&) { return x; }
static float evaluateAccelCurve(const AccelCurveShape&, float, float low, float) { return low; }
static MouseMotionAccumulator motion;
static double totalX = 0, totalY = 0;
static void moveMouse(float x, float y) {
    assert(std::isfinite(x) && std::isfinite(y));
    totalX += x; totalY += y;
    motion.add(x, y);
}
#include "lifted_process.inc"
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
