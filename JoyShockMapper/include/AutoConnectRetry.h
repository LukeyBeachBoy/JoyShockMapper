#pragma once

// The retry schedule AutoConnect runs when SDL lists a device JoyShockMapper
// has not managed to open. Kept free of SDL, threads and logging so the part
// that is easy to get subtly wrong -- when the next attempt is due, and when to
// stop -- can be tested on its own.
//
// Why retry at all: a controller waking from sleep is re-enumerated
// immediately, but its driver may not have classified it as a gamepad yet, so
// the reconnect fired by the device count changing can open nothing. The count
// is then back where it started, and a trigger watching only the count never
// fires again.
//
// Why it MUST be bounded, and why that is not a compromise:
// RECONNECT_CONTROLLERS is not a cheap poke. It clears every mapping, closes
// every open gamepad and reopens them -- and closing a Steam Controller hands
// it back to its own firmware, which is Lizard Mode. So an unbounded retry
// against a device that will not open does not sit quietly in the background:
// it drops the controller you are using, over and over, forever. An earlier
// version of this did exactly that.
//
// What replaces "never give up" is the caller re-arming on identity: the
// budget is refilled whenever the SET of unopened devices changes, which is
// precisely what happens when a controller is switched on, plugged in or woken.
// A device that simply cannot be opened -- a wheel, a pedal set -- goes quiet
// after a few tries instead of reconnecting the session forever.
struct AutoConnectRetry
{
	// Polls arrive every 1000ms, so this schedules attempts roughly 3s, 9s,
	// 21s and 45s after the triggering change -- long enough in total to
	// outlast a slow driver, spaced enough that a reconnect still in flight is
	// not mistaken for one that failed.
	static constexpr int MAX_ATTEMPTS = 4;
	static constexpr int FIRST_DELAY_TICKS = 3;

	// The device set changed: give it a full budget of attempts.
	void arm()
	{
		attemptsLeft = MAX_ATTEMPTS;
		delayTicks = FIRST_DELAY_TICKS;
		ticksUntilNext = FIRST_DELAY_TICKS;
	}

	// Everything is open, or we are done trying: stop until re-armed.
	void clear()
	{
		attemptsLeft = 0;
	}

	/// Whether an attempt has just been spent. True only on the ticks an
	/// attempt is due, at which point the next wait doubles.
	bool due()
	{
		if (attemptsLeft <= 0)
		{
			return false;
		}
		if (--ticksUntilNext > 0)
		{
			return false;
		}
		--attemptsLeft;
		delayTicks *= 2;
		ticksUntilNext = delayTicks;
		return true;
	}

	/// True once the budget is spent, so the caller can say so exactly once
	/// rather than silently going quiet.
	bool exhausted() const
	{
		return attemptsLeft <= 0;
	}

	int attemptsLeft = 0;
	int ticksUntilNext = 0;
	int delayTicks = FIRST_DELAY_TICKS;
};
