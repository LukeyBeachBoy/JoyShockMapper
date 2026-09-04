#!/usr/bin/env python3
"""Guards the fix for the "double cursor" jitter reported on every output path
(touchpad, gyro, stick mouse aim), reproducible on a roughly one-second beat.

None of those three paths share any smoothing code with each other by the time
this was found -- touchpad filters position and differentiates it, gyro and
stick aim integrate a velocity directly, and the earlier round's touch-only fix
could not explain a symptom present in all three. What they DO all share is the
process they run in, and the OS's mouse-motion injection.

AutoLoad::AutoLoadPoll runs on its own thread, forever, every 1000ms exactly --
the beat the symptom was reported at. Every one of those polls used to call
GetActiveWindowName(), which on win32 fetches the window TITLE via
GetWindowText -- a call that, for a window owned by another process, sends
WM_GETTEXT and waits for that process's own message pump to service it. A game
that is busy rendering, or simply doesn't promptly pump messages during active
play (common for exclusive-fullscreen titles), can leave that call blocked for
a real, if brief, stretch -- and because it runs on a thread in the SAME
process as JSM's own SendInput calls, win32's internal synchronization for
window-manager operations can make that stall visible to the mouse-output
thread too, regardless of which of JSM's own code produced the motion that
tick. Bigger cursor speed means more motion queued up when the stall resolves,
which is exactly the "distance between the two cursors scales with speed"
report.

The title is used only for one log line, printed only when the foreground app
has actually changed -- a rare event -- yet the call ran unconditionally on
every one of those once-a-second polls, including the overwhelming majority
where nothing had changed at all. The fix: a separate GetActiveWindowModule(),
a kernel-level lookup with no dependency on the target process's own
responsiveness, used for the frequent per-poll check; GetActiveWindowName()
(and its GetWindowText call) now runs only inside the rare "something changed"
branch.

Run: python3 tests/autoload_hang_regression.py (no dependencies)
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).parents[1]
AUTOLOAD_CPP = (ROOT / 'JoyShockMapper/src/AutoLoad.cpp').read_text(encoding='utf-8')
INPUT_HELPERS_H = (ROOT / 'JoyShockMapper/include/InputHelpers.h').read_text(encoding='utf-8')
WIN32_CPP = (ROOT / 'JoyShockMapper/src/win32/InputHelpers.cpp').read_text(encoding='utf-8')
LINUX_CPP = (ROOT / 'JoyShockMapper/src/linux/InputHelpers.cpp').read_text(encoding='utf-8')


def check(cond, msg):
    if not cond:
        raise AssertionError(msg)


def poll_body():
    m = re.search(r'bool AutoLoad::AutoLoadPoll\(void\* param\)\s*\{(.*?)\n\}', AUTOLOAD_CPP, re.S)
    check(m is not None, 'AutoLoadPoll not found in AutoLoad.cpp')
    return m.group(1)


def changed_branch():
    body = poll_body()
    m = re.search(r'if \(!windowModule\.empty\(\).*?\{(.*)', body, re.S)
    check(m is not None, 'the "module changed" branch was not found')
    return m.group(1)


def test_the_poll_still_runs_every_second():
    """The 1000ms period is the whole reason this beat matched the report; make
    sure nobody quietly changed it while fixing the hang."""
    m = re.search(r'PollingThread\("AutoLoad thread",.*?,\s*(\d+),\s*start\)', AUTOLOAD_CPP, re.S)
    check(m is not None, 'AutoLoad PollingThread construction not found')
    check(int(m.group(1)) == 1000, f'AutoLoad no longer polls every 1000ms (found {m.group(1)})')


def test_the_frequent_check_never_calls_the_blocking_lookup():
    body = poll_body()
    check('GetActiveWindowModule()' in body,
          'AutoLoadPoll no longer calls the lightweight module-only lookup at all')
    # Only the rare "something changed" branch may call GetActiveWindowName;
    # outside it, on every poll where nothing changed, it must not appear.
    before_branch = body.split('if (!windowModule.empty()', 1)[0]
    # Match an actual call (with parens), not the identifier inside a comment.
    check(re.search(r'GetActiveWindowName\s*\(', before_branch) is None,
          'GetActiveWindowName() (which fetches the window title) is called before '
          'the "something changed" check again -- back to blocking on every poll')


def test_the_title_lookup_moved_inside_the_rare_branch():
    branch = changed_branch()
    check('GetActiveWindowName()' in branch,
          'the window title is no longer fetched at all; the AUTOLOAD log message needs it')


def test_the_lightweight_lookup_exists_on_both_platforms():
    check('string GetActiveWindowModule();' in INPUT_HELPERS_H,
          'GetActiveWindowModule is not declared for both platforms to implement')
    check('GetActiveWindowModule()' in WIN32_CPP, 'win32 has no GetActiveWindowModule implementation')
    check('GetActiveWindowModule()' in LINUX_CPP, 'linux has no GetActiveWindowModule implementation')


def test_win32_module_lookup_never_touches_the_window_title():
    """The whole point: the fast path must not go anywhere near GetWindowText."""
    m = re.search(r'string GetActiveWindowModule\(\)\s*\{(.*?)\n\}', WIN32_CPP, re.S)
    check(m is not None, 'GetActiveWindowModule not found in win32/InputHelpers.cpp')
    check('GetWindowText' not in m.group(1),
          'win32 GetActiveWindowModule calls GetWindowText -- the exact blocking '
          'call this fix exists to keep off the hot, once-a-second path')


def test_win32_module_lookup_is_a_kernel_level_call():
    """QueryFullProcessImageName does not depend on the target process's own
    message pump, unlike GetWindowText -- that's what makes it safe to call
    unconditionally, every second, forever."""
    m = re.search(r'static string ModuleNameFromWindow\(HWND activeWindow\)\s*\{(.*?)\n\}', WIN32_CPP, re.S)
    check(m is not None, 'the shared module-lookup helper is gone from win32/InputHelpers.cpp')
    check('QueryFullProcessImageName' in m.group(1), 'module lookup no longer uses QueryFullProcessImageName')


def main():
    tests = [value for name, value in sorted(globals().items()) if name.startswith('test_')]
    failures = 0
    for test in tests:
        try:
            test()
        except AssertionError as exc:
            print(f'FAIL {test.__name__}: {exc}')
            failures += 1
        else:
            print(f'PASS {test.__name__}')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
