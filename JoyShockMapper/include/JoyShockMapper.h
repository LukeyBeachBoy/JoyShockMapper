#pragma once

#ifndef MAGIC_ENUM_RANGE_MIN
#define MAGIC_ENUM_RANGE_MIN (-1)
#endif
#ifndef MAGIC_ENUM_RANGE_MAX
#define MAGIC_ENUM_RANGE_MAX 512
#endif

#include "magic_enum.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <functional>
#include <sstream>
#include <string>
#include <memory>
#include <array>
#include <deque>

// This header file is meant to be included among all core JSM source files
// And as such it should contain only constants, types and functions related to them

using namespace std; // simplify all std calls

// All enums should have an INVALID field for proper use with templated << and >> operators

enum class ButtonID
{
	INVALID = -2, // Represents an error in user input
	NONE,         //  Represents no button when explicitely stated by the user. Not to be confused with NO_HOLD_MAPPED which is no action bound.
	UP,           // = 0 as the first index
	DOWN,
	LEFT,
	RIGHT,
	L,
	ZL,
	MINUS,
	E,
	S,
	N,
	W,
	R,
	ZR,
	PLUS,
	HOME,
	LSL,
	LSR,
	RSL,
	RSR,
	L3,
	R3,
	LEAN_LEFT,
	LEAN_RIGHT,
	MIC,
	LUP,
	LDOWN,
	LLEFT,
	LRIGHT,
	LRING,
	RUP,
	RDOWN,
	RLEFT,
	RRIGHT,
	RRING,
	MUP,
	MDOWN,
	MLEFT,
	MRIGHT,
	MRING,
	TOUCH,   // Touch anywhere on the touchpad
	LTOUCH,  // Left stick capacitive touch
	RTOUCH,  // Right stick capacitive touch
	LMINI,   // Left mini shoulder button
	RMINI,   // Right mini shoulder button
	MISC1,   // Additional button that varies by controller
	MISC2,   // Additional button that varies by controller
	MISC3,   // Additional button that varies by controller
	MISC4,   // Additional button that varies by controller
	MISC5,   // Additional button that varies by controller
	MISC6,   // Additional button that varies by controller
	ZLF,     // = FIRST_ANALOG_TRIGGER
	CAPTURE, // Full press of touchpad touch + press
	// insert more analog triggers here
	ZRF,  // =  LAST_ANALOG_TRIGGER

	TUP,
	TDOWN,
	TLEFT,
	TRIGHT,
	TRING,

	SIZE, // Not a button

	// Virtual buttons configured on the touchpad. The number of buttons vary dynamically, but they each need a different ID
	T1,  // FIRST_TOUCH_BUTTON
	T2,
	T3,
	T4,
	T5,
	T6,
	T7,
	T8,
	T9,
	T10,
	T11,
	T12,
	T13,
	T14,
	T15,
	T16,
	T17,
	T18,
	T19,
	T20,
	T21,
	T22,
	T23,
	T24,
	T25,
	// Add as necessary...

	// Per-pad grid buttons, for controllers that have two touchpads (Steam
	// Controller 2026). T1..T25 above stay exactly as they were: they are what
	// every single-pad controller uses, and both pads of a two-pad controller
	// used to share them -- which meant a cell on one pad fired the other pad's
	// binding, and a per-pad grid larger than the shared one indexed past the
	// end of the button array.
	LT1, // FIRST_LEFT_TOUCH_BUTTON
	LT2,
	LT3,
	LT4,
	LT5,
	LT6,
	LT7,
	LT8,
	LT9,
	LT10,
	LT11,
	LT12,
	LT13,
	LT14,
	LT15,
	LT16,
	LT17,
	LT18,
	LT19,
	LT20,
	LT21,
	LT22,
	LT23,
	LT24,
	LT25,
	RT1, // FIRST_RIGHT_TOUCH_BUTTON
	RT2,
	RT3,
	RT4,
	RT5,
	RT6,
	RT7,
	RT8,
	RT9,
	RT10,
	RT11,
	RT12,
	RT13,
	RT14,
	RT15,
	RT16,
	RT17,
	RT18,
	RT19,
	RT20,
	RT21,
	RT22,
	RT23,
	RT24,
	RT25,

	// Radial-menu segments for a stick in RADIAL_MENU mode. Appended AFTER
	// RT25 deliberately: T1/LT1/RT1 are addressed by offset from their own
	// first element and SIZE sits above T1, so nothing already numbered
	// moves. Inserting these anywhere earlier would silently retarget every
	// existing grid binding rather than failing to build.
	LM1, // FIRST_LEFT_STICK_MENU_BUTTON
	LM2,
	LM3,
	LM4,
	LM5,
	LM6,
	LM7,
	LM8,
	LM9,
	LM10,
	LM11,
	LM12,
	LM13,
	LM14,
	LM15,
	LM16,
	LM17,
	LM18,
	LM19,
	LM20,
	LM21,
	LM22,
	LM23,
	LM24,
	LM25,
	RM1, // FIRST_RIGHT_STICK_MENU_BUTTON
	RM2,
	RM3,
	RM4,
	RM5,
	RM6,
	RM7,
	RM8,
	RM9,
	RM10,
	RM11,
	RM12,
	RM13,
	RM14,
	RM15,
	RM16,
	RM17,
	RM18,
	RM19,
	RM20,
	RM21,
	RM22,
	RM23,
	RM24,
	RM25,
};

// help strings for each button
extern const map<ButtonID, string> buttonHelpMap;

enum class SettingID
{
	INVALID = -1,
	ZERO = 0,    // Represents an error in user input
	MIN_GYRO_SENS,  // Legacy but int value not used
	MAX_GYRO_SENS,
	MIN_GYRO_THRESHOLD,
		MAX_GYRO_THRESHOLD,
		ACCEL_CURVE,
		ACCEL_NATURAL_VHALF,
		ACCEL_POWER_VREF,
		ACCEL_POWER_EXPONENT,
		ACCEL_SIGMOID_MID,
		ACCEL_SIGMOID_WIDTH,
		ACCEL_JUMP_TAU,
		STICK_POWER,
		STICK_SENS,
		REAL_WORLD_CALIBRATION,
		VIRTUAL_STICK_CALIBRATION,
		IN_GAME_SENS,
		TRIGGER_THRESHOLD,
		TRIGGER_HYSTERESIS,
		RESET_MAPPINGS,
		IGNORE_GYRO_DEVICES,
		NO_GYRO_BUTTON,
		LEFT_STICK_MODE,
		RIGHT_STICK_MODE,
		MOTION_STICK_MODE,
		GYRO_OFF,
		GYRO_ON,
		LEFT_STICK_AXIS,
		RIGHT_STICK_AXIS,
		MOTION_STICK_AXIS,
		TOUCH_STICK_AXIS,
		STICK_AXIS_X, // Legacy command
		STICK_AXIS_Y, // Legacy command
		GYRO_AXIS_X,
		GYRO_AXIS_Y,
		RECONNECT_CONTROLLERS,
		COUNTER_OS_MOUSE_SPEED,
		IGNORE_OS_MOUSE_SPEED,
		JOYCON_GYRO_MASK,
		JOYCON_MOTION_MASK,
		GYRO_SENS,
		FLICK_TIME,
		GYRO_SMOOTH_THRESHOLD,
		GYRO_SMOOTH_TIME,
		GYRO_SMOOTHING_DECAY,
		GYRO_CUTOFF_SPEED,
		GYRO_CUTOFF_RECOVERY,
		ONE_EURO_MIN_CUTOFF,
		ONE_EURO_SPEED_COEFF,
		GYRO_ANGLE_SNAP,
		GYRO_ANGLE_SNAP_EASE,
		DECEL_BRAKE_STRENGTH,
		DECEL_BRAKE_THRESHOLD,
		STICK_ACCELERATION_RATE,
		STICK_ACCELERATION_CAP,
	LEFT_STICK_DEADZONE_INNER,
	LEFT_STICK_DEADZONE_OUTER,
	STICK_DEADZONE_INNER,
	STICK_DEADZONE_OUTER,
	CALCULATE_REAL_WORLD_CALIBRATION,
	FINISH_GYRO_CALIBRATION,
	RESTART_GYRO_CALIBRATION,
	MOUSE_X_FROM_GYRO_AXIS,
	MOUSE_Y_FROM_GYRO_AXIS,
	ZR_MODE,
	ZL_MODE,
	AUTOLOAD,
	AUTOCONNECT,
	HELP,
	WHITELIST_SHOW,
	WHITELIST_ADD,
	WHITELIST_REMOVE,
	LEFT_RING_MODE,
	RIGHT_RING_MODE,
	MOTION_RING_MODE,
	MOUSE_RING_RADIUS,
	SCREEN_RESOLUTION_X,
	SCREEN_RESOLUTION_Y,
	ROTATE_SMOOTH_OVERRIDE,
	FLICK_SNAP_MODE,
	FLICK_SNAP_STRENGTH,
	MOTION_DEADZONE_INNER,
	MOTION_DEADZONE_OUTER,
	ANGLE_TO_AXIS_DEADZONE_INNER,
	ANGLE_TO_AXIS_DEADZONE_OUTER,
	RIGHT_STICK_DEADZONE_INNER,
	RIGHT_STICK_DEADZONE_OUTER,
	LEAN_THRESHOLD,
	FLICK_DEADZONE_ANGLE,
	FLICK_TIME_EXPONENT,
	CONTROLLER_ORIENTATION,
	GYRO_SPACE,
	ROLL_CONTRIBUTION,
	TRACKBALL_DECAY,
	TRIGGER_SKIP_DELAY,
	TURBO_PERIOD,
	HOLD_PRESS_TIME,
	TICK_TIME,
	SIM_PRESS_WINDOW, // Unchorded setting
	DBL_PRESS_WINDOW, // Unchorded setting
	GRID_SIZE,        // Unchorded setting
	GRID_SHAPE,
	GRID_DEADZONE,
	TOUCHPAD_MODE,
	TOUCH_STICK_MODE,
	TOUCH_STICK_RADIUS,
	TOUCH_DEADZONE_INNER,
	TOUCH_RING_MODE,
	TOUCHPAD_SENS,
	TOUCHPAD_ACCELERATION,
	// Speed-based acceleration curve for the trackpad mouse, mirroring the gyro's
	// ACCEL_* family but measured in pixels per second and producing a gain.
	TOUCHPAD_ACCEL_CURVE,
	TOUCHPAD_ACCEL_MIN_SPEED,
	TOUCHPAD_ACCEL_MAX_SPEED,
	TOUCHPAD_ACCEL_MIN_GAIN,
	TOUCHPAD_ACCEL_MAX_GAIN,
	TOUCHPAD_ACCEL_NATURAL_VHALF,
	TOUCHPAD_ACCEL_POWER_VREF,
	TOUCHPAD_ACCEL_POWER_EXPONENT,
	TOUCHPAD_ACCEL_SIGMOID_MID,
	TOUCHPAD_ACCEL_SIGMOID_WIDTH,
	TOUCHPAD_ACCEL_JUMP_TAU,
	// Lets one input borrow the other's curve shape (see AccelCurveLink).
	ACCEL_CURVE_LINK,
	LIGHT_BAR,
	SCROLL_SENS,
	VIRTUAL_CONTROLLER,
	RUMBLE,
	TOUCHPAD_DUAL_STAGE_MODE,
	// When ON, a grid region only activates once the pad is actually clicked
	// (not merely touched) while the finger sits over it. See touchCallback.
	TOUCHPAD_GRID_REQUIRES_CLICK,
	CLEAR,
	ADAPTIVE_TRIGGER,
	LEFT_TRIGGER_EFFECT,
	RIGHT_TRIGGER_EFFECT,
	LEFT_TRIGGER_OFFSET,
	LEFT_TRIGGER_RANGE,
	RIGHT_TRIGGER_OFFSET,
	RIGHT_TRIGGER_RANGE,
	LEFT_STICK_UNDEADZONE_INNER,
	LEFT_STICK_UNDEADZONE_OUTER,
	LEFT_STICK_UNPOWER,
	RIGHT_STICK_UNDEADZONE_INNER,
	RIGHT_STICK_UNDEADZONE_OUTER,
	RIGHT_STICK_UNPOWER,
	LEFT_STICK_VIRTUAL_SCALE,
	RIGHT_STICK_VIRTUAL_SCALE,
	WIND_STICK_RANGE,
	WIND_STICK_POWER,
	UNWIND_RATE,
	GYRO_OUTPUT,
	FLICK_STICK_OUTPUT,
	HIDE_MINIMIZED,
	AUTO_CALIBRATE_GYRO,
	JSM_DIRECTORY,
	RETURN_DEADZONE_IS_ACTIVE,
	EDGE_PUSH_IS_ACTIVE,
	STICKLIKE_FACTOR,
	MOUSELIKE_FACTOR,
	RETURN_DEADZONE_ANGLE,
	RETURN_DEADZONE_ANGLE_CUTOFF,
	TELEMETRY_ENABLED,
	TELEMETRY_PORT,
	// Per-pad touchpad settings for controllers with two physical pads (Steam Controller 2026)
	LEFT_TOUCHPAD_MODE,
	RIGHT_TOUCHPAD_MODE,
	LEFT_GRID_SIZE,
	RIGHT_GRID_SIZE,
	LEFT_GRID_SHAPE,
	RIGHT_GRID_SHAPE,
	LEFT_GRID_DEADZONE,
	RIGHT_GRID_DEADZONE,
	// A stick's radial menu: how many segments, and how far the stick has to be
	// pushed before one is selected. Separate from the stick's own deadzone,
	// which decides when the stick counts as moved at all.
	LEFT_STICK_MENU_SIZE,
	RIGHT_STICK_MENU_SIZE,
	LEFT_STICK_MENU_DEADZONE,
	RIGHT_STICK_MENU_DEADZONE,
	LEFT_TOUCHPAD_SENS,
	RIGHT_TOUCHPAD_SENS,
	LEFT_TOUCHPAD_DUAL_STAGE_MODE,
	RIGHT_TOUCHPAD_DUAL_STAGE_MODE,
	LEFT_GRID_REQUIRES_CLICK,
	RIGHT_GRID_REQUIRES_CLICK,
	LEFT_TOUCH_STICK_MODE,
	RIGHT_TOUCH_STICK_MODE,
	LEFT_TOUCH_STICK_RADIUS,
	RIGHT_TOUCH_STICK_RADIUS,
	LEFT_TOUCH_DEADZONE_INNER,
	RIGHT_TOUCH_DEADZONE_INNER,
	LEFT_TOUCH_RING_MODE,
	RIGHT_TOUCH_RING_MODE,
	LEFT_TOUCH_STICK_AXIS,
	RIGHT_TOUCH_STICK_AXIS,
	// Touchpad mouse output shaping. The One Euro filter runs on pad POSITION and
	// adapts its cutoff to finger speed: heavy smoothing while panning slowly,
	// almost none during a flick.
	TOUCHPAD_MIN_CUTOFF,
	TOUCHPAD_SPEED_COEFF,
	// How fast the filter's own speed *estimate* reacts to a sudden change (Hz).
	// TOUCHPAD_SPEED_COEFF only matters once the filter has noticed you sped up;
	// at the gyro-inherited default of 1Hz that noticing takes ~100-150ms, which
	// reads as the whole filter lagging even on a fast flick. Raising this lets
	// a flick escape TOUCHPAD_MIN_CUTOFF's smoothing almost immediately without
	// changing how still it holds a resting or slow-panning finger.
	TOUCHPAD_D_CUTOFF,
	// Post-liftoff mouse coast, separate from the legacy TRACKBALL_DECAY (which is
	// shared with an unrelated stick-based trackball feature and must not change
	// behaviour for it). 0 disables coasting entirely: the cursor stops the instant
	// contact ends, matching Steam Input's Mouse touch style. This is the default.
	TOUCHPAD_TRACKBALL_DECAY,
	// Launch speed a swipe must still have at liftoff to start a coast at all.
	TOUCHPAD_TRACKBALL_MIN_VELOCITY,
	// Grip sensors (Steam Controller 2026): the capacitive strips inside the
	// handles, which sense how near your hands are rather than how hard you
	// squeeze. The controller decides the bit, so the two knobs live in its
	// firmware -- the same pair Steam Input's Grip Sensor Calibration drives:
	//
	//   GRIP_SENSOR_RANGE   how near a hand must be before the sensor trips
	//   GRIP_FLICKER_GUARD  extra travel needed to trip it back off, so a hand
	//                       resting at the edge of range can't chatter
	//
	// One pair, not one per side: the firmware carries a single capacitive
	// threshold pair, which is also why Steam Input shows a single pair.
	// -1 leaves the firmware's own value alone, which is the default.
	GRIP_SENSOR_RANGE,
	GRIP_FLICKER_GUARD,
	// Haptic pulse fired by the grip actuators when a grip sensor trips. 0 = off.
	GRIP_HAPTIC_INTENSITY,
	LEFT_GRIP_HAPTICS,
	RIGHT_GRIP_HAPTICS,
	// Which of the controller's effects that pulse plays.
	GRIP_HAPTIC_EFFECT,
	// Same pair, but for the pulse fired when a grip sensor releases (hand pulled
	// away) rather than trips. Independent so contact and release can be tuned
	// -- or disabled -- separately; 0 intensity = off.
	GRIP_RELEASE_HAPTIC_INTENSITY,
	GRIP_RELEASE_HAPTIC_EFFECT,
	// CALIBRATE_GYRO waits DELAY seconds (time to put the controller down), then
	// calibrates for TIME seconds. Both phases are reported over telemetry.
	GYRO_CALIBRATION_DELAY,
	GYRO_CALIBRATION_TIME,
	// A built-in controller tune (haptic script 0-13) played when a Steam
	// Controller 2026 connects to the mapper, and before TURN_OFF_CONTROLLER
	// powers it off. -1 (default) = none. The firmware's own power-on and
	// button power-off jingles are not configurable and still play.
	CONNECT_SOUND,
	SHUTDOWN_SOUND,
	// How loud those tunes (and PLAY_SOUND) play, as the firmware's gain in dB:
	// 0 (default) is the tune as the firmware plays it, negative is quieter.
	SOUND_GAIN,
	// How long, in milliseconds, a grip keeps reading "held" after the hand
	// leaves it. Per side, unlike range and flicker guard: this is time on the
	// host, not distance in the firmware, so each grip can have its own -- a
	// long one for a grip that holds a layer, none for the one that holds gyro.
	// 0 (default) = released the moment the firmware says so.
	LEFT_GRIP_RELEASE_DELAY,
	RIGHT_GRIP_RELEASE_DELAY,
	// The Steam Controller 2026's light, 0-100 (controller setting 45, the one
	// Steam's brightness slider writes). -1 (default) leaves it as it is. A
	// binding that runs "LED_BRIGHTNESS = n" makes it an output.
	LED_BRIGHTNESS,
	// A thumb trying to hold still on a capacitive pad never is: the contact patch
	// breathes and the reported position drifts, which the mouse path faithfully
	// turns into a crawling cursor. Below this finger speed, in pad pixels per
	// second, the displacement is dropped rather than sent. 0 (default) = off.
	TOUCHPAD_MOVEMENT_THRESHOLD,
	// Haptic ticks the pad's own actuator plays as the finger travels, the way a
	// scroll wheel detents. Intensity 0 = off; the interval is how far the finger
	// must travel, in pad pixels, between one tick and the next.
	TOUCHPAD_HAPTIC_INTENSITY,
	TOUCHPAD_HAPTIC_EFFECT,
	TOUCHPAD_HAPTIC_INTERVAL,
	// Physically clicking the pad down is a discrete event rather than a travel
	// distance, so it gets its own pair instead of sharing the ticks above.
	TOUCHPAD_CLICK_HAPTIC_INTENSITY,
	TOUCHPAD_CLICK_HAPTIC_EFFECT,
	// And the matching pair for letting the click back up. The pad's switch
	// releases well before your thumb leaves it, so without a pulse there is
	// nothing telling you the binding has actually let go. Independent of the
	// press pulse so the two can feel different, or either can run alone.
	TOUCHPAD_RELEASE_HAPTIC_INTENSITY,
	TOUCHPAD_RELEASE_HAPTIC_EFFECT,
	// Pressing a pad hard enough to click it rolls the finger, and in MOUSE mode
	// that roll is a camera movement you did not ask for. This scales mouse output
	// down as the press comes on: 0 (default) off, 1 stops output entirely while
	// the pad is being clicked.
	TOUCHPAD_CLICK_DAMPEN,
	// Analog pad pressure at which that damping starts easing in, so the cursor
	// is already settling before the click registers rather than stopping dead on
	// the switch. 0 damps only while the click is physically held. Shared with
	// GYRO_CLICK_DAMPEN: one press, so one description of how far into it you are.
	TOUCHPAD_CLICK_DAMPEN_THRESHOLD,
	TOUCHPAD_LIFT_SPEED,
	// The same press, applied to the gyro. Pressing a pad shoves the whole
	// controller, so a setup that pans with the pad and aims with the gyro gets
	// the jolt twice over -- once through the pad and once through the IMU.
	// Its own amount rather than sharing TOUCHPAD_CLICK_DAMPEN, because which of
	// the two outputs needs quieting depends on what the pad is even doing.
	GYRO_CLICK_DAMPEN,
	// Whether the Steam Controller 2026's firmware gyro auto-calibration is
	// switched off (controller settings 84/85, Steam's "Enable Software
	// Calibration" switch). The firmware re-estimates bias whenever the
	// controller looks still, and a slow deliberate tilt passes that test, so
	// small movements are eaten and the cursor slides back. ON (default).
	DISABLE_HARDWARE_GYRO_CALIBRATION,
};

// constexpr are like #define but with respect to typeness
constexpr size_t MAX_NO_OF_TOUCH = 2; // Could be obtained from JSL?
constexpr int MAPPING_SIZE = int(ButtonID::SIZE);

// "While released" chords: a modeshift written `!MISC5,W = X` applies while
// MISC5 is NOT held -- the right grip let go, say. The chord is its own id,
// far outside every real button, so it is only ever a key (in chorded
// variables and the chord stack), never an index into a button array.
constexpr int INVERTED_CHORD_OFFSET = 1 << 12;
constexpr bool isInvertedChord(ButtonID id) { return int(id) >= INVERTED_CHORD_OFFSET; }
constexpr ButtonID invertedChordOf(ButtonID base) { return ButtonID(int(base) + INVERTED_CHORD_OFFSET); }
constexpr ButtonID invertedChordBase(ButtonID id) { return ButtonID(int(id) - INVERTED_CHORD_OFFSET); }
// The buttons the loaded configuration uses as "!X". Written by the command
// thread as a configuration loads, read by every controller's poll
// (DigitalButton.cpp).
// (InvertedChords.cpp)
void useInvertedChord(ButtonID base);
void clearInvertedChords();
bool invertedChordInUse(ButtonID base);
// Keep a chord stack's "!X" entries in line with the registry and what is held.
void syncInvertedChordStack(std::deque<ButtonID> &stack);
// A button changed: move its "!X" entry, if its configuration uses one.
void updateInvertedChord(std::deque<ButtonID> &stack, bool isPressed, ButtonID id);
constexpr int FIRST_ANALOG_TRIGGER = int(ButtonID::ZLF);
constexpr int LAST_ANALOG_TRIGGER = int(ButtonID::ZRF);
constexpr int FIRST_TOUCH_BUTTON = MAPPING_SIZE + 1;
// The three grids are the same size limit apart, so one span serves all of them.
constexpr int MAX_GRID_BUTTONS = int(ButtonID::T25) - int(ButtonID::T1) + 1;
constexpr int FIRST_LEFT_TOUCH_BUTTON = int(ButtonID::LT1);
constexpr int FIRST_RIGHT_TOUCH_BUTTON = int(ButtonID::RT1);
constexpr int FIRST_LEFT_STICK_MENU_BUTTON = int(ButtonID::LM1);
constexpr int FIRST_RIGHT_STICK_MENU_BUTTON = int(ButtonID::RM1);

// ---------------------------------------------------------------------------
// The five virtual ranges -- T, LT, RT, LM, RM -- are each addressed by OFFSET
// from their own first element. Insert anything in the wrong place and every
// existing binding in the ranges below it silently retargets: a config that
// reloaded a weapon starts throwing a grenade, and nothing fails to build.
//
// So the layout is pinned here rather than in a test. These run on every build,
// which a harness someone has to remember to run does not.
// ---------------------------------------------------------------------------

// SIZE counts the real buttons, so it must stay below the virtual ranges or
// MAPPING_SIZE -- and with it FIRST_TOUCH_BUTTON -- moves.
static_assert(int(ButtonID::SIZE) < int(ButtonID::T1),
  "ButtonID::SIZE must precede the virtual button ranges");
static_assert(int(ButtonID::T1) == FIRST_TOUCH_BUTTON,
  "T1 moved: grid bindings are addressed by offset and would silently retarget");

// Order: T -> LT -> RT -> LM -> RM, none overlapping.
static_assert(int(ButtonID::T25) < int(ButtonID::LT1), "the shared grid must precede the left pad grid");
static_assert(int(ButtonID::LT25) < int(ButtonID::RT1), "the left pad grid must precede the right pad grid");
static_assert(int(ButtonID::RT25) < int(ButtonID::LM1), "the pad grids must precede the stick menus");
static_assert(int(ButtonID::LM25) < int(ButtonID::RM1), "the left stick menu must precede the right one");

// Every range is the same width, so one limit serves all five.
static_assert(int(ButtonID::T25) - int(ButtonID::T1) + 1 == MAX_GRID_BUTTONS, "shared grid span");
static_assert(int(ButtonID::LT25) - int(ButtonID::LT1) + 1 == MAX_GRID_BUTTONS, "left pad grid span");
static_assert(int(ButtonID::RT25) - int(ButtonID::RT1) + 1 == MAX_GRID_BUTTONS, "right pad grid span");
static_assert(int(ButtonID::LM25) - int(ButtonID::LM1) + 1 == MAX_GRID_BUTTONS, "left stick menu span");
static_assert(int(ButtonID::RM25) - int(ButtonID::RM1) + 1 == MAX_GRID_BUTTONS, "right stick menu span");

// magic_enum turns an index back into a ButtonID, and past its configured range
// it silently returns nothing -- a segment that could never fire.
static_assert(int(ButtonID::RM25) <= MAGIC_ENUM_RANGE_MAX,
  "ButtonID has outgrown MAGIC_ENUM_RANGE_MAX; enum_cast would start failing silently");
constexpr int NUM_ANALOG_TRIGGERS = int(LAST_ANALOG_TRIGGER) - int(FIRST_ANALOG_TRIGGER) + 1;
constexpr float MAGIC_TAP_DURATION = 40.0f;           // in milliseconds.
constexpr float MAGIC_INSTANT_DURATION = 40.0f;       // in milliseconds
constexpr float MAGIC_EXTENDED_TAP_DURATION = 500.0f; // in milliseconds
constexpr int MAGIC_TRIGGER_SMOOTHING = 5;            // in samples

enum class GyroSpace
{
	LOCAL,
	YAW_PLUS_ROLL,
	PLAYER_TURN,
	PLAYER_LEAN,
	WORLD_TURN,
	WORLD_LEAN,
	INVALID
};

enum class AccelCurve
{
	INVALID = -1,
	LINEAR = 0,
	NATURAL,
	POWER,
	QUADRATIC,
	SIGMOID,
	JUMP,
};

// The gyro and the trackpad mouse each have an acceleration curve. One can
// inherit the other's *shape* (curve type and its parameters): the borrowed
// curve is evaluated at the same fraction of the way between the borrower's own
// min and max speed thresholds, so the units (deg/s vs px/s) never mix.
enum class AccelCurveLink
{
	NONE,
	TOUCHPAD_USES_GYRO,
	GYRO_USES_TOUCHPAD,
	INVALID,
};
enum class ControllerOrientation
{
	FORWARD,
	LEFT,
	RIGHT,
	BACKWARD,
	JOYCON_SIDEWAYS,
	INVALID
};
// The controller's own haptic effects, in the controller's own order: the
// ordinal is what goes on the wire, so do not reorder. Shared by the
// GRIP_HAPTIC_EFFECT setting and by the HAPTIC_<side>_<effect> binding names, so
// the two can never drift apart.
enum class HapticEffect
{
	OFF,
	TICK,
	CLICK,
	TONE,
	RUMBLE,
	NOISE,
	SCRIPT,
	SWEEP,
	// Not firmware canned effects, so kept after the firmware's own list (whose
	// indices are what travel in the command report). PULSE is the single 300 us
	// pulse Steam's grip calibration plays; TAP is that pulse preceded by a pad
	// CLICK, as Steam plays it on the right grip. No underscores: a haptic binding
	// name reads anything after one as the gain.
	PULSE,
	TAP,
	INVALID
};

// The 0-100 dial every automatic haptic is configured with, mapped to the signed
// decibel gain the firmware actually takes. Shared so the grip pulse and the pad
// ticks cannot drift apart on what "40" feels like. The firmware's scale is
// logarithmic, so the bottom end has to reach a long way down to be gentle.
inline int hapticGainDb(float intensity)
{
	const float scale = std::clamp(intensity, 0.f, 100.f) / 100.f;
	return int(std::lround(-24.0f + scale * 36.0f));
}

enum class RingMode
{
	OUTER,
	INNER,
	INVALID
};
enum class StickMode
{
	NO_MOUSE,
	AIM,
	FLICK,
	FLICK_ONLY,
	ROTATE_ONLY,
	MOUSE_RING,
	MOUSE_AREA,
	OUTER_RING,
	INNER_RING,
	SCROLL_WHEEL,
	HYBRID_AIM,
	// A weapon wheel on the stick: deflection past the deadzone selects one of
	// LM1..LM25 / RM1..RM25 by angle, numbered clockwise from up exactly as a
	// RADIAL touch grid is. Deliberately placed ABOVE the virtual-controller
	// block below, which is range-checked as one contiguous span.
	RADIAL_MENU,
	// Following requires virtual controller (keep them contiguous)
	LEFT_STICK,
	RIGHT_STICK,
	LEFT_ANGLE_TO_X,
	LEFT_ANGLE_TO_Y,
	RIGHT_ANGLE_TO_X,
	RIGHT_ANGLE_TO_Y,
	LEFT_STEER_X,
	RIGHT_STEER_X,
	LEFT_WIND_X,
	RIGHT_WIND_X,
	INVALID
};
enum class FlickSnapMode
{
	NONE,
	FOUR,
	EIGHT,
	INVALID
};
enum class AxisMode
{
	STANDARD = 1,
	INVERTED = -1,
	INVALID = 0
}; // valid values are true!
enum class TriggerMode
{
	NO_FULL,
	NO_SKIP,
	MAY_SKIP,
	MUST_SKIP,
	MAY_SKIP_R,
	MUST_SKIP_R,
	NO_SKIP_EXCLUSIVE,
	X_LT,
	X_RT,
	PS_L2 = X_LT,
	PS_R2 = X_RT,
	INVALID
};
enum class GyroAxisMask
{
	NONE = 0,
	X = 1,
	Y = 2,
	Z = 4,
	INVALID = 8
};
enum class JoyconMask
{
	IGNORE_BOTH = 0b00,
	IGNORE_LEFT = 0b01,
	IGNORE_RIGHT = 0b10,
	USE_BOTH = 0b11,
	INVALID
};
enum class GyroIgnoreMode
{
	BUTTON,
	LEFT_STICK,
	RIGHT_STICK,
	INVALID
};
enum class DstState
{
	NoPress,
	PressStart,
	QuickSoftTap,
	QuickFullPress,
	QuickFullRelease,
	SoftPress,
	DelayFullPress,
	PressStartResp,
	ExclFullPress,
	INVALID
};
enum class GyroOutput
{
	MOUSE,
	LEFT_STICK,
	RIGHT_STICK,
	PS_MOTION,
	INVALID
};

enum class BtnEvent
{
	OnPress,
	OnTap,
	OnHold,
	OnTurbo,
	OnRelease,
	OnTapRelease,
	OnHoldRelease,
	INVALID
};
enum class Switch : char
{
	OFF,
	ON,
	INVALID,
}; // Used to parse autoload assignment

enum class ControllerScheme
{
	NONE,
	XBOX,
	DS4,
	INVALID
};

enum class TouchpadMode
{
	GRID_AND_STICK, // Grid and Stick
	MOUSE,          // gestures to be added as part of this mode
	PS_TOUCHPAD,
	INVALID
};

// Workaround default string streaming operator
class PathString : public string // Should be wstring
{
public:
	PathString() = default;
	PathString(const string& path)
	  : string(path)
	{
	}
	PathString(string_view path)
	  : string(path)
	{
	}
};

union Color
{
	Color(uint32_t color = 0x00ffffff)
	  : raw(color)
	{
	}
	uint32_t raw;
	struct RGB_t
	{
		uint8_t b;
		uint8_t g;
		uint8_t r;
		uint8_t a; // unused. animation? (blink, pulse, etc)
	} rgb;
};

using AxisSignPair = pair<AxisMode, AxisMode>;

// Used for XY pair values such as sensitivity or GyroSample
// that includes a nicer accessor
struct FloatXY : public pair<float, float>
{
	FloatXY(float x = 0, float y = 0)
	  : pair(x, y)
	{
	}

	inline float x() const
	{
		return first;
	}

	inline float y() const
	{
		return second;
	}

	FloatXY &operator+=(const FloatXY& rhs)
	{
		first += rhs.first;
		second += rhs.second;
		return *this;
	}
};

// Set of gyro control settings bundled in one structure
struct GyroSettings
{
	bool always_off = false;
	ButtonID button = ButtonID::NONE; // Ignore on button none means no GYRO_OFF button (or Always On);
	GyroIgnoreMode ignore_mode = GyroIgnoreMode::BUTTON;
};

class Mapping;

// This function is defined in main.cpp. It enables two sim press variables to
// listen to each other and make sure they both hold the same values.
void updateSimPressPartner(ButtonID sim, ButtonID origin, const Mapping &newVal);
void updateDiagPressPartner(ButtonID diag, ButtonID origin, const Mapping &newVal);

// This operator enables reading any enum from string
template<class E, class = std::enable_if_t<std::is_enum<E>{}>>
istream &operator>>(istream &in, E &rhv)
{
	string s;
	in >> s;
	auto opt = magic_enum::enum_cast<E>(s);
	rhv = opt ? *opt : *magic_enum::enum_cast<E>("INVALID");
	return in;
}

// This operator enables writing any enum to string
template<class E, class = std::enable_if_t<std::is_enum<E>{}>>
ostream &operator<<(ostream &out, E rhv)
{
	return out << magic_enum::enum_name(rhv);
}

istream &operator>>(istream &in, ButtonID &rhv);
ostream &operator<<(ostream &out, const ButtonID &rhv);

istream &operator>>(istream &in, FlickSnapMode &fsm);
ostream &operator<<(ostream &out, const FlickSnapMode &fsm);

istream &operator>>(istream &in, TriggerMode &tm); // Handle L2 / R2

istream &operator>>(istream &in, GyroSettings &gyro_settings);
ostream &operator<<(ostream &out, const GyroSettings &gyro_settings);
bool operator==(const GyroSettings &lhs, const GyroSettings &rhs);
inline bool operator!=(const GyroSettings &lhs, const GyroSettings &rhs)
{
	return !(lhs == rhs);
}

ostream &operator<<(ostream &out, const FloatXY &fxy);
istream &operator>>(istream &in, FloatXY &fxy);
bool operator==(const FloatXY &lhs, const FloatXY &rhs);
inline bool operator!=(const FloatXY &lhs, const FloatXY &rhs)
{
	return !(lhs == rhs);
}

ostream& operator<<(ostream& out, const AxisSignPair& fxy);
istream& operator>>(istream& in, AxisSignPair& fxy);
bool operator==(const AxisSignPair& lhs, const AxisSignPair& rhs);
inline bool operator!=(const AxisSignPair& lhs, const AxisSignPair& rhs)
{
	return !(lhs == rhs);
}

istream &operator>>(istream &in, Color &color);
ostream &operator<<(ostream &out, const Color &color);
bool operator==(const Color &lhs, const Color &rhs);
inline bool operator!=(const Color &lhs, const Color &rhs)
{
	return !(lhs == rhs);
}

istream &operator>>(istream &in, AxisMode &am);
// AxisMode can use the templated operator for writing

istream &operator>>(istream &in, PathString &fxy);

class Log
{
public:
	enum class Level
	{
		UT,
		BASE,
		BOLD,
		INFO,
		WARN,
		ERR,
	};

protected:
	// https://stackoverflow.com/questions/11826554/standard-no-op-output-stream
	class NullBuffer : public std::streambuf
	{
	public:
		int overflow(int c) override
		{
			return c;
		}
	};
	unique_ptr<streambuf> _buf;

	static streambuf *makeBuffer(Level level);

public:
	Log(Level level)
	  : _buf(makeBuffer(level))
	  , _str(_buf.get())
	{
	}
	~Log() { }

	ostream _str;
};

// This trickery doesn't work in Linux does it? :(
#define CERR Log(Log::Level::ERR)._str
#define COUT Log(Log::Level::BASE)._str
#define COUT_INFO Log(Log::Level::INFO)._str
#define COUT_WARN Log(Log::Level::WARN)._str
#define DEBUG_LOG Log(Log::Level::UT)._str
#define COUT_BOLD Log(Log::Level::BOLD)._str

bool do_RECONNECT_CONTROLLERS(string_view arguments, std::function<void()> loadOnReconnect);
