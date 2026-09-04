#!/usr/bin/env python3
"""Guards the fix for the same "double cursor" jitter the AutoLoad fix
targeted -- reported to still be present, on the same roughly-one-second
beat, on touch, gyro, AND stick mouse-aim, and specifically after the
AutoLoad fix was confirmed *not* to be the cause (tested with AutoLoad
disabled entirely, GUI closed, bare backend, console command only).

AutoConnect is a separate background thread that also polls every 1000ms,
unconditionally, for the life of the session, starting on by default
(AUTOCONNECT defaults ON). Its only job is to call GetDeviceCount() and
compare it to the last observed value. GetDeviceCount() used to take
controller_lock -- the SAME mutex the 333Hz main poll loop (pollDevices())
holds for its *entire* per-tick body: touch processing, gyro processing,
stick processing, and the mouse flush all three funnel through -- and hold
it across RefreshDeviceList(), which contains a hardcoded, unconditional
SDL_Delay(20). So once a second, forever, regardless of AutoLoad's state,
the mouse-output thread was guaranteed to stall for at least 20ms waiting
on a lock held by a background thread doing nothing but a passive settle
delay. That matches the reported beat exactly, explains why it hit touch,
gyro, and stick-aim alike (none of which share filtering code with each
other, but all three share this one flush), and explains why disabling
AutoLoad specifically did not fix it -- AutoConnect is a different feature
the user never touched.

The fix: RefreshDeviceList() now locks controller_lock only around its two
SDL update phases, releasing it during the passive SDL_Delay(20) in between,
so the poll loop is only ever blocked for the brief SDL calls (as it already
tolerates from its own 333Hz self-contention), never for the 20ms wait.
GetDeviceCount() and ListAvailableDevices() no longer wrap the whole
RefreshDeviceList() call in their own outer lock_guard (which would now
also deadlock against RefreshDeviceList()'s internal, non-recursive lock).

Run: python3 tests/autoconnect_lock_regression.py (no dependencies)
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).parents[1]
SDLWRAPPER_CPP = (ROOT / 'JoyShockMapper/src/SDLWrapper.cpp').read_text(encoding='utf-8')


def check(cond, msg):
    if not cond:
        raise AssertionError(msg)


def extract_method_body(source, signature_pattern):
    m = re.search(signature_pattern, source)
    check(m is not None, f'signature not found: {signature_pattern}')
    brace_start = source.index('{', m.end())
    depth = 0
    i = brace_start
    while i < len(source):
        if source[i] == '{':
            depth += 1
        elif source[i] == '}':
            depth -= 1
            if depth == 0:
                return source[brace_start:i + 1]
        i += 1
    raise AssertionError(f'unbalanced braces extracting body for {signature_pattern}')


def refresh_device_list_body():
    return extract_method_body(SDLWRAPPER_CPP, r'void RefreshDeviceList\(\)\s*')


def get_device_count_body():
    return extract_method_body(SDLWRAPPER_CPP, r'int GetDeviceCount\(\) override\s*')


def list_available_devices_body():
    return extract_method_body(SDLWRAPPER_CPP, r'std::vector<ControllerInfo> ListAvailableDevices\(\) override\s*')


def test_refresh_device_list_delay_is_unlocked():
    """The 20ms settle delay must sit strictly between two independently
    locked SDL update phases, never inside a lock_guard scope that also
    covers the delay itself."""
    body = refresh_device_list_body()
    delay_index = body.index('SDL_Delay(20)')
    before = body[:delay_index]
    lock_positions = [mm.start() for mm in re.finditer(r'lock_guard\s+\w*\s*\(controller_lock\)', before)]
    check(len(lock_positions) >= 1,
          'RefreshDeviceList no longer locks controller_lock at all before the delay -- '
          'the SDL calls are racing the poll loop\'s own calls unsynchronized')
    for lock_pos in lock_positions:
        scope_open = before.rfind('{', 0, lock_pos)
        check(scope_open != -1, 'could not find the scope this lock_guard belongs to')
        depth = 0
        j = scope_open
        closed_at = None
        while j < len(body):
            if body[j] == '{':
                depth += 1
            elif body[j] == '}':
                depth -= 1
                if depth == 0:
                    closed_at = j
                    break
            j += 1
        check(closed_at is not None, 'lock_guard scope never closes')
        check(closed_at < delay_index,
              'a controller_lock lock_guard opened before SDL_Delay(20) is still in scope '
              'at the delay -- the poll loop is blocked for the full 20ms again, which is '
              'the exact bug this fix removes')


def test_get_device_count_does_not_hold_lock_across_refresh():
    body = get_device_count_body()
    refresh_index = body.index('RefreshDeviceList()')
    before = body[:refresh_index]
    check('lock_guard' not in before,
          'GetDeviceCount() takes controller_lock before calling RefreshDeviceList() -- '
          'RefreshDeviceList() now locks internally per phase, so wrapping the whole call '
          'in an outer lock_guard re-introduces the 20ms stall on every AutoConnect poll '
          '(and would deadlock on a non-recursive mutex)')


def test_list_available_devices_does_not_hold_lock_across_refresh():
    body = list_available_devices_body()
    refresh_index = body.index('RefreshDeviceList()')
    before = body[:refresh_index]
    check('lock_guard' not in before,
          'ListAvailableDevices() takes controller_lock before calling RefreshDeviceList() -- '
          'same 20ms-stall / deadlock risk as GetDeviceCount()')


def test_refresh_device_list_still_settles_before_reading():
    """Guard against a lazier fix that just deletes the delay outright --
    the two-phase pump/wait/pump exists to let the OS/driver finish
    enumerating a device that was just plugged in, and AutoConnect's
    hotplug detection depends on that actually happening."""
    body = refresh_device_list_body()
    check(body.count('SDL_UpdateGamepads()') == 2,
          'RefreshDeviceList should still pump gamepads before and after the delay')
    check('SDL_Delay(20)' in body, 'the 20ms settle delay was removed entirely, not just unlocked')


def test_autoconnect_still_polls_every_second_by_default_on():
    """The once-a-second beat and default-on state are what made this the
    likely cause in the first place; make sure nobody quietly changed
    either while fixing the lock contention."""
    autoconnect_cpp = (ROOT / 'JoyShockMapper/src/AutoConnect.cpp').read_text(encoding='utf-8')
    check('1000' in autoconnect_cpp, 'AutoConnect no longer polls on a 1000ms period')
    check('GetDeviceCount()' in autoconnect_cpp, 'AutoConnectPoll no longer calls GetDeviceCount()')


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
