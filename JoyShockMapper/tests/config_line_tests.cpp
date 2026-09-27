// The split every configuration line goes through (ConfigLine.h). Standalone,
// like inverted_chord_tests.cpp: it needs no other source file.
//
// The case that motivated it: `!MISC6,S = X_UP`, a modeshift that applies
// while the left grip is released, was reported as an unknown command because
// the chord group of the pattern did not admit a leading '!'.
#include <cassert>
#include <iostream>

#include "ConfigLine.h"

namespace
{
ConfigLine split(const char *text, bool allowDiagonal = true)
{
	ConfigLine parts;
	const bool matched = splitConfigLine(text, parts, allowDiagonal);
	assert(matched);
	return parts;
}

void test_released_chord_modeshift()
{
	const auto parts = split("!MISC6,S = X_UP");
	assert(parts.combo == "!MISC6");
	assert(parts.op == ',');
	assert(parts.name == "S");
	assert(parts.arguments == "= X_UP");
	assert(parts.label.empty());
	// The same line as isCommandValid sees it.
	const auto strict = split("!MISC6,S = X_UP", false);
	assert(strict.combo == "!MISC6" && strict.op == ',' && strict.name == "S");
}

void test_released_chord_with_label_and_sign_buttons()
{
	const auto parts = split("  !-  ,  +  =  X_UP   # Heal ");
	assert(parts.combo == "!-");
	assert(parts.op == ',');
	assert(parts.name == "+");
	assert(parts.label == "Heal ");
}

void test_plain_lines_still_split_the_same()
{
	auto parts = split("S = X_A");
	assert(parts.combo.empty() && parts.op == '\0' && parts.name == "S" && parts.arguments == "= X_A");
	parts = split("MISC5,W = X # Reload");
	assert(parts.combo == "MISC5" && parts.op == ',' && parts.name == "W" && parts.label == "Reload");
	parts = split("L+R = ESC");
	assert(parts.combo == "L" && parts.op == '+' && parts.name == "R");
	parts = split("UP*LEFT = Q");
	assert(parts.combo == "UP" && parts.op == '*' && parts.name == "LEFT");
	parts = split("RESET_MAPPINGS");
	assert(parts.name == "RESET_MAPPINGS" && parts.arguments.empty());
	parts = split("");
	assert(parts.name.empty() && parts.combo.empty());
}

void test_released_is_only_a_chord()
{
	// "!S = X" has no chord, so the name is "!S": no command is registered
	// under it and the registry reports it, as it did before.
	const auto parts = split("!S = X");
	assert(parts.combo.empty() && parts.name == "!S");
	// A '!' on the partner of a sim press leaves the name empty, as any
	// stray character there did before, so the registry reports the line.
	const auto partner = split("L+!R = ESC");
	assert(partner.combo == "L" && partner.op == '+' && partner.name.empty());
}
}

int main()
{
	test_released_chord_modeshift();
	test_released_chord_with_label_and_sign_buttons();
	test_plain_lines_still_split_the_same();
	test_released_is_only_a_chord();
	std::cout << "config_line_tests: all passed\n";
	return 0;
}
