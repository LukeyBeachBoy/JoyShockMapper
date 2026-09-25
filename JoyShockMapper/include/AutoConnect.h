#pragma once
#include "AutoConnectRetry.h"
#include "InputHelpers.h"
#include "JslWrapper.h"


namespace JSM
{

class AutoConnect : public PollingThread
{
public:
	AutoConnect(shared_ptr<JslWrapper> joyshock, bool start);
	virtual ~AutoConnect() = default;

private:
	bool AutoConnectPoll(void* param);
	/// Issues RECONNECT_CONTROLLERS and starts the settle window. Every path
	/// that reconnects goes through here, so none of them can forget to.
	void reconnect(const char* reason);

	shared_ptr<JslWrapper> jsl;
	int lastSize = 0;
	AutoConnectRetry retry;

	/// Polls to ignore the device list for after we ask for a reconnect.
	///
	/// A reconnect tears down and rebuilds everything, and when the loaded
	/// configuration sets VIRTUAL_CONTROLLER that includes destroying and
	/// recreating our own virtual pad -- which comes back as a brand new SDL
	/// device with a brand new id. Watching the device list across our own
	/// reconnect is watching our own wake: it changes, so we reconnect, so it
	/// changes. Both triggers below sit behind this window, so neither can be
	/// set off by the reconnect that preceded it.
	static constexpr int SETTLE_TICKS = 5;
	int settleTicks = 0;

	/// Polls between from-scratch rescans while no controller is connected.
	/// SDL cannot see a controller come back through a dongle that never left
	/// (see SdlInstance::RescanDevices), so while nothing is connected it is
	/// asked again this often: the wait between turning a controller on and
	/// Studio seeing it.
	static constexpr int IDLE_RESCAN_TICKS = 3;
	int idleTicks = 0;
};

} //JSM
