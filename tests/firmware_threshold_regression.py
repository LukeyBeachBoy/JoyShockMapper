"""Source-level guards for the controller-side grip sensor calibration.

The grip sensors are the capacitive strips inside the handles: they sense how
near your hands are, not how hard you squeeze. Three wrong designs preceded the
right one, so all three are asserted *out*:

  * An analog grip axis (SDL_GetJoystickAxis(joy, 6/7)) gated by a host-side
    Schmitt trigger. There is no analog grip channel: SDL's Triton driver
    derives the grip purely from the TRITON_LEFT/RIGHT_GRIP_TOUCH bits and
    publishes it as capacitive sense, so that read returned nothing.
  * A millisecond debounce on the grip bit. Time is not the quantity anyone
    wants to tune about a grip, and it cannot make a light one register.
  * LEFT/RIGHT_GRIP_CLICK_PRESSURE. Those are force thresholds for the physical
    back buttons (L4/R4/L5/R5), which have nothing to do with the capacitive
    strips, so writing them changed nothing a grip sensor does.

What is actually adjustable is TIMP_TOUCH_THRESHOLD_ON/OFF, the firmware's one
capacitive threshold pair -- the same pair behind Steam Input's Grip Sensor
Calibration page, where it appears as "Grip Sensor Range" and "Flicker Guard
Size". Written with an ID_SET_SETTINGS_VALUES feature report through
SDL_SendGamepadEffect.
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
                 'LEFT_GRIP_OFF_MS', 'RIGHT_GRIP_OFF_MS',
                 'LEFT_GRIP_RANGE', 'RIGHT_GRIP_RANGE'):
        for name, text in (('SDLWrapper.cpp', SDL), ('main.cpp', MAIN),
                           ('JslWrapper.h', JSLW_H), ('JslWrapper.cpp', JSLW_CPP),
                           ('JoyShockMapper.h', JSM_H)):
            assert gone not in text, f'{gone} still in {name}'


def test_the_back_buttons_force_thresholds_are_not_written_as_grip_settings():
    """*_GRIP_CLICK_PRESSURE gates the physical back buttons. Writing it as a grip
    sensor range is the mistake that made the setting appear to do nothing, so the
    wire constants must be gone -- the surviving mentions are prose saying why."""
    for text, name in ((SDL, 'SDLWrapper.cpp'), (MAIN, 'main.cpp')):
        assert 'TRITON_SETTING_LEFT_GRIP_CLICK_PRESSURE' not in text, name
        assert 'TRITON_SETTING_RIGHT_GRIP_CLICK_PRESSURE' not in text, name


def test_firmware_setting_numbers_match_sdl_header():
    """Wire values from SDL's controller_constants.h. The enum is append-only
    by contract, so a mismatch here means someone mistyped an index."""
    expected = {
        'TRITON_ID_SET_SETTINGS_VALUES': 0x87,
        'TRITON_SETTING_TIMP_TOUCH_THRESHOLD_ON': 72,
        'TRITON_SETTING_TIMP_TOUCH_THRESHOLD_OFF': 73,
        'TRITON_ID_OUT_REPORT_HAPTIC_COMMAND': 0x82,
    }
    for name, value in expected.items():
        match = re.search(rf'{name} = (0x[0-9A-Fa-f]+|\d+);', SDL)
        assert match, f'{name} not defined'
        assert int(match.group(1), 0) == value, f'{name} = {match.group(1)}, want {value}'


def test_feature_report_starts_with_the_hid_report_id():
    """The report id is not part of FeatureReportMsg -- SDL's own SetSensorsEnabled
    writes it by building the message at buffer + 1. Omitting it puts every field
    one byte early and the controller silently ignores the report, which is what
    made the first grip-range implementation do nothing at all on the device."""
    body = SDL.split('static bool sendTritonSettings', 1)[1].split('\n\t}', 1)[0]
    assert 'buffer[0] = TRITON_HID_REPORT_ID;' in body
    assert re.search(r'TRITON_HID_REPORT_ID = 1;', SDL)
    assert 'buffer[1] = TRITON_ID_SET_SETTINGS_VALUES;' in body
    # Settings start after the 1-byte id plus the 2-byte header.
    assert 'size_t offset = 3;' in body


def test_feature_report_layout_matches_sdl_struct():
    """FeatureReportHeader is {type, length} then packed 3-byte ControllerSetting
    entries of {settingNum, little-endian settingValue}, in a 64-byte report."""
    assert 'TRITON_FEATURE_REPORT_BYTES = 64' in SDL
    assert 'TRITON_SETTING_BYTES = 3' in SDL
    body = SDL.split('static bool sendTritonSettings', 1)[1].split('\n\t}', 1)[0]
    assert 'uint8_t(setting.second & 0xFF)' in body
    assert 'uint8_t((setting.second >> 8) & 0xFF)' in body
    assert 'SDL_SendGamepadEffect(gamepad' in body
    # Length counts the settings actually written, not the ones requested: the
    # loop stops early if they would not fit, and a length longer than the
    # payload would have the controller read past what was sent.
    assert 'buffer[2] = uint8_t(written * TRITON_SETTING_BYTES);' in body
    assert body.index('++written;') < body.index('buffer[2] =')


def test_unset_thresholds_never_overwrite_the_firmware():
    """-1 means "leave the controller's own value alone". Without this a fresh
    install would stamp our defaults over whatever the device (or Steam) had,
    and a bad guess at the raw units could leave the pads unresponsive."""
    body = SDL.split('void applyTritonSettings', 1)[1].split('\n\tint pollDevices', 1)[0]
    assert 'value < 0.f ? -1' in body
    for setting in ('range', 'releasePoint'):
        assert f'{setting} >= 0 && {setting} != device->_applied' in body, setting
    for name in ('GRIP_SENSOR_RANGE', 'GRIP_FLICKER_GUARD'):
        assert re.search(rf'SettingID::{name}, -1\.f\)', MAIN), name
        assert re.search(rf'{name}[\s\S]{{0,400}}?setFilter\(&filterFirmwareThreshold\)', MAIN), name
    assert 'if (next < 0.f)' in MAIN.split('float filterFirmwareThreshold', 1)[1][:300]


def test_flicker_guard_is_a_distance_below_the_trip_point():
    """The user sets a guard *size*, the way Steam Input presents it; the firmware
    wants the release point itself. Converting in the wrong direction, or letting
    the release point rise above the trip point, latches the sensor on."""
    body = SDL.split('void applyTritonSettings', 1)[1].split('\n\tint pollDevices', 1)[0]
    assert 'releasePoint = guard >= 0 ? std::max(0, range - guard) : range;' in body


def test_grip_haptics_are_edge_triggered_and_off_by_default():
    """Level-triggered, this would buzz for as long as a hand rested on the grip."""
    assert re.search(r'SettingID::GRIP_HAPTIC_INTENSITY, 0\.f\)', MAIN), \
        'grip haptics must default to off'
    body = SDL.split('void updateGripHaptics', 1)[1].split('\n\t}', 1)[0]
    assert 'left && !device->_leftGripWasOn' in body
    assert 'right && !device->_rightGripWasOn' in body
    # The previous state has to be tracked even while haptics are off, or enabling
    # them mid-session fires for a hand that was already there.
    assert body.index('device->_leftGripWasOn = left;') > body.index('sendGripHaptic')
    assert 'if (gamepad == nullptr || intensity <= 0.f)' in SDL


def test_grip_haptic_asks_for_the_controllers_own_click():
    """A hand-rolled square burst was barely perceptible; the canned effect is the
    tap Steam plays while calibrating the grips."""
    body = SDL.split('static void sendGripHaptic', 1)[1].split('\n\t}', 1)[0]
    assert 'buffer[0] = TRITON_ID_OUT_REPORT_HAPTIC_COMMAND;' in body
    assert 'TRITON_HAPTIC_COMMAND_BYTES = 4' in SDL
    assert 'buffer[2] = TRITON_HAPTIC_CLICK;' in body


def test_grip_haptic_side_is_the_bitmask_the_firmware_expects():
    """side is a bitmask (0x01 left, 0x02 right), not an index. Sending 0 for the
    left grip selected no actuator at all, which is half of why it was inaudible."""
    assert 'TRITON_HAPTIC_SIDE_LEFT = 0x01' in SDL
    assert 'TRITON_HAPTIC_SIDE_RIGHT = 0x02' in SDL
    body = SDL.split('static void sendGripHaptic', 1)[1].split('\n\t}', 1)[0]
    assert 'rightSide ? TRITON_HAPTIC_SIDE_RIGHT : TRITON_HAPTIC_SIDE_LEFT' in body
    assert '? 1 : 0' not in body, 'side must not be encoded as an index again'


def test_settings_are_pushed_from_the_poll_loop_only_on_change():
    """A feature report is a device round trip; sending it every poll would
    compete with input reports for bandwidth."""
    assert 'applyTritonSettings(iter->second);' in SDL
    body = SDL.split('void applyTritonSettings', 1)[1].split('\n\tint pollDevices', 1)[0]
    assert 'if (pending.empty())' in body
    assert 'if (sendTritonSettings(device->_sdlController, pending))' in body
    # Cached per device, so a reconnect (which rebuilds the struct) re-applies.
    for field in ('_appliedGripRange', '_appliedGripRelease'):
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
    for name in ('GRIP_SENSOR_RANGE,', 'GRIP_FLICKER_GUARD,',
                 'GRIP_HAPTIC_INTENSITY,'):
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
