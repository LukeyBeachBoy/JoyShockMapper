#!/usr/bin/env python3
"""Guards the two-pad touch grid: separate buttons per pad, bounds-checked.

The Steam Controller 2026 has two trackpads, and the fork added
LEFT_GRID_SIZE/RIGHT_GRID_SIZE with left_grid_mappings/right_grid_mappings to
go with them. Both pads then drove the SAME T1..T25 button IDs, and those IDs
resolved against the SHARED grid's arrays:

  * a cell on the left pad fired whatever the right pad's matching cell was
    bound to, so the two pads were never independently bindable;
  * both per-pad grids registered their commands under the names T1..Tn, so
    they collided with the shared grid's and with each other, and the shrink
    path removed the shared grid's commands;
  * handleButtonChange indexed _gridButtons -- sized from GRID_SIZE -- with an
    index derived from the per-pad grid, so LEFT_GRID_SIZE = 3x3 against the
    default 2x1 read five entries past the end of the vector.

Every grid lookup now goes through one bounds-checked resolver, and each pad
has its own ID range (LT1.., RT1..). This checks that it stays that way.

Run: python3 tests/grid_button_routing_regression.py
Needs a C++ compiler and magic_enum.hpp for the layout harness; without them
that part reports SKIP and the source checks still run.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).parents[1]
HEADER = (ROOT / 'JoyShockMapper/include/JoyShockMapper.h').read_text(encoding='utf-8')
JOYSHOCK_H = (ROOT / 'JoyShockMapper/include/JoyShock.h').read_text(encoding='utf-8')
JOYSHOCK = (ROOT / 'JoyShockMapper/src/JoyShock.cpp').read_text(encoding='utf-8')
MAIN = (ROOT / 'JoyShockMapper/src/main.cpp').read_text(encoding='utf-8')
HARNESS = Path(__file__).parent / 'grid_button_layout_harness.cpp'


def check(cond, msg):
    if not cond:
        raise AssertionError(msg)


def find_magic_enum():
    env = os.environ.get('MAGIC_ENUM_INCLUDE')
    if env and (Path(env) / 'magic_enum.hpp').is_file():
        return Path(env)
    cache = os.environ.get('CPM_SOURCE_CACHE')
    roots = [Path(cache)] if cache else []
    roots += list(ROOT.glob('build*'))
    for base in roots:
        for candidate in base.glob('**/magic_enum.hpp'):
            return candidate.parent
    return None


def test_the_grid_id_ranges_are_laid_out_as_every_lookup_assumes():
    compiler = shutil.which('g++') or shutil.which('clang++')
    include = find_magic_enum()
    if not compiler or include is None:
        print('SKIP test_the_grid_id_ranges_are_laid_out_as_every_lookup_assumes '
              '(needs a C++ compiler and magic_enum.hpp; set MAGIC_ENUM_INCLUDE)')
        return
    with tempfile.TemporaryDirectory() as tmp:
        binary = Path(tmp) / 'grid_layout'
        build = subprocess.run(
            [compiler, '-std=c++20', '-I', str(ROOT / 'JoyShockMapper/include'),
             '-I', str(include), '-o', str(binary), str(HARNESS)],
            capture_output=True, text=True)
        check(build.returncode == 0, f'the layout harness did not compile:\n{build.stderr[-2000:]}')
        run = subprocess.run([str(binary)], capture_output=True, text=True)
        for line in run.stdout.splitlines():
            print(f'  {line}')
        check(run.returncode == 0, 'the grid ID layout harness reported a failure')


def test_each_pad_has_its_own_button_ids():
    for name in ('LT1', 'LT25', 'RT1', 'RT25'):
        check(re.search(rf'^\t{name},', HEADER, re.M) is not None,
              f'ButtonID::{name} is gone; the pads would share T1..T25 again')
    check('FIRST_LEFT_TOUCH_BUTTON = int(ButtonID::LT1)' in HEADER,
          'FIRST_LEFT_TOUCH_BUTTON no longer anchors to LT1')
    check('FIRST_RIGHT_TOUCH_BUTTON = int(ButtonID::RT1)' in HEADER,
          'FIRST_RIGHT_TOUCH_BUTTON no longer anchors to RT1')


def test_each_pad_drives_its_own_buttons():
    # The left pad's dispatch loop, and the right pad's, each keyed off their own base.
    left = re.search(r'for \(size_t i = 0; i < left_grid_mappings\.size\(\); \+\+i\)\s*\{(.*?)\}', MAIN, re.S)
    check(left is not None, "the left pad's grid dispatch loop is gone")
    check('FIRST_LEFT_TOUCH_BUTTON' in left.group(1),
          "the left pad still dispatches shared T* IDs, so both pads fire the same bindings")
    right = re.search(r'for \(size_t i = 0; i < right_grid_mappings\.size\(\); \+\+i\)\s*\{(.*?)\}', MAIN, re.S)
    check(right is not None, "the right pad's grid dispatch loop is gone")
    check('FIRST_RIGHT_TOUCH_BUTTON' in right.group(1),
          "the right pad still dispatches shared T* IDs, so both pads fire the same bindings")


def test_per_pad_commands_register_under_their_own_names():
    for side, base in (('Left', 'FIRST_LEFT_TOUCH_BUTTON'), ('Right', 'FIRST_RIGHT_TOUCH_BUTTON')):
        body = re.search(rf'void onNew{side}GridDimensions.*?\n\}}', MAIN, re.S)
        check(body is not None, f'onNew{side}GridDimensions is gone')
        body = body.group(0)
        check('FIRST_TOUCH_BUTTON' not in body,
              f'onNew{side}GridDimensions still names its commands off the shared base, '
              'which registers duplicate T1..Tn and removes the shared grid\'s commands')
        check(body.count(base) == 2,
              f'onNew{side}GridDimensions should use {base} for both the add and remove paths')
        check('updateGridSize' in body,
              f'onNew{side}GridDimensions does not resize the DigitalButtons, so a resized '
              'grid would index past the end of the button array')


def test_every_grid_lookup_is_bounds_checked():
    check('JoyShock::GridSlot JoyShock::findGridSlot' in JOYSHOCK,
          'the single grid resolver is gone')
    resolver = re.search(r'JoyShock::GridSlot JoyShock::findGridSlot.*?\n\}', JOYSHOCK, re.S).group(0)
    check('>= maps->size()' in resolver and '>= buttons->size()' in resolver,
          'findGridSlot no longer range-checks before indexing')

    # No raw subscripting of the grid arrays outside the resolver and the resize.
    stripped = JOYSHOCK.replace(resolver, '')
    for pattern in (r'_gridButtons\[', r'_leftGridButtons\[', r'_rightGridButtons\[',
                    r'grid_mappings\['):
        check(re.search(pattern, stripped) is None,
              f'{pattern[:-2]} is indexed directly again instead of through findGridSlot')

    check('GridSlot findGridSlot(ButtonID id);' in JOYSHOCK_H, 'findGridSlot is not declared')
    for member in ('_leftGridButtons', '_rightGridButtons'):
        check(member in JOYSHOCK_H, f'{member} is gone; the pads would share one button array')


def test_the_grid_arrays_are_reserved_to_their_real_maximum():
    # A JSMAssignment holds a reference to its JSMButton, so a reallocation
    # dangles it. The old reserve was T25 - T1 = 24, one short of 25.
    for vector in ('grid_mappings', 'left_grid_mappings', 'right_grid_mappings'):
        check(f'{vector}.reserve(MAX_GRID_BUTTONS)' in MAIN,
              f'{vector} is not reserved to MAX_GRID_BUTTONS')


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
