#pragma once
#include <algorithm>
#include <cmath>

// TOUCHPAD_MODE = MOUSE_AREA: the pad is a map of one rectangle of the screen.
// Where the finger is on the pad is where the cursor goes inside that rectangle,
// absolutely -- the top-left corner of the pad is the top-left corner of the
// area, and the finger cannot take the cursor out of it. What Steam Input calls
// a "mouse region". It exists so a hotbar or an inventory can be swept with a
// thumb instead of swiping the whole screen.
//
// The area is stored as FRACTIONS of the screen the game is on (x y w h, each
// 0..1), never pixels, so one profile lands on the same part of a 1080p monitor
// and a 4K TV. Which screen that is gets decided where the cursor is placed
// (InputHelpers), not here.
//
// Everything in this header is pure arithmetic with no dependency on the rest
// of the mapper, so Studio's TypeScript mirror (utils/mouseArea.ts) can be
// checked against it by compiling this file as-is.
namespace touch_area
{
struct Rect
{
	float x = 0.f;
	float y = 0.f;
	float w = 1.f;
	float h = 1.f;
};

// How a pad whose shape differs from the area's is laid over it.
//
//   STRETCH  the whole pad is the whole area. A square Steam Controller pad
//            driving a wide hotbar moves faster sideways than up and down, but
//            every point of the area is reachable and the pad's edges are the
//            area's edges. This is what Steam does, and the default.
//   UNIFORM  the finger moves the cursor the same distance per millimetre in
//            both directions. The pad is scaled so it COVERS the area (like
//            CSS background-size: cover), then centred on it: the area's
//            longer side spans the pad and the pad's spare travel on the other
//            axis clamps to the area's edge. Nothing in the area is out of
//            reach; some of the pad is dead travel.
enum class Fit
{
	STRETCH,
	UNIFORM,
};

inline float clamp01(float v)
{
	return std::min(1.f, std::max(0.f, v));
}

// A rect with its corners put in range and a minimum size, so a stray value in
// a config cannot park the cursor on one pixel or off the screen.
inline Rect sanitize(Rect r)
{
	constexpr float kMin = 0.005f;
	if (!std::isfinite(r.x) || !std::isfinite(r.y) || !std::isfinite(r.w) || !std::isfinite(r.h))
		return Rect{};
	r.x = clamp01(r.x);
	r.y = clamp01(r.y);
	r.w = std::min(1.f - r.x, std::max(kMin, r.w));
	r.h = std::min(1.f - r.y, std::max(kMin, r.h));
	// x or y at exactly 1 leaves no room: back the origin off so the minimum
	// size still fits on screen.
	if (r.w < kMin)
	{
		r.x = 1.f - kMin;
		r.w = kMin;
	}
	if (r.h < kMin)
	{
		r.y = 1.f - kMin;
		r.h = kMin;
	}
	return r;
}

// Maps a finger at (u, v) -- the pad's normalised 0..1 position, y down -- to a
// screen position, also as fractions 0..1 of the screen, y down.
//
// padAspect is the pad's width over its height (1 for a square pad, about 2 for
// a DualSense). screenAspect is the target screen's width over its height. Both
// only matter for UNIFORM; STRETCH ignores shape entirely. Aspect values that
// make no sense fall back to STRETCH rather than producing a NaN cursor.
inline void map(float u, float v, const Rect &rect, Fit fit, float padAspect, float screenAspect, float &outX, float &outY)
{
	const Rect area = sanitize(rect);
	u = clamp01(u);
	v = clamp01(v);

	const bool shapesKnown = std::isfinite(padAspect) && padAspect > 0.f && std::isfinite(screenAspect) && screenAspect > 0.f;
	if (fit == Fit::STRETCH || !shapesKnown)
	{
		outX = area.x + u * area.w;
		outY = area.y + v * area.h;
		return;
	}

	// Work in a frame where the screen is screenAspect wide and 1 tall, so
	// distances are the same unit in both directions (that is the whole point
	// of UNIFORM). The pad is padAspect wide and 1 tall before scaling.
	const float areaW = area.w * screenAspect;
	const float areaH = area.h;
	const float scale = std::max(areaW / padAspect, areaH);
	const float padW = scale * padAspect;
	const float padH = scale;
	const float centreX = area.x * screenAspect + areaW * 0.5f;
	const float centreY = area.y + areaH * 0.5f;

	float px = centreX + (u - 0.5f) * padW;
	float py = centreY + (v - 0.5f) * padH;
	// The pad's spare travel on the short axis is clamped to the area's edge,
	// so the cursor never leaves the rectangle the user drew.
	px = std::min(area.x * screenAspect + areaW, std::max(area.x * screenAspect, px));
	py = std::min(area.y + areaH, std::max(area.y, py));

	outX = px / screenAspect;
	outY = py;
}
} // namespace touch_area
