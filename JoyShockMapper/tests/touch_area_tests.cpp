// MOUSE_AREA (TouchAreaMapping.h): where a finger on the pad puts the cursor.
#include <cassert>
#include <cmath>
#include <iostream>

#include "TouchAreaMapping.h"

namespace
{
bool near(float a, float b, float tolerance = 1e-4f)
{
	return std::fabs(a - b) <= tolerance;
}

void expect(float u, float v, const touch_area::Rect &area, touch_area::Fit fit, float padAspect, float screenAspect, float expectX, float expectY)
{
	float x = -1.f, y = -1.f;
	touch_area::map(u, v, area, fit, padAspect, screenAspect, x, y);
	if (!near(x, expectX) || !near(y, expectY))
	{
		std::cerr << "map(" << u << "," << v << ") gave " << x << "," << y << " expected " << expectX << "," << expectY << "\n";
		assert(false);
	}
}

void test_stretch_maps_pad_corners_to_area_corners()
{
	const touch_area::Rect hotbar{ 0.3f, 0.9f, 0.4f, 0.08f };
	expect(0.f, 0.f, hotbar, touch_area::Fit::STRETCH, 1.f, 16.f / 9.f, 0.3f, 0.9f);
	expect(1.f, 1.f, hotbar, touch_area::Fit::STRETCH, 1.f, 16.f / 9.f, 0.7f, 0.98f);
	expect(0.5f, 0.5f, hotbar, touch_area::Fit::STRETCH, 1.f, 16.f / 9.f, 0.5f, 0.94f);
	// The pad's shape is irrelevant to STRETCH.
	expect(0.25f, 0.5f, hotbar, touch_area::Fit::STRETCH, 2.f, 16.f / 9.f, 0.4f, 0.94f);
}

void test_default_area_is_the_whole_screen()
{
	expect(0.f, 0.f, touch_area::Rect{}, touch_area::Fit::STRETCH, 1.f, 16.f / 9.f, 0.f, 0.f);
	expect(1.f, 1.f, touch_area::Rect{}, touch_area::Fit::STRETCH, 1.f, 16.f / 9.f, 1.f, 1.f);
}

void test_finger_cannot_leave_the_area()
{
	const touch_area::Rect area{ 0.25f, 0.25f, 0.5f, 0.5f };
	// Readings just outside 0..1 (a rotated corner) clamp to the edge.
	expect(-0.2f, 1.3f, area, touch_area::Fit::STRETCH, 1.f, 16.f / 9.f, 0.25f, 0.75f);
}

void test_uniform_square_pad_over_wide_area_spans_the_width()
{
	// A square pad over a 16:9 screen's full width: the pad covers the screen,
	// so left edge is left edge and right edge is right edge...
	const touch_area::Rect full{};
	const float screen = 16.f / 9.f;
	expect(0.f, 0.5f, full, touch_area::Fit::UNIFORM, 1.f, screen, 0.f, 0.5f);
	expect(1.f, 0.5f, full, touch_area::Fit::UNIFORM, 1.f, screen, 1.f, 0.5f);
	// ...and vertical travel is the same scale, so the pad's full height would
	// reach well past the screen and clamps: the top of the pad is the top of
	// the screen, and a quarter of the way down the pad is already past it.
	expect(0.5f, 0.f, full, touch_area::Fit::UNIFORM, 1.f, screen, 0.5f, 0.f);
	// Centre of pad is the centre of the screen; a point 0.1 down the pad moves
	// 0.1 * screenAspect of the screen height (pad height == screen width).
	expect(0.5f, 0.5f + 0.1f, full, touch_area::Fit::UNIFORM, 1.f, screen, 0.5f, 0.5f + 0.1f * screen);
}

void test_uniform_is_isotropic()
{
	// A square pad over a square area on a 16:9 screen: moving a tenth of the
	// pad sideways and a tenth down must cover the same number of pixels.
	const float screen = 16.f / 9.f;
	// 0.2 of the width on 16:9 is 0.2*16/9 = 0.3556 of the height, so a square
	// area is 0.2 wide and 0.3556 tall in screen fractions.
	const touch_area::Rect square{ 0.4f, 0.3f, 0.2f, 0.2f * screen };
	float cx, cy, rx, ry, dx, dy;
	touch_area::map(0.5f, 0.5f, square, touch_area::Fit::UNIFORM, 1.f, screen, cx, cy);
	touch_area::map(0.6f, 0.5f, square, touch_area::Fit::UNIFORM, 1.f, screen, rx, ry);
	touch_area::map(0.5f, 0.6f, square, touch_area::Fit::UNIFORM, 1.f, screen, dx, dy);
	const float pixelsRight = (rx - cx) * screen; // in units of screen height
	const float pixelsDown = dy - cy;
	assert(near(pixelsRight, pixelsDown));
	assert(near(ry, cy) && near(dx, cx));
	// And the pad's corners are the square's corners exactly.
	expect(0.f, 0.f, square, touch_area::Fit::UNIFORM, 1.f, screen, 0.4f, 0.3f);
	expect(1.f, 1.f, square, touch_area::Fit::UNIFORM, 1.f, screen, 0.6f, 0.3f + 0.2f * screen);
}

void test_uniform_wide_pad_over_tall_area_spans_the_height()
{
	// A DualSense-shaped pad (about 2:1) over a tall column: the pad's height
	// spans the column, its spare width clamps to the column's sides.
	const float screen = 16.f / 9.f;
	const touch_area::Rect column{ 0.45f, 0.1f, 0.1f, 0.8f };
	expect(0.5f, 0.f, column, touch_area::Fit::UNIFORM, 2.f, screen, 0.5f, 0.1f);
	expect(0.5f, 1.f, column, touch_area::Fit::UNIFORM, 2.f, screen, 0.5f, 0.9f);
	expect(0.f, 0.5f, column, touch_area::Fit::UNIFORM, 2.f, screen, 0.45f, 0.5f);
	expect(1.f, 0.5f, column, touch_area::Fit::UNIFORM, 2.f, screen, 0.55f, 0.5f);
}

void test_unknown_shapes_fall_back_to_stretch()
{
	const touch_area::Rect area{ 0.1f, 0.1f, 0.5f, 0.5f };
	expect(1.f, 1.f, area, touch_area::Fit::UNIFORM, 0.f, 16.f / 9.f, 0.6f, 0.6f);
	expect(1.f, 1.f, area, touch_area::Fit::UNIFORM, 1.f, NAN, 0.6f, 0.6f);
}

void test_sanitize_keeps_a_rect_on_screen()
{
	auto r = touch_area::sanitize(touch_area::Rect{ 0.9f, -0.5f, 0.5f, 0.3f });
	assert(near(r.x, 0.9f) && near(r.y, 0.f) && near(r.w, 0.1f) && near(r.h, 0.3f));
	r = touch_area::sanitize(touch_area::Rect{ 1.f, 1.f, 0.f, 0.f });
	assert(r.w > 0.f && r.h > 0.f && r.x + r.w <= 1.f + 1e-6f && r.y + r.h <= 1.f + 1e-6f);
	r = touch_area::sanitize(touch_area::Rect{ NAN, 0.f, 1.f, 1.f });
	assert(near(r.x, 0.f) && near(r.w, 1.f));
}
} // namespace

int main()
{
	test_stretch_maps_pad_corners_to_area_corners();
	test_default_area_is_the_whole_screen();
	test_finger_cannot_leave_the_area();
	test_uniform_square_pad_over_wide_area_spans_the_width();
	test_uniform_is_isotropic();
	test_uniform_wide_pad_over_tall_area_spans_the_height();
	test_unknown_shapes_fall_back_to_stretch();
	test_sanitize_keeps_a_rect_on_screen();
	std::cout << "touch_area_tests: ok\n";
	return 0;
}
