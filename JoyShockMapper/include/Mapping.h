#pragma once

#include "JoyShockMapper.h"
#include "PlatformDefinitions.h"

class Mapping;

// The list of different function that can be bound in the mapping
class EventActionIf
{
public:
	typedef function<void(EventActionIf *)> Callback;

	virtual void RegisterInstant(BtnEvent evt, Callback cb) = 0;
	virtual void TickBindingTurbo(size_t index, float elapsed, float period, Callback apply) {}
	virtual void FinishBindingTurbo() {}
	virtual void ApplyGyroAction(KeyCode gyroAction) = 0;
	// Normal releases belong to their input; explicit release may clear all owners.
	virtual void RemoveGyroAction(KeyCode gyroAction, bool allOwners, bool toggle) = 0;
	virtual void SetRumble(int smallRumble, int bigRumble) = 0;
	// One-shot: a haptic effect has its own duration, so there is nothing to release.
	virtual void FireHaptic(int side, int effect, int gainDb) = 0;
	virtual void ApplyBtnPress(KeyCode key) = 0;
	virtual void ApplyBtnRelease(KeyCode key) = 0;
	// Cycle state belongs to the physical input instance, not a shared Mapping.
	virtual void ApplyCycle(const string &identity, const vector<Mapping> &choices) = 0;
	virtual void ReleaseCycle(const string &identity) = 0;
	virtual void StudioCommand(const string &command) {}
	virtual void MenuCommand(const string &id, const string &verb, bool release) {}
	virtual void ApplyButtonToggle(KeyCode key, Callback apply, Callback release) = 0;
	virtual void StartCalibration() = 0;
	virtual void FinishCalibration() = 0;
	virtual const char *getDisplayName() = 0;
};

// This structure handles the mapping of a button, buy processing and action
// to be done on tap, hold, turbo and others. It holds a map of actions to perform
// when a specific event happens. This replaces the old Mapping structure.
class Mapping
{
public:
	enum class ActionModifier
	{
		None,
		Toggle,
		Instant,
		Release,
		INVALID
	};
	enum class EventModifier
	{
		Auto,
		StartPress,
		ReleasePress,
		TurboPress,
		TapPress,
		HoldPress,
		INVALID
	};

	// Identifies having no binding mapped
	static const Mapping NO_MAPPING;

	// This functor nees to be set to way to validate a command line string;
	static function<bool(string_view)> _isCommandValid;

	friend istream &operator>>(istream &in, Mapping &mapping);
	friend ostream &operator<<(ostream &out, const Mapping &mapping);

private:
	string _description = "no input";
	string _command;

	map<BtnEvent, EventActionIf::Callback> _eventMapping;
	vector<pair<float, EventActionIf::Callback>> _bindingTurbos;
	float _tapDurationMs = MAGIC_TAP_DURATION;
	bool _hasViGEmBtn = false;

	void InsertEventMapping(BtnEvent evt, EventActionIf::Callback action);
	static void RunBothActions(EventActionIf *btn, EventActionIf::Callback action1, EventActionIf::Callback action2);

public:
	Mapping() = default;

	Mapping(string_view mapping);

	Mapping(int dummy)
	  : Mapping()
	{
	}

	string_view description() const
	{
		return _description;
	}

	string_view command() const
	{
		return _command;
	}
	void ProcessEvent(BtnEvent evt, EventActionIf &button) const;
	void ProcessBindingTurbos(float elapsed, EventActionIf &button) const;

	bool AddMapping(KeyCode key, EventModifier evtMod, ActionModifier actMod = ActionModifier::None, float turboInterval = 0);

	bool AppendToCommand(KeyCode key, EventModifier evtMod, ActionModifier actMod = ActionModifier::None);

	inline bool isValid() const
	{
		return !_command.empty();
	}

	inline float getTapDuration() const
	{
		return _tapDurationMs;
	}

	inline void clear()
	{
		_command.clear();
		_eventMapping.clear();
		_bindingTurbos.clear();
		_description.clear();
		_tapDurationMs = MAGIC_TAP_DURATION;
		_hasViGEmBtn = false;
	}

	inline bool hasViGEmBtn() const
	{
		return _hasViGEmBtn;
	}
};

bool operator==(const Mapping &lhs, const Mapping &rhs);
inline bool operator!=(const Mapping &lhs, const Mapping &rhs)
{
	return !(lhs == rhs);
}
