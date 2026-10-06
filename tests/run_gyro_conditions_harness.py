"""Exercise native gyro condition parsing, printing and matching without hardware."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
def balanced(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

header = (ROOT / 'JoyShockMapper/include/JoyShockMapper.h').read_text()
operators = (ROOT / 'JoyShockMapper/src/operators.cpp').read_text()
native = '\n'.join(balanced(header, name) + ';' for name in ['enum class ButtonID', 'enum class GyroIgnoreMode', 'struct GyroSettings'])
native += '''
bool isInvertedChord(ButtonID) { return false; }
ButtonID invertedChordBase(ButtonID id) { return id; }
ButtonID invertedChordOf(ButtonID) { return ButtonID::INVALID; }
'''
native += '\ntemplate<class E, class = std::enable_if_t<std::is_enum<E>{}>>\n' + balanced(header, 'ostream &operator<<(ostream &out, E rhv)')
native += '\n'.join(balanced(operators, name) for name in [
    'istream &operator>>(istream &in, ButtonID &rhv)', 'ostream &operator<<(ostream &out, const ButtonID &rhv)',
    'istream &operator>>(istream &in, GyroSettings &gyro_settings)', 'ostream &operator<<(ostream &out, const GyroSettings &gyro_settings)',
    'bool operator==(const GyroSettings &lhs, const GyroSettings &rhs)',
])
cpp_text = r'''
#define MAGIC_ENUM_RANGE_MIN -1
#define MAGIC_ENUM_RANGE_MAX 512
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <optional>
#include "magic_enum.hpp"
using namespace std;
NATIVE
GyroSettings parse(string source, bool valid = true) {
 GyroSettings value; stringstream stream(source); stream >> value;
 assert(!stream.fail() == valid); return value;
}
int main() {
 auto any = parse("ANY MISC5 MISC6");
 auto pressed = [](ButtonID id) { return id == ButtonID::MISC5; };
 assert(any.active(pressed));
 auto all = parse("ALL MISC5 MISC6"); assert(!all.active(pressed));
 auto released = parse("ALL MISC5 !MISC6"); assert(released.active(pressed));
 assert(!released.active([](ButtonID){ return false; }));
 auto nonePressed = [](ButtonID){return false;};
 assert(parse("ANY !MISC5 !MISC6").active(nonePressed));
 assert(!parse("NONE").active(nonePressed));
 assert(parse("MISC5").active(pressed));
 assert(parse("LEFT_STICK").ignore_mode == GyroIgnoreMode::LEFT_STICK);
 auto cells = parse("ANY RT25 LT25 - +");
 assert(cells.active([](ButtonID id){return id == ButtonID::RT25;}));
 stringstream printed; printed << released; assert(printed.str() == "ALL MISC5 !MISC6");
 assert(parse(printed.str()) == released); assert(!(released == all));
 auto comments = parse("ANY MISC5 MISC6 # preserved note"); assert(comments == any);
 parse("ANY", false); parse("ALL NONE", false); parse("ANY INVALID", false); parse("ANY SIZE", false); parse("ANY MISC5 unknown", false);
 string limit = "ALL"; for(int i=0;i<16;++i) limit += " MISC5";
 parse(limit); parse(limit + " MISC6", false);
 GyroSettings changed=all; stringstream single("MISC5"); single >> changed;
 assert(changed.conditions.empty() && !changed.require_all && changed.active(pressed));
 cout << "PASS: native Any/All, released conditions, legacy modes, all pad cells, +/- inputs, equality, round-trip, comments and invalid syntax\n";
}
'''.replace('NATIVE', native)
magic = ROOT.parent / 'build-jsm-sdl/_deps/magic_enum-src/include'
compiler = shutil.which('g++') or shutil.which('clang++')
vcvars = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat'
with tempfile.TemporaryDirectory() as folder:
    folder = Path(folder)
    cpp, binary = folder / 'conditions.cpp', folder / ('conditions.exe' if os.name == 'nt' else 'conditions')
    cpp.write_text(cpp_text)
    if compiler:
        cmd = [compiler, '-std=c++17', '-O2', '-I', str(magic), str(cpp), '-o', str(binary)]
    else:
        batch = folder / 'build.bat'
        batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /EHsc /std:c++17 /I"{magic}" "{cpp}" /Fe:"{binary}"\n')
        cmd = ['cmd.exe', '/d', '/c', str(batch)]
    result = subprocess.run(cmd, cwd=folder, capture_output=True, text=True)
    if result.returncode:
        print(result.stdout, result.stderr)
        raise SystemExit(result.returncode)
    raise SystemExit(subprocess.run([str(binary)]).returncode)
