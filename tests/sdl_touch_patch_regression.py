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
itself: it must OR (never replace) the pressure test, it must fail loudly if the
driver changes under it, and it must stay wired into the build.

Run: python3 tests/sdl_touch_patch_regression.py     (no dependencies)
"""
import re
from pathlib import Path

ROOT = Path(__file__).parents[1]
PATCH = (ROOT / 'cmake/PatchSdlTritonTouch.cmake').read_text()
CMAKELISTS = (ROOT / 'JoyShockMapper/CMakeLists.txt').read_text()


def test_patch_is_applied_to_the_fetched_sdl_source():
    assert 'include (${CMAKE_SOURCE_DIR}/cmake/PatchSdlTritonTouch.cmake)' in CMAKELISTS
    assert 'patch_sdl_triton_touch (${SDL3_SOURCE_DIR})' in CMAKELISTS
    # Must run against the source CPM just fetched, and therefore after it.
    assert CMAKELISTS.index('GITHUB_REPOSITORY libsdl-org/SDL') < CMAKELISTS.index('patch_sdl_triton_touch')


def test_capacitive_contact_is_added_never_substituted():
    """A pure replacement would leave the pads dead if a firmware revision ever
    stopped setting the touch bits. ORing can only add contacts."""
    assert '_replacement' in PATCH
    replacement = re.search(r'set\(_replacement\s*\n?\s*"([^"]+)"\)', PATCH).group(1)
    assert 'TRITON_${_SIDE}_TOUCHPAD_TOUCH' in replacement
    assert '||' in replacement
    assert 'pTritonReport->sPressure${_side} > 0' in replacement, \
        'the original pressure test must survive as the fallback'


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
