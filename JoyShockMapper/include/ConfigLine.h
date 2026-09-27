#pragma once

#include <regex>
#include <string>

// One line of configuration, broken into the parts the registry dispatches
// on: `[chord<op>]name [arguments] [# label]`. Header-only so the tests can
// split lines without linking the registry.
struct ConfigLine
{
	std::string combo;     // the chord, sim-press partner or diagonal partner, if any
	char op = '\0';        // ',' modeshift / chord, '+' simultaneous press, '*' diagonal
	std::string name;      // the command or button the line is about
	std::string arguments; // everything after the name up to the comment
	std::string label;     // the comment's text, without its '#'
};

// The chord may be a "while released" chord, written "!X" (InvertedChords.cpp).
// `!` is neither a word character nor a sign, so the pattern names it: without
// that, `!MISC6,S = X_UP` did not match at all and was reported as an unknown
// command while every stage after the split already understood it.
// Buttons + and - are not word characters either, hence the leading sign.
inline bool splitConfigLine(const std::string &line, ConfigLine &out, bool allowDiagonal = true)
{
	static const std::regex withDiagonal(R"(^\s*(!?[+-]?\w*)\s*([,+\*]\s*([+-]?\w*))?\s*([^#\n]*)(#\s*(.*))?$)");
	static const std::regex withoutDiagonal(R"(^\s*(!?[+-]?\w*)\s*([,+]\s*([+-]?\w*))?\s*([^#\n]*)(#\s*(.*))?$)");
	std::smatch results;
	if (!std::regex_match(line, results, allowDiagonal ? withDiagonal : withoutDiagonal))
		return false;
	out = ConfigLine{};
	if (results[2].length() > 0)
	{
		out.combo = results[1];
		out.op = results[2].str()[0];
		out.name = results[3];
	}
	else
	{
		out.name = results[1];
	}
	out.arguments = results[4];
	out.label = results[6];
	return true;
}
