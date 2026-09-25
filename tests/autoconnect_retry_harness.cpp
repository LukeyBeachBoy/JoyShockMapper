// Guards the retry schedule behind the "controller switched on but never
// picked up" bug.
//
// AutoConnect's only trigger used to be "the device count changed". A
// controller waking from sleep is re-enumerated immediately, so the reconnect
// fired while its driver still had it unclassified, the gamepad open failed,
// and the device was dropped -- at the same device count it started from. The
// trigger was edge-only, so it never fired again and the controller stayed
// dead for the rest of the session. Nothing else in JSM compared "SDL sees a
// device" against "JSM has one open", so nothing noticed.
//
// Build & run (from the repo root):
//   cl /std:c++20 /EHsc /nologo /I JoyShockMapper/JoyShockMapper/include \
//      JoyShockMapper/tests/autoconnect_retry_harness.cpp && autoconnect_retry_harness.exe

#include "AutoConnectRetry.h"
#include <cassert>
#include <cstdio>
#include <vector>

// Which poll ticks, counting from the arm(), an attempt lands on.
static std::vector<int> attemptTicks(AutoConnectRetry& retry, int ticks)
{
	std::vector<int> fired;
	for (int tick = 1; tick <= ticks; ++tick)
	{
		if (retry.due())
		{
			fired.push_back(tick);
		}
	}
	return fired;
}

int main()
{
	// Nothing armed: a steady, healthy session must never reconnect itself.
	{
		AutoConnectRetry idle;
		assert(attemptTicks(idle, 1000).empty());
	}

	// Armed once: attempts back off, and there are only ever MAX_ATTEMPTS.
	{
		AutoConnectRetry retry;
		retry.arm();
		const std::vector<int> expected{ 3, 9, 21, 45 };
		assert(attemptTicks(retry, 1000) == expected);
		assert(int(expected.size()) == AutoConnectRetry::MAX_ATTEMPTS);
	}

	// The first attempt must not land on the very next tick. connectDevices()
	// can be seconds long, and observing a reconnect mid-flight looks exactly
	// like one that failed -- spacing is what stops a pile-up.
	{
		AutoConnectRetry retry;
		retry.arm();
		assert(!retry.due());
		assert(AutoConnectRetry::FIRST_DELAY_TICKS >= 2);
	}

	// Re-arming is what a device-count change does. A spent budget must come
	// back in full, so a device that can never be opened cannot starve a
	// controller that was just switched on of its retries.
	{
		AutoConnectRetry retry;
		retry.arm();
		attemptTicks(retry, 1000);
		assert(retry.attemptsLeft == 0);
		retry.arm();
		const std::vector<int> expected{ 3, 9, 21, 45 };
		assert(attemptTicks(retry, 1000) == expected);
	}

	// Re-arming mid-schedule restarts it rather than continuing the backoff:
	// the state that was being retried is gone.
	{
		AutoConnectRetry retry;
		retry.arm();
		assert(attemptTicks(retry, 10) == std::vector<int>({ 3, 9 }));
		retry.arm();
		assert(attemptTicks(retry, 4) == std::vector<int>({ 3 }));
	}

	// clear() is "everything is attached": no further attempts until re-armed,
	// and re-arming still works afterwards.
	{
		AutoConnectRetry retry;
		retry.arm();
		assert(attemptTicks(retry, 3) == std::vector<int>({ 3 }));
		retry.clear();
		assert(attemptTicks(retry, 1000).empty());
		retry.arm();
		assert(attemptTicks(retry, 3) == std::vector<int>({ 3 }));
	}

	// The whole point: a controller switched on gets several chances spread
	// over enough wall-clock time to outlast a slow driver.
	{
		AutoConnectRetry retry;
		retry.arm();
		const std::vector<int> fired = attemptTicks(retry, 1000);
		assert(!fired.empty());
		assert(fired.back() >= 30); // Polls are 1000ms, so >= 30 seconds of grace.
	}

	// The schedule MUST run dry on its own. Every attempt closes each open
	// gamepad before reopening it, and closing a Steam Controller drops it into
	// Lizard Mode -- so a schedule that keeps firing against a device that will
	// not open does not idle in the background, it repeatedly drops the
	// controller in your hands. A previous version renewed itself here and did
	// exactly that; the caller re-arms on device identity instead.
	{
		AutoConnectRetry retry;
		retry.arm();
		const std::vector<int> fired = attemptTicks(retry, 100000);
		assert(int(fired.size()) == AutoConnectRetry::MAX_ATTEMPTS);
		assert(retry.exhausted());
		// And it stays quiet: no amount of further polling revives it.
		assert(attemptTicks(retry, 100000).empty());
	}

	// exhausted() is what lets the caller say "giving up" exactly once, so a
	// stuck device is visible in the log rather than silently abandoned.
	{
		AutoConnectRetry retry;
		assert(retry.exhausted());
		retry.arm();
		assert(!retry.exhausted());
		attemptTicks(retry, 1000); // runs the whole schedule to its end
		assert(retry.exhausted());
		// clear() reaches the same state, for "everything is open after all".
		retry.arm();
		retry.clear();
		assert(retry.exhausted());
	}

	std::puts("PASS autoconnect_retry_harness");
	return 0;
}
