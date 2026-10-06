#include "SettingsManager.h"
#include <algorithm>
#include <ranges>
#include <set>

SettingsManager::SettingsMap SettingsManager::_settings;

bool SettingsManager::add(SettingID id, JSMVariableBase *setting)
{
	return _settings.emplace(id, setting).second;
}


void SettingsManager::resetAllSettings()
{
	static constexpr auto callReset = [](SettingsMap::value_type &kvPair)
	{
		if (!ControllerContext::writeScope.empty() && (kvPair.first == SettingID::TELEMETRY_ENABLED || kvPair.first == SettingID::TELEMETRY_PORT || kvPair.first == SettingID::AUTOCONNECT)) return;
        kvPair.second->reset();
	};
	static constexpr auto exceptions = [](SettingsMap::value_type &kvPair)
	{
		static set<SettingID> exceptions = {
			SettingID::AUTOLOAD,
			SettingID::JSM_DIRECTORY,
			SettingID::HIDE_MINIMIZED,
			SettingID::VIRTUAL_CONTROLLER,
			SettingID::ADAPTIVE_TRIGGER,
			SettingID::RUMBLE,
			// A firmware setting, written to the controller when it changes. A
			// reset between two profiles would flip it to the default and back,
			// two round trips on the poll thread for nothing.
			SettingID::DISABLE_HARDWARE_GYRO_CALIBRATION,
			// Global by design (Studio keeps them in StudioDefaults.txt, which a
			// reset reloads anyway). The pad is still being sampled between the
			// reset and that reload, so zeroing the rotation here would move a
			// finger that is on the pad during a profile switch; and the jingle
			// level is written to the controller's flash when it changes.
			SettingID::LEFT_TOUCHPAD_ROTATION,
			SettingID::RIGHT_TOUCHPAD_ROTATION,
			SettingID::BOOT_SOUND_LEVEL,
		};
		return exceptions.find(kvPair.first) == exceptions.end();
	};
	ranges::for_each(_settings | views::filter(exceptions), callReset);
}
