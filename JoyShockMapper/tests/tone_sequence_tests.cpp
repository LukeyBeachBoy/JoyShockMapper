// The tone sequence file Studio writes from a trimmed MP3 (ToneSequence.h):
// the format, its clamps and limits, and the two-place lookup the mapper uses
// for a file a setting or PLAY_SOUND names (docs/plans/controller-sounds-library.md).
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "ToneSequence.h"

namespace
{
std::vector<Tone> parse(const std::string &text)
{
	std::istringstream in(text);
	return parseToneSequence(in);
}

void test_valid_file_in_order()
{
	const auto tones = parse("# JSM Evolved tone sequence v1\n"
	                         "# frequency_hz duration_ms gain_db\n"
	                         "588 80 0\n"
	                         "0 20 0\n"
	                         "699 80 -3\n");
	assert(tones.size() == 3);
	assert((tones[0] == Tone { 588, 80, 0 }));
	assert((tones[1] == Tone { 0, 20, 0 })); // a rest
	assert((tones[2] == Tone { 699, 80, -3 }));
	assert(tone_sequence::totalLengthMs(tones) == 180);
}

void test_comments_blank_lines_and_crlf()
{
	const auto tones = parse("\r\n   \r\n588 80 0   # the first note\r\n\r\n# 699 80 0\r\n699 80 -3\r\n");
	assert(tones.size() == 2);
	assert(tones[0].frequencyHz == 588 && tones[1].frequencyHz == 699);
}

void test_values_are_clamped_not_rejected()
{
	const auto tones = parse("5 5 -90\n9000 9000 40\n");
	assert(tones.size() == 2);
	assert((tones[0] == Tone { 40, 10, -60 }));
	assert((tones[1] == Tone { 2000, 2000, 0 }));
	// 0 Hz stays a rest rather than being pulled up to 40 Hz.
	assert(parse("0 100 0\n")[0].frequencyHz == 0);
}

void test_stops_at_the_note_limit()
{
	using namespace tone_sequence;
	std::string text;
	for (size_t i = 0; i < kMaxNotes + 50; ++i)
		text += "440 10 0\n";
	const auto tones = parse(text);
	assert(tones.size() == kMaxNotes);
	assert(totalLengthMs(tones) == int(kMaxNotes) * 10);
}

void test_stops_at_the_time_limit()
{
	using namespace tone_sequence;
	const int fitting = kMaxTotalMs / 2000;
	std::string text;
	for (int i = 0; i < fitting; ++i)
		text += "440 2000 0\n";
	// Exactly at the limit is still in.
	assert(parse(text).size() == size_t(fitting));
	// One more would pass the limit, so reading stops before it. A note that
	// fits after a stop is never reached either: order is playback order.
	const auto tones = parse(text + "440 1000 0\n440 10 0\n");
	assert(tones.size() == size_t(fitting));
	assert(totalLengthMs(tones) == kMaxTotalMs);
}

void test_empty_file_plays_nothing()
{
	assert(parse("").empty());
	assert(parse("# only a comment\n\n").empty());
}

void test_garbage_lines_are_skipped()
{
	const auto tones = parse("hello\n588 80\n588 80 0\n-5 80 0\nx 80 0\n699 80 -3 trailing words\n");
	assert(tones.size() == 2);
	assert(tones[0].frequencyHz == 588);
	assert(tones[1].frequencyHz == 699 && tones[1].gainDb == -3);
}

void test_playback_gain_is_clamped_to_the_report()
{
	using namespace tone_sequence;
	assert(playbackGainDb(0, 0) == 0);
	assert(playbackGainDb(-3, -12) == -15);
	assert(playbackGainDb(0, 6) == 6);
	assert(playbackGainDb(0, 30) == 6);
	assert(playbackGainDb(-60, -127) == -127);
}

// Where a sequence plays: the grip motors by default, one report for the
// pair, at the file's gain; the pads keep their 6 dB; BOTH is the two reports.
void test_routes_follow_the_actuator_choice()
{
	using namespace tone_sequence;
	const auto grips = toneRoutes(SoundActuators::GRIPS);
	assert(grips.size() == 1 && grips[0].side == kSideBothGrips && grips[0].gainOffsetDb == 0);
	const auto pads = toneRoutes(SoundActuators::PADS);
	assert(pads.size() == 1 && pads[0].side == kSideBothPads && pads[0].gainOffsetDb == kPadGainOffsetDb);
	const auto both = toneRoutes(SoundActuators::BOTH);
	assert(both.size() == 2 && both[0].side == kSideBothGrips && both[1].side == kSideBothPads);
	// An unparsed value plays where the firmware's own tunes play.
	assert(toneRoutes(SoundActuators::INVALID)[0].side == kSideBothGrips);
	// The side bytes are the report's channel-pair selectors, not bitmasks.
	assert(kSideBothPads == 2 && kSideBothGrips == 5);
}

void test_none_means_no_file()
{
	using namespace tone_sequence;
	assert(isNoToneFile(""));
	assert(isNoToneFile("   "));
	assert(isNoToneFile("NONE"));
	assert(isNoToneFile("none"));
	assert(isNoToneFile(" None \r"));
	assert(!isNoToneFile("NONE.txt"));
	assert(!isNoToneFile("sounds/snd-1-abcd/tones.txt"));
}

void test_load_and_lookup()
{
	namespace fs = std::filesystem;
	const fs::path folder = fs::temp_directory_path() / "jsm-tone-sequence-tests";
	fs::create_directories(folder / "sounds");
	{
		std::ofstream out(folder / "sounds" / "tones.txt");
		out << "588 80 0\n699 80 -3\n";
	}
	{
		std::ofstream out(folder / "sounds" / "empty.txt");
		out << "# nothing here\n";
	}
	const std::string fallback = folder.string() + "/";

	assert(loadToneSequence((folder / "sounds" / "tones.txt").string()).size() == 2);
	assert(loadToneSequence((folder / "missing.txt").string()).empty());

	// Relative to the fallback folder, as a configuration name resolves.
	const auto found = readToneFile("sounds/tones.txt", fallback);
	assert(found.error.empty() && found.tones.size() == 2);
	const auto missing = readToneFile("sounds/missing.txt", fallback);
	assert(missing.tones.empty() && missing.error == "cannot be opened");
	const auto empty = readToneFile("sounds/empty.txt", fallback);
	assert(empty.tones.empty() && empty.error == "holds no valid notes");
	assert(readToneFile("", fallback).error == "no file named");
	// An absolute path needs no fallback.
	assert(readToneFile((folder / "sounds" / "tones.txt").string(), "").error.empty());

	fs::remove_all(folder);
}
}

int main()
{
	test_valid_file_in_order();
	test_comments_blank_lines_and_crlf();
	test_values_are_clamped_not_rejected();
	test_stops_at_the_note_limit();
	test_stops_at_the_time_limit();
	test_empty_file_plays_nothing();
	test_garbage_lines_are_skipped();
	test_playback_gain_is_clamped_to_the_report();
	test_none_means_no_file();
	test_routes_follow_the_actuator_choice();
	test_load_and_lookup();
	std::cout << "tone_sequence_tests passed\n";
	return 0;
}
