#include "JoyShockMapper.h"

#include <algorithm>
#include <atomic>
#include <deque>

// "While released" chords (INVERTED_CHORD_OFFSET in JoyShockMapper.h): a
// modeshift written `!MISC5,W = X` applies while MISC5 is not held. The chord
// stack holds the id !MISC5 whenever MISC5 is up and the loaded configuration
// uses it, so the ordinary chord lookup finds it like any held button.
//
// Kept apart from DigitalButton so it can be tested on a bare stack.

namespace
{
// One bit per real button, written by the command thread as a configuration
// loads and read by every controller's poll.
std::atomic<uint64_t> invertedChordBits[2] = { 0, 0 };
static_assert(int(ButtonID::SIZE) <= 128, "one bit per button");

bool anyInvertedChord()
{
	return invertedChordBits[0].load(std::memory_order_relaxed) || invertedChordBits[1].load(std::memory_order_relaxed);
}
}

void useInvertedChord(ButtonID base)
{
	if (base <= ButtonID::NONE || base >= ButtonID::SIZE) return;
	invertedChordBits[int(base) / 64].fetch_or(uint64_t(1) << (int(base) % 64));
}

void clearInvertedChords()
{
	invertedChordBits[0] = 0;
	invertedChordBits[1] = 0;
}

bool invertedChordInUse(ButtonID base)
{
	if (base <= ButtonID::NONE || base >= ButtonID::SIZE) return false;
	return (invertedChordBits[int(base) / 64].load(std::memory_order_relaxed) >> (int(base) % 64)) & 1;
}

void syncInvertedChordStack(std::deque<ButtonID> &stack)
{
	// Drop the ones the loaded configuration no longer uses.
	for (auto chord = stack.begin(); chord != stack.end();)
	{
		if (isInvertedChord(*chord) && !invertedChordInUse(invertedChordBase(*chord))) chord = stack.erase(chord);
		else ++chord;
	}
	if (!anyInvertedChord()) return;
	// Hold the ones it does use from the moment they are defined, before their
	// button has ever been touched: released is where every button starts.
	for (int index = int(ButtonID::NONE) + 1; index < int(ButtonID::SIZE); ++index)
	{
		const ButtonID base = ButtonID(index);
		if (!invertedChordInUse(base)) continue;
		const bool held = std::find(stack.begin(), stack.end(), base) != stack.end();
		const auto present = std::find(stack.begin(), stack.end(), invertedChordOf(base));
		// Oldest, just ahead of NONE (always last): anything pressed since is
		// more recent and keeps its precedence.
		if (!held && present == stack.end()) stack.insert(!stack.empty() && stack.back() == ButtonID::NONE ? stack.end() - 1 : stack.end(), invertedChordOf(base));
		else if (held && present != stack.end()) stack.erase(present);
	}
}

void updateInvertedChord(std::deque<ButtonID> &stack, bool isPressed, ButtonID id)
{
	// A button used as "!X" swaps its released chord in and out opposite to
	// itself; letting go makes the released chord the most recent one.
	if (id <= ButtonID::NONE || id >= ButtonID::SIZE || !invertedChordInUse(id)) return;
	const auto inverted = std::find(stack.begin(), stack.end(), invertedChordOf(id));
	if (isPressed && inverted != stack.end()) stack.erase(inverted);
	else if (!isPressed && inverted == stack.end()) stack.push_front(invertedChordOf(id));
}
