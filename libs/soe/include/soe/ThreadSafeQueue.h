#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace soe {

// A simple, general-purpose blocking queue for cross-thread handoff - the
// shared primitive behind every producer/consumer boundary in the engine
// loop redesign (Phase 14): the asset job queue, the asset ready queue, and
// the network thread's outbound message queue. Deliberately minimal: no
// bounded capacity/backpressure (none of this project's current queues need
// it) and no lock-free tricks (object/message counts are small enough that a
// plain mutex+condition_variable is not a measured bottleneck) - matches
// this project's own "smallest thing that solves the real problem"
// discipline (see TerrainChunkManager's fixed-radius-not-quadtree
// precedent).
template <typename T>
class ThreadSafeQueue {
public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(value));
        }
        cv_.notify_one();
    }

    // Blocks until an item is available or `timeout` elapses, returning
    // nullopt on timeout. For a worker/network thread's own main loop -
    // waking periodically even with nothing queued lets the loop check a
    // shutdown flag without needing a separate wake mechanism.
    std::optional<T> pop(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this] { return !queue_.empty(); })) {
            return std::nullopt;
        }
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    // Non-blocking - for the render thread's per-frame drain, where even a
    // brief block would stall a frame.
    std::optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
};

} // namespace soe
