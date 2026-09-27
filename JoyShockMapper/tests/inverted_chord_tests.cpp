// "While released" chords (InvertedChords.cpp): a modeshift `!MISC5,W = X`
// applies while MISC5 is not held. Standalone, like the other assert tests:
// build with src/InvertedChords.cpp and src/operators.cpp.
#include <cassert>
#include <deque>
#include <iostream>
#include <sstream>

#include "JoyShockMapper.h"

namespace
{
using Stack = std::deque<ButtonID>;

bool has(const Stack &stack, ButtonID id)
{
	return std::find(stack.begin(), stack.end(), id) != stack.end();
}

// A fresh controller context: NONE only, always last.
Stack fresh()
{
	return Stack{ ButtonID::NONE };
}

// What DigitalButton does on a press or release.
void press(Stack &stack, ButtonID id, bool down)
{
	updateInvertedChord(stack, down, id);
	const auto found = std::find(stack.begin(), stack.end(), id);
	if (down && found == stack.end()) stack.push_front(id);
	if (!down && found != stack.end()) stack.erase(found);
}

void test_names()
{
	auto parse = [](const char *text) { std::stringstream in(text); ButtonID id; in >> id; return id; };
	auto print = [](ButtonID id) { std::stringstream out; out << id; return out.str(); };
	assert(parse("!MISC5") == invertedChordOf(ButtonID::MISC5));
	assert(print(parse("!MISC5")) == "!MISC5");
	assert(parse("!-") == invertedChordOf(ButtonID::MINUS));
	// Touch and menu cells exist only while touched; they cannot be "released".
	assert(parse("!T1") == ButtonID::INVALID);
	assert(parse("!NOPE") == ButtonID::INVALID);
	assert(!isInvertedChord(ButtonID::RM25));
}

void test_released_from_the_start()
{
	clearInvertedChords();
	Stack stack = fresh();
	useInvertedChord(ButtonID::MISC5);
	syncInvertedChordStack(stack);
	// Never touched is released: the chord holds at once, ahead of NONE.
	assert(stack.size() == 2 && stack[0] == invertedChordOf(ButtonID::MISC5) && stack[1] == ButtonID::NONE);
	syncInvertedChordStack(stack);
	assert(stack.size() == 2);
}

void test_grip_press_and_release()
{
	clearInvertedChords();
	Stack stack = fresh();
	useInvertedChord(ButtonID::MISC5);
	syncInvertedChordStack(stack);
	press(stack, ButtonID::MISC5, true);
	assert(!has(stack, invertedChordOf(ButtonID::MISC5)) && has(stack, ButtonID::MISC5));
	press(stack, ButtonID::MISC5, false);
	// Letting go brings it back as the most recent chord.
	assert(stack.front() == invertedChordOf(ButtonID::MISC5) && !has(stack, ButtonID::MISC5));
	// Another button's chord pressed after it takes precedence.
	press(stack, ButtonID::L, true);
	assert(stack.front() == ButtonID::L);
	// Only buttons in use invert.
	press(stack, ButtonID::MISC6, false);
	assert(!has(stack, invertedChordOf(ButtonID::MISC6)));
	assert(stack.back() == ButtonID::NONE);
}

void test_held_while_loaded()
{
	clearInvertedChords();
	Stack stack = fresh();
	press(stack, ButtonID::MISC5, true); // gripping as the configuration loads
	useInvertedChord(ButtonID::MISC5);
	syncInvertedChordStack(stack);
	assert(!has(stack, invertedChordOf(ButtonID::MISC5)));
	press(stack, ButtonID::MISC5, false);
	assert(stack.front() == invertedChordOf(ButtonID::MISC5));
}

void test_next_configuration_drops_them()
{
	clearInvertedChords();
	Stack stack = fresh();
	useInvertedChord(ButtonID::MISC5);
	syncInvertedChordStack(stack);
	clearInvertedChords(); // RESET_MAPPINGS
	syncInvertedChordStack(stack);
	assert(stack.size() == 1 && stack[0] == ButtonID::NONE);
	// And a release after that pushes nothing.
	press(stack, ButtonID::MISC5, true);
	press(stack, ButtonID::MISC5, false);
	assert(stack.size() == 1);
}
}

int main()
{
	test_names();
	test_released_from_the_start();
	test_grip_press_and_release();
	test_held_while_loaded();
	test_next_configuration_drops_them();
	std::cout << "inverted chord tests passed\n";
	return 0;
}
