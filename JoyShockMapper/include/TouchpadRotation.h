#pragma once
#include <algorithm>
#include <cmath>

// Turning a pad's reading about the pad's centre, for pads mounted at an angle:
// the Steam Controller 2026's are canted about 10.6 degrees outward, so a swipe
// straight up the controller body reads as slightly diagonal.
//
// Coordinates are SDL's normalised 0..1 with y down. A positive angle turns the
// reading clockwise as the user sees it, which is the direction that undoes the
// LEFT pad's cant (the pad itself is turned clockwise on the body, so its "up"
// leans right; turning the reading clockwise by the same angle makes a swipe
// straight up the body read as straight up). The right pad is canted the other
// way and takes a negative angle.
//
// This runs where the pad is read, before anything consumes the position, so
// grids, wedge menus, the touch stick, the mouse and the telemetry Studio draws
// all see one frame.
namespace touchpad_rotation
{
constexpr float kPi = 3.14159265358979323846f;

// Studio's controller artwork, measured from the SVG: the angle each pad is
// turned on the body. These are the values that make a pad read square to the
// body ("level with the controller").
constexpr float kLeftPadCantDegrees = 10.7f;
constexpr float kRightPadCantDegrees = -10.5f;

inline bool isActive(float degrees)
{
	return std::isfinite(degrees) && degrees != 0.f;
}

// Rotates (x, y) in place. A point that leaves the unit square after turning --
// a corner does at any angle that is not a multiple of 90 -- is pulled back
// along its own ray from the centre, so the finger keeps pointing the same way
// and TOUCH_POINT::isDown() (which treats anything outside 0..1 as lifted)
// still sees the touch. Positions already outside 0..1 are no contact and are
// left alone.
inline void rotate(float &x, float &y, float degrees)
{
	if (!isActive(degrees) || !std::isfinite(x) || !std::isfinite(y))
		return;
	if (x < 0.f || x > 1.f || y < 0.f || y > 1.f)
		return;
	const float radians = degrees * kPi / 180.f;
	const float c = std::cos(radians);
	const float s = std::sin(radians);
	const float dx = x - .5f;
	const float dy = y - .5f;
	float rx = dx * c - dy * s;
	float ry = dx * s + dy * c;
	const float extent = std::max(std::fabs(rx), std::fabs(ry));
	if (extent > .5f)
	{
		const float k = .5f / extent;
		rx *= k;
		ry *= k;
	}
	x = std::clamp(rx + .5f, 0.f, 1.f);
	y = std::clamp(ry + .5f, 0.f, 1.f);
}
}
