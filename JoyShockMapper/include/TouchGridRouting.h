#pragma once
#include <algorithm>
#include <cmath>

// How a pad's touch regions are laid out. RECTANGLE is the original rows x
// columns grid. FOUR_WAY splits the pad into the four cardinal wedges about its
// centre, the way Steam Input's button pad does, so a thumb anywhere in the top
// quarter presses "up" rather than one of two top corners.
//
// New layouts append before INVALID: EIGHT_WAY belongs here when diagonals are
// wanted as their own regions. Existing names keep their meaning, so a profile
// written today still reads the same after that is added.
enum class GridShape
{
	RECTANGLE,
	FOUR_WAY,
	// A weapon-wheel: the pad is divided into equal angular segments about its
	// centre, with the deadzone acting as the hole of the donut. The segment
	// count is rows x columns, so GRID_SIZE = 8 1 gives eight segments; segment
	// 0 is CENTRED on up and they run clockwise, matching FOUR_WAY. A RADIAL of
	// four segments is therefore exactly a FOUR_WAY.
	RADIAL,
	// Fixed eight wedges, clockwise from up. Unlike RADIAL this does not use
	// GRID_SIZE; it is the button-pad layout with diagonals as first-class
	// regions.
	EIGHT_WAY,
	INVALID
};

// How many bindable regions a shape has. A wedge layout ignores the configured
// rows and columns: its region count is a property of the shape itself.
inline int touchGridRegionCount(GridShape shape, int columns, int rows)
{
	if (shape == GridShape::FOUR_WAY) return 4;
	if (shape == GridShape::EIGHT_WAY) return 8;
	const int cells = columns > 0 && rows > 0 ? columns * rows : 0;
	// Fewer than two segments is not a wheel, and one segment covering the whole
	// pad would fire wherever the thumb landed.
	if (shape == GridShape::RADIAL) return cells >= 2 ? cells : 0;
	return cells;
}

// Release the old region before entering a different layout, even when the
// finger has not moved and the new region happens to have the same index.
struct TouchGridRouting {
  bool active = false;
  int chord = -1;
  int columns = 0;
  int rows = 0;
  GridShape shape = GridShape::RECTANGLE;

  bool update(bool nextActive, int nextChord, int nextColumns, int nextRows, GridShape nextShape = GridShape::RECTANGLE) {
    const bool release = active && (!nextActive || chord != nextChord || columns != nextColumns || rows != nextRows || shape != nextShape);
    active = nextActive;
    chord = nextChord;
    columns = nextColumns;
    rows = nextRows;
    shape = nextShape;
    return release;
  }
};

// Cardinal wedge under the finger, clockwise from the top: 0 up, 1 right,
// 2 down, 3 left. deadzone is a fraction of the pad's half-width; inside it no
// region is pressed, so a thumb resting at dead centre does not flicker between
// two directions as the reading jitters. The split is on the diagonals, which
// is what makes this a direction pad rather than four corners.
inline int touchFourWayCell(bool active, float x, float y, float deadzone) {
  if (!active || !std::isfinite(x) || !std::isfinite(y)) return -1;
  const float dx = x - .5f;
  const float dy = y - .5f;
  const float limit = std::clamp(std::isfinite(deadzone) ? deadzone : 0.f, 0.f, 1.f) * .5f;
  // Guarded on limit so that turning the deadzone off does not make the one
  // exact centre coordinate the only unreachable point on the pad.
  if (limit > 0.f && std::hypot(dx, dy) <= limit) return -1;
  // Ties go to the vertical axis so an exact diagonal is still deterministic,
  // and the degenerate dead centre resolves to up rather than to nothing.
  if (std::abs(dy) >= std::abs(dx)) return dy <= 0.f ? 0 : 2;
  return dx > 0.f ? 1 : 3;
}

// Segment under the finger for a weapon-wheel layout. Segment 0 is centred on
// up and they run clockwise, so a four-segment wheel is identical to a FOUR_WAY
// and an eight-segment one adds the diagonals. deadzone is the donut's hole, as
// a fraction of the pad's half-width: inside it nothing is selected, which is
// how a wheel is dismissed without picking anything.
inline int touchRadialCell(bool active, float x, float y, int segments, float deadzone) {
  if (!active || segments < 2 || !std::isfinite(x) || !std::isfinite(y)) return -1;
  const float dx = x - .5f;
  const float dy = y - .5f;
  const float limit = std::clamp(std::isfinite(deadzone) ? deadzone : 0.f, 0.f, 1.f) * .5f;
  if (limit > 0.f && std::hypot(dx, dy) <= limit) return -1;
  // atan2(dx, -dy) is 0 pointing up and grows clockwise, which is the order the
  // regions are numbered in. Half a segment of bias centres segment 0 on up
  // rather than starting its edge there.
  constexpr float kTau = 6.2831853071795864769f;
  const float step = kTau / float(segments);
  float angle = std::atan2(dx, -dy) + step * .5f;
  while (angle < 0.f) angle += kTau;
  while (angle >= kTau) angle -= kTau;
  return std::clamp(int(angle / step), 0, segments - 1);
}

inline int touchGridCell(bool active, float x, float y, int columns, int rows,
                         GridShape shape = GridShape::RECTANGLE, float deadzone = 0.f) {
  if (shape == GridShape::FOUR_WAY) return touchFourWayCell(active, x, y, deadzone);
  if (shape == GridShape::RADIAL)
    return touchRadialCell(active, x, y, touchGridRegionCount(shape, columns, rows), deadzone);
  if (shape == GridShape::EIGHT_WAY)
    return touchRadialCell(active, x, y, 8, deadzone);
  if (!active || columns <= 0 || rows <= 0 || !std::isfinite(x) || !std::isfinite(y)) return -1;
  const int col = std::clamp(int(std::floor(x * columns)), 0, columns - 1);
  const int row = std::clamp(int(std::floor(y * rows)), 0, rows - 1);
  return row * columns + col;
}
