// Pad rotation (TouchpadRotation.h): the frame, the direction of a positive
// angle, and what happens to a corner that turns out of the square.
#include <cassert>
#include <cmath>
#include <iostream>

#include "TouchpadRotation.h"

namespace
{
bool near(float a, float b, float tolerance = 1e-4f)
{
	return std::fabs(a - b) <= tolerance;
}

void rotated(float x, float y, float degrees, float expectX, float expectY)
{
	touchpad_rotation::rotate(x, y, degrees);
	if (!near(x, expectX) || !near(y, expectY))
	{
		std::cerr << "rotate(" << degrees << ") gave " << x << "," << y << " expected " << expectX << "," << expectY << "\n";
		assert(false);
	}
}

void test_zero_and_invalid_angles_are_identity()
{
	rotated(0.2f, 0.7f, 0.f, 0.2f, 0.7f);
	rotated(0.2f, 0.7f, NAN, 0.2f, 0.7f);
	rotated(0.2f, 0.7f, INFINITY, 0.2f, 0.7f);
}

void test_positive_is_clockwise_in_the_y_down_frame()
{
	// Top-centre turned 90 degrees clockwise is right-centre, as on a clock face.
	rotated(0.5f, 0.f, 90.f, 1.f, 0.5f);
	// Right-centre -> bottom-centre, bottom -> left, left -> top.
	rotated(1.f, 0.5f, 90.f, 0.5f, 1.f);
	rotated(0.5f, 1.f, 90.f, 0.f, 0.5f);
	rotated(0.f, 0.5f, 90.f, 0.5f, 0.f);
	// Negative turns the other way.
	rotated(0.5f, 0.f, -90.f, 0.f, 0.5f);
	// A half turn is a point reflection.
	rotated(0.25f, 0.25f, 180.f, 0.75f, 0.75f);
}

void test_the_centre_never_moves()
{
	for (float degrees = -180.f; degrees <= 180.f; degrees += 7.5f)
		rotated(0.5f, 0.5f, degrees, 0.5f, 0.5f);
}

void test_the_cant_undo_is_small_and_in_the_right_direction()
{
	// A swipe straight up the LEFT pad's own frame lands slightly right of
	// top-centre once its cant is undone (the pad's up leans right on the body).
	float x = 0.5f, y = 0.f;
	touchpad_rotation::rotate(x, y, touchpad_rotation::kLeftPadCantDegrees);
	assert(x > 0.5f && x < 0.6f);
	assert(y >= 0.f && y < 0.05f);
	// The right pad's cant is the mirror image.
	x = 0.5f; y = 0.f;
	touchpad_rotation::rotate(x, y, touchpad_rotation::kRightPadCantDegrees);
	assert(x < 0.5f && x > 0.4f);
}

void test_a_corner_is_pulled_back_along_its_ray_and_stays_a_touch()
{
	// The top-right corner at 45 degrees clockwise points straight right; it must
	// land on the right edge, not outside the pad.
	float x = 1.f, y = 0.f;
	touchpad_rotation::rotate(x, y, 45.f);
	assert(near(x, 1.f) && near(y, 0.5f));
	// Any angle: the result is inside the square, and the direction from the
	// centre is the rotated direction.
	for (float degrees = -180.f; degrees <= 180.f; degrees += 5.f)
	{
		x = 1.f; y = 1.f;
		touchpad_rotation::rotate(x, y, degrees);
		assert(x >= 0.f && x <= 1.f && y >= 0.f && y <= 1.f);
		const float radians = degrees * touchpad_rotation::kPi / 180.f;
		const float wantX = 0.5f * std::cos(radians) - 0.5f * std::sin(radians);
		const float wantY = 0.5f * std::sin(radians) + 0.5f * std::cos(radians);
		const float gotAngle = std::atan2(y - 0.5f, x - 0.5f);
		const float wantAngle = std::atan2(wantY, wantX);
		// Compared modulo a full turn: at 135 degrees the corner points exactly
		// left, where atan2 sits on the +/-pi seam.
		const float seamSafe = std::fmod(gotAngle - wantAngle + 3.f * touchpad_rotation::kPi, 2.f * touchpad_rotation::kPi) - touchpad_rotation::kPi;
		assert(near(seamSafe, 0.f, 1e-3f));
	}
}

void test_no_contact_positions_are_left_alone()
{
	// Outside 0..1 means "lifted" to the mapper; turning it could make it look
	// like a touch.
	rotated(-1.f, -1.f, 45.f, -1.f, -1.f);
	rotated(2.f, 0.5f, 45.f, 2.f, 0.5f);
}

void test_rotation_preserves_distance_from_centre_inside_the_disc()
{
	// Anything within the inscribed circle never leaves the square, so it is a
	// pure rotation: same radius, angle advanced by the argument.
	for (float degrees = -170.f; degrees <= 170.f; degrees += 23.f)
	{
		float x = 0.5f + 0.3f, y = 0.5f;
		touchpad_rotation::rotate(x, y, degrees);
		assert(near(std::hypot(x - 0.5f, y - 0.5f), 0.3f));
		const float gotDegrees = std::atan2(y - 0.5f, x - 0.5f) * 180.f / touchpad_rotation::kPi;
		float diff = std::fmod(gotDegrees - degrees + 540.f, 360.f) - 180.f;
		assert(near(diff, 0.f, 1e-2f));
	}
}
}

int main()
{
	test_zero_and_invalid_angles_are_identity();
	test_positive_is_clockwise_in_the_y_down_frame();
	test_the_centre_never_moves();
	test_the_cant_undo_is_small_and_in_the_right_direction();
	test_a_corner_is_pulled_back_along_its_ray_and_stays_a_touch();
	test_no_contact_positions_are_left_alone();
	test_rotation_preserves_distance_from_centre_inside_the_disc();
	std::cout << "touch_rotation_tests passed\n";
	return 0;
}
