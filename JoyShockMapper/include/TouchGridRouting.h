#pragma once
#include <algorithm>
#include <cmath>

// Release the old region before entering a different layout, even when the
// finger has not moved and the new region happens to have the same index.
struct TouchGridRouting {
  bool active = false;
  int chord = -1;
  int columns = 0;
  int rows = 0;

  bool update(bool nextActive, int nextChord, int nextColumns, int nextRows) {
    const bool release = active && (!nextActive || chord != nextChord || columns != nextColumns || rows != nextRows);
    active = nextActive;
    chord = nextChord;
    columns = nextColumns;
    rows = nextRows;
    return release;
  }
};

inline int touchGridCell(bool active, float x, float y, int columns, int rows) {
  if (!active || columns <= 0 || rows <= 0 || !std::isfinite(x) || !std::isfinite(y)) return -1;
  const int col = std::clamp(int(std::floor(x * columns)), 0, columns - 1);
  const int row = std::clamp(int(std::floor(y * rows)), 0, rows - 1);
  return row * columns + col;
}
