// A full-pull binding must engage when the trigger reaches the top of its range
// and then STAY engaged while it is held there.
//
// Two faults, one after the other. The mapper asked for `position == 1.0` -- an
// exact comparison against SDL's axis maximum -- so a trigger that stops a count
// short never fired its full-pull binding at all. Relaxing only the engage test
// then left the release tests at `position < 1.0`, so anything resting between
// the two thresholds engaged and released on alternating polls and machine-gunned
// the binding.
//
// Numeric check only; no controller, no SDL, no mapper state machine.
#include "../JoyShockMapper/include/InputGuards.h"
#include <cassert>
#include <cstdio>

int main()
{
	// A trigger that reaches the very top engages.
	assert(fullPullPressed(false, 1.0f));
	// ...and so does one that stops a couple of counts short, which is the case
	// that reported "the full pull does nothing".
	assert(fullPullPressed(false, 32766.f / 32767.f));
	assert(fullPullPressed(false, 32740.f / 32767.f));

	// A half pull is not a full pull.
	assert(!fullPullPressed(false, 0.5f));
	assert(!fullPullPressed(false, 0.98f));

	// Held at the engage point, it must not drop out: this is the chatter.
	for (float position : { 0.99f, 0.995f, 0.9899f, 0.985f, 0.98f, 1.0f })
	{
		assert(fullPullPressed(true, position));
	}

	// Releasing still has to register, with a margin below the engage point.
	assert(!fullPullPressed(true, 0.96f));
	assert(!fullPullPressed(true, 0.5f));
	assert(!fullPullPressed(true, 0.f));

	// Engaging needs more than holding does, or there is no hysteresis at all.
	assert(!fullPullPressed(false, 0.98f) && fullPullPressed(true, 0.98f));

	// Garbage in the axis is not a press, and does not latch one that was held:
	// a reading that is not a number cannot be evidence the trigger is down.
	assert(!fullPullPressed(false, NAN));
	assert(!fullPullPressed(true, NAN));
	assert(!fullPullPressed(false, INFINITY));
	assert(!fullPullPressed(true, INFINITY));

	std::printf("PASS: full pull engages at the top of the range, holds through jitter, and releases cleanly\n");
	return 0;
}
