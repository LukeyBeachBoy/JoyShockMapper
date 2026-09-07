// Exercise the real processTouchMouse and integer accumulator, not a mirrored
// algorithm. Different report/poll/frame phases expose beats hidden by averages.
#include "touch_mouse_test_support.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <string>

struct Event { double t, x; };
static int failures = 0;
static void check(bool ok, const char *name) {
    if (!ok) { printf("FAIL: %s\n", name); ++failures; }
}
static void sample(shared_ptr<JoyShock>& js, double x, double dt) {
    TOUCH_POINT p{float(x), .5f};
    processTouchMouse(js, 1, p, 1, {1920, 1920}, {10, 10}, float(dt));
}

static int replay(const char *input, const char *output) {
    std::ifstream in(input);
    std::ofstream out(output);
    if (!in || !out) return 2;
    auto js = std::make_shared<JoyShock>();
    js->settings[SettingID::TOUCHPAD_MIN_CUTOFF] = 2.5f;
    js->settings[SettingID::TOUCHPAD_SPEED_COEFF] = 3;
    std::string line;
    std::getline(in, line);
    out.precision(12);
    out << "time,dx,dy,ix,iy\n";
    while (std::getline(in, line)) {
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        double t, dt, x, y, pressure; int down;
        if (!(row >> t >> dt >> down >> x >> y >> pressure)) return 3;
        TOUCH_POINT p{down ? float(x) : -1.f, down ? float(y) : -1.f};
        double bx = totalX, by = totalY;
        processTouchMouse(js, 1, p, 1, {1920, 1920}, {5.6f, 5.6f}, float(dt));
        out << t << ',' << totalX-bx << ',' << totalY-by << ','
            << MouseMotionAccumulator::consume(motion.x) << ','
            << MouseMotionAccumulator::consume(motion.y) << '\n';
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 3) return replay(argv[1], argv[2]);
    const float cutoffs[] = {0, 10, 6, 2.5f};
    const float betas[] = {0, .8f, .6f, 3};
    for (int preset = 0; preset < 4; ++preset) {
        double worstCV = 0, worstMeanError = 0, zeroFraction = 0;
        for (int scenario = 0; scenario < 4; ++scenario) {
            const double hz = scenario == 1 ? 300 : scenario == 2 ? 333.333333 : 250;
            const double delays[] = {0, .004, .002, 0};
            for (double phase : {.0001, .0005, .0009}) {
                auto js = std::make_shared<JoyShock>();
                js->settings[SettingID::TOUCHPAD_MIN_CUTOFF] = cutoffs[preset];
                js->settings[SettingID::TOUCHPAD_SPEED_COEFF] = betas[preset];
                motion = {}; totalX = totalY = 0;
                const double speed = .15, expected = speed * 19200 / 240;
                double reportAt = phase, reportX = .2;
                int reportIndex = 0;
                double deliveryAt = reportAt;
                std::vector<Event> events;
                for (int i = 0; i <= 3000; ++i) {
                    double t = i * .001;
                    while (deliveryAt <= t + 1e-10) {
                        reportX = std::round((.2 + speed * reportAt) * 65536) / 65536;
                        reportAt += 1 / hz;
                        ++reportIndex;
                        deliveryAt = reportAt + (scenario == 3 ? delays[reportIndex % 4] : 0);
                    }
                    sample(js, reportX, .001);
                    events.push_back({t, double(MouseMotionAccumulator::consume(motion.x))});
                }
                for (int fp = 0; fp < 12; ++fp) {
                    double prev = .6 + fp / 12000., sum = 0, squares = 0;
                    int count = 0, zeros = 0;
                    size_t cursor = 0;
                    while (cursor < events.size() && events[cursor].t <= prev) ++cursor;
                    for (double frame = prev + 1/240.; frame < 2.9; frame += 1/240.) {
                        double dx = 0;
                        while (cursor < events.size() && events[cursor].t <= frame) dx += events[cursor++].x;
                        sum += dx; squares += dx * dx; ++count;
                        if (dx == 0) ++zeros;
                    }
                    double mean = sum / count;
                    worstCV = std::max(worstCV, std::sqrt(squares/count - mean*mean) / mean);
                    worstMeanError = std::max(worstMeanError, std::abs(mean / expected - 1));
                    zeroFraction = std::max(zeroFraction, double(zeros) / count);
                }
            }
        }
        printf("preset=%d: worst 240 Hz frame CV=%.2f%% mean error=%.2f%% empty frames=%.2f%%\n",
               preset, worstCV*100, worstMeanError*100, zeroFraction*100);
        // Off intentionally retains coordinate/timing noise. The always-on
        // resampler still bounds packet bursts, but cannot infer finger speed.
        check(worstCV < (preset == 0 ? .55 : .14),
              "enabled smoothing must approach the 1 kHz polling/whole-count floor");
        check(worstMeanError < .01, "steady sensitivity preserved");
        check(zeroFraction == 0, "steady pan cannot leave empty frames at 12 counts/frame");
    }
    // A tiny last movement must not set the repayment rate of a large backlog.
    auto js = std::make_shared<JoyShock>();
    js->settings[SettingID::TOUCHPAD_MIN_CUTOFF] = 0;
    motion = {}; totalX = totalY = 0;
    double x = .2;
    sample(js, x, .001);
    for (int i = 1; i <= 18; ++i) {
        if (i % 3 == 0) x += i < 18 ? .04 : .00001;
        sample(js, x, .001);
    }
    double lastMotionMs = 0;
    for (int i = 1; i <= 300; ++i) {
        double before = totalX;
        sample(js, x, .001);
        if (std::abs(totalX - before) > 1e-6) lastMotionMs = i;
    }
    printf("decelerating flick: last output %.0f ms after last changed position; distance %.3f / %.3f\n",
           lastMotionMs, totalX, (x-.2)*19200);
    check(lastMotionMs <= 8, "pacing tail has a fixed deadline");
    check(std::abs(totalX - (x-.2)*19200) < .01, "pacing preserves displacement");
    return failures ? 1 : 0;
}
