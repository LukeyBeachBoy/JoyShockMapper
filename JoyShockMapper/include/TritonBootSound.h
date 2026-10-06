#pragma once
#include <array>
#include <cstdint>

// The Steam Controller 2026 plays its own jingle when it powers on and off (and a
// cue when it loses its link). The tune is fixed in its firmware, but how loud --
// or whether at all -- is a byte the firmware keeps in its own settings store,
// `user/haptic_boot_level`, with a command that writes and persists it in one go
// (ID_SET_USER_STORE, docs/triton-firmware-customisation.md section 2). Because it
// lives in the controller, it keeps applying with JoyShockMapper closed, and one
// write per connection and change is all it needs.
namespace triton_boot_sound
{
constexpr uint8_t kSetUserStoreCommand = 0xDC;
// The only selector the command knows: byte 1 = the boot-level key.
constexpr uint8_t kBootLevelSelector = 0x01;
constexpr size_t kReportBytes = 64;

constexpr int kOff = 0;    // no power-on, power-off or lost-link sound
constexpr int kQuiet = 1;  // -18 dB
constexpr int kNormal = 2; // -12 dB, the factory value

inline bool isLevel(int level)
{
	return level >= kOff && level <= kNormal;
}

inline std::array<uint8_t, kReportBytes> levelReport(int level)
{
	std::array<uint8_t, kReportBytes> report {};
	report[0] = 1;
	report[1] = kSetUserStoreCommand;
	report[2] = 2;
	report[3] = kBootLevelSelector;
	report[4] = uint8_t(level);
	return report;
}
}
