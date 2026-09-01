#!/usr/bin/env python3
"""Build and run tests/touch_pipeline_harness.cpp against the real header.

Lifts LowPassFilter1E / OneEuroFilter / TouchMousePipeline out of JoyShock.h into
lifted.inc, so the harness measures the committed structs and cannot drift from
them. Needs only a C++17 compiler -- no SDL3, no Windows toolchain.
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).parents[1]
HEADER = ROOT / 'JoyShockMapper/include/JoyShock.h'
HARNESS = Path(__file__).parent / 'touch_pipeline_harness.cpp'

BEGIN = 'struct LowPassFilter1E'
END = '// An instance of this class represents'


def main() -> int:
    compiler = shutil.which('g++') or shutil.which('clang++')
    if compiler is None:
        print('SKIP: no C++ compiler on PATH')
        return 0

    src = HEADER.read_text(encoding='utf-8')
    try:
        lifted = src[src.index(BEGIN):src.index(END)]
    except ValueError:
        print(f'FAIL: could not locate {BEGIN!r}..{END!r} in {HEADER}')
        return 1

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        (tmp / 'lifted.inc').write_text(lifted, encoding='utf-8')
        binary = tmp / 'harness'
        build = subprocess.run(
            [compiler, '-O2', '-std=c++17', '-I', str(tmp), '-o', str(binary), str(HARNESS)],
            capture_output=True, text=True)
        if build.returncode != 0:
            print('FAIL: harness did not compile')
            print(build.stderr)
            return 1
        return subprocess.run([str(binary)]).returncode


if __name__ == '__main__':
    sys.exit(main())
