// Compiles the real ButtonID enum and checks the three touch-grid ID ranges are
// laid out the way every grid lookup assumes: shared grid first, then the left
// pad's 25, then the right pad's 25, all inside magic_enum's configured range.
//
// The range matters more than it looks. magic_enum can only name values inside
// MAGIC_ENUM_RANGE_MIN..MAX, and the per-pad grids register their commands by
// asking magic_enum for the name of FIRST_<SIDE>_TOUCH_BUTTON + i. Push the tail
// of the enum past the ceiling and enum_cast starts returning nullopt: no crash,
// no diagnostic, the per-pad bindings simply stop existing.
#include "JoyShockMapper.h"

#include <cstdio>

static int failures = 0;

static void check(bool cond, const char *what)
{
	if (cond)
	{
		printf("PASS %s\n", what);
	}
	else
	{
		printf("FAIL %s\n", what);
		++failures;
	}
}

int main()
{
	check(MAX_GRID_BUTTONS == 25, "a grid holds 25 cells, matching the 1..25 GRID_SIZE filter");

	check(FIRST_TOUCH_BUTTON == int(ButtonID::T1),
	  "the shared grid starts at T1");
	check(FIRST_LEFT_TOUCH_BUTTON == int(ButtonID::T25) + 1,
	  "the left pad's grid starts right after the shared one");
	check(FIRST_RIGHT_TOUCH_BUTTON == FIRST_LEFT_TOUCH_BUTTON + MAX_GRID_BUTTONS,
	  "the right pad's grid starts right after the left pad's");
	check(int(ButtonID::LT25) - int(ButtonID::LT1) + 1 == MAX_GRID_BUTTONS,
	  "the left pad has a full grid's worth of IDs");
	check(int(ButtonID::RT25) - int(ButtonID::RT1) + 1 == MAX_GRID_BUTTONS,
	  "the right pad has a full grid's worth of IDs");

	// The ranges must not overlap, or one pad's cell resolves to another's.
	check(int(ButtonID::T25) < FIRST_LEFT_TOUCH_BUTTON && int(ButtonID::LT25) < FIRST_RIGHT_TOUCH_BUTTON,
	  "the three grid ranges are disjoint and in ascending order");

	// Every grid ID has to survive the round trip the registration code does.
	bool allNamed = true;
	for (int id = FIRST_TOUCH_BUTTON; id <= int(ButtonID::RT25); ++id)
	{
		auto cast = magic_enum::enum_cast<ButtonID>(id);
		if (!cast || magic_enum::enum_name(*cast).empty())
		{
			printf("  id %d has no magic_enum name (MAGIC_ENUM_RANGE_MAX is %d)\n", id, MAGIC_ENUM_RANGE_MAX);
			allNamed = false;
			break;
		}
	}
	check(allNamed, "every grid button ID is nameable, so its command can be registered");

	check(magic_enum::enum_name(*magic_enum::enum_cast<ButtonID>(FIRST_LEFT_TOUCH_BUTTON)) == "LT1",
	  "the left pad's first cell registers as LT1, not as a second T1");
	check(magic_enum::enum_name(*magic_enum::enum_cast<ButtonID>(FIRST_RIGHT_TOUCH_BUTTON)) == "RT1",
	  "the right pad's first cell registers as RT1");

	// MAPPING_SIZE is what ButtonHelp.cpp is asserted against; the tail additions
	// live past SIZE and must not disturb it.
	check(MAPPING_SIZE == int(ButtonID::SIZE), "the mapped button count is unchanged by the grid IDs");

	return failures ? 1 : 0;
}
