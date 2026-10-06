"""Compile and run the actual native menu routing header, without hardware."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
vcvars = Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat')
with tempfile.TemporaryDirectory(prefix='jsm-virtual-menu-') as temporary:
    folder = Path(temporary)
    binary = folder / 'virtual-menu.exe'
    batch = folder / 'build.cmd'
    batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /EHsc /std:c++17 /I"{ROOT / "JoyShockMapper/include"}" "{ROOT / "tests/virtual_menu_routing_harness.cpp"}" /Fe:"{binary}"\n', encoding='utf-8')
    result = subprocess.run(['cmd.exe', '/d', '/c', str(batch)], cwd=folder, capture_output=True, text=True)
    if result.returncode:
        print(result.stdout, result.stderr)
        raise SystemExit(result.returncode)
    raise SystemExit(subprocess.run([str(binary)]).returncode)
