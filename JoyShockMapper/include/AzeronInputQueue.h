#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <chrono>

namespace AzeronNative {
struct Snapshot {
    uint64_t buttons = 0;
    float x = 0, y = 0;
    const char* status = "connecting";
};

// The HID reader must never wait for mapping, JSON, or the UI. Store every
// button transition, coalesce analog-only updates, and allocate nothing after
// construction. One mapper tick takes a bounded batch under a short lock.
class InputQueue {
public:
    using Clock = std::chrono::steady_clock;
    static constexpr auto maxBacklogAge = std::chrono::milliseconds(50);
    static constexpr size_t capacity = 256;
    using Batch = std::array<Snapshot, capacity>;
    void publish(Snapshot next, bool active, Clock::time_point now = Clock::now()) {
        std::lock_guard lock(mutex_);
        if (!active) {
            // Disconnect/mode loss cancels stale presses, including queued ones.
            count_ = 0;
            latest_ = next;
            return;
        }
        if (next.buttons != latest_.buttons) {
            if (count_ == capacity) {
                // A stalled consumer must not grow memory or replay seconds of
                // stale actions. Release all keys, then resume the physical state.
                count_ = 1;
                pending_[0] = Snapshot{0, 0, 0, next.status};
                ++overflows_;
                oldest_ = now;
            }
            // The timestamp belongs to the batch, not analog updates.
            if (!count_) oldest_ = now;
            pending_[count_++] = next;
        } else if (count_) {
            pending_[count_ - 1] = next;
        }
        latest_ = next;
    }
    size_t take(Batch& batch, Clock::time_point now = Clock::now()) {
        std::lock_guard lock(mutex_);
        if (!count_) { batch[0] = latest_; return 1; }
        if (now - oldest_ > maxBacklogAge) {
            // A slow configuration load must not replay old movement/taps in
            // a burst. Neutral releases old outputs before the current state.
            batch[0] = Snapshot{0, 0, 0, latest_.status};
            batch[1] = latest_;
            count_ = 0;
            ++staleBatches_;
            return 2;
        }
        const auto count = count_;
        std::copy_n(pending_.begin(), count, batch.begin());
        count_ = 0;
        return count;
    }
    uint64_t overflows() const { std::lock_guard lock(mutex_); return overflows_; }
    uint64_t staleBatches() const { std::lock_guard lock(mutex_); return staleBatches_; }
private:
    mutable std::mutex mutex_;
    Batch pending_{};
    Snapshot latest_;
    size_t count_ = 0;
    uint64_t overflows_ = 0;
    uint64_t staleBatches_ = 0;
    Clock::time_point oldest_{};
};
}
