"""Source-level guards for the touchpad -> mouse pipeline.

These assert the *shape* of the fix rather than its numeric behaviour, so they
catch a silent revert during a merge or refactor. The numeric behaviour is
covered by tests/touch_pipeline_harness.cpp, which lifts the real structs out of
JoyShock.h and runs them.
"""
from pathlib import Path

ROOT = Path(__file__).parents[1]
HEADER = (ROOT / 'JoyShockMapper/include/JoyShock.h').read_text()
MAIN = (ROOT / 'JoyShockMapper/src/main.cpp').read_text()
SDL = (ROOT / 'JoyShockMapper/src/SDLWrapper.cpp').read_text()
JSLW = (ROOT / 'JoyShockMapper/include/JslWrapper.h').read_text()

# JoyShockMapper is vendored as a submodule of JSM_Studio; the GUI assertions only
# run in that layout.
APP_PATH = ROOT.parent / 'JSM_GUI/jsm_gui_tauri/src/App.tsx'


def test_touch_deltas_are_never_truncated_to_integers():
    """The original bug: a slow swipe yields <1 pad unit per 3ms poll, and an
    int16_t cast turned every one of those into zero."""
    body = MAIN.split('struct TOUCH_POINT', 1)[1].split('\n};', 1)[0]
    assert 'float movX = 0.f;' in body
    assert 'float movY = 0.f;' in body
    assert 'short movX' not in body and 'short movY' not in body
    assert 'int16_t(' not in body
    # The consumer has to be widened too, or the cast just moves one frame down.
    assert 'handleTouchStickChange(TouchStick &ts, bool down, float movX, float movY' in \
        (ROOT / 'JoyShockMapper/src/JoyShock.cpp').read_text()


def test_one_euro_filter_runs_on_position_not_on_deltas():
    body = HEADER.split('struct TouchMousePipeline', 1)[1].split('\n};', 1)[0]
    assert 'OneEuroFilter posFilterX, posFilterY;' in body
    assert 'posFilterX.filter(rawX, dt, minCutoff, beta)' in body
    assert 'posFilterY.filter(rawY, dt, minCutoff, beta)' in body
    # The old fixed-alpha EMA on deltas plus lead compensation must be gone: the
    # lead step re-amplified exactly the jitter the smoothing had just removed.
    assert 'filteredX = a * x + (1.f - a) * filteredX;' not in body
    assert 'float dx = (filteredX - previousX) * a;' not in body
    # A fresh contact must not differentiate a position discontinuity.
    assert 'if (!initialized)' in body and 'return { 0.f, 0.f };' in body


def test_touch_callback_measures_real_elapsed_seconds():
    """delta_time arrived as the nominal TICK_TIME in *milliseconds*; every
    consumer downstream wants measured seconds."""
    body = MAIN.split('void touchCallback(', 1)[1].split('\nvoid ', 1)[0]
    assert '_touchTimeInitialized' in body
    assert 'chrono::steady_clock::now()' in body
    assert 'chrono::duration_cast<chrono::microseconds>' in body


def test_steam_controller_pads_are_square():
    """Reusing the DS4's 1920x920 made vertical gain 2.09x too low."""
    body = SDL.split('bool GetTouchpadDimension', 1)[1].split('\n\t}', 1)[0]
    steam = body.split('case JS_TYPE_STEAM_CONTROLLER_2026:', 1)[1].split('break;', 1)[0]
    assert 'sizeX = 1920;' in steam
    assert 'sizeY = 1920;' in steam


def test_pressure_threshold_promotes_contact_but_never_vetoes_it():
    """`(A || B) && B` reduces to `B`, which discarded SDL's capacitive down bit
    entirely and forced users to press hard."""
    body = SDL.split('TOUCH_STATE readTouchState', 1)[1].split('bool GetTouchpadDimension', 1)[0]
    assert 'state.t0Down = state.t0Down || pressure0 >= promoteThreshold;' in body
    assert 'state.t1Down = state.t1Down || pressure1 >= promoteThreshold;' in body
    assert '&& pressure0 >= promoteThreshold;' not in body
    assert '&& pressure1 >= promoteThreshold;' not in body
    assert 'state.t0Down = pressure0 >=' not in body
    assert 'state.t1Down = pressure1 >=' not in body


def test_position_only_contact_fallback_is_opt_in():
    """In-range coordinates latch the pad permanently down (an idle finger still
    reports an in-range 0,0), so this can never be the default."""
    assert 'SettingID::TOUCHPAD_POSITION_FALLBACK, Switch::OFF' in MAIN
    body = SDL.split('TOUCH_STATE readTouchState', 1)[1].split('bool GetTouchpadDimension', 1)[0]
    guard = 'getV<Switch>(SettingID::TOUCHPAD_POSITION_FALLBACK)->value() == Switch::ON'
    assert guard in body
    # Every coordinate-range test must sit inside that guard.
    after_guard = body.split(guard, 1)[1]
    assert body.count('state.t0X <= 1.f') == after_guard.count('state.t0X <= 1.f') == 1


def test_liftoff_state_machine_only_advances_on_the_polling_path():
    """joyShockPollCallback reads the touch state for telemetry before the poll
    loop reads it for the touch callback. Advancing the decaying-peak tracker
    twice per poll leaves `pressure < prevPressure` permanently false, which
    silently disables TOUCHPAD_LIFTOFF_RATIO."""
    assert 'return readTouchState(deviceId, false);' in SDL
    assert 'readTouchState(iter->first, true);' in SDL
    assert 'for (int pad = 0; advanceLiftoff && pad < 2; ++pad)' in SDL
    assert 'bool t0Lifting;' in JSLW and 'bool t1Lifting;' in JSLW


def test_liftoff_suppresses_motion_without_dropping_tracking():
    body = MAIN.split('static void processTouchMouse', 1)[1].split('\nvoid touchCallback', 1)[0]
    # Return before momentum is latched, so the involuntary tail cannot be flung.
    assert 'if (point.lifting)' in body
    lifting_idx = body.index('if (point.lifting)')
    assert body.index('pipe.momentumX = moved.x();') > lifting_idx
    assert body.index('moveMouse(moved.x(), moved.y());') > lifting_idx
    # Position tracking still ran, so the filter stays primed through the release.
    assert body.index('pipe.step(') < lifting_idx


def test_pipelines_are_not_wiped_on_finger_up():
    """Two unconditional reset() calls used to erase trackball momentum before it
    could ever be applied."""
    assert 'if (!point0.isDown()) js->touchPipelines[0].reset();' not in MAIN
    assert 'if (!point1.isDown()) js->touchPipelines[1].reset();' not in MAIN
    assert 'touchPipelines[2]' in HEADER
    assert 'TRACKBALL_DECAY' in MAIN


def test_new_tuning_settings_are_registered():
    for name in ('TOUCHPAD_MIN_CUTOFF', 'TOUCHPAD_SPEED_COEFF',
                 'TOUCHPAD_LIFTOFF_RATIO', 'TOUCHPAD_POSITION_FALLBACK'):
        assert f'SettingID::{name}' in MAIN, name
    # Old configs must still parse even though the setting no longer does anything.
    assert 'TOUCHPAD_SMOOTHING' in MAIN
    assert 'Deprecated' in MAIN.split('"TOUCHPAD_SMOOTHING"', 1)[1][:400]


def test_keymap_call_sites_forward_light_touch_threshold():
    if not APP_PATH.exists():
        return  # standalone checkout, not vendored under JSM_Studio
    app = APP_PATH.read_text()
    assert app.count('lightTouchThreshold={lightTouchThreshold}') >= 2
    assert app.count('onLightTouchThresholdChange={handleLightTouchThresholdChange}') >= 2


if __name__ == '__main__':
    failures = 0
    for name, test in sorted(globals().items()):
        if name.startswith('test_'):
            try:
                test()
                print(f'PASS {name}')
            except AssertionError as exc:
                failures += 1
                print(f'FAIL {name}: {exc}')
    raise SystemExit(1 if failures else 0)
