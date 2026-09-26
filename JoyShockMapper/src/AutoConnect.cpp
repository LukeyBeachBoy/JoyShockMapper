#include "AutoConnect.h"
#include "JslWrapper.h"
#include "InputHelpers.h"
#include "Gamepad.h"


namespace JSM
{

AutoConnect::AutoConnect(shared_ptr<JslWrapper> joyshock, bool start)
  : PollingThread("AutoConnect thread", std::bind(&AutoConnect::AutoConnectPoll, this, std::placeholders::_1), nullptr, 1000, start)
  , jsl(joyshock)
{
}

void AutoConnect::reconnect(const char* reason)
{
	COUT_INFO << "[AUTOCONNECT] " << reason << ". Reconnecting controllers.\n";
	settleTicks = SETTLE_TICKS;
	WriteToConsole("RECONNECT_CONTROLLERS");
}

bool AutoConnect::AutoConnectPoll(void* param)
{
	// One refresh per tick feeds both checks below. Asking twice would pay
	// RefreshDeviceList's 20ms settle twice a second, for the whole session.
	const DeviceCensus census = jsl->TakeDeviceCensus();

	// Our own virtual pads are SDL devices too, and are discounted here exactly
	// as they always have been.
	const int realSize = census.listed - int(Gamepad::getCount());

	// A controller we hold open that SDL says is gone: switched off, or out of
	// range. SDL keeps the gamepad object valid and answering with its last
	// state, so the poll goes on reporting a pad that is no longer there --
	// which is what Studio then shows. The device count does notice the loss,
	// but not inside the settle window: the chord that powers a Steam
	// Controller off follows a profile load, and a profile load that touches
	// VIRTUAL_CONTROLLER has just reconnected, so the loss was absorbed as our
	// own churn and the dead controller stayed open for the rest of the
	// session. So this is asked before the window, every tick. It cannot loop:
	// the reconnect closes the dead controller and finds nothing to reopen.
	if (census.disconnected > 0)
	{
		COUT_INFO << "[AUTOCONNECT] " << census.disconnected << " open controller(s) no longer connected.\n";
		lastSize = realSize;
		reconnect("A controller disconnected");
		return true;
	}

	// Nothing connected: SDL may simply not be able to see a controller that has
	// come back (a Steam Controller switched on through its dongle), so every few
	// polls it enumerates from scratch. It closes nothing -- nothing is open --
	// and a controller it finds is the device-count change the next poll's census
	// sees, which reconnects exactly as a plugged-in one would. Not during a
	// settle window: that churn is our own reconnect.
	if (settleTicks == 0 && realSize <= 0 && census.opened == 0)
	{
		if (++idleTicks >= IDLE_RESCAN_TICKS)
		{
			idleTicks = 0;
			jsl->RescanDevices();
			return true;
		}
	}
	else
	{
		idleTicks = 0;
	}

	// Everything below is gated on this. A reconnect churns the device list by
	// itself -- most visibly when the configuration sets VIRTUAL_CONTROLLER,
	// where our own pad is unplugged and replugged as a new device -- so for a
	// few seconds afterwards the list says nothing about the outside world.
	// Absorb whatever it does and resync, rather than reacting to our own wake.
	if (settleTicks > 0)
	{
		--settleTicks;
		lastSize = realSize;
		// The window absorbs a controller switched on inside it too: the count
		// moved, lastSize followed, and the count trigger below never sees it.
		// So the window ends by asking the one thing our own churn cannot fake
		// -- are real devices listed that the connect attempt did not try? A
		// device it tried and could not open is the retry's job, hence
		// failedToOpen == 0; and it is asked once per count change (caughtUp),
		// so a device that is listed but never ours to open -- an unselected
		// controller in manual mode -- cannot reconnect the session at the end
		// of every window. That would be the loop this file exists to prevent.
		if (settleTicks == 0 && !caughtUp && census.failedToOpen == 0 && realSize > census.opened)
		{
			caughtUp = true;
			COUT_INFO << "[AUTOCONNECT] " << (realSize - census.opened) << " device(s) arrived during the settle window.\n";
			retry.arm();
			reconnect("A controller arrived during the settle window");
		}
		return true;
	}

	// The ordinary signal: a controller was switched on or off.
	if (lastSize != realSize)
	{
		COUT_INFO << "[AUTOCONNECT] Going from " << lastSize << " devices to " << realSize << ".\n";
		lastSize = realSize;
		caughtUp = false;
		retry.arm();
		reconnect("Device count changed");
		return true;
	}

	// The last connect attempt opened everything it tried. The healthy steady
	// state: cost nothing, touch nothing.
	if (census.failedToOpen == 0)
	{
		retry.clear();
		if (realSize <= census.opened)
		{
			caughtUp = false;
		}
		return true;
	}

	// The last connect attempt left a device behind. A reconnect that opens
	// nothing leaves the device count exactly where it was, so the trigger
	// above cannot see that case; this is the one that can.
	//
	// Note this asks what the CONNECT ATTEMPT failed to open, not what SDL
	// lists that we do not hold. The virtual pad our own configuration creates
	// is listed but never opened, permanently, so the second question burns the
	// whole budget chasing a device that is ours and is fine.
	//
	// The budget comes only from the count trigger above -- nothing here ever
	// refills it. That is deliberate and it is the whole lesson of this file:
	// RECONNECT_CONTROLLERS closes every open gamepad before reopening it, and
	// closing a Steam Controller hands it back to its firmware as Lizard Mode.
	// Any rule that can re-arm from the state a reconnect itself produces is a
	// loop that unplugs the user's controller several times a minute. Two
	// separate attempts at "keep trying until it works" -- a self-renewing
	// budget, then re-arming on which devices were unopened -- both did exactly
	// that, because a reconnect changes both of those things.
	if (retry.due())
	{
		COUT_INFO << "[AUTOCONNECT] " << census.failedToOpen << " device(s) did not open ("
		          << retry.attemptsLeft << " attempt(s) left after this one).\n";
		reconnect("Retrying");
		if (retry.exhausted())
		{
			COUT_INFO << "[AUTOCONNECT] No further attempts until the device count changes.\n";
		}
	}
	return true;
}

} // namespace JSM
