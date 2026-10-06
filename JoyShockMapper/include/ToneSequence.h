#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <istream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// A tune for the Steam Controller 2026's pad actuators, as Studio writes it
// from a trimmed MP3 (docs/plans/controller-sounds-library.md, "Tone sequence
// file"). The controller has no speaker: each note is one LFO tone report
// (frequency, duration, gain), so a file is a single-voice melody. This header
// only reads the format; playing it is the wrapper's job (PlayToneSequence).
//
//   # comment
//   frequency_hz duration_ms gain_db     one note per line, 0 Hz = rest
//
// Values outside the ranges below are clamped rather than rejected, and reading
// stops at the note or time limit (20000 notes, ten minutes) instead of
// failing: a file Studio wrote by hand or an older Studio wrote a little
// differently still plays.
struct Tone
{
	uint16_t frequencyHz; // 0 = rest
	uint16_t durationMs;
	int8_t gainDb; // relative; the player adds the configured gain
};

inline bool operator==(const Tone &lhs, const Tone &rhs)
{
	return lhs.frequencyHz == rhs.frequencyHz && lhs.durationMs == rhs.durationMs && lhs.gainDb == rhs.gainDb;
}

// Which of the controller's four actuators a tone sequence plays on. The
// firmware's own tunes -- the power-on jingle, Steam's identify ping -- are
// haptic scripts whose steps are tone requests aimed at channels 2 and 3, the
// two motors behind the grips (docs/triton-firmware-customisation.md, section
// 2). GRIPS (the default) is therefore what gives a custom sound the same voice
// as the built-in ones. PADS is the trackpads' actuators (channels 0 and 1);
// BOTH plays on all four.
enum class SoundActuators
{
	GRIPS,
	PADS,
	BOTH,
	INVALID,
};

namespace tone_sequence
{
// Generous: a whole song can be a sound if someone wants one. The limits
// exist so a runaway file cannot be read forever, not to shape the feature.
constexpr size_t kMaxNotes = 20000;
constexpr int kMaxTotalMs = 600000;
constexpr int kMinFrequencyHz = 40;
constexpr int kMaxFrequencyHz = 2000;
constexpr int kMinDurationMs = 10;
constexpr int kMaxDurationMs = 2000;
constexpr int kMinGainDb = -60;
constexpr int kMaxGainDb = 0;
// What the LFO tone report's gain byte is allowed to carry once the file's
// relative gain and the configured gain are added.
constexpr int kMinPlayGainDb = -127;
constexpr int kMaxPlayGainDb = 6;

// The LFO tone report's side byte selects channels rather than naming one:
// 0 / 1 = left / right pad, 2 = both pads, 3 / 4 = left / right grip motor,
// 5 = both grip motors. One report on 2 or 5 starts the pair together, where
// two single-channel reports start a few milliseconds apart.
constexpr uint8_t kSideBothPads = 2;
constexpr uint8_t kSideBothGrips = 5;

// Where one note goes and how its gain is adjusted on the way. A firmware
// script plays each tone request at the gain it names, so the grips take the
// file's gain plus SOUND_GAIN unchanged; the pads' actuators are noticeably
// quieter for the same request and keep the 6 dB that was tuned for them.
struct ToneRoute
{
	uint8_t side;
	int gainOffsetDb;
};
constexpr int kPadGainOffsetDb = 6;

inline std::vector<ToneRoute> toneRoutes(SoundActuators actuators)
{
	switch (actuators)
	{
	case SoundActuators::PADS:
		return { { kSideBothPads, kPadGainOffsetDb } };
	case SoundActuators::BOTH:
		return { { kSideBothGrips, 0 }, { kSideBothPads, kPadGainOffsetDb } };
	default:
		return { { kSideBothGrips, 0 } };
	}
}

// The gain byte for one note: the file's relative gain plus SOUND_GAIN (or the
// gain a PLAY_SOUND call names), kept in the report's range.
inline int playbackGainDb(int noteGainDb, int gainDb)
{
	return std::clamp(noteGainDb + gainDb, kMinPlayGainDb, kMaxPlayGainDb);
}

inline int totalLengthMs(const std::vector<Tone> &tones)
{
	int total = 0;
	for (const auto &tone : tones)
		total += tone.durationMs;
	return total;
}

inline std::string trimmed(std::string_view text)
{
	const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
	const auto first = std::find_if(text.begin(), text.end(), notSpace);
	const auto last = std::find_if(text.rbegin(), text.rend(), notSpace).base();
	return first < last ? std::string(first, last) : std::string();
}

// CONNECT_SOUND_FILE / SHUTDOWN_SOUND_FILE: Studio writes NONE rather than
// leaving the line out, so a cleared choice reaches a running mapper. Any case.
inline bool isNoToneFile(std::string_view value)
{
	const std::string name = trimmed(value);
	if (name.empty())
		return true;
	if (name.size() != 4)
		return false;
	return std::equal(name.begin(), name.end(), "NONE", [](unsigned char a, unsigned char b) { return std::toupper(a) == b; });
}
}

// Reads notes until the stream ends or a limit is reached. Lines that do not
// hold three integers are skipped, so a stray word costs one note, not the tune.
inline std::vector<Tone> parseToneSequence(std::istream &in)
{
	using namespace tone_sequence;
	std::vector<Tone> tones;
	int totalMs = 0;
	std::string line;
	while (tones.size() < kMaxNotes && std::getline(in, line))
	{
		if (const auto comment = line.find('#'); comment != std::string::npos)
			line.erase(comment);
		std::istringstream words(line);
		long frequency = 0, duration = 0, gain = 0;
		if (!(words >> frequency >> duration >> gain) || frequency < 0)
			continue;
		if (frequency > 0)
			frequency = std::clamp(frequency, long(kMinFrequencyHz), long(kMaxFrequencyHz));
		duration = std::clamp(duration, long(kMinDurationMs), long(kMaxDurationMs));
		gain = std::clamp(gain, long(kMinGainDb), long(kMaxGainDb));
		// The time limit is a stop, not a trim: a note cut to fit would end the
		// tune on a blip.
		if (totalMs + duration > kMaxTotalMs)
			break;
		totalMs += int(duration);
		tones.push_back(Tone { uint16_t(frequency), uint16_t(duration), int8_t(gain) });
	}
	return tones;
}

inline std::vector<Tone> loadToneSequence(const std::string &path)
{
	std::ifstream file(path);
	if (!file.is_open())
		return {};
	return parseToneSequence(file);
}

// A tone file as a setting or PLAY_SOUND names it: tried as given (relative to
// the working directory, which JSM_DIRECTORY sets), then under the fallback
// folder -- the two places CmdRegistry::loadConfigFile looks for a
// configuration. `error` is empty when there is something to play, otherwise
// one phrase for the log line, since "not found" and "empty" call for
// different fixes.
struct ToneFile
{
	std::vector<Tone> tones;
	std::string error;
};

inline ToneFile readToneFile(const std::string &path, const std::string &fallbackFolder)
{
	ToneFile result;
	if (path.empty())
	{
		result.error = "no file named";
		return result;
	}
	std::ifstream file(path);
	if (!file.is_open() && !fallbackFolder.empty())
		file.open(fallbackFolder + path);
	if (!file.is_open())
	{
		result.error = "cannot be opened";
		return result;
	}
	result.tones = parseToneSequence(file);
	if (result.tones.empty())
		result.error = "holds no valid notes";
	return result;
}
