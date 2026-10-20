#pragma once
#include <atomic>

// Command-thread state, with a one-shot notification to the AutoLoad thread.
// Recheck the current foreground app on release; a saved path could belong to
// a window the player has already left during Alt+Tab.
class DeferredAutoLoad
{
    bool deferred = false;
    std::atomic<bool> retry = false;
public:
    void defer() { deferred = true; }
    void release(bool stillHeld) {
        if (!stillHeld && deferred) {
            deferred = false;
            retry.store(true);
        }
    }
    void cancel() { deferred = false; retry.store(false); }
    bool takeRetry() { return retry.exchange(false); }
};
