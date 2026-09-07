#pragma once
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
#include "lifted_one_euro.inc"
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
