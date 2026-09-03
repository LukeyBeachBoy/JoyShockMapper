"""Source-level guards for the grip sensor threshold/hysteresis feature.

Before this, GRIP_L/GRIP_R were read straight off SDL's own pre-thresholded
digital button bits (raw joystick indices 20/21) -- the analog squeeze value
never reached JoyShockMapper at all, so "customise the grip distance" had
nothing to act on. This asserts the redesign: an analog reading gated by a
JSM-side Schmitt trigger (GRIP_THRESHOLD / GRIP_HYSTERESIS), with the raw
value and the derived digital state both reaching telemetry for the preview.
"""
import re
from pathlib import Path

ROOT = Path(__file__).parents[1]
SDL = (ROOT / 'JoyShockMapper/src/SDLWrapper.cpp').read_text()
MAIN = (ROOT / 'JoyShockMapper/src/main.cpp').read_text()
JSLW_H = (ROOT / 'JoyShockMapper/include/JslWrapper.h').read_text()
JSLW_CPP = (ROOT / 'JoyShockMapper/src/JslWrapper.cpp').read_text()
TELEMETRY_H = (ROOT / 'JoyShockMapper/include/Telemetry.h').read_text()
TELEMETRY_CPP = (ROOT / 'JoyShockMapper/src/Telemetry.cpp').read_text()
JSM_H = (ROOT / 'JoyShockMapper/include/JoyShockMapper.h').read_text()


def test_grip_is_read_as_analog_not_a_pretresholded_button():
    body = SDL.split('case JS_TYPE_STEAM_CONTROLLER_2026:', 1)[1].split('case JS_TYPE_DS:', 1)[0]
    assert 'SDL_GetJoystickAxis(joy, 6)' in body
    assert 'SDL_GetJoystickAxis(joy, 7)' in body
    # The old direct-to-button reads must be gone, not just supplemented.
    assert 'SDL_GetJoystickButton(joy, 20)' not in body
    assert 'SDL_GetJoystickButton(joy, 21)' not in body


def test_grip_gate_is_a_schmitt_trigger_with_persistent_state():
    """Hysteresis needs memory of the previous digital state, or the release
    point can never differ from the press point."""
    assert '_gripPressed[2]' in SDL
    body = SDL.split('case JS_TYPE_STEAM_CONTROLLER_2026:', 1)[1].split('case JS_TYPE_DS:', 1)[0]
    assert 'SettingID::GRIP_THRESHOLD' in body
    assert 'SettingID::GRIP_HYSTERESIS' in body
    assert 'releaseThreshold' in body
    # Release point must be strictly at or below the press point, never above.
    assert 'pressThreshold - hysteresis' in body
    assert 'pressed ? (gripRaw[side] > releaseThreshold) : (gripRaw[side] >= pressThreshold)' in body


def test_grip_settings_registered_with_bounded_defaults():
    for name in ('GRIP_THRESHOLD', 'GRIP_HYSTERESIS'):
        assert f'SettingID::{name}' in MAIN, name
    threshold = re.search(r'SettingID::GRIP_THRESHOLD,\s*(\d+\.?\d*)f\)', MAIN)
    hysteresis = re.search(r'SettingID::GRIP_HYSTERESIS,\s*(\d+\.?\d*)f\)', MAIN)
    assert threshold and 0.0 < float(threshold.group(1)) < 1.0
    assert hysteresis and 0.0 <= float(hysteresis.group(1)) < 1.0
    # Both settings clamp into [0,1] -- a value outside that range is meaningless
    # for a normalised squeeze distance.
    grip_block = MAIN[MAIN.index('SettingID::GRIP_THRESHOLD'):MAIN.index('SettingID::GRIP_HYSTERESIS') + 200]
    assert grip_block.count('clamp(next, 0.f, 1.f)') >= 2


def test_grip_analog_getters_exist_on_both_backends():
    assert 'virtual float GetLeftGrip(int deviceId) = 0;' in JSLW_H
    assert 'virtual float GetRightGrip(int deviceId) = 0;' in JSLW_H
    assert 'float GetLeftGrip(int deviceId) override' in SDL
    assert 'float GetRightGrip(int deviceId) override' in SDL
    # The non-SDL vendor backend has no Steam Controller 2026 support at all;
    # it must still implement the interface (stubbed at 0), or the build breaks.
    assert 'float GetLeftGrip(int deviceId) override' in JSLW_CPP
    assert 'float GetRightGrip(int deviceId) override' in JSLW_CPP


def test_telemetry_carries_raw_value_and_derived_digital_state():
    assert 'struct TelemetryGripState' in TELEMETRY_H
    grip_struct = TELEMETRY_H.split('struct TelemetryGripState', 1)[1].split('};', 1)[0]
    assert 'float value' in grip_struct
    assert 'bool pressed' in grip_struct
    assert 'TelemetryGripState leftGrip;' in TELEMETRY_H
    assert 'TelemetryGripState rightGrip;' in TELEMETRY_H
    # main.cpp must populate both from the same digital state GetButtons() derived,
    # not recompute the threshold independently (which could desync from the
    # actual bound action if the two ever drifted).
    body = MAIN.split('Grip sensors: raw analog value', 1)[1][:600]
    assert 'jsl->GetLeftGrip(device->_handle)' in body
    assert 'jsl->GetRightGrip(device->_handle)' in body
    assert 'JSOFFSET_MISC6' in body and 'JSOFFSET_MISC5' in body
    # Serialized to the GUI. The source is a C++ string literal, so the JSON
    # quotes are backslash-escaped in the file text.
    assert r'\"leftGrip\"' in TELEMETRY_CPP and r'\"rightGrip\"' in TELEMETRY_CPP
    leftGripJson = TELEMETRY_CPP.split(r'\"leftGrip\"', 1)[1][:80]
    assert r'\"value\"' in leftGripJson
    assert r'\"pressed\"' in leftGripJson


def test_grip_setting_ids_reserved():
    assert 'GRIP_THRESHOLD,' in JSM_H
    assert 'GRIP_HYSTERESIS,' in JSM_H


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
