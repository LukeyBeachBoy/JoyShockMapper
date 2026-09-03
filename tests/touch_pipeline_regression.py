"""Source-level guards for the touchpad -> mouse pipeline.

These assert the *shape* of the fix rather than its numeric behaviour, so they
catch a silent revert during a merge or refactor. The numeric behaviour is
covered by tests/touch_pipeline_harness.cpp, which lifts the real structs out of
JoyShock.h and runs them.
"""
import re
from pathlib import Path

ROOT = Path(__file__).parents[1]
HEADER = (ROOT / 'JoyShockMapper/include/JoyShock.h').read_text()
MAIN = (ROOT / 'JoyShockMapper/src/main.cpp').read_text()
SDL = (ROOT / 'JoyShockMapper/src/SDLWrapper.cpp').read_text()
JSLW = (ROOT / 'JoyShockMapper/include/JslWrapper.h').read_text()

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


def test_host_side_contact_gating_is_gone_entirely():
    """Three host-side gates used to sit on top of the driver's touch bit: a
    pressure veto (`(A || B) && B`, which reduces to `B` and threw the capacitive
    down bit away), an opt-in position fallback, and a decaying-peak liftoff
    tracker. All three were second-guessing a decision the controller already
    makes with a proper Schmitt trigger, and the pressure one is what forced
    people to press hard. The firmware thresholds replace all of them."""
    body = SDL.split('TOUCH_STATE GetTouchState', 1)[1].split('bool GetTouchpadDimension', 1)[0]
    steam = body.split('if (isSteam', 1)[1].split('else', 1)[0]
    # SDL's own down bit reaches TOUCH_STATE untouched.
    assert 'SDL_GetGamepadTouchpadFinger(_controllerMap[deviceId]->_sdlController, 0, 0, &state.t0Down' in steam
    assert 'SDL_GetGamepadTouchpadFinger(_controllerMap[deviceId]->_sdlController, 1, 0, &state.t1Down' in steam
    for gate in ('promoteThreshold', 'state.t0Down =', 'state.t1Down =',
                 'TOUCHPAD_POSITION_FALLBACK', 'lifting', 'Lifting'):
        assert gate not in steam, f'host-side gate {gate!r} is back'
    # Pressure is still surfaced, purely as a readout.
    assert 'state.t0Pressure = pressure0;' in steam
    assert 'state.t1Pressure = pressure1;' in steam


def test_touch_state_is_read_once_per_poll_with_no_hidden_state():
    """The liftoff tracker had to be split out of GetTouchState because telemetry
    and the poll loop both call it, advancing a stateful gate twice per poll. With
    the gate gone the read is pure again, so the split is dead weight -- and any
    new per-call state here would silently reintroduce that bug."""
    assert 'readTouchState' not in SDL
    assert 'TOUCH_STATE GetTouchState(int deviceId, bool previous) override' in SDL
    assert 'GetTouchState(iter->first, false)' in SDL
    assert 'bool t0Lifting' not in JSLW and 'bool t1Lifting' not in JSLW


def test_pipelines_are_not_wiped_on_finger_up():
    """Two unconditional reset() calls used to erase trackball momentum before it
    could ever be applied."""
    assert 'if (!point0.isDown()) js->touchPipelines[0].reset();' not in MAIN
    assert 'if (!point1.isDown()) js->touchPipelines[1].reset();' not in MAIN
    assert 'touchPipelines[2]' in HEADER
    assert 'TRACKBALL_DECAY' in MAIN


def test_new_tuning_settings_are_registered():
    for name in ('TOUCHPAD_MIN_CUTOFF', 'TOUCHPAD_SPEED_COEFF',
                 'TOUCHPAD_TOUCH_ON', 'TOUCHPAD_TOUCH_OFF'):
        assert f'SettingID::{name}' in MAIN, name
    # The superseded host-side knobs must be gone, not left as dead aliases the
    # GUI could still write to.
    for name in ('TOUCHPAD_LIGHT_TOUCH_THRESHOLD', 'TOUCHPAD_LIFTOFF_RATIO',
                 'TOUCHPAD_POSITION_FALLBACK'):
        assert name not in MAIN, name
    # Old configs must still parse even though the setting no longer does anything.
    assert 'TOUCHPAD_SMOOTHING' in MAIN
    assert 'Deprecated' in MAIN.split('"TOUCHPAD_SMOOTHING"', 1)[1][:400]

def test_touchpad_filter_defaults_favour_live_tracking_over_deferred_coast():
    """The literature 1-euro defaults (0.8Hz / 0.015) were tuned for a desk mouse,
    not a touch gesture: they deferred most of a short swipe's displacement into a
    single momentum sample released only on lift. Combined with the legacy
    TRACKBALL_DECAY default (1.0, shared with an unrelated feature), that produced
    a multi-second post-lift slide with almost nothing visible during the actual
    touch -- easy to read as "the touchpad doesn't move the mouse at all". See
    tests/touch_short_gesture_harness.cpp for the numeric reproduction."""
    cutoff = re.search(r'SettingID::TOUCHPAD_MIN_CUTOFF,\s*(\d+\.?\d*)f\)', MAIN)
    speed = re.search(r'SettingID::TOUCHPAD_SPEED_COEFF,\s*(\d+\.?\d*)f\)', MAIN)
    assert cutoff and float(cutoff.group(1)) >= 3.0, \
        'TOUCHPAD_MIN_CUTOFF default is back near the too-laggy 0.8Hz literature value'
    assert speed and float(speed.group(1)) >= 0.1, \
        'TOUCHPAD_SPEED_COEFF default is back near the too-laggy 0.015 literature value'


def test_touchpad_coast_is_a_dedicated_opt_in_setting_defaulting_off():
    """Must not reuse the legacy TRACKBALL_DECAY (a different, unrelated stick-based
    trackball feature with its own tuned default) -- and must default to no coast at
    all, matching Steam Input's Mouse touch style, which does not fling the cursor
    after release."""
    assert 'SettingID::TOUCHPAD_TRACKBALL_DECAY' in MAIN
    decay = re.search(r'SettingID::TOUCHPAD_TRACKBALL_DECAY,\s*(\d+\.?\d*)f\)', MAIN)
    assert decay and float(decay.group(1)) == 0.0, \
        'TOUCHPAD_TRACKBALL_DECAY must default to 0 (coast disabled)'
    body = MAIN.split('static void processTouchMouse', 1)[1].split('\nvoid touchCallback', 1)[0]
    assert 'SettingID::TOUCHPAD_TRACKBALL_DECAY' in body
    assert 'SettingID::TRACKBALL_DECAY)' not in body, \
        'processTouchMouse must not read the legacy shared TRACKBALL_DECAY setting'
    assert 'if (decaySetting <= 0.f)' in body and 'pipe.reset();' in body.split('if (decaySetting <= 0.f)', 1)[1][:80]


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
