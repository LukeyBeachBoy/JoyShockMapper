// Guards the datagram JSM Studio sends to play a haptic (StudioFeedback.h).
//
// Studio's grip haptic preview has to feel like the grip sensors' own pulse:
// PULSE and TAP at the grip actuators, not the pads where a binding plays
// them. The datagram gained an optional sixth field for that; a Studio that
// predates it sends five, and must still parse with the old meaning.
//
// Build & run (from the repo root):
//   cl /std:c++20 /EHsc /nologo /I JoyShockMapper/JoyShockMapper/include \
//      JoyShockMapper/tests/studio_feedback_harness.cpp JoyShockMapper/JoyShockMapper/src/StudioFeedback.cpp \
//      && studio_feedback_harness.exe
#include "StudioFeedback.h"

#include <cassert>
#include <iostream>

int main()
{
	StudioFeedback::Request request;

	// Five fields: as before, and not aimed at the grips.
	assert(StudioFeedback::Parse("FEEDBACK 2 65 2 30 32", request));
	assert(request.effect == 2 && request.intensity == 65.f && request.side == 2);
	assert(request.rumbleMs == 30 && request.rumble == 32.f);
	assert(!request.grips);

	// Sixth field 1: the grips. PULSE (8) and TAP (9) are playable effects.
	assert(StudioFeedback::Parse("FEEDBACK 8 66 3 0 0 1", request));
	assert(request.effect == 8 && request.side == 3 && request.grips);
	assert(StudioFeedback::Parse("FEEDBACK 9 40 1 0 0 1", request));
	assert(request.effect == 9 && request.grips);

	// Sixth field 0: the pads, as a binding plays it.
	assert(StudioFeedback::Parse("FEEDBACK 8 66 3 0 0 0", request));
	assert(!request.grips);

	// Captured Steam keyboard mode: signed gain, Tick and exact short Pulse only.
    assert(StudioFeedback::Parse("FEEDBACK 1 35 1 12 8 2 5", request));
    assert(request.steamKeyboard && request.gainDb == 5 && !request.grips);
    assert(StudioFeedback::Parse("FEEDBACK 8 35 2 12 8 2 0", request));
    assert(request.steamKeyboard && request.effect == 8);
    assert(!StudioFeedback::Parse("FEEDBACK 2 35 1 12 8 2 5", request));
    assert(!StudioFeedback::Parse("FEEDBACK 1 35 1 12 8 2 128", request));
    assert(StudioFeedback::Parse("FEEDBACK 1 35 1 12 8", request));
    assert(!request.steamKeyboard);

    // Still rejected: OFF, a fourth side, anything else.
	assert(!StudioFeedback::Parse("FEEDBACK 0 50 1 0 0 1", request));
	assert(!StudioFeedback::Parse("FEEDBACK 2 50 4 0 0 1", request));
	assert(!StudioFeedback::Parse("FEEDBACK 2 50", request));
	assert(!StudioFeedback::Parse("RUMBLE 2 50 1 0 0", request));

	std::cout << "studio feedback datagrams: ok\n";
	return 0;
}
