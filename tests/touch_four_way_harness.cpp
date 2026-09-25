// GRID_SHAPE = FOUR_WAY divides a pad into the four cardinal wedges about its
// centre, the way Steam Input's button pad does: the split is on the diagonals,
// so anywhere in the top quarter is "up" rather than one of two top corners.
// Region order is clockwise from the top -- 0 up, 1 right, 2 down, 3 left.
#include "TouchGridRouting.h"
#include <cstdio>
#include <cmath>

static int failures = 0;
static void check(const char *what, bool ok) {
  std::printf("%-68s %s\n", what, ok ? "PASS" : "FAIL");
  if (!ok) ++failures;
}

// Pad coordinates are 0..1 with +y downward, so the top of the pad is y < .5.
static int cell(float x, float y, float deadzone = 0.f) {
  return touchGridCell(true, x, y, 2, 2, GridShape::FOUR_WAY, deadzone);
}

int main() {
  check("the top of the pad is up", cell(.5f, .1f) == 0);
  check("the right of the pad is right", cell(.9f, .5f) == 1);
  check("the bottom of the pad is down", cell(.5f, .9f) == 2);
  check("the left of the pad is left", cell(.1f, .5f) == 3);

  // The defining difference from a 2x2 rectangle: a top corner is still "up",
  // because the boundary runs along the diagonal rather than down the middle.
  check("the top-left corner is up, not left", cell(.05f, .05f) == 0);
  check("the top-right corner is up, not right", cell(.95f, .05f) == 0);
  check("the bottom-left corner is down, not left", cell(.05f, .95f) == 2);
  check("the bottom-right corner is down, not right", cell(.95f, .95f) == 2);
  // ... and the same points under RECTANGLE still give four distinct corners,
  // so the original layout is untouched.
  check("a 2x2 rectangle still splits the corners four ways",
    touchGridCell(true, .05f, .05f, 2, 2) == 0 && touchGridCell(true, .95f, .05f, 2, 2) == 1 &&
    touchGridCell(true, .05f, .95f, 2, 2) == 2 && touchGridCell(true, .95f, .95f, 2, 2) == 3);

  // Just off the diagonal resolves to the side it leans toward; exactly on it
  // is deterministic rather than flickering between two regions.
  check("just above the diagonal is up", cell(.30f, .29f) == 0);
  check("just below the diagonal is left", cell(.29f, .30f) == 3);
  check("an exact diagonal is stable", cell(.3f, .3f) == cell(.3f, .3f));

  // The centre deadzone: nothing is pressed inside it, everything outside it
  // still resolves. Expressed as a fraction of the pad's half-width, so .2
  // means a tenth of the pad's width from the centre.
  check("dead centre presses nothing with a deadzone set", cell(.5f, .5f, .2f) == -1);
  check("just inside the deadzone presses nothing", cell(.5f, .45f, .2f) == -1);
  check("just outside the deadzone presses up", cell(.5f, .39f, .2f) == 0);
  check("a zero deadzone still resolves the exact centre", cell(.5f, .5f, 0.f) >= 0);
  check("a full deadzone suppresses the whole pad", cell(.9f, .5f, 1.f) == -1);

  // Inactive and non-finite inputs behave as they do for a rectangle.
  check("an inactive pad presses nothing",
    touchGridCell(false, .5f, .1f, 2, 2, GridShape::FOUR_WAY, 0.f) == -1);
  check("a non-finite coordinate presses nothing",
    cell(std::nanf(""), .5f) == -1);

  // A wedge layout has four regions whatever the configured grid size is.
  check("four-way always has four regions", touchGridRegionCount(GridShape::FOUR_WAY, 5, 5) == 4);
  check("a rectangle has as many regions as cells", touchGridRegionCount(GridShape::RECTANGLE, 3, 4) == 12);

  auto eight = [](float x, float y, float deadzone = 0.f) {
    return touchGridCell(true, x, y, 2, 2, GridShape::EIGHT_WAY, deadzone);
  };
  check("eight-way has eight fixed regions", touchGridRegionCount(GridShape::EIGHT_WAY, 1, 1) == 8);
  check("eight-way runs clockwise from up",
    eight(.5f, .05f) == 0 && eight(.9f, .1f) == 1 && eight(.95f, .5f) == 2 &&
    eight(.9f, .9f) == 3 && eight(.5f, .95f) == 4 && eight(.1f, .9f) == 5 &&
    eight(.05f, .5f) == 6 && eight(.1f, .1f) == 7);
  check("eight-way deadzone suppresses the centre", eight(.5f, .5f, .3f) == -1);

  // Changing shape has to release the old region even when the finger has not
  // moved, or the previously pressed button sticks down under the new layout.
  TouchGridRouting routing;
  routing.update(true, -1, 2, 2, GridShape::RECTANGLE);
  check("switching shape releases the old region",
    routing.update(true, -1, 2, 2, GridShape::FOUR_WAY) == true);
  check("holding the same shape releases nothing",
    routing.update(true, -1, 2, 2, GridShape::FOUR_WAY) == false);

  // --- RADIAL: a weapon wheel ------------------------------------------------
  auto wheel = [](float x, float y, int segments, float deadzone = 0.f) {
    return touchGridCell(true, x, y, segments, 1, GridShape::RADIAL, deadzone);
  };

  // The shapes must agree where they overlap, or four-segment wheels and
  // four-way pads would select differently from the same thumb position.
  check("a four-segment wheel is exactly a four-way",
    wheel(.5f, .1f, 4) == 0 && wheel(.9f, .5f, 4) == 1 &&
    wheel(.5f, .9f, 4) == 2 && wheel(.1f, .5f, 4) == 3);
  check("a four-segment wheel puts the top-left corner in the up segment",
    wheel(.05f, .05f, 4) == 0);

  check("segment 0 is centred on up, not starting there", wheel(.5f, .0f, 8) == 0);
  check("eight segments run clockwise from up",
    wheel(.9f, .1f, 8) == 1 && wheel(.9f, .5f, 8) == 2 && wheel(.9f, .9f, 8) == 3 &&
    wheel(.5f, .9f, 8) == 4 && wheel(.1f, .9f, 8) == 5 && wheel(.1f, .5f, 8) == 6 &&
    wheel(.1f, .1f, 8) == 7);

  check("the hole in the donut selects nothing", wheel(.5f, .5f, 8, .3f) == -1);
  check("just outside the hole selects again", wheel(.5f, .2f, 8, .3f) == 0);
  check("a zero deadzone still resolves the exact centre", wheel(.5f, .5f, 8, 0.f) >= 0);
  check("fewer than two segments is not a wheel", wheel(.9f, .5f, 1) == -1);
  check("an inactive pad selects nothing",
    touchGridCell(false, .5f, .1f, 8, 1, GridShape::RADIAL, 0.f) == -1);
  check("a non-finite coordinate selects nothing", wheel(std::nanf(""), .5f, 8) == -1);

  // Sweeping the full circle: every segment must be reachable, none skipped,
  // and nothing may fall outside the range -- which is what a wrong wrap or an
  // off-by-one in the bias would produce.
  check("every segment is reachable and none is skipped", [&] {
    bool seen[12] = {};
    for (int i = 0; i < 720; ++i) {
      const float a = float(i) * 3.14159265f / 360.f;
      const int cell = wheel(.5f + .4f * std::sin(a), .5f - .4f * std::cos(a), 12);
      if (cell < 0 || cell >= 12) return false;
      seen[cell] = true;
    }
    for (bool s : seen)
      if (!s) return false;
    return true;
  }());

  check("region count is rows times columns, and one segment is not a wheel",
    touchGridRegionCount(GridShape::RADIAL, 4, 2) == 8 &&
    touchGridRegionCount(GridShape::RADIAL, 1, 1) == 0);

  check("switching to a wheel releases the old region", [&] {
    TouchGridRouting routing2;
    routing2.update(true, -1, 8, 1, GridShape::FOUR_WAY);
    return routing2.update(true, -1, 8, 1, GridShape::RADIAL) == true;
  }());
  check("switching to eight-way releases the old region", [&] {
    TouchGridRouting routing3;
    routing3.update(true, -1, 2, 2, GridShape::FOUR_WAY);
    return routing3.update(true, -1, 2, 2, GridShape::EIGHT_WAY) == true;
  }());

  std::printf("\n%s\n", failures ? "FAILURES" : "All checks passed.");
  return failures ? 1 : 0;
}
