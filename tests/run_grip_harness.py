"""Compile production grip calibration and haptic routing; missing tools fail."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
include = root / 'JoyShockMapper/include'
compiler = shutil.which('g++') or shutil.which('clang++')
def run_harness(source, directory):
    output = Path(directory) / ('grip.exe' if os.name == 'nt' else 'grip')
    if compiler:
        subprocess.run([compiler, '-std=c++17', '-I', str(include), str(source), '-o', str(output)], check=True)
    else:
        vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        if not vswhere.exists():
            raise SystemExit('FAIL: no C++ compiler; grip tests did not run')
        installation = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        vcvars = Path(installation) / 'VC/Auxiliary/Build/vcvars64.bat'
        if not vcvars.exists():
            raise SystemExit('FAIL: MSVC environment not found')
        batch = Path(directory) / 'build.bat'
        batch.write_text(f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /EHsc /std:c++17 /I"{include}" "{source}" /Fe:"{output}"\n')
        subprocess.run(['cmd.exe', '/d', '/c', str(batch)], cwd=directory, check=True)
    subprocess.run([str(output)], check=True)


with tempfile.TemporaryDirectory(prefix='jsm-grip-test-') as directory:
    run_harness(root / 'tests/grip_settings_harness.cpp', directory)
    sdl = (root / 'JoyShockMapper/src/SDLWrapper.cpp').read_text()
    start = sdl.index('void updateGripHaptics(')
    end = sdl.index('\n\t}', start) + len('\n\t}')
    harness = (root / 'tests/grip_haptics_harness.cpp').read_text()
    source = Path(directory) / 'grip_haptics.cpp'
    source.write_text(harness.replace('// UPDATE_GRIP_HAPTICS', sdl[start:end]))
    run_harness(source, directory)
