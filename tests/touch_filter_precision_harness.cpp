// The touch pipeline filters absolute pad coordinates and differentiates the
// result, which makes it sensitive to how finely the filtered position can be
// represented: any rounding in the recurrence becomes displacement on an axis
// the finger never moved. TouchPositionFilter keeps the recurrence and the
// subtraction in double precision and updates by the error rather than summing
// two weighted absolute positions.
//
// NOTE: these checks pass against the single-precision predecessor too -- the
// property is locked here so it cannot silently regress, not because this file
// reproduces a defect. The reported single-precision error was about 7.45e-9 pad
// widths and was never large enough to explain visible jitter.
#include "touch_mouse_test_support.h"

static int failures = 0;
static void check(const char *what, bool ok) {
    std::printf("%-70s %s\n", what, ok ? "PASS" : "FAIL");
    if (!ok) ++failures;
}

int main() {
    auto js = std::make_shared<JoyShock>();
    auto &pipe = js->touchPipelines[0];

    // Strictly horizontal travel: Y is byte-identical on every sample.
    const float cutoff = js->getSetting(SettingID::TOUCHPAD_MIN_CUTOFF);
    const float beta = js->getSetting(SettingID::TOUCHPAD_SPEED_COEFF);
    const float dCut = js->getSetting(SettingID::TOUCHPAD_D_CUTOFF);

    auto sweep = [&](float minCutoff) {
        pipe.reset();
        double worst = 0;
        for (int i = 0; i < 600; ++i) {
            const FloatXY d = pipe.step(.2f + i * .0005f, .5f, .001f, minCutoff, beta, dCut);
            worst = std::max(worst, std::abs(double(d.y())));
        }
        return worst;
    };

    check("a constant perpendicular coordinate yields exactly zero displacement",
      sweep(cutoff) == 0.0);
    // The worst case for precision is a very low cutoff, where each step moves
    // the filtered position by a vanishing fraction of the distance to target.
    check("the same holds at a 0.3 Hz cutoff, where the steps are smallest",
      sweep(.3f) == 0.0);
    check("and with smoothing disabled entirely", sweep(0.f) == 0.0);

    // The moving axis must still track: a precision fix that flattened the
    // signal would satisfy the checks above for the wrong reason.
    pipe.reset();
    double travelled = 0;
    for (int i = 0; i < 600; ++i)
        travelled += pipe.step(.2f + i * .0005f, .5f, .001f, cutoff, beta, dCut).x();
    check("the moving axis still tracks the full sweep",
      travelled > .28 && travelled < .30);

    std::printf("\n%s\n", failures ? "FAILURES" : "All checks passed.");
    return failures ? 1 : 0;
}
