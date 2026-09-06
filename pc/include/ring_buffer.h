#pragma once
/// @file ring_buffer.h
/// @brief Lock-free Single-Producer Single-Consumer (SPSC) ring buffer.
///
/// Design rationale:
/// - Used between Thread 1 (EMG acquisition) and Thread 2 (signal processing).
/// - EMG samples arrive at 1 kHz+; mutex contention would introduce jitter.
/// - Lock-free SPSC is safe because there is exactly ONE producer and ONE consumer.
/// - When the buffer is full, push() overwrites the oldest sample (the consumer's
///   tail advances). For real-time EMG, stale samples are useless — it is better
///   to drop old data than to block the producer.
///
/// Memory ordering:
/// - head_ (write index) is written by producer, read by consumer → release/acquire
/// - tail_ (read index) is written by consumer, read by producer → release/acquire

#include <atomic>
#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace emg {

/// @tparam T    Element type (e.g., double for EMG samples)
/// @tparam Cap  Fixed capacity (must be power of 2 for efficient modular arithmetic)
template <typename T, size_t Cap>
class RingBuffer {
    static_assert(Cap > 0, "Capacity must be > 0");
    static_assert((Cap & (Cap - 1)) == 0, "Capacity must be a power of 2");

public:
    RingBuffer() : head_(0), tail_(0), shutdown_(false) {}

    /// Push a single element. If the buffer is full, the oldest element is overwritten.
    /// @param value The element to push.
    /// @return true if pushed without overflow, false if an element was overwritten.
    bool push(const T& value) {
        const size_t head = head_.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & mask_;
        const size_t tail = tail_.load(std::memory_order_acquire);

        bool overwritten = false;
        if (next_head == tail) {
            // Buffer is full — advance tail to overwrite oldest
            tail_.store((tail + 1) & mask_, std::memory_order_release);
            overwritten = true;
        }

        buffer_[head] = value;
        head_.store(next_head, std::memory_order_release);
        return !overwritten;
    }

    /// Try to pop a single element.
    /// @return The element if available, std::nullopt if empty.
    std::optional<T> tryPop() {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t head = head_.load(std::memory_order_acquire);

        if (tail == head) {
            return std::nullopt;  // Empty
        }

        T value = buffer_[tail];
        tail_.store((tail + 1) & mask_, std::memory_order_release);
        return value;
    }

    /// Try to pop up to `count` elements into a vector.
    /// @param count Maximum number of elements to pop.
    /// @return Vector of popped elements (may be smaller than count or empty).
    std::vector<T> tryPopBatch(size_t count) {
        std::vector<T> result;
        result.reserve(count);

        for (size_t i = 0; i < count; ++i) {
            auto val = tryPop();
            if (!val.has_value()) break;
            result.push_back(val.value());
        }
        return result;
    }

    /// Return the number of elements currently in the buffer.
    /// Note: This is approximate in a concurrent context.
    size_t size() const {
        const size_t head = head_.load(std::memory_order_acquire);
        const size_t tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & mask_;
    }

    /// Return true if the buffer is empty.
    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    /// Return the fixed capacity.
    constexpr size_t capacity() const { return Cap; }

    /// Signal shutdown. Currently only sets a flag; the ring buffer itself
    /// does not block, so shutdown is purely informational.
    void shutdown() {
        shutdown_.store(true, std::memory_order_release);
    }

    /// Check if shutdown has been signaled.
    bool isShutdown() const {
        return shutdown_.load(std::memory_order_acquire);
    }

    /// Reset the buffer to empty state. NOT thread-safe — call only when
    /// no producer/consumer is active.
    void reset() {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
        shutdown_.store(false, std::memory_order_relaxed);
    }

private:
    static constexpr size_t mask_ = Cap - 1;

    std::array<T, Cap> buffer_;
    alignas(64) std::atomic<size_t> head_;  // Cache-line aligned to prevent false sharing
    alignas(64) std::atomic<size_t> tail_;
    std::atomic<bool> shutdown_;
};

} // namespace emg
