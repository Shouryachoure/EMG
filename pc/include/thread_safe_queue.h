#pragma once
/// @file thread_safe_queue.h
/// @brief Bounded, thread-safe queue with blocking pop and clean shutdown.
///
/// Design rationale:
/// - Used between Thread 2→3 (FeatureQueue) and Thread 3→4 (DecisionQueue).
/// - These operate at lower frequency (10–50 Hz), so mutex overhead is acceptable.
/// - Blocking pop via condition_variable lets consumer threads sleep instead of busy-wait.
/// - Bounded capacity prevents runaway memory growth if a consumer stalls.
/// - When full, push() drops the OLDEST entry (front of queue) — this ensures the
///   system always acts on the most recent data.
/// - shutdown() wakes all blocked consumers so they can exit cleanly.

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <cstddef>

namespace emg {

/// @tparam T Element type.
template <typename T>
class ThreadSafeQueue {
public:
    /// @param max_capacity Maximum number of elements. 0 = unbounded.
    explicit ThreadSafeQueue(size_t max_capacity = 0)
        : max_capacity_(max_capacity), shutdown_(false) {}

    // Non-copyable, non-movable
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    /// Push an element. If the queue is full (bounded), drops the oldest entry.
    /// @param value Element to push.
    /// @return true if pushed without overflow, false if an element was dropped.
    bool push(T value) {
        bool dropped = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (shutdown_) return false;

            if (max_capacity_ > 0 && queue_.size() >= max_capacity_) {
                queue_.pop();  // Drop oldest
                dropped = true;
            }
            queue_.push(std::move(value));
        }
        cv_.notify_one();
        return !dropped;
    }

    /// Blocking pop. Waits until an element is available or shutdown is signaled.
    /// @return The element, or std::nullopt if the queue has been shut down.
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] {
            return !queue_.empty() || shutdown_;
        });

        if (queue_.empty()) {
            return std::nullopt;  // Shutdown with empty queue
        }

        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    /// Non-blocking pop attempt.
    /// @return The element if available, std::nullopt otherwise.
    std::optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    /// Check if the queue is empty (approximate in concurrent context).
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    /// Return current size (approximate in concurrent context).
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /// Return the maximum capacity (0 = unbounded).
    size_t capacity() const {
        return max_capacity_;
    }

    /// Signal shutdown. Wakes all blocked consumers.
    /// After shutdown, push() is a no-op and pop() returns nullopt when empty.
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
        }
        cv_.notify_all();
    }

    /// Check if shutdown has been signaled.
    bool isShutdown() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return shutdown_;
    }

    /// Reset the queue. NOT thread-safe — call only when no producers/consumers active.
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty;
        queue_.swap(empty);
        shutdown_ = false;
    }

private:
    std::queue<T>           queue_;
    mutable std::mutex      mutex_;
    std::condition_variable cv_;
    size_t                  max_capacity_;
    bool                    shutdown_;
};

} // namespace emg
