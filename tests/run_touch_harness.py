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
import argparse
import os
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
    Path(__file__).parent / 'touch_retouch_harness.cpp',
    Path(__file__).parent / 'touch_stall_catchup_harness.cpp',
    Path(__file__).parent / 'touch_release_harness.cpp',
    Path(__file__).parent / 'touch_cadence_harness.cpp',
    Path(__file__).parent / 'touch_resampler_harness.cpp',
]

BEGIN = 'struct LowPassFilter1E'
END = '// An instance of this class represents'


def check_defaults_in_sync() -> bool:
    # Every harness that declares one of these SHIPPED_* constants is checked
    # against main.cpp, not just one designated file -- touch_pipeline_harness.cpp
    # used to hardcode 0.8f/0.015f inline, labelled "shipped defaults" in a
    # comment, and stayed that way silently after TOUCHPAD_MIN_CUTOFF/
    # TOUCHPAD_SPEED_COEFF were retuned to 6.0f/0.6f, because this check only
    # ever looked at touch_short_gesture_harness.cpp.
    main_src = MAIN.read_text(encoding='utf-8')
    checks = [
        ('SettingID::TOUCHPAD_MIN_CUTOFF, (\\d+\\.?\\d*)f\\)', 'SHIPPED_MIN_CUTOFF\\s*=\\s*(\\d+\\.?\\d*)f;'),
        ('SettingID::TOUCHPAD_SPEED_COEFF, (\\d+\\.?\\d*)f\\)', 'SHIPPED_SPEED_COEFF\\s*=\\s*(\\d+\\.?\\d*)f;'),
        ('SettingID::TOUCHPAD_TRACKBALL_DECAY, (\\d+\\.?\\d*)f\\)', 'SHIPPED_TRACKBALL_DECAY\\s*=\\s*(\\d+\\.?\\d*)f;'),
    ]
    ok = True
    checked_any = False
    for harness in HARNESSES:
        harness_src = harness.read_text(encoding='utf-8')
        for main_pat, harness_pat in checks:
            harness_m = re.search(harness_pat, harness_src)
            if not harness_m:
                continue  # this harness doesn't declare this particular constant
            checked_any = True
            main_m = re.search(main_pat, main_src)
            if not main_m:
                print(f'FAIL: could not find pattern {main_pat!r} in main.cpp')
                ok = False
                continue
            if float(main_m.group(1)) != float(harness_m.group(1)):
                print(f'FAIL: main.cpp registers {main_m.group(1)} but {harness.name} asserts '
                      f'{harness_m.group(1)} for {main_pat}')
                ok = False
    if not checked_any:
        print('FAIL: no harness declared any SHIPPED_* default to check')
        return False
    return ok


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--only', help='Run only the named harness stem')
    parser.add_argument('--source-ref', help='Replay/test an earlier backend git revision')
    parser.add_argument('--replay', help='Input CSV recorded by capture_touch.py')
    parser.add_argument('--output', help='Replayed mouse-output CSV')
    args = parser.parse_args()
    if args.replay and (args.only != 'touch_cadence_harness' or not args.output):
        parser.error('--replay requires --only touch_cadence_harness and --output')
    selected = [h for h in HARNESSES if args.only is None or h.stem == args.only]
    if not selected:
        parser.error('unknown harness')
    compiler = shutil.which('g++') or shutil.which('clang++')
    vcvars = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
    if compiler is None and not vcvars.exists():
        print('FAIL: no C++ compiler available; numeric touch tests were not run')
        return 1

    if not check_defaults_in_sync():
        return 1
    print('Harness defaults match main.cpp registrations.\n')

    def source(path):
        if args.source_ref:
            return subprocess.check_output(['git', '-C', str(ROOT), 'show', f'{args.source_ref}:{path.relative_to(ROOT).as_posix()}'], text=True)
        return path.read_text(encoding='utf-8')
    src = source(HEADER)
    try:
        lifted = '#include "TouchMouseResampler.h"\n' + src[src.index(BEGIN):src.index(END)]
    except ValueError:
        print(f'FAIL: could not locate {BEGIN!r}..{END!r} in {HEADER}')
        return 1

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        shutil.copyfile(ROOT / 'JoyShockMapper/include/TouchMouseResampler.h', tmp / 'TouchMouseResampler.h')
        (tmp / 'lifted.inc').write_text(lifted, encoding='utf-8')
        filter_src = source(ROOT / 'JoyShockMapper/src/JoyShock.cpp')
        start = filter_src.index('float OneEuroFilter::filter(float x, float dt, float minCutoff, float beta)')
        end = filter_src.index('\n}', start) + 2
        (tmp / 'lifted_one_euro.inc').write_text(filter_src[start:end], encoding='utf-8')
        main = source(MAIN)
        process = main[main.index('static void processTouchMouse('):main.index('void touchCallback(')]
        (tmp / 'lifted_process.inc').write_text(process, encoding='utf-8')
        (tmp / 'MouseMotionAccumulator.h').write_text((ROOT / 'JoyShockMapper/include/MouseMotionAccumulator.h').read_text(encoding='utf-8'), encoding='utf-8')
        for harness in selected:
            binary = tmp / (harness.stem + ('.exe' if os.name == 'nt' else ''))
            if compiler:
                build = subprocess.run(
                    [compiler, '-O2', '-std=c++17', '-I', str(tmp), '-o', str(binary), str(harness)],
                    capture_output=True, text=True)
            else:
                batch = tmp / 'build.bat'
                batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /EHsc /std:c++17 /I"{tmp}" "{harness.resolve()}" /Fe:"{binary}"\n', encoding='utf-8')
                build = subprocess.run(['cmd.exe', '/d', '/c', str(batch)], cwd=tmp, capture_output=True, text=True)
            if build.returncode != 0:
                print(f'FAIL: {harness.name} did not compile')
                print(build.stdout, build.stderr)
                return 1
            print(f'--- {harness.name} ---')
            command = [str(binary)]
            if args.replay:
                command += [args.replay, args.output]
            result = subprocess.run(command)
            print()
            if result.returncode != 0:
                return result.returncode
    return 0


if __name__ == '__main__':
    sys.exit(main())
