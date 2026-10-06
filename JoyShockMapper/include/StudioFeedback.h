#pragma once

#include <chrono>
#include <cstdint>

// Feedback JSM Studio asks the controller to play while the pad drives Studio's
// own window: a tick as focus moves, a click as A selects, something firmer as
// LB/RB step sections and LT/RT step pages.
//
// Studio sends one small datagram per effect to a loopback-only UDP port, the
// reverse of telemetry. It used to be that the only way into the mapper was
// the console injector, which starts a process per command -- far too slow
// and too noisy (every command echoes in the Debug console) for a tick on
// each D-pad press. The effect is played from the controller's own poll
// callback, under its lock, so it never races the input thread; it lands
// within one poll of arriving.
//
// Datagram: "FEEDBACK <effect> <intensity> <side> <rumbleMs> <rumble> [<target>]"
//   effect     HapticEffect ordinal (TICK 1, CLICK 2, ...)
//   intensity  0-100, the same dial as the other haptic settings
//   side       1 left, 2 right, 3 both
//   rumbleMs   pulse length on controllers without haptic actuators (they
//              get a short rumble instead); 0 plays nothing there
//   rumble     0-100 motor strength for that pulse
//   target     optional: 0 (or absent) plays where a binding would, 1 plays
//              where the grip sensors' own haptic would -- PULSE and TAP on
//              the grip actuators rather than the pads. Studio's grip haptic
//              preview sends 1, so a chosen effect feels as it will in play.
//              2 plus a seventh signed gainDb field selects captured Steam keyboard
//              Tick (1) / 400 us Pulse (8) packets; other effects are rejected.
namespace StudioFeedback
{

constexpr uint16_t kDefaultPort = 8976;

struct Request
{
	uint32_t sequence = 0;
	int effect = 0;
	float intensity = 0.f;
	int side = 3;
	int rumbleMs = 0;
	float rumble = 0.f;
	bool grips = false;
    bool steamKeyboard = false;
    int gainDb = 0;
	std::chrono::steady_clock::time_point received{};
};

// Listen on 127.0.0.1:port. Safe to call more than once; a port already in
// use (a second mapper) leaves this one without Studio feedback, nothing more.
void Start(uint16_t port = kDefaultPort);
void Stop();

// Cheap check for the poll callback: changes whenever a request arrives.
uint32_t Sequence();
// The most recent request. Only the latest is kept: two effects asked for
// within one poll are one effect.
Request Latest();

// For tests: parse one datagram. False if it is not a feedback request.
bool Parse(const char *text, Request &out);

} // namespace StudioFeedback
