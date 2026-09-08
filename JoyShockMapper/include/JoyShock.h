#pragma once

#include "InputGuards.h"
#include "JoyShockMapper.h"
#include "TouchMouseResampler.h"
#include "TouchGridRouting.h"
#include "MotionIf.h"
#include "DigitalButton.h"
#include "Stick.h"
#include "JslWrapper.h"
#include "SettingsManager.h"
#include "../src/quatMaths.cpp"

struct LowPassFilter1E
{
	float prev = 0.f;
	bool initialized = false;
	float filter(float x, float alpha)
	{
		if (!initialized) { prev = x; initialized = true; }
		prev = alpha * x + (1.f - alpha) * prev;
		return prev;
	}
	void reset() { initialized = false; }
};

struct OneEuroFilter
{
	LowPassFilter1E xFilt, dxFilt;
	float xPrev = 0.f;
	bool initialized = false;
	// How fast the internal speed estimate reacts to a sudden change, in Hz.
	// Was a shared compile-time constant; gyro and touchpads need very different
	// values here (see TOUCHPAD_D_CUTOFF), so each instance now owns its own.
	float dCutoff = 1.0f;

	static float alpha(float cutoff, float dt)
	{
		float tau = 1.f / (6.2831853f * cutoff);
		return 1.f / (1.f + tau / dt);
	}
	// Uses the global gyro ONE_EURO_* settings.
	float filter(float x, float dt);
	// Explicit parameters, so independent consumers (gyro, touchpads) can each run
	// their own tuning. Position in normalised pad units needs a far lower cutoff
	// than gyro in degrees/sec, so sharing one pair of settings does not work.
	float filter(float x, float dt, float minCutoff, float beta);
	void reset() { initialized = false; xFilt.reset(); dxFilt.reset(); }
	// Forces the filter's state to "has been perfectly tracking x, stationary,
	// for a long time" -- the correct terminal state once a raw signal is
	// confirmed to have genuinely stopped changing, used instead of running the
	// normal recursive update so that reaching it emits no output of its own.
	void snapTo(float x)
	{
		xPrev = x;
		initialized = true;
		xFilt.prev = x;
		xFilt.initialized = true;
		dxFilt.prev = 0.f;
		dxFilt.initialized = true;
	}
};

struct TouchMousePipeline
{
	// Filter absolute pad position, then differentiate in floating point. Retain
	// sub-pixel motion through sensitivity and acceleration until OS injection.
	OneEuroFilter posFilterX, posFilterY;
	float lastX = 0.f, lastY = 0.f;
	bool initialized = false;
	// Last observed coordinates and time since they changed. Coordinate equality
	// cannot distinguish an unchanged report from a poll without a new report.
	float lastRawX = 0.f, lastRawY = 0.f;
	float pendingDt = 0.f;
	// Whether step() evaluated a displacement on this poll.
	bool sampleConsumed = false;
	float consumedDt = 0.f;
	// Which touch point currently feeds this pipeline. Position-space filtering has
	// to restart when the source finger changes, otherwise handing over between two
	// contacts teleports the cursor.
	int sourceIndex = -1;
	// Coast velocity in mouse units per SECOND, not per poll. Storing a per-poll
	// displacement made the coast speed track however long the last tick happened
	// to be, so it visibly stuttered whenever the poll interval jittered.
	float momentumX = 0.f, momentumY = 0.f;
	TouchMouseResampler output;
	TouchLiftGuard liftGuard;
	float fingerSpeed = 0.f;
	// active: a coast is in flight. contact: a finger is on the pad RIGHT NOW.
	// These are not the same thing, and conflating them is what let a re-touch
	// mid-coast differentiate the gap between liftoff and touchdown.
	bool active = false;
	bool contact = false;
	// Pad pixels travelled since the last movement haptic tick. Distance, not time:
	// ticking on a timer would buzz while a finger rests and go silent during a
	// fast flick, which is the opposite of what a detent should feel like.
	float hapticTravel = 0.f;

	void reset()
	{
		posFilterX.reset();
		posFilterY.reset();
		lastX = lastY = 0.f;
		initialized = false;
		lastRawX = lastRawY = 0.f;
		pendingDt = 0.f;
		sampleConsumed = false;
		consumedDt = 0.f;
		sourceIndex = -1;
		momentumX = momentumY = 0.f;
		output.reset();
		liftGuard.reset();
		fingerSpeed = 0.f;
		active = false;
		contact = false;
		hapticTravel = 0.f;
	}

	// rawX / rawY: normalised pad position in [0, 1]. dt in SECONDS.
	// Returns the normalised displacement since the previous call. The first call
	// after a fresh contact returns zero, so touching down never jerks the cursor.
	// sampleConsumed is false for unfiltered duplicates or a confirmed hold.
	// With smoothing enabled, held samples advance the filter until that hold.
	FloatXY step(float rawX, float rawY, float dt, float minCutoff, float beta, float dCutoff)
	{
		if (!std::isfinite(rawX) || !std::isfinite(rawY) ||
		    rawX < 0.f || rawX > 1.f || rawY < 0.f || rawY > 1.f)
		{
			reset();
			return { 0.f, 0.f };
		}
		if (!std::isfinite(dt) || dt <= 0.f || dt > 0.1f)
			dt = 0.003f; // fall back to the nominal tick if the clock misbehaved

		posFilterX.dCutoff = dCutoff;
		posFilterY.dCutoff = dCutoff;

		// Evaluate the position filter on the output clock. A held coordinate is
		// still its current target. Deferring these updates and charging the whole
		// wait to the next sample turns wireless timing jitter into a catch-up step.
		const bool sameAsLast = initialized && rawX == lastRawX && rawY == lastRawY;
		const float elapsed = pendingDt + dt;
		pendingDt = sameAsLast ? elapsed : 0.f;
		constexpr float kStopTime = .016f;
		if (sameAsLast && (minCutoff <= 0.f || pendingDt >= kStopTime))
		{
			// Keep the existing bounded-stop policy: cancel remaining filter lag
			// silently after a hold. Already computed output drains independently.
			if (pendingDt >= kStopTime)
			{
				momentumX = momentumY = 0.f;
				if (minCutoff > 0.f)
				{
					posFilterX.snapTo(rawX);
					posFilterY.snapTo(rawY);
					lastX = rawX;
					lastY = rawY;
				}
			}
			sampleConsumed = false;
			return { 0.f, 0.f };
		}
		sampleConsumed = true;
		// Unfiltered coordinates remain packets; acceleration needs their actual
		// interval. Filtered displacement instead describes this output poll.
		consumedDt = minCutoff > 0.f ? dt : elapsed;
		lastRawX = rawX;
		lastRawY = rawY;

		float fx = rawX;
		float fy = rawY;
		if (minCutoff > 0.f)
		{
			fx = posFilterX.filter(rawX, dt, minCutoff, beta);
			fy = posFilterY.filter(rawY, dt, minCutoff, beta);
		}

		if (!std::isfinite(fx) || !std::isfinite(fy))
		{
			reset();
			return { 0.f, 0.f };
		}
		if (!initialized)
		{
			lastX = fx;
			lastY = fy;
			initialized = true;
			return { 0.f, 0.f };
		}

		FloatXY delta{ fx - lastX, fy - lastY };
		lastX = fx;
		lastY = fy;
		return delta;
	}

	// Applied AFTER the delta has been scaled into mouse units, so the speed term is
	// measured in the same space the user tunes sensitivity in.
	static FloatXY accelerate(FloatXY delta, float acceleration, float dt = .003f)
	{
		if (acceleration <= 0.f)
			return delta;
		float speed = sqrtf(delta.x() * delta.x() + delta.y() * delta.y());
		// Preserve the legacy gain at the nominal 3 ms tick, independently of
		// whether the displacement was generated over 1, 3, or 8 ms.
		if (!std::isfinite(dt) || dt <= 0.f) dt = .003f;
		float gain = 1.f + acceleration * speed * (.003f / dt);
		if (gain > 4.f)
			gain = 4.f;
		return { delta.x() * gain, delta.y() * gain };
	}
};

// An instance of this class represents a single controller device that JSM is listening to.
class JoyShock
{
public:
	JoyShock(int uniqueHandle, int controllerSplitType, shared_ptr<DigitalButton::Context> sharedButtonCommon = nullptr);

	~JoyShock();

	// These two large functions are defined further down
	void processStick(float stickX, float stickY, Stick &stick, float mouseCalibrationFactor, float deltaTime, bool &anyStickInput, bool &lockMouse, float &camSpeedX, float &camSpeedY);

	void handleTouchStickChange(TouchStick &ts, bool down, float movX, float movY, float delta_time);

	bool hasVirtualController();

	void onVirtualControllerNotification(uint8_t largeMotor, uint8_t smallMotor, Indicator indicator);

	template<typename E>
	E getSetting(SettingID index);

	float getSetting(SettingID index);

	template<>
	FloatXY getSetting<FloatXY>(SettingID index);

	template<>
	GyroSettings getSetting<GyroSettings>(SettingID index);

	template<>
	Color getSetting<Color>(SettingID index);

	template<>
	AdaptiveTriggerSetting getSetting<AdaptiveTriggerSetting>(SettingID index);

	template<>
	AxisSignPair getSetting<AxisSignPair>(SettingID index);

	void getSmoothedGyro(float x, float y, float length, float bottomThreshold, float topThreshold, int maxSamples, float &outX, float &outY);
	void applyGyroDecaySmoothing(float rawX, float rawY, float deltaTime, float smoothingTime, float threshold, float &outX, float &outY);
	void disableGyroDecaySmoothing();
	void applyOneEuroFilter(float rawX, float rawY, float deltaTime, float &outX, float &outY);
	void resetOneEuroFilter();

	void handleButtonChange(ButtonID id, bool pressed, int touchpadID = -1);

	void handleTriggerChange(ButtonID softIndex, ButtonID fullIndex, TriggerMode mode, float position, AdaptiveTriggerSetting &trigger_rumble);

	bool isPressed(ButtonID btn);

	// return true if it hits the outer deadzone
	bool processDeadZones(float &x, float &y, float innerDeadzone, float outerDeadzone);

	void updateGridSize();

	// A grid button lives in one of three parallel arrays: the shared grid, or
	// one per pad on a two-pad controller. Every site that indexes them has to
	// agree on which, and on whether the index is in range, so resolve it once.
	struct GridSlot
	{
		JSMButton *mapping = nullptr;
		DigitalButton *button = nullptr;
	};
	GridSlot findGridSlot(ButtonID id);

	bool processGyroStick(float stickX, float stickY, float stickLength, StickMode stickMode, bool forceOutput);

	shared_ptr<DigitalButton::Context> _context;
	vector<DigitalButton> _buttons;
	vector<DigitalButton> _gridButtons;
	vector<DigitalButton> _leftGridButtons;
	vector<DigitalButton> _rightGridButtons;
	vector<TouchStick> _touchpads;
	chrono::steady_clock::time_point _timeNow;
	// Separate clock for the touch callback: it runs on the same poll iteration as
	// joyShockPollCallback but must not consume that callback's timestamp.
	chrono::steady_clock::time_point _touchTimeNow;
	bool _touchTimeInitialized = false;
	shared_ptr<MotionIf> _motion;
	int _handle;
	int _controllerType;
	int _splitType = 0;
	int _vendorId = 0;
	int _productId = 0;
	bool _ignoreGyro = false;


	float neutralQuatW = 1.0f;
	float neutralQuatX = 0.0f;
	float neutralQuatY = 0.0f;
	float neutralQuatZ = 0.0f;

	bool set_neutral_quat = false;

	Color _light_bar;
	AdaptiveTriggerSetting _leftEffect;
	AdaptiveTriggerSetting _rightEffect;
	static AdaptiveTriggerSetting _unusedEffect;

	Stick _leftStick;
	Stick _rightStick;
	Stick _motionStick;

	bool processed_gyro_stick = false;
	static constexpr int NUM_LAST_GYRO_SAMPLES = 100;
	array<float, NUM_LAST_GYRO_SAMPLES> lastGyroX = { 0.f };
	array<float, NUM_LAST_GYRO_SAMPLES> lastGyroY = { 0.f };
	float lastGyroAbsX = 0.f;
	float lastGyroAbsY = 0.f;
	int lastGyroIndexX = 0;
	int lastGyroIndexY = 0;

	float gyroXVelocity = 0.f;
	float gyroYVelocity = 0.f;

	// Trackball state is owned by each pad's pipeline.

	// Keep independent filter state for dual-pad controllers; a left-pad sample
	// must not influence the next right-pad sample.
	TouchMousePipeline touchPipelines[2];
	TouchGridRouting touchGridRouting[2];

	// Previous pad-click state, indexed the same way as touchPipelines (0 = left,
	// 1 = right). The click haptic is edge-triggered off this: a level-triggered
	// pulse would replay for every poll the pad stayed held down.
	bool padClickWasOn[2] = { false, false };

	// How far into a press the harder-pressed pad is, 0 to 1. Written by the touch
	// callback, read by the gyro path a tick later -- the poll callback runs first,
	// so the gyro sees the press about 3ms after the pad does, which is well under
	// the time it takes a thumb to actually push the switch down.
	float padPressLevel = 0.f;

	// Plays one of the controller's own effects on this controller's actuators.
	// Public because the touch path in main.cpp drives the pad haptics directly,
	// rather than going through a binding the way sendHaptic's other caller does.
	void fireHaptic(int side, int effect, int gainDb);

	std::deque<std::pair<std::chrono::steady_clock::time_point, float>> decelBrakeHistory;
	float decelBrakeEngagement = 0.f;
	float decelBrakeOmegaRaw = 0.f;

private:
	// this large functions is defined further down
	float handleFlickStick(float stickX, float stickY, Stick &stick, float stickLength, StickMode mode);

	bool isSoftPullPressed(int triggerIndex, float triggerPosition);

	float getTriggerEffectStartPos();

	template<typename E>
	optional<E> getSettingAtChord(SettingID id, ButtonID chord);

	void sendRumble(int smallRumble, int bigRumble);
	void sendHaptic(int side, int effect, int gainDb);

	DigitalButton *getMatchingSimBtn(ButtonID index);
	DigitalButton *getMatchingDiagBtn(ButtonID index, optional<MapIterator> &iter);

	void resetSmoothSample();

	float getSmoothedStickRotation(float value, float bottomThreshold, float topThreshold, int maxSamples);

	static constexpr int MAX_GYRO_SAMPLES = 256;
	static constexpr int NUM_SAMPLES = 256;

	array<float, NUM_SAMPLES> _flickSamples;
	int _frontSample = 0;

	array<FloatXY, MAX_GYRO_SAMPLES> _gyroSamples;
	int _frontGyroSample = 0;
	float _gyroDecayX = 0.f;
	float _gyroDecayY = 0.f;
	bool _gyroDecayInit = false;
	bool _gyroDecayEnabledLast = false;

	OneEuroFilter _oneEuroX;
	OneEuroFilter _oneEuroY;

	Vec _lastGrav = Vec(0.f, -1.f, 0.f);

	float _windingAngleLeft = 0.f;
	float _windingAngleRight = 0.f;

	ScrollAxis _touchScrollX;
	ScrollAxis _touchScrollY;

	bool _softPullDown[NUM_ANALOG_TRIGGERS] = {};
	vector<DstState> _triggerState; // State of analog triggers when skip mode is active
	vector<deque<float>> _prevTriggerPosition;
};

template<typename E>
optional<E> JoyShock::getSettingAtChord(SettingID id, ButtonID chord)
{
	auto setting = SettingsManager::get<E>(id);
	if (setting)
	{
		auto chordedValue = setting->chordedValue(chord);
		return chordedValue ? optional<E>(setting->chordedValue(chord)) : nullopt;
	}
	return nullopt;
}

template<typename E>
E JoyShock::getSetting(SettingID index)
{
	static_assert(is_enum<E>::value, "Parameter of JoyShock::getSetting<E> has to be an enum type");
	// Look at active chord mappings starting with the latest activates chord
	for (auto activeChord = _context->chordStack.begin(); activeChord != _context->chordStack.end(); activeChord++)
	{
		optional<E> opt = getSettingAtChord<E>(index, *activeChord);
		if constexpr (is_same_v<E, StickMode>)
		{
			switch (index)
			{
			case SettingID::LEFT_STICK_MODE:
				if (_leftStick.flick_percent_done < 1.f && opt && (*opt != StickMode::FLICK && *opt != StickMode::FLICK_ONLY))
					opt = make_optional(StickMode::FLICK_ONLY);
				else if (_leftStick.ignore_stick_mode && *activeChord == ButtonID::NONE)
					opt = StickMode::INVALID;
				else
					_leftStick.ignore_stick_mode |= (opt && *activeChord != ButtonID::NONE);
				break;
			case SettingID::RIGHT_STICK_MODE:
				if (_rightStick.flick_percent_done < 1.f && opt && (*opt != StickMode::FLICK && *opt != StickMode::FLICK_ONLY))
					opt = make_optional(StickMode::FLICK_ONLY);
				else if (_rightStick.ignore_stick_mode && *activeChord == ButtonID::NONE)
					opt = make_optional(StickMode::INVALID);
				else
					_rightStick.ignore_stick_mode |= (opt && *activeChord != ButtonID::NONE);
				break;
			case SettingID::MOTION_STICK_MODE:
				if (_motionStick.flick_percent_done < 1.f && opt && (*opt != StickMode::FLICK && *opt != StickMode::FLICK_ONLY))
					opt = make_optional(StickMode::FLICK_ONLY);
				else if (_motionStick.ignore_stick_mode && *activeChord == ButtonID::NONE)
					opt = make_optional(StickMode::INVALID);
				else
					_motionStick.ignore_stick_mode |= (opt && *activeChord != ButtonID::NONE);
				break;
			}
		}

		if constexpr (is_same_v<E, ControllerOrientation>)
		{
			if (index == SettingID::CONTROLLER_ORIENTATION && opt &&
			  opt.value() == ControllerOrientation::JOYCON_SIDEWAYS)
			{
				if (_splitType == JS_SPLIT_TYPE_LEFT)
				{
					opt = optional<E>(static_cast<E>(ControllerOrientation::LEFT));
				}
				else if (_splitType == JS_SPLIT_TYPE_RIGHT)
				{
					opt = optional<E>(static_cast<E>(ControllerOrientation::RIGHT));
				}
				else
				{
					opt = optional<E>(static_cast<E>(ControllerOrientation::FORWARD));
				}
			}
		}
		if (opt)
			return *opt;
	}
	stringstream ss;
	ss << "Index " << index << " is not a valid enum setting";
	throw invalid_argument(ss.str().c_str());
}
