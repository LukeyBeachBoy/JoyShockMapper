#include "touch_mouse_test_support.h"

int main()
{
    // Reversals and unequal axes retain their signed displacement through
    // irregular polls. A near-zero final packet cannot prolong old output.
    TouchMouseResampler resampler;
    double expectedX = 0, expectedY = 0, actualX = 0, actualY = 0;
    for (int i = 0; i < 200; ++i) {
        float x = i % 7 == 0 ? -17.f : 3.f;
        float y = i % 11 == 0 ? 12.f : -.25f;
        resampler.add(x, y, i % 2 ? .002f : .004f);
        expectedX += x; expectedY += y;
        float dx, dy;
        resampler.advance(i % 3 == 0 ? .0023f : .0007f, dx, dy);
        actualX += dx; actualY += dy;
    }
    resampler.add(.00001f, -.00001f, .004f);
    expectedX += .00001f; expectedY -= .00001f;
    float dx, dy;
    resampler.advance(.008f, dx, dy);
    actualX += dx; actualY += dy;
    assert(std::abs(actualX-expectedX) < .0001);
    assert(std::abs(actualY-expectedY) < .0001);
    resampler.advance(.001f, dx, dy);
    assert(dx == 0 && dy == 0);

    // Fractional motion survives a release exactly once, without a second tail.
    resampler.add(.25f, -.75f, .004f);
    resampler.advance(.001f, dx, dy);
    float x = dx, y = dy;
    resampler.finish(dx, dy);
    assert(std::abs(x+dx-.25f) < 1e-7);
    assert(std::abs(y+dy+.75f) < 1e-7);
    resampler.finish(dx, dy);
    assert(dx == 0 && dy == 0);

    // Reset cancels a previous contact, including an unused packet pool.
    resampler.add(100, -100, .004f);
    resampler.reset();
    resampler.advance(.05f, dx, dy);
    assert(dx == 0 && dy == 0);
    resampler.add(std::numeric_limits<float>::quiet_NaN(), 1, .001f);
    resampler.advance(.02f, dx, dy);
    assert(dx == 0 && dy == 0);

    // Pool overflow is defensive only (normal 1 ms polls need <=8 slots).
    for (int i = 0; i < 100; ++i) resampler.add(1, -2, .001f);
    resampler.advance(.01f, dx, dy);
    assert(std::abs(dx-100) < .001 && std::abs(dy+200) < .001);

    // Moving the position filter to the output clock must not change legacy
    // acceleration merely because each displacement now represents less time.
    for (float dt : {.001f, .003f, .008f}) {
        FloatXY moved = TouchMousePipeline::accelerate({1000*dt, -500*dt}, .1f, dt);
        FloatXY nominal = TouchMousePipeline::accelerate({3, -1.5f}, .1f);
        assert(std::abs(moved.x()/dt-nominal.x()/.003f) < .001);
        assert(std::abs(moved.y()/dt-nominal.y()/.003f) < .001);
    }

    // End-to-end flick/hold deadline, on all presets and both axes.
    for (float cutoff : {0.f, 10.f, 6.f, 2.5f}) {
        auto js = std::make_shared<JoyShock>();
        js->settings[SettingID::TOUCHPAD_MIN_CUTOFF] = cutoff;
        js->settings[SettingID::TOUCHPAD_TRACKBALL_DECAY] = 30;
        TOUCH_POINT p{.2f, .7f};
        auto poll = [&] { processTouchMouse(js, 1, p, 1, {1920,1920}, {5.6f,5.6f}, .001f); };
        totalX = totalY = 0;
        poll();
        int onset = 0;
        for (int i = 1; i <= 90; ++i) {
            if (i % 4 == 0) { p.posX += .01f; p.posY -= .008f; }
            poll();
            if (onset == 0 && totalX != 0) onset = i;
        }
        assert(onset > 0 && onset <= 8);
        assert(totalX > 0 && totalY < 0);
        for (int i = 0; i < 25; ++i) poll();
        double beforeX = totalX, beforeY = totalY;
        for (int i = 0; i < 200; ++i) poll();
        assert(totalX == beforeX && totalY == beforeY);
        p = {-1, -1};
        poll();
        assert(totalX == beforeX && totalY == beforeY); // No stale coast after a hold.
    }
    puts("PASS: signed conservation, deadlines, release, reset, invalid input, acceleration timing, flick response, stationary coast cancellation");
}
