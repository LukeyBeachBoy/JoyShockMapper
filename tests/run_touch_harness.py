#!/usr/bin/env python3
"""Build and run the touch pipeline harnesses against the real header/source.

Lifts LowPassFilter1E / OneEuroFilter / TouchMousePipeline out of JoyShock.h into
lifted.inc, so the harnesses measure the committed structs and cannot drift from
them. Needs only a C++17 compiler -- no SDL3, no Windows toolchain.

Also cross-checks touch_short_gesture_harness.cpp's hardcoded "shipped defaults"
against main.cpp's actual JSMSetting registration lines for TOUCHPAD_MIN_CUTOFF /
TOUCHPAD_SPEED_COEFF / TOUCHPAD_TRACKBALL_DECAY, so the harness can't silently
drift out of sync with what's really shipped.
"""
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).parents[1]
HEADER = ROOT / 'JoyShockMapper/include/JoyShock.h'
MAIN = ROOT / 'JoyShockMapper/src/main.cpp'
HARNESSES = [
    Path(__file__).parent / 'touch_pipeline_harness.cpp',
    Path(__file__).parent / 'touch_short_gesture_harness.cpp',
]

BEGIN = 'struct LowPassFilter1E'
END = '// An instance of this class represents'


def check_defaults_in_sync() -> bool:
    main_src = MAIN.read_text(encoding='utf-8')
    harness_src = HARNESSES[1].read_text(encoding='utf-8')
    checks = [
        ('SettingID::TOUCHPAD_MIN_CUTOFF, (\\d+\\.?\\d*)f\\)', 'SHIPPED_MIN_CUTOFF = (\\d+\\.?\\d*)f;'),
        ('SettingID::TOUCHPAD_SPEED_COEFF, (\\d+\\.?\\d*)f\\)', 'SHIPPED_SPEED_COEFF = (\\d+\\.?\\d*)f;'),
        ('SettingID::TOUCHPAD_TRACKBALL_DECAY, (\\d+\\.?\\d*)f\\)', 'SHIPPED_TRACKBALL_DECAY = (\\d+\\.?\\d*)f;'),
    ]
    ok = True
    for main_pat, harness_pat in checks:
        main_m = re.search(main_pat, main_src)
        harness_m = re.search(harness_pat, harness_src)
        if not main_m or not harness_m:
            print(f'FAIL: could not find pattern {main_pat!r} or {harness_pat!r}')
            ok = False
            continue
        if float(main_m.group(1)) != float(harness_m.group(1)):
            print(f'FAIL: main.cpp registers {main_m.group(1)} but harness asserts {harness_m.group(1)} for {main_pat}')
            ok = False
    return ok


def main() -> int:
    compiler = shutil.which('g++') or shutil.which('clang++')
    if compiler is None:
        print('SKIP: no C++ compiler on PATH')
        return 0

    if not check_defaults_in_sync():
        return 1
    print('Harness defaults match main.cpp registrations.\n')

    src = HEADER.read_text(encoding='utf-8')
    try:
        lifted = src[src.index(BEGIN):src.index(END)]
    except ValueError:
        print(f'FAIL: could not locate {BEGIN!r}..{END!r} in {HEADER}')
        return 1

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        (tmp / 'lifted.inc').write_text(lifted, encoding='utf-8')
        for harness in HARNESSES:
            binary = tmp / harness.stem
            build = subprocess.run(
                [compiler, '-O2', '-std=c++17', '-I', str(tmp), '-o', str(binary), str(harness)],
                capture_output=True, text=True)
            if build.returncode != 0:
                print(f'FAIL: {harness.name} did not compile')
                print(build.stderr)
                return 1
            print(f'--- {harness.name} ---')
            result = subprocess.run([str(binary)])
            print()
            if result.returncode != 0:
                return result.returncode
    return 0


if __name__ == '__main__':
    sys.exit(main())
