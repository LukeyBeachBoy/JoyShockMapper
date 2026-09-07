#include "JSMVariable.hpp"
#include "JslWrapper.h"
#include "SteamController2026.h"
#include "TritonGripSettings.h"
#include "JSMVariable.hpp"
 #include "TriggerEffectGenerator.h"
#include "SettingsManager.h"
#include "InputHelpers.h"
#include "SDL3/SDL.h"
#include <map>
#include <mutex>
#include <atomic>
#define _USE_MATH_DEFINES
#include <math.h> // M_PI
#include <algorithm>
#include <memory>
#include <iostream>
#include <cstring>
#include <span>
#include <vector>

typedef struct
{
	Uint8 ucEnableBits1;              /* 0 */
	Uint8 ucEnableBits2;              /* 1 */
	Uint8 ucRumbleRight;              /* 2 */
	Uint8 ucRumbleLeft;               /* 3 */
	Uint8 ucHeadphoneVolume;          /* 4 */
	Uint8 ucSpeakerVolume;            /* 5 */
	Uint8 ucMicrophoneVolume;         /* 6 */
	Uint8 ucAudioEnableBits;          /* 7 */
	Uint8 ucMicLightMode;             /* 8 */
	Uint8 ucAudioMuteBits;            /* 9 */
	Uint8 rgucRightTriggerEffect[11]; /* 10 */
	Uint8 rgucLeftTriggerEffect[11];  /* 21 */
	Uint8 rgucUnknown1[6];            /* 32 */
	Uint8 ucLedFlags;                 /* 38 */
	Uint8 rgucUnknown2[2];            /* 39 */
	Uint8 ucLedAnim;                  /* 41 */
	Uint8 ucLedBrightness;            /* 42 */
	Uint8 ucPadLights;                /* 43 */
	Uint8 ucLedRed;                   /* 44 */
	Uint8 ucLedGreen;                 /* 45 */
	Uint8 ucLedBlue;                  /* 46 */
} DS5EffectsState_t;

struct ControllerDevice
{
	ControllerDevice(int id)
	  : _has_accel(false)
	  , _has_gyro(false)
	  , _vendorId(JS_VENDOR_UNKNOWN)
	  , _productId(JS_PRODUCT_UNKNOWN)
	{
		_prevTouchState.t0Down = false;
		_prevTouchState.t1Down = false;
		if (SDL_IsGamepad(id))
		{
			_sdlController = nullptr;
			for (int retry = 3; retry > 0 && _sdlController == nullptr; --retry)
			{
				_sdlController = SDL_OpenGamepad(id);

				if (_sdlController == nullptr)
				{
					CERR << SDL_GetError() << ". Trying again!\n";
					SDL_Delay(1000);
				}
				else
				{
					_has_gyro = SDL_GamepadHasSensor(_sdlController, SDL_SENSOR_GYRO);
					_has_accel = SDL_GamepadHasSensor(_sdlController, SDL_SENSOR_ACCEL);

					if (_has_gyro)
					{
						SDL_SetGamepadSensorEnabled(_sdlController, SDL_SENSOR_GYRO, true);
					}
					if (_has_accel)
					{
						SDL_SetGamepadSensorEnabled(_sdlController, SDL_SENSOR_ACCEL, true);
					}

					_vendorId = SDL_GetGamepadVendor(_sdlController);
					_productId = SDL_GetGamepadProduct(_sdlController);
					_guid = SDL_GetJoystickGUID(SDL_GetGamepadJoystick(_sdlController));
					_ctrlr_type = JS_TYPE_UNKNOWN;

					switch (_vendorId)
					{
					case JS_VENDOR_8BITDO:
						switch (_productId)
						{
						case JS_PRODUCT_8BITDO_SF30_PRO:
							_ctrlr_type = JS_TYPE_8BITDO_SF30_PRO;
							break;
						case JS_PRODUCT_8BITDO_SF30_PRO_BT:
							_ctrlr_type = JS_TYPE_8BITDO_SF30_PRO_BT;
							break;
						case JS_PRODUCT_8BITDO_SN30_PRO:
							_ctrlr_type = JS_TYPE_8BITDO_SN30_PRO;
							break;
						case JS_PRODUCT_8BITDO_SN30_PRO_BT:
							_ctrlr_type = JS_TYPE_8BITDO_SN30_PRO_BT;
							break;
						case JS_PRODUCT_8BITDO_PRO_2:
							_ctrlr_type = JS_TYPE_8BITDO_PRO_2;
							break;
						case JS_PRODUCT_8BITDO_PRO_2_BT:
							_ctrlr_type = JS_TYPE_8BITDO_PRO_2_BT;
							break;
						case JS_PRODUCT_8BITDO_PRO_3:
							_ctrlr_type = JS_TYPE_8BITDO_PRO_3;
							break;
						case JS_PRODUCT_8BITDO_ULTIMATE2_WIRELESS:
							_ctrlr_type = JS_TYPE_8BITDO_ULTIMATE2_WIRELESS;
							break;
						}
						break;
					case JS_VENDOR_HORI:
						if (_productId == JS_PRODUCT_HORI_STEAM_CONTROLLER ||
							_productId == JS_PRODUCT_HORI_STEAM_CONTROLLER_BT)
						{
							_ctrlr_type = JS_TYPE_HORI_STEAM;
						}
						break;
					case JS_VENDOR_FLYDIGI_V1:
					case JS_VENDOR_FLYDIGI_V2:
						if ((_vendorId == JS_VENDOR_FLYDIGI_V1 &&
							 _productId == JS_PRODUCT_FLYDIGI_V1_GAMEPAD) ||
							(_vendorId == JS_VENDOR_FLYDIGI_V2 &&
							 (_productId == JS_PRODUCT_FLYDIGI_V2_APEX ||
							  _productId == JS_PRODUCT_FLYDIGI_V2_VADER)))
						{
							switch (_guid.data[15])
							{
							case JS_FLYDIGI_APEX5:
								_ctrlr_type = JS_TYPE_FLYDIGI_APEX5;
								break;
							case JS_FLYDIGI_VADER3_PRO:
								_ctrlr_type = JS_TYPE_FLYDIGI_VADER3_PRO;
								break;
							case JS_FLYDIGI_VADER4_PRO:
								_ctrlr_type = JS_TYPE_FLYDIGI_VADER4_PRO;
								break;
							case JS_FLYDIGI_VADER5_PRO:
								_ctrlr_type = JS_TYPE_FLYDIGI_VADER5_PRO;
								break;
							}
						}
						break;
					case JS_VENDOR_GAMESIR:
						if (_productId == JS_PRODUCT_GAMESIR_GAMEPAD_G7_PRO_8K &&
							_guid.data[0] == JS_HARDWARE_BUS_USB) // No extended features over Bluetooth
						{
							_ctrlr_type = JS_TYPE_G7_PRO_8K;
						}
						break;
					case JS_VENDOR_NINTENDO:
						if (_productId == JS_PRODUCT_NINTENDO_SWITCH2_PRO)
						{
							_ctrlr_type = JS_TYPE_SWITCH2_PRO_CONTROLLER;
						}
						break;
					case JS_VENDOR_VALVE:
						if (_productId == JS_PRODUCT_VALVE_STEAM_2026_USB ||
							_productId == JS_PRODUCT_VALVE_STEAM_2026_BLE ||
							_productId == JS_PRODUCT_VALVE_PROTEUS_DONGLE ||
							_productId == JS_PRODUCT_VALVE_NEREID_DONGLE)
						{
							_ctrlr_type = JS_TYPE_STEAM_CONTROLLER_2026;
						}
						break;
					}

					if (_ctrlr_type != JS_TYPE_UNKNOWN)
					{
						continue;
					}

					auto sdl_ctrlr_type = SDL_GetGamepadType(_sdlController);
					switch (sdl_ctrlr_type)
					{
					case SDL_GamepadType::SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
						_ctrlr_type = JS_TYPE_JOYCON_LEFT;
						_split_type = JS_SPLIT_TYPE_LEFT;
						break;
					case SDL_GamepadType::SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
						_ctrlr_type = JS_TYPE_JOYCON_RIGHT;
						_split_type = JS_SPLIT_TYPE_RIGHT;
						break;
					case SDL_GamepadType::SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
					case SDL_GamepadType::SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
						_ctrlr_type = JS_TYPE_PRO_CONTROLLER;
						break;
					case SDL_GamepadType::SDL_GAMEPAD_TYPE_PS4:
						_ctrlr_type = JS_TYPE_DS4;
						break;
					case SDL_GamepadType::SDL_GAMEPAD_TYPE_PS5:
						_ctrlr_type = JS_TYPE_DS;
						break;
					case SDL_GamepadType::SDL_GAMEPAD_TYPE_XBOXONE:
						_ctrlr_type = JS_TYPE_XBOXONE;
						switch (_vendorId)
						{
						case JS_VENDOR_PDP:
						case JS_VENDOR_POWERA:
							_ctrlr_type = JS_TYPE_XBOX_SERIES;
							break;
						case JS_VENDOR_MICROSOFT:
							switch (_productId)
							{
							case JS_PRODUCT_XBOX_ONE_ELITE_SERIES_1:
							case JS_PRODUCT_XBOX_ONE_ELITE_SERIES_2:
							case JS_PRODUCT_XBOX_ONE_ELITE_SERIES_2_BLUETOOTH:
							case JS_PRODUCT_XBOX_ONE_ELITE_SERIES_2_BLE:
							case JS_PRODUCT_XBOX_ONE_XBOXGIP_CONTROLLER:
								_ctrlr_type = JS_TYPE_XBOXONE_ELITE;
								break;
							case JS_PRODUCT_XBOX_SERIES_X:
							case JS_PRODUCT_XBOX_SERIES_X_BLE:
								_ctrlr_type = JS_TYPE_XBOX_SERIES;
								break;
							}
							break;
						}
						break;
					}
				}// next attempt?
			}
		}
	}

	virtual ~ControllerDevice()
	{
		_micLight = 0;
		memset(&_leftTriggerEffect, 0, sizeof(_leftTriggerEffect));
		memset(&_rightTriggerEffect, 0, sizeof(_rightTriggerEffect));
		_big_rumble = 0;
		_small_rumble = 0;
		SendEffect();
		SDL_CloseGamepad(_sdlController);
	}

	inline bool isValid()
	{
		return _sdlController != nullptr;
	}

private:
	void LoadTriggerEffect(uint8_t *rgucTriggerEffect, const AdaptiveTriggerSetting *trigger_effect)
	{
		using namespace ExtendInput::DataTools::DualSense;
		rgucTriggerEffect[0] = (uint8_t)trigger_effect->mode;
		switch (trigger_effect->mode)
		{
		case AdaptiveTriggerMode::RESISTANCE_RAW:
		{
			TriggerEffectGenerator::Simple_Feedback(rgucTriggerEffect, 0, trigger_effect->start, trigger_effect->force);
		}
		break;
		case AdaptiveTriggerMode::SEGMENT:
			rgucTriggerEffect[1] = trigger_effect->start;
			rgucTriggerEffect[2] = trigger_effect->end;
			rgucTriggerEffect[3] = trigger_effect->force;
			break;
		case AdaptiveTriggerMode::RESISTANCE:
			TriggerEffectGenerator::Feedback(rgucTriggerEffect, 0, trigger_effect->start, trigger_effect->force);
			break;
		case AdaptiveTriggerMode::BOW:
			TriggerEffectGenerator::Bow(rgucTriggerEffect, 0, trigger_effect->start, trigger_effect->end, trigger_effect->force, trigger_effect->forceExtra);
			break;
		case AdaptiveTriggerMode::GALLOPING:
			TriggerEffectGenerator::Galloping(rgucTriggerEffect, 0, trigger_effect->start, trigger_effect->end, trigger_effect->force, trigger_effect->forceExtra, trigger_effect->frequency);
			break;
	    case AdaptiveTriggerMode::SEMI_AUTOMATIC:
			TriggerEffectGenerator::Simple_Weapon(rgucTriggerEffect, 0, trigger_effect->start, trigger_effect->end, trigger_effect->force);
			break;
		case AdaptiveTriggerMode::AUTOMATIC:
			TriggerEffectGenerator::Simple_Vibration(rgucTriggerEffect, 0, trigger_effect->start, trigger_effect->force, trigger_effect->frequency);
			break;
		case AdaptiveTriggerMode::MACHINE:
			TriggerEffectGenerator::Machine(rgucTriggerEffect, 0, trigger_effect->start, trigger_effect->end, trigger_effect->force, trigger_effect->forceExtra, trigger_effect->frequency, trigger_effect->frequencyExtra);
			break;
		default:
			rgucTriggerEffect[0] = 0x05; // no effect
		}
	}

public:
	void SendEffect()
	{
		if (_ctrlr_type == JS_TYPE_DS)
		{
			DS5EffectsState_t effectPacket;
			memset(&effectPacket, 0, sizeof(effectPacket));

			// Add adaptive trigger data
			effectPacket.ucEnableBits1 |= 0x08 | 0x04; // Enable left and right trigger effect respectively
			LoadTriggerEffect(effectPacket.rgucLeftTriggerEffect, &_leftTriggerEffect);
			LoadTriggerEffect(effectPacket.rgucRightTriggerEffect, &_rightTriggerEffect);

			// Add current rumbling data
			effectPacket.ucEnableBits1 |= 0x01 | 0x02;
			effectPacket.ucRumbleLeft = _big_rumble >> 8;
			effectPacket.ucRumbleRight = _small_rumble >> 8;

			// Add current mic light
			effectPacket.ucEnableBits2 |= 0x01;      /* Enable microphone light */
			effectPacket.ucMicLightMode = _micLight; /* Bitmask, 0x00 = off, 0x01 = solid, 0x02 = pulse */

			// Send to controller
			SDL_SendGamepadEffect(_sdlController, &effectPacket, sizeof(effectPacket));
		}
	}

	bool _has_gyro;
	bool _has_accel;
	int _split_type = JS_SPLIT_TYPE_FULL;
	int _ctrlr_type = JS_TYPE_UNKNOWN;
	int _vendorId = JS_VENDOR_UNKNOWN;
	int _productId = JS_PRODUCT_UNKNOWN;
	SDL_GUID _guid;
	uint16_t _small_rumble = 0;
	uint16_t _big_rumble = 0;
	AdaptiveTriggerSetting _leftTriggerEffect;
	AdaptiveTriggerSetting _rightTriggerEffect;
	uint8_t _micLight = 0;
	SDL_Gamepad *_sdlController = nullptr;
	TOUCH_STATE _prevTouchState;
	// Last values pushed to the controller's firmware, so the settings feature
	// report is only re-sent when something actually changed. -1 = never applied,
	// which also forces a re-apply after a reconnect (the struct is rebuilt).
	int _appliedGripRange = -1;
	int _appliedGripRelease = -1;
	// Rising edge of each grip sensor, for the haptic pulse.
	bool _leftGripWasOn = false;
	bool _rightGripWasOn = false;
};

struct SdlInstance : public JslWrapper
{
private:
#ifdef _WIN32
	// Make Windows 11 honor timer resolution when window is minimized or
	// another application is fullscreen. EcoQoS is also disabled.
	// https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod
	// https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessinformation
	void DisableProcessPowerThrottling()
	{
		PROCESS_POWER_THROTTLING_STATE state;
		memset(&state, 0, sizeof(state));
		state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
		state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED
		                    | PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
		state.StateMask = 0;
		SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling,
		                      &state, sizeof(state));
	}

	// Raise process priority, but not too high.
	// https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities
	void RaiseProcessPriority()
	{
		SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
	}

	// Raise thread priority, but not too high.
	// https://learn.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities
	void RaiseThreadPriority()
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	}

	typedef long NTSTATUS;
	typedef NTSTATUS (NTAPI *PZEQTR)(PULONG MinRes, PULONG MaxRes, PULONG CurrentRes);
	typedef NTSTATUS (NTAPI *PZESTR)(ULONG DesiredRes, BOOLEAN SetRes, PULONG CurrentRes);
	PZEQTR ZwQueryTimerResolution = nullptr;
	PZESTR ZwSetTimerResolution = nullptr;
	ULONG win_timer_res = 0;
	uint64_t timer_res_ns = 0;

	// Reduce system clock interrupt interval to 0.5 ms. Windows default is
	// 15.625 ms (1000/64). SDL default is 1 ms but it uses timeBeginPeriod.
	void SetMaxTimerResolution()
	{
		ZwQueryTimerResolution = (PZEQTR)GetProcAddress(
		    GetModuleHandle(TEXT("ntdll.dll")), "ZwQueryTimerResolution");
		ZwSetTimerResolution = (PZESTR)GetProcAddress(
		    GetModuleHandle(TEXT("ntdll.dll")), "ZwSetTimerResolution");
		ULONG min_res = 0, max_res = 0, cur_res = 0;
		bool result = false;

		if (ZwQueryTimerResolution != nullptr
		    && ZwQueryTimerResolution(&min_res, &max_res, &cur_res) == 0)
		{
			if (ZwSetTimerResolution != nullptr
			    && ZwSetTimerResolution(max_res, TRUE, &cur_res) == 0)
			{
				win_timer_res = cur_res;
				timer_res_ns = win_timer_res * 100; // 100-ns to ns.
				result = true;
			}
		}

		if (!result)
		{
			// Don't call again.
			ZwQueryTimerResolution = nullptr;
			ZwSetTimerResolution = nullptr;

			// Use safe defaults.
			win_timer_res = 10000;  // 1 ms in 100-ns units.
			timer_res_ns = 1000000; // 1 ms.
		}
	}

	// Call before using a waitable timer to ensure resolution is still correct.
	void ReapplyMaxTimerRes()
	{
		if (ZwSetTimerResolution != nullptr)
		{
			ULONG cur_res = 0;
			ZwSetTimerResolution(win_timer_res, TRUE, &cur_res);
		}
	}

	uint64_t next_poll_time = 0;

	void InitPollingTimer()
	{
		next_poll_time = SDL_GetTicksNS();
	}

	void PollingTimer(uint64_t interval_ms)
	{
		const uint64_t interval_ns = interval_ms * 1000000ULL;
		next_poll_time += interval_ns;

		uint64_t now = SDL_GetTicksNS();
		if (now < next_poll_time)
		{
			// Sleep when delay is longer than timer resolution.
			const uint64_t delay_thresh_ns = now + timer_res_ns;
			if (delay_thresh_ns < next_poll_time)
			{
				// Leave a gap equal to the timer resolution.
				const uint64_t delay_ns = next_poll_time - delay_thresh_ns;
				ReapplyMaxTimerRes();
				SDL_DelayNS(delay_ns);
				now = SDL_GetTicksNS();
			}

			// Busy-wait for the remaining time.
			while (now < next_poll_time)
			{
				SDL_CPUPauseInstruction();
				now = SDL_GetTicksNS();
			}
		}
		else
		{
			// Fell behind.
			next_poll_time = now;
		}
	}
#else
	void DisableProcessPowerThrottling()
	{
	}

	void RaiseProcessPriority()
	{
	}

	void RaiseThreadPriority()
	{
	}

	void SetMaxTimerResolution()
	{
	}

	void InitPollingTimer()
	{
	}

	void PollingTimer(uint64_t interval_ms)
	{
		SDL_DelayNS(interval_ms * 1000000ULL);
	}
#endif // _WIN32

public:
	SdlInstance()
	{
		SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS, "1");
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "1");
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS3, "1");
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE, "1");
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS, "0");
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS, "1");
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_JOYCON_HOME_LED, "0");
		SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_SWITCH_HOME_LED, "0");
		SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "1");
		SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
		SDL_SetHintWithPriority(SDL_HINT_TIMER_RESOLUTION, "1", SDL_HINT_OVERRIDE);
		SDL_Init(SDL_INIT_GAMEPAD);
		SetMaxTimerResolution();
	}

	virtual ~SdlInstance()
	{
		SDL_Quit();
	}

	// --- Steam Controller 2026 (Triton) firmware settings -------------------
	//
	// The grip sensors and the trackpad touch gate are NOT analog on the host
	// side: the Triton HID report carries them as plain bits
	// (TRITON_LEFT/RIGHT_GRIP_TOUCH, TRITON_LEFT/RIGHT_TOUCHPAD_TOUCH). What IS
	// adjustable is the threshold the controller's own firmware uses to decide
	// those bits, which is what Steam Input exposes as a grip range. Those
	// thresholds are written with an ID_SET_SETTINGS_VALUES feature report --
	// the same mechanism SDL's own driver uses to turn off lizard mode -- and
	// SDL's Triton driver forwards any 64-byte payload straight through
	// SDL_SendGamepadEffect, so JSM can set them without patching SDL.
	//
	// Grip calibration uses Triton's setting namespace. The older generic SDL
	// controller enum's TIMP entries do not address these grip sensors.
	static constexpr uint8_t TRITON_ID_SET_SETTINGS_VALUES = 0x87;

	// Grip haptics ride an OUTPUT report rather than a feature report. SDL's
	// SendJoystickEffect only forwarded feature reports until the build's SDL patch
	// let the 0x80..0x85 haptic reports through; see cmake/PatchSdlTritonTouch.cmake.
	//
	// The command report asks the controller for one of its own canned effects
	// rather than describing a waveform by hand, which is what makes it a crisp
	// audible tap instead of a faint tick:
	//
	//   [0] report id 0x82
	//   [1] side, 0x01 = left, 0x02 = right, 0x03 = both
	//   [2] effect, 0 = off, 1 = tick, 2 = click, 3 = tone, 4 = rumble ...
	//   [3] gain in dB, signed, and allowed to be positive
	static constexpr uint8_t TRITON_ID_OUT_REPORT_HAPTIC_COMMAND = 0x82;
	static constexpr int TRITON_HAPTIC_COMMAND_BYTES = 4;
	static constexpr uint8_t TRITON_HAPTIC_SIDE_LEFT = 0x01;
	static constexpr uint8_t TRITON_HAPTIC_SIDE_RIGHT = 0x02;
	static constexpr int TRITON_FEATURE_REPORT_BYTES = 64;

	// Builds the payload SDL's FeatureReportMsg describes and hands it to the
	// driver:
	//
	//   [0]     HID report id, always 1
	//   [1]     FeatureReportHeader.type
	//   [2]     FeatureReportHeader.length, in bytes
	//   [3...]  ControllerSetting { uint8 settingNum; uint16 settingValue; }
	//
	// The leading report id is easy to miss -- it is not part of FeatureReportMsg,
	// SDL's own SetSensorsEnabled writes it by building the message at buffer + 1 --
	// and without it every field lands one byte early and the controller ignores
	// the report. That is exactly what happened the first time: setting a grip
	// range changed nothing on the device.
	//
	// ControllerSetting is 3 bytes because the whole header is inside a
	// #pragma pack(1) region, so the layout is written out by hand here rather
	// than depending on SDL's private headers.
	static constexpr uint8_t TRITON_HID_REPORT_ID = 1;
	static constexpr size_t TRITON_SETTING_BYTES = 3;

	static bool sendTritonSettings(SDL_Gamepad *gamepad, const vector<pair<uint8_t, uint16_t>> &settings)
	{
		if (gamepad == nullptr || settings.empty())
			return false;

		uint8_t buffer[TRITON_FEATURE_REPORT_BYTES] = { 0 };
		buffer[0] = TRITON_HID_REPORT_ID;
		buffer[1] = TRITON_ID_SET_SETTINGS_VALUES;
		size_t offset = 3;
		size_t written = 0;
		for (const auto &setting : settings)
		{
			if (offset + TRITON_SETTING_BYTES > sizeof(buffer))
				break;
			buffer[offset++] = setting.first;
			buffer[offset++] = uint8_t(setting.second & 0xFF);
			buffer[offset++] = uint8_t((setting.second >> 8) & 0xFF);
			++written;
		}
		// Length counts only the settings, not the header, and must match what was
		// actually written rather than what was asked for.
		buffer[2] = uint8_t(written * TRITON_SETTING_BYTES);
		return SDL_SendGamepadEffect(gamepad, buffer, int(sizeof(buffer)));
	}

	static bool sendTurnOffController(SDL_Gamepad *gamepad)
	{
		if (gamepad == nullptr)
			return false;

		const auto report = steam_controller_2026::powerOffReport();
		return SDL_SendGamepadEffect(gamepad, report.data(), int(report.size()));
	}

	// Plays one of the controller's own effects. side is a bitmask (1 = left,
	// 2 = right, 3 = both), effect indexes the firmware's own list, and gainDb is a
	// signed decibel gain the firmware limits rather than clips -- which is why
	// positive values are legitimate and a hand-rolled waveform was never going to
	// match the tap Steam plays while calibrating the grips.
	static bool sendHapticEffect(SDL_Gamepad *gamepad, int side, int effect, int gainDb)
	{
		if (gamepad == nullptr)
			return false;

		uint8_t buffer[TRITON_HAPTIC_COMMAND_BYTES] = { 0 };
		buffer[0] = TRITON_ID_OUT_REPORT_HAPTIC_COMMAND;
		buffer[1] = uint8_t(std::clamp(side, 1, 3));
		buffer[2] = uint8_t(std::clamp(effect, 0, 7));
		buffer[3] = uint8_t(int8_t(std::clamp(gainDb, -128, 127)));
		return SDL_SendGamepadEffect(gamepad, buffer, int(sizeof(buffer)));
	}

	// The automatic pulse when a grip sensor trips, as opposed to one a binding
	// asked for. Intensity is a 0-100 dial rather than raw decibels because it is
	// the only haptic a user meets without choosing an effect by name; hapticGainDb
	// is the shared mapping, so the pad ticks agree on what a given dial feels like.
	static void sendGripHaptic(SDL_Gamepad *gamepad, bool rightSide, float intensity, HapticEffect effect)
	{
		if (intensity <= 0.f || effect == HapticEffect::OFF || effect == HapticEffect::INVALID)
			return;

		sendHapticEffect(gamepad, rightSide ? TRITON_HAPTIC_SIDE_RIGHT : TRITON_HAPTIC_SIDE_LEFT,
		  uint8_t(effect), hapticGainDb(intensity));
	}

	// Pulses whichever grip sensor just changed state. Edge-triggered on purpose:
	// a level-triggered pulse would buzz continuously for as long as you held (or
	// released) the controller. Contact and release use independent settings so
	// either can be tuned or turned off without affecting the other.
	void updateGripHaptics(ControllerDevice *device)
	{
		if (device == nullptr || device->_sdlController == nullptr ||
		    device->_ctrlr_type != JS_TYPE_STEAM_CONTROLLER_2026)
		{
			return;
		}

		const float intensity = SettingsManager::get<float>(SettingID::GRIP_HAPTIC_INTENSITY)->value();
		const HapticEffect effect = SettingsManager::get<HapticEffect>(SettingID::GRIP_HAPTIC_EFFECT)->value();
		const float releaseIntensity = SettingsManager::get<float>(SettingID::GRIP_RELEASE_HAPTIC_INTENSITY)->value();
		const HapticEffect releaseEffect = SettingsManager::get<HapticEffect>(SettingID::GRIP_RELEASE_HAPTIC_EFFECT)->value();
		const bool left = SDL_GetGamepadCapSense(device->_sdlController, SDL_GAMEPAD_CAPSENSE_LEFT_GRIP);
		const bool right = SDL_GetGamepadCapSense(device->_sdlController, SDL_GAMEPAD_CAPSENSE_RIGHT_GRIP);

		if (left && !device->_leftGripWasOn)
			sendGripHaptic(device->_sdlController, false, intensity, effect);
		else if (!left && device->_leftGripWasOn)
			sendGripHaptic(device->_sdlController, false, releaseIntensity, releaseEffect);

		if (right && !device->_rightGripWasOn)
			sendGripHaptic(device->_sdlController, true, intensity, effect);
		else if (!right && device->_rightGripWasOn)
			sendGripHaptic(device->_sdlController, true, releaseIntensity, releaseEffect);

		// Tracked even when haptics are off, so turning them on mid-session doesn't
		// fire for a hand that was already resting there.
		device->_leftGripWasOn = left;
		device->_rightGripWasOn = right;
	}

	// Pushes grip range / touch gate to the controller when the user changes them.
	// Only writes on an actual change: a feature report is a round trip to the
	// device and has no business running every poll.
	void applyTritonSettings(ControllerDevice *device)
	{
		if (device == nullptr || device->_sdlController == nullptr ||
		    device->_ctrlr_type != JS_TYPE_STEAM_CONTROLLER_2026)
		{
			return;
		}

		// Negative means "leave the firmware's own value alone" -- the settings
		// default to that, so a fresh install never overwrites thresholds the
		// device (or Steam) already had, and the pads keep working out of the box.
		const int range = triton_grip::threshold(SettingsManager::get<float>(SettingID::GRIP_SENSOR_RANGE)->value());
		const int guard = triton_grip::hysteresis(SettingsManager::get<float>(SettingID::GRIP_FLICKER_GUARD)->value());
		const auto pending = triton_grip::pending(range, guard,
		    device->_appliedGripRange, device->_appliedGripRelease);

		if (pending.empty())
			return;

		if (sendTritonSettings(device->_sdlController, pending))
		{
			device->_appliedGripRange = range;
			device->_appliedGripRelease = guard;
		}
		// On failure the cached values stay stale, so the next poll retries.
	}

	int pollDevices()
	{
		RaiseThreadPriority();
		InitPollingTimer();

		while (keep_polling)
		{
			auto tick_time = SettingsManager::get<float>(SettingID::TICK_TIME)->value();
			PollingTimer(uint64_t(tick_time));

			lock_guard guard(controller_lock);
			SDL_UpdateGamepads();
			for (auto iter = _controllerMap.begin(); iter != _controllerMap.end(); ++iter)
			{
				// No-op unless a grip range / flicker guard actually changed.
				applyTritonSettings(iter->second);
				updateGripHaptics(iter->second);
				if (g_callback)
				{
					JOY_SHOCK_STATE dummy1;
					IMU_STATE dummy2;
					memset(&dummy1, 0, sizeof(dummy1));
					memset(&dummy2, 0, sizeof(dummy2));
					g_callback(iter->first, dummy1, dummy1, dummy2, dummy2, tick_time);
				}
				if (g_touch_callback)
				{
					TOUCH_STATE touch = GetTouchState(iter->first, false);
					g_touch_callback(iter->first, touch, iter->second->_prevTouchState, tick_time);
					iter->second->_prevTouchState = touch;
				}
				// Perform rumble
				SDL_RumbleGamepad(iter->second->_sdlController, iter->second->_big_rumble, iter->second->_small_rumble, Uint32(tick_time + 5));
			}

			// One mouse event per tick, no matter how many sources moved it: both
			// pads in mouse mode plus gyro used to emit three separate OS events
			// microseconds apart, all describing one intended cursor position.
			flushMouseMotion();
		}

		return 1;
	}

	std::vector<SDL_JoystickID> _joysticks;
	map<int, ControllerDevice *> _controllerMap;
	void (*g_callback)(int, JOY_SHOCK_STATE, JOY_SHOCK_STATE, IMU_STATE, IMU_STATE, float) = nullptr;
	void (*g_touch_callback)(int, TOUCH_STATE, TOUCH_STATE, float) = nullptr;
	atomic_bool keep_polling = false;
	mutex controller_lock;

	void RefreshDeviceList()
	{
		{
			std::lock_guard guard(controller_lock);
			SDL_PumpEvents();
			SDL_UpdateJoysticks();
			SDL_UpdateGamepads();
		}
		// Passive wait for the OS/driver to finish enumerating a device that
		// was just plugged in -- nothing here needs synchronizing against the
		// poll loop. Previously controller_lock was held across this delay
		// too, which meant every caller -- including GetDeviceCount(), which
		// AutoConnect calls unconditionally every 1000ms for the life of the
		// session -- blocked pollDevices()'s entire per-tick body (touch,
		// gyro, stick processing, and the mouse flush that all three funnel
		// through) for a guaranteed 20ms, once a second, forever. That is
		// exactly the beat and the shape ("distance between the two cursors
		// scales with speed") of the reported touch/gyro/stick-aim "double
		// cursor" jitter, and it ran regardless of AutoLoad's state.
		SDL_Delay(20);
		{
			std::lock_guard guard(controller_lock);
			SDL_PumpEvents();
			SDL_UpdateJoysticks();
			SDL_UpdateGamepads();
		}
	}

	int ConnectDevices() override
	{
		return ConnectDevices({});
	}

	int ConnectDevices(const std::vector<int>& selectedDeviceIds) override
	{
		DisableProcessPowerThrottling();
		RaiseProcessPriority();

		bool isFalse = false;
		if (keep_polling.compare_exchange_strong(isFalse, true))
		{
			// keep polling was false! It is set to true now.
			SDL_Thread* controller_polling_thread = SDL_CreateThread([] (void *obj)
			{
				  auto this_ = static_cast<SdlInstance *>(obj);
				  return this_->pollDevices();
			  },
			  "Poll Devices", this);
			SDL_DetachThread(controller_polling_thread);
		}
		RefreshDeviceList(); // Refresh driver listing
		int count = 0;
		SDL_JoystickID *joysticksArray = SDL_GetJoysticks(&count);
		_joysticks.clear();
		_joysticks.reserve(count);

		for (int i = 0; i < count; ++i)
		{
			SDL_JoystickID joystickId = joysticksArray[i];
			if (!selectedDeviceIds.empty() &&
				std::find(selectedDeviceIds.begin(), selectedDeviceIds.end(), int(joystickId)) == selectedDeviceIds.end())
			{
				continue;
			}
			_joysticks.push_back(joystickId);
		}

		SDL_free(joysticksArray);
		return int(_joysticks.size());
	}

	int GetDeviceCount() override
	{
		// RefreshDeviceList() locks internally, per phase, so that its
		// passive 20ms settle delay doesn't hold controller_lock against the
		// poll loop. Only the final read needs the lock here.
		RefreshDeviceList();
		std::lock_guard guard(controller_lock);
		int count = 0;
		SDL_JoystickID *joysticksArray = SDL_GetJoysticks(&count);
		SDL_free(joysticksArray);
		return count;
	}

	std::vector<ControllerInfo> ListAvailableDevices() override
	{
		RefreshDeviceList();
		std::lock_guard guard(controller_lock);

		int count = 0;
		SDL_JoystickID *joysticksArray = SDL_GetJoysticks(&count);
		std::vector<ControllerInfo> devices;
		devices.reserve(count);

		for (int i = 0; i < count; ++i)
		{
			SDL_JoystickID joystickId = joysticksArray[i];
			bool isGamepad = SDL_IsGamepad(joystickId);
			const char *name = isGamepad ? SDL_GetGamepadNameForID(joystickId) : SDL_GetJoystickNameForID(joystickId);
			int vendorId = isGamepad ? SDL_GetGamepadVendorForID(joystickId) : SDL_GetJoystickVendorForID(joystickId);
			int productId = isGamepad ? SDL_GetGamepadProductForID(joystickId) : SDL_GetJoystickProductForID(joystickId);

			devices.push_back({
				int(joystickId),
				vendorId,
				productId,
				isGamepad,
				name != nullptr ? name : "Unknown controller"
			});
		}

		SDL_free(joysticksArray);
		return devices;
	}

	int GetConnectedDeviceHandles(int *deviceHandleArray, int size) override
	{
		lock_guard guard(controller_lock);
		auto iter = _controllerMap.begin();
		while (iter != _controllerMap.end())
		{
			delete iter->second;
			iter = _controllerMap.erase(iter);
		}
		int limit = std::min(size, int(_joysticks.size()));
		for (int i = 0; i < limit; i++)
		{
			ControllerDevice *device = new ControllerDevice(_joysticks[i]);
			if (device->isValid())
			{
				deviceHandleArray[i] = i + 1;
				_controllerMap[deviceHandleArray[i]] = device;
			}
			else
			{
                deviceHandleArray[i] = -1;
				delete device;
			}
		}
		for (int i = limit; i < size; i++)
		{
			deviceHandleArray[i] = -1;
		}
		return int(_controllerMap.size());
	}

	void DisconnectAndDisposeAll() override
	{
		lock_guard guard(controller_lock);
		keep_polling = false;
		g_callback = nullptr;
		g_touch_callback = nullptr;
		auto iter = _controllerMap.begin();
		while (iter != _controllerMap.end())
		{
			delete iter->second;
			iter = _controllerMap.erase(iter);
		}
		_joysticks.clear();
		SDL_Delay(200);
	}

	JOY_SHOCK_STATE GetSimpleState(int deviceId) override
	{
		return JOY_SHOCK_STATE();
	}

	IMU_STATE GetIMUState(int deviceId) override
	{
		IMU_STATE imuState;
		memset(&imuState, 0, sizeof(imuState));
		if (_controllerMap[deviceId]->_has_gyro)
		{
			array<float, 3> gyro;
			SDL_GetGamepadSensorData(_controllerMap[deviceId]->_sdlController, SDL_SENSOR_GYRO, &gyro[0], 3);
			static constexpr float toDegPerSec = float(180. / M_PI);
			imuState.gyroX = gyro[0] * toDegPerSec;
			imuState.gyroY = gyro[1] * toDegPerSec;
			imuState.gyroZ = gyro[2] * toDegPerSec;
		}
		if (_controllerMap[deviceId]->_has_accel)
		{
			array<float, 3> accel;
			SDL_GetGamepadSensorData(_controllerMap[deviceId]->_sdlController, SDL_SENSOR_ACCEL, &accel[0], 3);
			static constexpr float toGs = 1.f / SDL_STANDARD_GRAVITY;
			imuState.accelX = accel[0] * toGs;
			imuState.accelY = accel[1] * toGs;
			imuState.accelZ = accel[2] * toGs;
		}
		return imuState;
	}

	MOTION_STATE GetMotionState(int deviceId) override
	{
		return MOTION_STATE();
	}

	TOUCH_STATE GetTouchState(int deviceId, bool previous) override
	{
		TOUCH_STATE state;
		memset(&state, 0, sizeof(TOUCH_STATE));

		if (_controllerMap[deviceId] == nullptr ||
			_controllerMap[deviceId]->_sdlController == nullptr ||
			SDL_GetNumGamepadTouchpads(_controllerMap[deviceId]->_sdlController) <= 0)
		{
			return state;
		}

		bool isSteam = _controllerMap[deviceId]->_ctrlr_type == JS_TYPE_STEAM_CONTROLLER_2026;
		// Steam Controller 2026: two single-finger physical pads
		// DS4/DualSense: one pad with two fingers
		if (isSteam && SDL_GetNumGamepadTouchpads(_controllerMap[deviceId]->_sdlController) >= 2)
		{
			// Left pad = touchpad index 0, Right pad = touchpad index 1
			// Pressure is exposed only for the telemetry readout.
			float pressure0 = 0.f, pressure1 = 0.f;
			SDL_GetGamepadTouchpadFinger(_controllerMap[deviceId]->_sdlController, 0, 0, &state.t0Down, &state.t0X, &state.t0Y, &pressure0);
			SDL_GetGamepadTouchpadFinger(_controllerMap[deviceId]->_sdlController, 1, 0, &state.t1Down, &state.t1X, &state.t1Y, &pressure1);
			state.t0Pressure = pressure0;
			state.t1Pressure = pressure1;
			// SDL uses the firmware's capacitive contact bit. Pressure is a readout,
			// not a second way to keep contact alive after the finger lifts.

		}
		else
		{
			if (!SDL_GetGamepadTouchpadFinger(_controllerMap[deviceId]->_sdlController, 0, 0, &state.t0Down, &state.t0X, &state.t0Y, nullptr) || 
				!SDL_GetGamepadTouchpadFinger(_controllerMap[deviceId]->_sdlController, 0, 1, &state.t1Down, &state.t1X, &state.t1Y, nullptr))
			{
				CERR << "Cannot get finger state: " << SDL_GetError() << '\n';
			}
		}
		return state;
	}

	bool GetStickTouch(int deviceId, bool rightStick) override
	{
		auto *jc = _controllerMap[deviceId];
		if (jc == nullptr || jc->_sdlController == nullptr)
			return false;
		return SDL_GetGamepadCapSense(jc->_sdlController,
		  rightStick ? SDL_GAMEPAD_CAPSENSE_RIGHT_STICK : SDL_GAMEPAD_CAPSENSE_LEFT_STICK);
	}

	bool GetTouchpadDimension(int deviceId, int &sizeX, int &sizeY) override
	{
		// I am assuming a single touchpad (or all _touchpads are the same dimension)?
		auto *jc = _controllerMap[deviceId];
		if (jc != nullptr)
		{
			switch (_controllerMap[deviceId]->_ctrlr_type)
			{
			case JS_TYPE_DS4:
			case JS_TYPE_DS:
				// Matching SDL resolution
				sizeX = 1920;
				sizeY = 920;
				break;
			case JS_TYPE_STEAM_CONTROLLER_2026:
				// The 2026 pads are square (the front artwork measures 102.1 x 101.9
				// units per pad). Reusing the DS4's 1920x920 made vertical gain 2.09x
				// lower than horizontal, so a physical 45 degree swipe came out at
				// roughly 64 degrees. X is left at 1920 so existing horizontal
				// sensitivity values keep their feel; vertical is now correct, which
				// means TOUCHPAD_SENS Y may need roughly halving from old configs.
				sizeX = 1920;
				sizeY = 1920;
				break;
			default:
				sizeX = 0;
				sizeY = 0;
				break;
			}
			return true;
		}
		return false;
	}

	uint64_t GetButtons(int deviceId) override { return ReadButtons(deviceId, false); }
    uint64_t GetSupportedButtons(int deviceId) override { return ReadButtons(deviceId, true); }
    uint64_t ReadButtons(int deviceId, bool supported)
    {
        auto readButton = [&](SDL_GamepadButton button) {
            auto pad = _controllerMap[deviceId]->_sdlController;
            return supported ? SDL_GamepadHasButton(pad, button) : SDL_GetGamepadButton(pad, button);
        };
		static const map<int, uint64_t> sdl2jsl = {
			{ SDL_GAMEPAD_BUTTON_SOUTH, JSOFFSET_S },
			{ SDL_GAMEPAD_BUTTON_EAST, JSOFFSET_E },
			{ SDL_GAMEPAD_BUTTON_WEST, JSOFFSET_W },
			{ SDL_GAMEPAD_BUTTON_NORTH, JSOFFSET_N },
			{ SDL_GAMEPAD_BUTTON_BACK, JSOFFSET_MINUS },
			{ SDL_GAMEPAD_BUTTON_GUIDE, JSOFFSET_HOME },
			{ SDL_GAMEPAD_BUTTON_START, JSOFFSET_PLUS },
			{ SDL_GAMEPAD_BUTTON_LEFT_STICK, JSOFFSET_LCLICK },
			{ SDL_GAMEPAD_BUTTON_RIGHT_STICK, JSOFFSET_RCLICK },
			{ SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, JSOFFSET_L },
			{ SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, JSOFFSET_R },
			{ SDL_GAMEPAD_BUTTON_DPAD_UP, JSOFFSET_UP },
			{ SDL_GAMEPAD_BUTTON_DPAD_DOWN, JSOFFSET_DOWN },
			{ SDL_GAMEPAD_BUTTON_DPAD_LEFT, JSOFFSET_LEFT },
			{ SDL_GAMEPAD_BUTTON_DPAD_RIGHT, JSOFFSET_RIGHT }
		};

		uint64_t buttons = 0;
		for (auto pair : sdl2jsl)
		{
			buttons |= readButton(SDL_GamepadButton(pair.first)) ? 1ULL << pair.second : 0;

		}
		switch (_controllerMap[deviceId]->_ctrlr_type)
		{
		case JS_TYPE_JOYCON_LEFT:
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC1) ? 1ULL << JSOFFSET_CAPTURE : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1) ? 1ULL << JSOFFSET_SL : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2) ? 1ULL << JSOFFSET_SR : 0;
			break;
		case JS_TYPE_JOYCON_RIGHT:
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1) ? 1ULL << JSOFFSET_SR : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2) ? 1ULL << JSOFFSET_SL : 0;
			break;
		case JS_TYPE_PRO_CONTROLLER:
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC1) ? 1ULL << JSOFFSET_CAPTURE : 0;
			break;
		case JS_TYPE_SWITCH2_PRO_CONTROLLER:
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC1) ? 1ULL << JSOFFSET_CAPTURE : 0;    // Capture button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1) ? 1ULL << JSOFFSET_SR : 0; // GR back button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1) ? 1ULL << JSOFFSET_SL : 0;  // GL back button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC2) ? 1ULL << JSOFFSET_MISC1 : 0;      // C button
			break;
		case JS_TYPE_STEAM_CONTROLLER_2026:
		{
			SDL_Joystick *joy = SDL_GetGamepadJoystick(_controllerMap[deviceId]->_sdlController);
			// QAM button (raw index 11)
			buttons |= (supported ? SDL_GetNumJoystickButtons(joy) > 11 : SDL_GetJoystickButton(joy, 11)) ? 1ULL << JSOFFSET_MISC1 : 0;
			// Four paddles (raw indices 12-15)
			buttons |= (supported ? SDL_GetNumJoystickButtons(joy) > 12 : SDL_GetJoystickButton(joy, 12)) ? 1ULL << JSOFFSET_SR : 0;
			buttons |= (supported ? SDL_GetNumJoystickButtons(joy) > 13 : SDL_GetJoystickButton(joy, 13)) ? 1ULL << JSOFFSET_SL : 0;
			buttons |= (supported ? SDL_GetNumJoystickButtons(joy) > 14 : SDL_GetJoystickButton(joy, 14)) ? 1ULL << JSOFFSET_FNR : 0;
			buttons |= (supported ? SDL_GetNumJoystickButtons(joy) > 15 : SDL_GetJoystickButton(joy, 15)) ? 1ULL << JSOFFSET_FNL : 0;
			// Right pad click (raw index 16), Left pad click (raw index 17)
			buttons |= (supported ? SDL_GetNumJoystickButtons(joy) > 16 : SDL_GetJoystickButton(joy, 16)) ? 1ULL << JSOFFSET_MISC2 : 0;
			buttons |= (supported ? SDL_GetNumJoystickButtons(joy) > 17 : SDL_GetJoystickButton(joy, 17)) ? 1ULL << JSOFFSET_MISC3 : 0;
			// Stick capacitive touch (LTOUCH/RTOUCH already exposed via cap-sense)
			// Grip sensors are capacitive contact bits, not an analog channel: the
			// Triton report carries them as TRITON_LEFT/RIGHT_GRIP_TOUCH inside
			// the button field, which SDL surfaces through the capacitive-sense
			// API rather than as a joystick button or axis. How near a hand has to
			// come before that bit trips is set in the controller's firmware, from
			// GRIP_SENSOR_RANGE / GRIP_FLICKER_GUARD (see applyTritonSettings) --
			// the pair behind Steam Input's Grip Sensor Calibration page. Nothing
			// to threshold here; just read the bit.
			buttons |= (supported || SDL_GetGamepadCapSense(_controllerMap[deviceId]->_sdlController, SDL_GAMEPAD_CAPSENSE_LEFT_GRIP)) ? 1ULL << JSOFFSET_MISC6 : 0;
			buttons |= (supported || SDL_GetGamepadCapSense(_controllerMap[deviceId]->_sdlController, SDL_GAMEPAD_CAPSENSE_RIGHT_GRIP)) ? 1ULL << JSOFFSET_MISC5 : 0;
		}
		break;
		case JS_TYPE_DS:
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC1) ? 1ULL << JSOFFSET_MIC : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_TOUCHPAD) ? 1ULL << JSOFFSET_CAPTURE : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1) ? 1ULL << JSOFFSET_SR : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1) ? 1ULL << JSOFFSET_SL : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2) ? 1ULL << JSOFFSET_FNR : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2) ? 1ULL << JSOFFSET_FNL : 0;
			break;
		case JS_TYPE_DS4:
			buttons |= readButton(SDL_GAMEPAD_BUTTON_TOUCHPAD) ? 1ULL << JSOFFSET_CAPTURE : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1) ? 1ULL << JSOFFSET_SL : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2) ? 1ULL << JSOFFSET_SR : 0;
			break;
		case JS_TYPE_HORI_STEAM:
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1) ? 1ULL << JSOFFSET_SR : 0;  // R4 back button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1) ? 1ULL << JSOFFSET_SL : 0;   // L4 back button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2) ? 1ULL << JSOFFSET_FNR : 0; // M2 button below right stick
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2) ? 1ULL << JSOFFSET_FNL : 0;  // M1 button below left stick
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC2) ? 1ULL << JSOFFSET_MISC1 : 0;       // QAM button ("..." button)
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC3) ? 1ULL << JSOFFSET_LTOUCH : 0;      // Left stick capacitive touch
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC4) ? 1ULL << JSOFFSET_RTOUCH : 0;      // Right stick capacitive touch
			break;
		case JS_TYPE_G7_PRO_8K:
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC1) ? 1ULL << JSOFFSET_CAPTURE : 0;     // Share button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1) ? 1ULL << JSOFFSET_SR : 0;  // R4 back button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1) ? 1ULL << JSOFFSET_SL : 0;   // L4 back button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC2) ? 1ULL << JSOFFSET_LMINI : 0;       // L5 mini shoulder button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC3) ? 1ULL << JSOFFSET_RMINI : 0;       // R5 mini shoulder button
			break;
		// 8BitDo controllers with gyro and no additional buttons.
		case JS_TYPE_8BITDO_SF30_PRO:
		case JS_TYPE_8BITDO_SF30_PRO_BT:
		case JS_TYPE_8BITDO_SN30_PRO:
		case JS_TYPE_8BITDO_SN30_PRO_BT:
			break;
		// 8BitDo controllers with gyro and two additional buttons.
		case JS_TYPE_8BITDO_PRO_2:
		case JS_TYPE_8BITDO_PRO_2_BT:
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1) ? 1ULL << JSOFFSET_SR : 0; // P1 back button (right)
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1) ? 1ULL << JSOFFSET_SL : 0;  // P2 back button (left)
			break;
		// 8BitDo controllers with gyro and four additional buttons.
		case JS_TYPE_8BITDO_PRO_3:
		case JS_TYPE_8BITDO_ULTIMATE2_WIRELESS:
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1) ? 1ULL << JSOFFSET_RMINI : 0; // R4 mini shoulder button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1) ? 1ULL << JSOFFSET_LMINI : 0;  // L4 mini shoulder button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2) ? 1ULL << JSOFFSET_SR : 0;    // PR back button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2) ? 1ULL << JSOFFSET_SL : 0;     // PL back button
			break;
		case JS_TYPE_FLYDIGI_APEX5:
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1) ? 1ULL << JSOFFSET_SR : 0;  // M1 back button (top right)
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1) ? 1ULL << JSOFFSET_SL : 0;   // M2 back button (top left)
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2) ? 1ULL << JSOFFSET_FNR : 0; // M3 back button (bottom left)
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2) ? 1ULL << JSOFFSET_FNL : 0;  // M4 back button (bottom right)
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC2) ? 1ULL << JSOFFSET_LMINI : 0;       // LM mini shoulder button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC3) ? 1ULL << JSOFFSET_RMINI : 0;       // RM mini shoulder button
			break;
		case JS_TYPE_FLYDIGI_VADER5_PRO:
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC4) ? 1ULL << JSOFFSET_LMINI : 0;       // LM mini shoulder button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC5) ? 1ULL << JSOFFSET_RMINI : 0;       // RM mini shoulder button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC6) ? 1ULL << JSOFFSET_MISC3 : 0;       // Circle button below right stick
			// Fall through.
		case JS_TYPE_FLYDIGI_VADER4_PRO:
		case JS_TYPE_FLYDIGI_VADER3_PRO:
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1) ? 1ULL << JSOFFSET_SR : 0;  // M1 back button (top right)
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1) ? 1ULL << JSOFFSET_SL : 0;   // M2 back button (top left)
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2) ? 1ULL << JSOFFSET_FNR : 0; // M3 back button (bottom left)
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2) ? 1ULL << JSOFFSET_FNL : 0;  // M4 back button (bottom right)
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC2) ? 1ULL << JSOFFSET_MISC1 : 0;       // C face button
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC3) ? 1ULL << JSOFFSET_MISC2 : 0;       // Z face button
			break;
		default:
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC1) ? 1ULL << JSOFFSET_MISC1 : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1) ? 1ULL << JSOFFSET_SR : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1) ? 1ULL << JSOFFSET_SL : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2) ? 1ULL << JSOFFSET_FNR : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2) ? 1ULL << JSOFFSET_FNL : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC2) ? 1ULL << JSOFFSET_MISC2 : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC3) ? 1ULL << JSOFFSET_MISC3 : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC4) ? 1ULL << JSOFFSET_MISC4 : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC5) ? 1ULL << JSOFFSET_MISC5 : 0;
			buttons |= readButton(SDL_GAMEPAD_BUTTON_MISC6) ? 1ULL << JSOFFSET_MISC6 : 0;
			break;
		}
		return buttons;
	}

	float GetLeftX(int deviceId) override
	{
		return SDL_GetGamepadAxis(_controllerMap[deviceId]->_sdlController, SDL_GAMEPAD_AXIS_LEFTX) / (float)SDL_JOYSTICK_AXIS_MAX;
	}

	float GetLeftY(int deviceId) override
	{
		return -SDL_GetGamepadAxis(_controllerMap[deviceId]->_sdlController, SDL_GAMEPAD_AXIS_LEFTY) / (float)SDL_JOYSTICK_AXIS_MAX;
	}

	float GetRightX(int deviceId) override
	{
		return SDL_GetGamepadAxis(_controllerMap[deviceId]->_sdlController, SDL_GAMEPAD_AXIS_RIGHTX) / (float)SDL_JOYSTICK_AXIS_MAX;
	}

	float GetRightY(int deviceId) override
	{
		return -SDL_GetGamepadAxis(_controllerMap[deviceId]->_sdlController, SDL_GAMEPAD_AXIS_RIGHTY) / (float)SDL_JOYSTICK_AXIS_MAX;
	}

	float GetLeftTrigger(int deviceId) override
	{
		return (SDL_GetGamepadAxis(_controllerMap[deviceId]->_sdlController, SDL_GAMEPAD_AXIS_LEFT_TRIGGER)) / (float)(SDL_JOYSTICK_AXIS_MAX);
	}

	float GetRightTrigger(int deviceId) override
	{
		return (SDL_GetGamepadAxis(_controllerMap[deviceId]->_sdlController, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)) / (float)(SDL_JOYSTICK_AXIS_MAX);
	}

	float GetGyroX(int deviceId) override
	{
		if (_controllerMap[deviceId]->_has_gyro)
		{
			float rawGyro[3];
			SDL_GetGamepadSensorData(_controllerMap[deviceId]->_sdlController, SDL_SENSOR_GYRO, rawGyro, 3);
		}
		return float();
	}

	float GetGyroY(int deviceId) override
	{
		if (_controllerMap[deviceId]->_has_gyro)
		{
			float rawGyro[3];
			SDL_GetGamepadSensorData(_controllerMap[deviceId]->_sdlController, SDL_SENSOR_GYRO, rawGyro, 3);
		}
		return float();
	}

	float GetGyroZ(int deviceId) override
	{
		if (_controllerMap[deviceId]->_has_gyro)
		{
			float rawGyro[3];
			SDL_GetGamepadSensorData(_controllerMap[deviceId]->_sdlController, SDL_SENSOR_GYRO, rawGyro, 3);
		}
		return float();
	}

	float GetAccelX(int deviceId) override
	{
		return float();
	}

	float GetAccelY(int deviceId) override
	{
		return float();
	}

	float GetAccelZ(int deviceId) override
	{
		return float();
	}

	int GetTouchId(int deviceId, bool secondTouch = false) override
	{
		return int();
	}

	bool GetTouchDown(int deviceId, bool secondTouch)
	{
		bool touchState = 0;
		return SDL_GetGamepadTouchpadFinger(_controllerMap[deviceId]->_sdlController, 0, secondTouch ? 1 : 0, &touchState, nullptr, nullptr, nullptr) ? touchState : false;
	}

	float GetTouchX(int deviceId, bool secondTouch = false) override
	{
		float x = 0;
		if (SDL_GetGamepadTouchpadFinger(_controllerMap[deviceId]->_sdlController, 0, secondTouch ? 1 : 0, nullptr, nullptr, &x, nullptr))
		{
			return x;
		}
		return x;
	}

	float GetTouchY(int deviceId, bool secondTouch = false) override
	{
		float y = 0;
		if (SDL_GetGamepadTouchpadFinger(_controllerMap[deviceId]->_sdlController, 0, secondTouch ? 1 : 0, nullptr, nullptr, &y, nullptr))
		{
			return y;
		}
		return y;
	}

	float GetStickStep(int deviceId) override
	{
		return float();
	}

	float GetTriggerStep(int deviceId) override
	{
		return float();
	}

	float GetPollRate(int deviceId) override
	{
		return float();
	}

	float GetSampleRateHz(int deviceId) override
	{
		return 0.0f;
	}

	float GetTimeSinceLastUpdate(int deviceId) override
	{
		return 0.0f;
	}

	void ResetContinuousCalibration(int deviceId) override
	{
	}

	void StartContinuousCalibration(int deviceId) override
	{
	}

	void PauseContinuousCalibration(int deviceId) override
	{
	}

	void GetCalibrationOffset(int deviceId, float &xOffset, float &yOffset, float &zOffset) override
	{
	}

	void SetCalibrationOffset(int deviceId, float xOffset, float yOffset, float zOffset) override
	{
	}

	void SetCallback(void (*callback)(int, JOY_SHOCK_STATE, JOY_SHOCK_STATE, IMU_STATE, IMU_STATE, float)) override
	{
		lock_guard guard(controller_lock);
		g_callback = callback;
	}

	void SetTouchCallback(void (*callback)(int, TOUCH_STATE, TOUCH_STATE, float)) override
	{
		lock_guard guard(controller_lock);
		g_touch_callback = callback;
	}

	int GetControllerType(int deviceId) override
	{
		return _controllerMap[deviceId]->_ctrlr_type;
	}

	int GetControllerSplitType(int deviceId) override
	{
		return _controllerMap[deviceId]->_split_type;
	}

	int GetControllerVendor(int deviceId) override
	{
		return _controllerMap[deviceId]->_vendorId;
	}

	int GetControllerProduct(int deviceId) override
	{
		return _controllerMap[deviceId]->_productId;
	}

	int GetControllerColour(int deviceId) override
	{
		return int();
	}

	void SetLightColour(int deviceId, int colour) override
	{
		auto prop = SDL_GetGamepadProperties(_controllerMap[deviceId]->_sdlController);
		
		if (SDL_GetStringProperty(prop, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN, nullptr) != nullptr)
		{
			union
			{
				uint32_t raw;
				uint8_t argb[4];
			} uColour;
			uColour.raw = colour;
			SDL_SetGamepadLED(_controllerMap[deviceId]->_sdlController, uColour.argb[2], uColour.argb[1], uColour.argb[0]);
		}
	}

	void SetHaptic(int deviceId, int side, int effect, int gainDb) override
	{
		auto *jc = _controllerMap[deviceId];
		if (jc == nullptr || jc->_ctrlr_type != JS_TYPE_STEAM_CONTROLLER_2026)
			return;
		sendHapticEffect(jc->_sdlController, side, effect, gainDb);
	}

	bool TurnOffController(int deviceId) override
	{
		auto *jc = _controllerMap[deviceId];
		if (jc == nullptr)
		{
			CERR << "TURN_OFF_CONTROLLER: no controller with id " << deviceId << ".\n";
			return false;
		}
		if (jc->_ctrlr_type != JS_TYPE_STEAM_CONTROLLER_2026)
		{
			CERR << "TURN_OFF_CONTROLLER: controller " << deviceId
			     << " does not support a power-off command; only the Steam Controller does.\n";
			return false;
		}
		if (!sendTurnOffController(jc->_sdlController))
		{
			CERR << "TURN_OFF_CONTROLLER: failed to write the power-off report: "
			     << SDL_GetError() << '\n';
			return false;
		}
		return true;
	}

	void GetBatteryLevel(int deviceId, int &percent, int &state) override
	{
		percent = -1;
		state = 0;
		auto *jc = _controllerMap[deviceId];
		if (jc == nullptr || jc->_sdlController == nullptr)
			return;
		int sdlPercent = -1;
		SDL_PowerState sdlState = SDL_GetGamepadPowerInfo(jc->_sdlController, &sdlPercent);
		percent = sdlPercent;
		state = int(sdlState);
	}

	void SetRumble(int deviceId, int smallRumble, int bigRumble) override
	{
		// sendRumble command needs to be sent at every poll in SDL, so the next value is set here and the actual call
		// is done after the callback return
		_controllerMap[deviceId]->_small_rumble = clamp(smallRumble, 0, int(UINT16_MAX));
		_controllerMap[deviceId]->_big_rumble = clamp(bigRumble, 0, int(UINT16_MAX));
	}

	void SetPlayerNumber(int deviceId, int number) override
	{
		SDL_SetGamepadPlayerIndex(_controllerMap[deviceId]->_sdlController, number);
	}

	void SetTriggerEffect(int deviceId, const AdaptiveTriggerSetting &_leftTriggerEffect, const AdaptiveTriggerSetting &_rightTriggerEffect) override
	{
		if (_leftTriggerEffect != _controllerMap[deviceId]->_leftTriggerEffect || _rightTriggerEffect != _controllerMap[deviceId]->_rightTriggerEffect)
		{
			// Update active trigger effect
			_controllerMap[deviceId]->_leftTriggerEffect = _leftTriggerEffect;
			_controllerMap[deviceId]->_rightTriggerEffect = _rightTriggerEffect;
		}
		_controllerMap[deviceId]->SendEffect();
	}

	virtual void SetMicLight(int deviceId, uint8_t mode) override
	{
		if (mode != _controllerMap[deviceId]->_micLight)
		{
			_controllerMap[deviceId]->_micLight = mode;

			_controllerMap[deviceId]->SendEffect();
		}
	}
};

JslWrapper *JslWrapper::getNew()
{
	return new SdlInstance();
}
