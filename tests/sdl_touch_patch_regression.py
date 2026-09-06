"""Guards the SDL patch that makes the pads report a touch, not a press.

SDL's Triton driver builds the touchpad finger-down flag from the pressure
channel:

    SDL_SendJoystickTouchpad(..., pTritonReport->sPressureLeft > 0, ...)

while TRITON_LEFT/RIGHT_TOUCHPAD_TOUCH -- the pads' own capacitive contact bits,
decoded from the very same report a few lines earlier -- are never used for
anything. So resting a finger on the pad reports nothing until you press hard
enough to register force, and right at that boundary the pressure reading sits
in the noise, so contact flickers on and off. No amount of host-side work fixes
that: the contact never reaches the host in the first place.

cmake/PatchSdlTritonTouch.cmake rewrites those two expressions at configure
time. These tests pin the parts that make it safe rather than the rewrite
itself: contact must follow the touch bit, pressure noise must not hold a
released finger down, and both pristine and previously patched builds upgrade.


Run: python3 tests/sdl_touch_patch_regression.py     (no dependencies)
"""
import re
from pathlib import Path

ROOT = Path(__file__).parents[1]
PATCH = (ROOT / 'cmake/PatchSdlTritonTouch.cmake').read_text()
CMAKELISTS = (ROOT / 'JoyShockMapper/CMakeLists.txt').read_text()


def test_patch_is_applied_to_the_fetched_sdl_source():
    root = (ROOT / 'CMakeLists.txt').read_text()
    assert 'include (cmake/PatchSdlTritonTouch.cmake)' in root, \
        'the module must be included from the root, like every other cmake/ module'
    assert 'patch_sdl_triton_touch (${SDL3_SOURCE_DIR})' in CMAKELISTS
    # Must run against the source CPM just fetched, and therefore after it.
    assert CMAKELISTS.index('GITHUB_REPOSITORY libsdl-org/SDL') < CMAKELISTS.index('patch_sdl_triton_touch')


def test_the_module_is_actually_tracked_by_git():
    """.gitignore carries a blanket *.cmake rule for generated build files, which
    silently swallowed this module: git add -A skipped it, the commit looked
    clean, and CI failed at configure with "include could not find requested
    file". The other modules in cmake/ only survive because they predate it."""
    ignore = (ROOT / '.gitignore').read_text()
    assert '!cmake/*.cmake' in ignore, \
        'cmake/ modules must be exempted from the blanket *.cmake ignore rule'


def test_haptic_output_reports_are_let_through_by_report_id():
    """The grip actuators are only reachable through an output report, which
    SendJoystickEffect refused. Gating on the report id rather than one payload
    length keeps every haptic effect reachable without widening it to anything
    else the caller might pass."""
    assert 'ID_OUT_REPORT_HAPTIC_RUMBLE' in PATCH and 'ID_OUT_REPORT_HAPTIC_SCRIPT' in PATCH, \
        "the haptic passthrough must be bounded by the driver's own report-id block"
    assert 'SDL_hid_write' in PATCH, 'haptic reports must be written as output reports'
    assert 'size < HID_FEATURE_REPORT_BYTES' in PATCH, \
        'the feature-report path must still be reached for settings'


def test_capacitive_contact_replaces_pressure_fallback():
    replacement = re.search(r'set\(_replacement\s*\n?\s*"([^"]+)"\)', PATCH).group(1)
    assert 'TRITON_${_SIDE}_TOUCHPAD_TOUCH' in replacement
    assert '||' not in replacement and 'Pressure' not in replacement
    assert '_legacy' in PATCH, 'already-populated SDL builds must upgrade too'
    assert ': 0.0f' in PATCH, 'release must preserve the last valid coordinates'


def test_real_cmake_patch_handles_clean_upgrade_and_repeat_runs():
    import shutil, subprocess, tempfile
    cmake = shutil.which('cmake')
    assert cmake, 'CMake is required to exercise the SDL source patch'
    original = """void touch() {
    SDL_SendJoystickTouchpad(timestamp, joystick, 0, 0,
                             pTritonReport->sPressureLeft > 0,
                             pTritonReport->sLeftPadX / 65536.0f + 0.5f,
                             -(float)pTritonReport->sLeftPadY / 65536.0f + 0.5f,
                             pTritonReport->sPressureLeft / 32768.0f);
    SDL_SendJoystickTouchpad(timestamp, joystick, 1, 0,
                             pTritonReport->sPressureRight > 0,
                             pTritonReport->sRightPadX / 65536.0f + 0.5f,
                             -(float)pTritonReport->sRightPadY / 65536.0f + 0.5f,
                             pTritonReport->sPressureRight / 32768.0f);
    if (size == HID_FEATURE_REPORT_BYTES) {
    }
}
"""
    with tempfile.TemporaryDirectory(prefix='jsm-sdl-touch-') as directory:
        root = Path(directory)
        driver = root / 'src/joystick/hidapi/SDL_hidapi_steam_triton.c'
        driver.parent.mkdir(parents=True)
        script = root / 'patch.cmake'
        script.write_text(f'include("{(ROOT / "cmake/PatchSdlTritonTouch.cmake").as_posix()}")\npatch_sdl_triton_touch("{root.as_posix()}")\n')
        outputs = []
        for legacy in (False, True):
            source = original
            if legacy:
                for side in ('Left', 'Right'):
                    source = source.replace(f'pTritonReport->sPressure{side} > 0,', f'((pTritonReport->buttons & TRITON_{side.upper()}_TOUCHPAD_TOUCH) != 0 || pTritonReport->sPressure{side} > 0),')
            driver.write_text(source)
            subprocess.run([cmake, '-P', str(script)], check=True, capture_output=True)
            once = driver.read_text()
            subprocess.run([cmake, '-P', str(script)], check=True, capture_output=True)
            assert driver.read_text() == once
            assert 'sPressureLeft > 0' not in once and 'sPressureRight > 0' not in once
            assert once.count(': 0.0f') == 4
            outputs.append(once)
        assert outputs[0] == outputs[1], 'clean and previously patched SDL must produce the same driver'


def test_a_driver_change_fails_the_build_instead_of_being_ignored():
    """The failure mode to avoid is an SDL bump silently dropping the fix and
    the pads quietly going back to needing a press."""
    assert PATCH.count('FATAL_ERROR') >= 2
    assert 'does not exist' in PATCH
    assert 'could not find' in PATCH


def test_patch_is_idempotent():
    """Configure runs more than once against the same populated source tree."""
    assert 'string(FIND "${_source}" "${_replacement}" _already)' in PATCH
    assert 'if(_already GREATER_EQUAL 0)' in PATCH
    assert 'continue()' in PATCH
    # And it only writes when something actually changed.
    assert 'if(NOT _patched)' in PATCH


def test_both_pads_are_patched():
    assert 'foreach(_side Left Right)' in PATCH


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
