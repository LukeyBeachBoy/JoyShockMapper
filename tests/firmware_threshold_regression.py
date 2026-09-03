"""Source-level guards for the controller-side grip range and touch gate.

Both of these went through two wrong designs before this one, so the wrong
designs are asserted *out* as well as the right one in:

  * An analog grip axis (SDL_GetJoystickAxis(joy, 6/7)) gated by a host-side
    Schmitt trigger. There is no analog grip channel: SDL's Triton driver
    derives the grip purely from the TRITON_LEFT/RIGHT_GRIP_TOUCH bits and
    publishes it as capacitive sense, so that read returned nothing.
  * A millisecond debounce on the grip bit. Time is not the quantity anyone
    wants to tune about a squeeze, and it cannot make a light grip register.

What is actually adjustable is the threshold the controller's own firmware
uses, per side for the grips and as an on/off pair for touch -- the same
settings Steam Input drives. They are written with an ID_SET_SETTINGS_VALUES
feature report through SDL_SendGamepadEffect.
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

STEAM_BODY = SDL.split('case JS_TYPE_STEAM_CONTROLLER_2026:', 1)[1].split('case JS_TYPE_DS:', 1)[0]


def test_grip_is_read_from_sdl_capacitive_sense():
    assert 'SDL_GAMEPAD_CAPSENSE_LEFT_GRIP' in STEAM_BODY
    assert 'SDL_GAMEPAD_CAPSENSE_RIGHT_GRIP' in STEAM_BODY
    assert 'JSOFFSET_MISC6' in STEAM_BODY and 'JSOFFSET_MISC5' in STEAM_BODY


def test_abandoned_grip_designs_are_gone():
    """Neither an analog grip axis nor a host-side gate on the grip bit."""
    assert 'SDL_GetJoystickAxis(joy, 6)' not in SDL
    assert 'SDL_GetJoystickAxis(joy, 7)' not in SDL
    for gone in ('GRIP_THRESHOLD', 'GRIP_HYSTERESIS', '_gripPressed',
                 'GetLeftGrip', 'GetRightGrip',
                 'LEFT_GRIP_ON_MS', 'RIGHT_GRIP_ON_MS',
                 'LEFT_GRIP_OFF_MS', 'RIGHT_GRIP_OFF_MS'):
        for name, text in (('SDLWrapper.cpp', SDL), ('main.cpp', MAIN),
                           ('JslWrapper.h', JSLW_H), ('JslWrapper.cpp', JSLW_CPP),
                           ('JoyShockMapper.h', JSM_H)):
            assert gone not in text, f'{gone} still in {name}'


def test_firmware_setting_numbers_match_sdl_header():
    """Wire values from SDL's controller_constants.h. The enum is append-only
    by contract, so a mismatch here means someone mistyped an index."""
    expected = {
        'TRITON_ID_SET_SETTINGS_VALUES': 0x87,
        'TRITON_SETTING_LEFT_GRIP_CLICK_PRESSURE': 56,
        'TRITON_SETTING_RIGHT_GRIP_CLICK_PRESSURE': 57,
        'TRITON_SETTING_TIMP_TOUCH_THRESHOLD_ON': 72,
        'TRITON_SETTING_TIMP_TOUCH_THRESHOLD_OFF': 73,
    }
    for name, value in expected.items():
        match = re.search(rf'{name} = (0x[0-9A-Fa-f]+|\d+);', SDL)
        assert match, f'{name} not defined'
        assert int(match.group(1), 0) == value, f'{name} = {match.group(1)}, want {value}'


def test_feature_report_layout_matches_sdl_struct():
    """FeatureReportHeader is {type, length} then packed 3-byte ControllerSetting
    entries of {settingNum, little-endian settingValue}, in a 64-byte report."""
    assert 'TRITON_FEATURE_REPORT_BYTES = 64' in SDL
    body = SDL.split('static bool sendTritonSettings', 1)[1].split('\n\t}', 1)[0]
    assert 'buffer[0] = TRITON_ID_SET_SETTINGS_VALUES;' in body
    assert 'buffer[1] = uint8_t(settings.size() * 3);' in body
    assert 'uint8_t(setting.second & 0xFF)' in body
    assert 'uint8_t((setting.second >> 8) & 0xFF)' in body
    assert 'SDL_SendGamepadEffect(gamepad' in body


def test_unset_thresholds_never_overwrite_the_firmware():
    """-1 means "leave the controller's own value alone". Without this a fresh
    install would stamp our defaults over whatever the device (or Steam) had,
    and a bad guess at the raw units could leave the pads unresponsive."""
    body = SDL.split('void applyTritonSettings', 1)[1].split('\n\tint pollDevices', 1)[0]
    assert 'value < 0.f ? -1' in body
    for setting in ('leftGrip', 'rightGrip', 'touchOn', 'touchOff'):
        assert f'{setting} >= 0 && {setting} != device->_applied' in body, setting
    # Every registered firmware threshold defaults to the sentinel.
    for name in ('TOUCHPAD_TOUCH_ON', 'TOUCHPAD_TOUCH_OFF',
                 'LEFT_GRIP_RANGE', 'RIGHT_GRIP_RANGE'):
        assert re.search(rf'SettingID::{name}, -1\.f\)', MAIN), name
        assert re.search(rf'{name}[\s\S]{{0,400}}?setFilter\(&filterFirmwareThreshold\)', MAIN), name
    assert 'if (next < 0.f)' in MAIN.split('float filterFirmwareThreshold', 1)[1][:300]


def test_release_threshold_cannot_sit_above_the_press_threshold():
    """Touch hysteresis only makes sense downward; inverted, the pad latches on."""
    body = SDL.split('void applyTritonSettings', 1)[1].split('\n\tint pollDevices', 1)[0]
    assert 'touchOff = std::min(touchOff, touchOn);' in body


def test_settings_are_pushed_from_the_poll_loop_only_on_change():
    """A feature report is a device round trip; sending it every poll would
    compete with input reports for bandwidth."""
    assert 'applyTritonSettings(iter->second);' in SDL
    body = SDL.split('void applyTritonSettings', 1)[1].split('\n\tint pollDevices', 1)[0]
    assert 'if (pending.empty())' in body
    assert 'if (sendTritonSettings(device->_sdlController, pending))' in body
    # Cached per device, so a reconnect (which rebuilds the struct) re-applies.
    for field in ('_appliedLeftGripRange', '_appliedRightGripRange',
                  '_appliedTouchOn', '_appliedTouchOff'):
        assert f'int {field} = -1;' in SDL, field


def test_settings_only_go_to_hardware_that_has_them():
    body = SDL.split('void applyTritonSettings', 1)[1].split('\n\tint pollDevices', 1)[0]
    assert "_ctrlr_type != JS_TYPE_STEAM_CONTROLLER_2026" in body


def test_telemetry_reports_grip_as_the_bit_it_is():
    grip_struct = TELEMETRY_H.split('struct TelemetryGripState', 1)[1].split('};', 1)[0]
    assert 'bool pressed' in grip_struct
    assert 'float value' not in grip_struct, 'there is no analog grip value to report'
    # The preview must show exactly the state a binding sees, not a second
    # derivation that could drift from it.
    body = MAIN.split('status.leftGrip.pressed', 1)[1][:200]
    assert 'JSOFFSET_MISC5' in body
    assert 'status.buttons &' in MAIN.split('status.leftGrip.pressed', 1)[1][:100]
    # Serialized to the GUI; the source is a C++ string literal, so the JSON
    # quotes are backslash-escaped in the file text.
    assert r'\"leftGrip\"' in TELEMETRY_CPP and r'\"rightGrip\"' in TELEMETRY_CPP
    assert r'\"pressed\"' in TELEMETRY_CPP.split(r'\"leftGrip\"', 1)[1][:80]


def test_setting_ids_reserved():
    for name in ('TOUCHPAD_TOUCH_ON,', 'TOUCHPAD_TOUCH_OFF,',
                 'LEFT_GRIP_RANGE,', 'RIGHT_GRIP_RANGE,'):
        assert name in JSM_H, name


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
