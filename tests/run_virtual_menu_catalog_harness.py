"""Exercise the real catalog and Mapping parser; stub only OS output/key input."""
import ast
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
script = ast.parse((ROOT / 'tests/run_cycle_harness.py').read_text(encoding='utf-8'))
stub = next(ast.literal_eval(node.value) for node in script.body if isinstance(node, ast.Assign) and any(isinstance(target, ast.Name) and target.id == 'stub' for target in node.targets))
header = (ROOT / 'JoyShockMapper/include/Mapping.h').read_text(encoding='utf-8')
header = header[header.index('class Mapping;'):]
mapping = (ROOT / 'JoyShockMapper/src/Mapping.cpp').read_text(encoding='utf-8')
mapping = re.sub(r'^#include "(?:Mapping|InputHelpers)\.h"\s*$', '', mapping, flags=re.M)
catalog = (ROOT / 'JoyShockMapper/include/VirtualMenuCatalog.h').read_text(encoding='utf-8').replace('#include "Mapping.h"', '')
source = (ROOT / 'JoyShockMapper/src/VirtualMenuCatalog.cpp').read_text(encoding='utf-8').replace('#include "VirtualMenuCatalog.h"', '')
native_header = (ROOT / 'JoyShockMapper/include/JoyShockMapper.h').read_text(encoding='utf-8')
button_enum = re.search(r'enum class ButtonID\s*\{[\s\S]*?\};', native_header)[0]
operators = (ROOT / 'JoyShockMapper/src/operators.cpp').read_text(encoding='utf-8')
# The parser has nested branches: extract its balanced function, not a fixture list.
start = operators.index('istream &operator>>(istream &in, ButtonID &rhv)')
opening = operators.index('{', start)
depth, end = 1, opening + 1
while depth:
    depth += (operators[end] == '{') - (operators[end] == '}')
    end += 1
enum_helpers = "\n".join(line for line in native_header.splitlines() if line.startswith(('constexpr int INVERTED_CHORD_OFFSET', 'constexpr bool isInvertedChord', 'constexpr ButtonID invertedChordOf', 'constexpr ButtonID invertedChordBase')))
button = '#define MAGIC_ENUM_RANGE_MIN (-1)\n#define MAGIC_ENUM_RANGE_MAX 512\n#include "magic_enum.hpp"\n' + button_enum + "\n" + enum_helpers + "\n" + operators[start:end] + "\n"
tests = r'''
string packed(string text) { string value="= HEX:"; const char *hex="0123456789abcdef"; for(unsigned char c:text) {value+=hex[c>>4];value+=hex[c&15];} return value; }
int main() {
  Mapping::_isCommandValid=[](string_view){ return false; };
  using namespace VirtualMenus;
  clear();
  assert(define("wheel RADIAL 8 8 .2"));
  assert(action("wheel 1 1"));
  assert(action("wheel 0 X_A")); assert(snapshot()->definitions.at("wheel").centerAction->hasViGEmBtn());
  assert(define("wheel RADIAL 8 8 .25")); assert(snapshot()->definitions.at("wheel").centerAction.has_value());
  assert(!action("wheel -1 1"));
  assert(action("wheel 2 X_A"));
  assert(action("wheel 3 \"CYCLE 1 | 2 | 3\""));
  assert(attach("wheel RIGHT HOLD L ACTIVATION_RELEASE NONE NONE"));
  auto old=snapshot(); assert(old->attachments.size()==1 && old->definitions.size()==1);
  assert(attach("wheel RIGHT HOLD L CLICK NONE R")); assert(snapshot()->attachments.size()==1);
  assert(old->attachments[0].selection==VirtualMenuSelection::ACTIVATION_RELEASE); // immutable readers
  assert(!define("wheel BAD 8 8 .2")); assert(!define("wheel RADIAL 1 1 .2"));
  assert(!define("wheel RADIAL 26 26 .2")); assert(!define("wheel RADIAL 8 8 nan"));
  assert(!action("missing 1 1")); assert(!action("wheel 26 1")); assert(!action("wheel 1 BAD"));
  assert(attach("wheel RSTICK HOLD L ACTIVATION_RELEASE NONE NONE JOYSTICK_CURSOR"));
  assert(snapshot()->attachments.back().joystickCursor);
  assert(attach("wheel LSTICK COMMAND NONE CLICK NONE NONE JOYSTICK"));
  assert(!snapshot()->attachments.back().joystickCursor);
  assert(!attach("wheel RIGHT HOLD L CLICK NONE NONE JOYSTICK_CURSOR"));
  assert(!attach("wheel RSTICK HOLD L CLICK NONE NONE FUTURE_NAV"));
  assert(!attach("wheel RSTICK HOLD L CLICK NONE NONE JOYSTICK_CURSOR EXTRA"));
  assert(!attach("wheel DPAD HOLD L CLICK NONE NONE"));
  assert(!attach("wheel RIGHT HOLD NONE CLICK NONE NONE"));
  auto beforeInputs = snapshot();
  for (const char *invalid : {"SIZE", "INVALID", "FUTURE_INPUT", "!LT1"}) {
    assert(!attach(string("wheel RIGHT HOLD ") + invalid + " CLICK NONE NONE"));
    assert(!attach(string("wheel RIGHT HOLD L CLICK ") + invalid + " NONE"));
    assert(!attach(string("wheel RIGHT HOLD L CLICK NONE ") + invalid));
    assert(snapshot() == beforeInputs);
  }
  assert(attach("wheel LEFT HOLD !MISC5 CLICK LT25 RM25"));
  assert(snapshot()->attachments.back().input == invertedChordOf(ButtonID::MISC5));
  assert(!attach("wheel RIGHT ALWAYS NONE ACTIVATION_RELEASE NONE NONE"));
  assert(!replace("= HEX:0g"));
  auto valid=snapshot(); assert(!replace(packed("DEFINE bad RADIAL 8 8 .2\nACTION bad 1 BAD"))); assert(snapshot()==valid);
  assert(replace(packed("DEFINE bar HOTBAR 3 3 .1\nACTION bar 1 1\nACTION bar 2 X_A\nACTION bar 3 \"CYCLE 1|2\"\nPRESENTATION bar {\"name\":\"é # inventory\"}\nSOURCE bar DPAD HOLD L CLICK NONE R")));
  auto replacement=snapshot(); assert(replacement->definitions.size()==1 && replacement->definitions.count("bar"));
  assert(!attach("bar RSTICK HOLD L CLICK NONE NONE JOYSTICK_CURSOR"));
  assert(!attach("bar DPAD HOLD L TOUCH_RELEASE NONE NONE"));
  assert(replacement->definitions.at("bar").actions[1].hasViGEmBtn());
  assert(replacement->attachments[0].source==VirtualMenuSource::DPAD);
  assert(!replace(packed("DEFINE bar HOTBAR 3 3 .1\nFUTURE bar"))); assert(snapshot()==replacement);
  assert(attach("bar ABXY TOGGLE S CLICK NONE R"));
  assert(snapshot()->attachments.back().source==VirtualMenuSource::ABXY);
  assert(!attach("bar ABXY HOLD L TOUCH_RELEASE NONE NONE"));
  assert(!attach("bar COUNT HOLD L CLICK NONE NONE"));
  assert(define("wheel RADIAL 8 8 .2"));assert(!attach("wheel ABXY HOLD L CLICK NONE NONE"));
  assert(replace("= HEX:")); assert(snapshot()->definitions.empty());
  cout << "PASS: actual native menu catalog/Mapping parser, immutable snapshots, replace transaction, layer catalog, Unicode/hash presentation, Cycle/gamepad actions, invalid and bounded definitions\n";
}
'''
with tempfile.TemporaryDirectory(prefix='jsm-menu-catalog-') as temporary:
    folder = Path(temporary)
    cpp, binary = folder / 'catalog.cpp', folder / 'catalog.exe'
    cpp.write_text(stub + header + mapping + button + catalog + source + tests, encoding='utf-8')
    batch = folder / 'build.cmd'
    vcvars = Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat')
    batch.write_text(f'@echo off\ncall "{vcvars}" >nul\ncl /nologo /utf-8 /EHsc /std:c++20 /I"{ROOT / "JoyShockMapper/include"}" /I"{ROOT.parent / "build-jsm-sdl/_deps/magic_enum-src/include"}" "{cpp}" /Fe:"{binary}"\n', encoding='utf-8')
    result = subprocess.run(['cmd.exe', '/d', '/c', str(batch)], cwd=folder, capture_output=True, text=True)
    if result.returncode:
        print(result.stdout, result.stderr)
        raise SystemExit(result.returncode)
    raise SystemExit(subprocess.run([str(binary)]).returncode)
