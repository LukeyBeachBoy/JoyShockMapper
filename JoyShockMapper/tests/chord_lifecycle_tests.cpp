// Hold to swap and the controller thread (2026-10-08 hardware session): the
// pieces of the mapper that a held chord and a reconnect depend on, testable
// without a controller.
//
// - Pad rotation is global: the one-pad fallback must not turn
//   RIGHT_TOUCHPAD_ROTATION into a TOUCHPAD_ROTATION that doesn't exist.
// - A chord keeps the virtual pad unless its own lines set VIRTUAL_CONTROLLER
//   (it used to unplug the virtual Xbox pad on press and replug it on release).
// - The controller thread waits only briefly for the configuration lock: a
//   command that is itself waiting on the controller thread must not deadlock
//   (the mapper froze in Lizard Mode), and an ordinary short command must not
//   cost a reading (gyro adds up reading by reading; skipping killed it).
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

#include "ControllerCompatibility.h"
#include "ControllerContext.h"

namespace
{
using namespace std::chrono;

void test_pad_rotation_stays_global()
{
	for (const bool left : { false, true })
	{
		assert(ControllerCompatibility::fallback({ "LEFT_TOUCHPAD_ROTATION = -4", "RIGHT_TOUCHPAD_ROTATION = 4" }, left).empty());
		assert(ControllerCompatibility::fallback({ "LEFT_TOUCHPAD_ROTATION = -4", "RIGHT_TOUCHPAD_ROTATION = 4" }, left, true).empty());
	}
	// The other per-pad keys still translate for a one-pad controller.
	const auto translated = ControllerCompatibility::fallback({ "RIGHT_TOUCHPAD_MODE = MOUSE", "RT1 = J" }, false);
	assert(translated.size() == 2);
	assert(translated[0].rfind("TOUCHPAD_MODE", 0) == 0);
	assert(translated[1].rfind("T1", 0) == 0);
}

void test_a_chord_sets_its_own_virtual_pad_only_when_it_says_so()
{
	using ControllerCompatibility::setsKey;
	assert(setsKey({ "RESET_MAPPINGS", "VIRTUAL_CONTROLLER = DS4" }, "VIRTUAL_CONTROLLER"));
	assert(setsKey({ "  virtual_controller=xbox  " }, "VIRTUAL_CONTROLLER"));
	assert(!setsKey({ "# VIRTUAL_CONTROLLER = XBOX", "GYRO_ON = MISC5" }, "VIRTUAL_CONTROLLER"));
	assert(!setsKey({ "VIRTUAL_CONTROLLER_RUMBLE = OFF" }, "VIRTUAL_CONTROLLER"));
	assert(!setsKey({}, "VIRTUAL_CONTROLLER"));
}

// The deadlock as it happened: the command thread holds the configuration lock
// and waits for the controller lock; the controller thread holds the controller
// lock and asks for the configuration lock. The controller thread must give
// up its reading and let go, so the command can finish.
void test_a_command_waiting_on_the_controller_thread_cannot_deadlock()
{
	std::mutex controllerLock;
	std::atomic<bool> commandHasConfiguration = false, commandDone = false, readingSkipped = false;
	std::unique_lock<std::mutex> pollHoldsController(controllerLock);

	std::thread command([&] {
		std::lock_guard<std::recursive_timed_mutex> configuration(ControllerContext::mutex);
		commandHasConfiguration = true;
		std::lock_guard<std::mutex> waitForController(controllerLock); // RECONNECT_CONTROLLERS
		commandDone = true;
	});
	while (!commandHasConfiguration) std::this_thread::yield();

	const auto started = steady_clock::now();
	std::unique_lock<std::recursive_timed_mutex> reading;
	readingSkipped = !ControllerContext::lockForPoll(reading);
	pollHoldsController.unlock(); // the reading ends; the poll loop lets go
	command.join();

	assert(readingSkipped);
	assert(commandDone);
	assert(steady_clock::now() - started < seconds(2));
}

// An ordinary command (a few ms) only delays the reading; it is not dropped.
void test_a_short_command_does_not_cost_a_reading()
{
	std::atomic<bool> commandHasConfiguration = false;
	std::thread command([&] {
		std::lock_guard<std::recursive_timed_mutex> configuration(ControllerContext::mutex);
		commandHasConfiguration = true;
		std::this_thread::sleep_for(milliseconds(5));
	});
	while (!commandHasConfiguration) std::this_thread::yield();
	std::unique_lock<std::recursive_timed_mutex> reading;
	assert(ControllerContext::lockForPoll(reading));
	assert(reading.owns_lock());
	reading.unlock();
	command.join();
	assert(ControllerContext::kPollLockWait >= milliseconds(10));
}
}

int main()
{
	test_pad_rotation_stays_global();
	test_a_chord_sets_its_own_virtual_pad_only_when_it_says_so();
	test_a_command_waiting_on_the_controller_thread_cannot_deadlock();
	test_a_short_command_does_not_cost_a_reading();
	std::cout << "chord_lifecycle_tests passed\n";
	return 0;
}
