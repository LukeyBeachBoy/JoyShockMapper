#include "TouchGridRouting.h"
#include <cassert>
#include <cstdio>
#include <limits>

int main() {
  TouchGridRouting left, right;
  int pressed = -1, presses = 0, releases = 0;
  auto tick = [&](bool shifted, int chord, int columns, int rows, float x, float y) {
    if (right.update(shifted, chord, columns, rows) && pressed >= 0) { ++releases; pressed = -1; }
    const int cell = touchGridCell(shifted, x, y, columns, rows);
    if (cell != pressed) {
      if (pressed >= 0) ++releases;
      if (cell >= 0) ++presses;
      pressed = cell;
    }
  };
  tick(false, -1, 2, 1, .75f, .75f); // resting thumb: mouse only
  assert(presses == 0);
  tick(true, 2, 2, 2, .75f, .75f); // click with no movement
  assert(pressed == 3 && presses == 1 && releases == 0);
  for (int i=0; i<100; ++i) tick(true, 2, 2, 2, .75f, .75f);
  assert(presses == 1); // held click does not retrigger
  tick(false, -1, 2, 1, .75f, .75f); // release without lifting finger
  assert(pressed == -1 && releases == 1);
  tick(true, 2, 3, 2, .75f, .75f);
  assert(pressed == 5 && presses == 2);
  tick(true, 4, 3, 2, .75f, .75f); // same cell in different shift rebinds
  assert(presses == 3 && releases == 2);
  assert(!left.active); // right-pad changes do not activate left
  assert(touchGridCell(true, 1, 1, 5, 5) == 24);
  assert(touchGridCell(false, .5f, .5f, 2, 2) == -1);
  assert(touchGridCell(true, std::numeric_limits<float>::quiet_NaN(), .5f, 2, 2) == -1);
  puts("PASS: stationary click activates current region, hold does not repeat, release clears, shifted layouts rebind, pads stay independent");
}
