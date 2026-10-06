#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

// The Steam Controller 2026's Steam-button light is an RGBW package on four PWM
// channels. Its firmware takes a colour as one byte per channel, in percent, but
// only shows it while its "user colour" setting (id 37) is on; otherwise it
// draws its own patterns -- white when connected, orange while charging, green
// when full, a red blink when the battery is low. Both the colour and the switch
// are RAM in the controller: a reconnect has to send them again and a power
// cycle clears them. (docs/triton-firmware-customisation.md, section 1.)
//
// A host colour is an RGB triple. The part the three channels share drives the
// white LED, whose factory gain is the controller's calibrated white point, and
// only the remainder goes to the coloured LEDs: pure white lights W alone, pure
// red lights R alone, pink is W plus some R.
namespace triton_led
{
// Feature report id 1, Valve's ID_SET_LED_COLOR, four payload bytes [R G B W].
constexpr uint8_t kSetColorCommand = 0xC5;
// The setting id that switches the user colour on (1) or back to the firmware's
// own patterns (0). Written with ID_SET_SETTINGS_VALUES like the grip settings.
constexpr uint8_t kUserColorSetting = 0x25;
constexpr size_t kReportBytes = 64;

struct Payload
{
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t w = 0;
	bool operator==(const Payload &other) const { return r == other.r && g == other.g && b == other.b && w == other.w; }
	bool operator!=(const Payload &other) const { return !(*this == other); }
};

// 0..255 -> 0..100, rounded.
inline uint8_t percent(int channel)
{
	return uint8_t((std::clamp(channel, 0, 255) * 100 + 127) / 255);
}

// rgb is 0x00RRGGBB, the layout JoyShockMapper's Color keeps.
inline Payload payload(uint32_t rgb)
{
	const int r = int((rgb >> 16) & 0xFF);
	const int g = int((rgb >> 8) & 0xFF);
	const int b = int(rgb & 0xFF);
	const int w = std::min({ r, g, b });
	return { percent(r - w), percent(g - w), percent(b - w), percent(w) };
}

inline std::array<uint8_t, kReportBytes> colorReport(const Payload &p)
{
	std::array<uint8_t, kReportBytes> report {};
	report[0] = 1;
	report[1] = kSetColorCommand;
	report[2] = 4;
	report[3] = p.r;
	report[4] = p.g;
	report[5] = p.b;
	report[6] = p.w;
	return report;
}
}
