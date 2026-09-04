#!/usr/bin/env python3
"""Guards haptics as a bindable action rather than a one-off feature.

The controller has a set of haptic effects its firmware plays on either handle.
Exposing them as a KeyCode -- the same trick RUMBLE already uses, where the
binding's name carries its parameters -- means every input that can take a
binding can fire one, and so can anything built on the binding machinery later:
chords, taps, holds, and mode shifts when they arrive. A bespoke "haptic
settings" panel would have had to be re-plumbed for each of those.

Run: python3 tests/haptic_binding_regression.py     (no dependencies)
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).parents[1]
PLATFORM_H = (ROOT / 'JoyShockMapper/include/PlatformDefinitions.h').read_text()
MAPPING = (ROOT / 'JoyShockMapper/src/Mapping.cpp').read_text()
MAPPING_H = (ROOT / 'JoyShockMapper/include/Mapping.h').read_text()
SDL = (ROOT / 'JoyShockMapper/src/SDLWrapper.cpp').read_text()
JSLW = (ROOT / 'JoyShockMapper/include/JslWrapper.h').read_text()
JSLW_CPP = (ROOT / 'JoyShockMapper/src/JslWrapper.cpp').read_text()

LINUX_PLATFORM = ROOT / 'JoyShockMapper/src/linux/PlatformDefinitions.cpp'
WIN32_PLATFORM = ROOT / 'JoyShockMapper/src/win32/PlatformDefinitions.cpp'


def test_haptic_is_a_key_code_so_any_binding_can_fire_it():
    assert 'constexpr uint16_t HAPTIC = 0xE7;' in PLATFORM_H
    assert 'parseHapticName' in PLATFORM_H
    # Routed in the same place every other binding action is.
    assert 'key.code == HAPTIC' in MAPPING
    assert 'EventActionIf::FireHaptic' in MAPPING
    assert 'virtual void FireHaptic(int side, int effect, int gainDb) = 0;' in MAPPING_H


def test_a_haptic_binding_has_nothing_to_release():
    """The effect has its own duration. Binding a release would either cut it
    short or, worse, look like a stuck key to the rest of the machinery."""
    body = MAPPING.split('key.code == HAPTIC', 1)[1].split('else //', 1)[0]
    assert 'release = EventActionIf::Callback();' in body


def test_both_platforms_share_one_parser():
    """A name that parses on Windows and not on Linux would be a config that
    silently does nothing on one of them."""
    for path in (LINUX_PLATFORM, WIN32_PLATFORM):
        assert 'std::string parseHapticName' in path.read_text(), path.name


def test_every_firmware_effect_is_reachable():
    src = LINUX_PLATFORM.read_text()
    effects = re.search(r'static constexpr std::string_view effects\[\] = \{(.*?)\}', src, re.S)
    assert effects is not None, 'no effect table'
    for name in ('OFF', 'TICK', 'CLICK', 'TONE', 'RUMBLE', 'NOISE', 'SCRIPT', 'SWEEP'):
        assert f'"{name}"' in effects.group(1), name


def test_the_backend_reaches_the_device_and_only_the_right_device():
    assert 'virtual void SetHaptic(int deviceId, int side, int effect, int gainDb) = 0;' in JSLW
    assert 'void SetHaptic(int deviceId, int side, int effect, int gainDb) override' in JSLW_CPP, \
        'the legacy backend must still implement the interface or the build breaks'
    body = SDL.split('void SetHaptic(int deviceId', 1)[1].split('\n\t}', 1)[0]
    assert '_ctrlr_type != JS_TYPE_STEAM_CONTROLLER_2026' in body
    assert 'sendHapticEffect' in body


def test_the_grip_pulse_reuses_the_general_sender():
    """Two code paths building the same report is how the side encoding came to be
    wrong in one of them."""
    body = SDL.split('static void sendGripHaptic', 1)[1].split('\n\t}', 1)[0]
    assert 'sendHapticEffect(' in body
    assert 'buffer[0]' not in body, 'the grip pulse must not build its own report'


def test_parser_round_trips():
    """Compile the real parser and check the payloads Mapping.cpp will decode."""
    impl = LINUX_PLATFORM.read_text()
    impl = impl[impl.index('std::string parseHapticName'):]
    harness = '''
#include <string>
#include <string_view>
#include <iterator>
#include <cstdio>
''' + impl + '''
int main() {
    struct { const char *name; const char *want; } cases[] = {
        {"HAPTIC_L_CLICK", "HL2g0"},
        {"HAPTIC_R_TICK_N12", "HR1g-12"},
        {"HAPTIC_BOTH_TONE_6", "HB3g6"},
        {"HAPTIC_L_SWEEP", "HL7g0"},
        {"HAPTIC_X_CLICK", ""},
        {"HAPTIC_L_BOGUS", ""},
        {"SMALL_RUMBLE", ""},
        {"HAPTIC_", ""},
    };
    int bad = 0;
    for (auto &c : cases) {
        std::string got = parseHapticName(c.name);
        if (got != c.want) { printf("%s -> '%s', want '%s'\\n", c.name, got.c_str(), c.want); bad++; }
    }
    return bad;
}
'''
    with tempfile.TemporaryDirectory() as tmp:
        src = Path(tmp) / 'p.cpp'
        src.write_text(harness)
        exe = Path(tmp) / 'p'
        build = subprocess.run(['g++', '-std=c++20', '-o', str(exe), str(src)],
                               capture_output=True, text=True)
        if build.returncode != 0:
            raise AssertionError(f'parser did not compile:\n{build.stderr[:800]}')
        run = subprocess.run([str(exe)], capture_output=True, text=True)
        assert run.returncode == 0, f'parser round trip failed:\n{run.stdout}'


if __name__ == '__main__':
    failures = 0
    for name, fn in sorted(globals().items()):
        if name.startswith('test_'):
            try:
                fn()
                print(f'PASS {name}')
            except AssertionError as exc:
                failures += 1
                print(f'FAIL {name}: {exc}')
    sys.exit(1 if failures else 0)
