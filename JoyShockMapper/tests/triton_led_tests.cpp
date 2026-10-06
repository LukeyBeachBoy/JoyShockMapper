// The Steam Controller 2026 light (TritonLed.h) and jingle volume
// (TritonBootSound.h): RGB -> RGBW percentages and the exact report bytes the
// firmware expects (docs/triton-firmware-customisation.md).
#include <cassert>
#include <iostream>

#include "TritonBootSound.h"
#include "TritonLed.h"

namespace
{
void test_primaries_light_one_channel()
{
	assert((triton_led::payload(0xFF0000) == triton_led::Payload { 100, 0, 0, 0 }));
	assert((triton_led::payload(0x00FF00) == triton_led::Payload { 0, 100, 0, 0 }));
	assert((triton_led::payload(0x0000FF) == triton_led::Payload { 0, 0, 100, 0 }));
}

void test_white_uses_the_white_led_alone()
{
	assert((triton_led::payload(0xFFFFFF) == triton_led::Payload { 0, 0, 0, 100 }));
	// Half-bright white is half-bright W, not three dim colour LEDs.
	assert((triton_led::payload(0x808080) == triton_led::Payload { 0, 0, 0, 50 }));
}

void test_off_is_all_zero()
{
	assert((triton_led::payload(0x000000) == triton_led::Payload { 0, 0, 0, 0 }));
}

void test_mixed_colours_split_into_white_plus_tint()
{
	// Pink: full red, half green and blue -> half white plus the red remainder.
	assert((triton_led::payload(0xFF8080) == triton_led::Payload { 50, 0, 0, 50 }));
	// Orange: red plus a little green, no blue -> no white component.
	const auto orange = triton_led::payload(0xFFA500);
	assert(orange.r == 100 && orange.g == 65 && orange.b == 0 && orange.w == 0);
}

void test_percent_rounds_and_clamps()
{
	assert(triton_led::percent(0) == 0);
	assert(triton_led::percent(255) == 100);
	assert(triton_led::percent(128) == 50);
	assert(triton_led::percent(1) == 0);
	assert(triton_led::percent(3) == 1);
	assert(triton_led::percent(-5) == 0);
	assert(triton_led::percent(999) == 100);
}

void test_colour_report_layout()
{
	const auto report = triton_led::colorReport({ 10, 20, 30, 40 });
	assert(report.size() == 64);
	assert(report[0] == 1);    // HID report id
	assert(report[1] == 0xC5); // ID_SET_LED_COLOR
	assert(report[2] == 4);    // payload length
	assert(report[3] == 10 && report[4] == 20 && report[5] == 30 && report[6] == 40);
	for (size_t i = 7; i < report.size(); ++i)
		assert(report[i] == 0);
	assert(triton_led::kUserColorSetting == 0x25);
}

void test_boot_sound_levels()
{
	using namespace triton_boot_sound;
	assert(isLevel(kOff) && isLevel(kQuiet) && isLevel(kNormal));
	assert(!isLevel(-1) && !isLevel(3));
	const auto report = levelReport(kQuiet);
	assert(report.size() == 64);
	assert(report[0] == 1);
	assert(report[1] == 0xDC); // ID_SET_USER_STORE
	assert(report[2] == 2);    // selector + level
	assert(report[3] == 1);    // user/haptic_boot_level
	assert(report[4] == 1);
	for (size_t i = 5; i < report.size(); ++i)
		assert(report[i] == 0);
}
}

int main()
{
	test_primaries_light_one_channel();
	test_white_uses_the_white_led_alone();
	test_off_is_all_zero();
	test_mixed_colours_split_into_white_plus_tint();
	test_percent_rounds_and_clamps();
	test_colour_report_layout();
	test_boot_sound_levels();
	std::cout << "triton_led_tests passed\n";
	return 0;
}
