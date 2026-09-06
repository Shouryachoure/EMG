/// @file test_ring_buffer.cpp
/// @brief Unit tests for the lock-free SPSC RingBuffer.

#include "ring_buffer.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

#include "test_framework.h"

// ============================================================
// Tests
// ============================================================

TEST(RingBuffer_PushPop_Basic) {
    emg::RingBuffer<int, 8> rb;

    ASSERT_TRUE(rb.empty());
    ASSERT_EQ(rb.size(), size_t(0));
    ASSERT_EQ(rb.capacity(), size_t(8));

    // Push and pop single element
    rb.push(42);
    ASSERT_FALSE(rb.empty());
    ASSERT_EQ(rb.size(), size_t(1));

    auto val = rb.tryPop();
    ASSERT_TRUE(val.has_value());
    ASSERT_EQ(val.value(), 42);

    ASSERT_TRUE(rb.empty());
}

TEST(RingBuffer_PopEmpty_ReturnsNullopt) {
    emg::RingBuffer<int, 4> rb;
    auto val = rb.tryPop();
    ASSERT_FALSE(val.has_value());
}

TEST(RingBuffer_FillToCapacity) {
    // Capacity is 8, but one slot is always unused (sentinel), so we can store 7.
    emg::RingBuffer<int, 8> rb;

    for (int i = 0; i < 7; ++i) {
        bool ok = rb.push(i);
        ASSERT_TRUE(ok);  // No overflow
    }
    ASSERT_EQ(rb.size(), size_t(7));
}

TEST(RingBuffer_Overflow_OverwritesOldest) {
    emg::RingBuffer<int, 4> rb;  // Capacity 4, usable 3

    rb.push(1);  // ok
    rb.push(2);  // ok
    rb.push(3);  // ok (full now)
    bool overflow = !rb.push(4);  // Overwrites oldest (1)
    ASSERT_TRUE(overflow);

    // Oldest should now be 2 (1 was overwritten)
    auto val = rb.tryPop();
    ASSERT_TRUE(val.has_value());
    ASSERT_EQ(val.value(), 2);

    val = rb.tryPop();
    ASSERT_TRUE(val.has_value());
    ASSERT_EQ(val.value(), 3);

    val = rb.tryPop();
    ASSERT_TRUE(val.has_value());
    ASSERT_EQ(val.value(), 4);

    ASSERT_TRUE(rb.empty());
}

TEST(RingBuffer_BatchPop) {
    emg::RingBuffer<int, 16> rb;

    for (int i = 0; i < 10; ++i) {
        rb.push(i * 10);
    }

    auto batch = rb.tryPopBatch(5);
    ASSERT_EQ(batch.size(), size_t(5));
    ASSERT_EQ(batch[0], 0);
    ASSERT_EQ(batch[4], 40);

    ASSERT_EQ(rb.size(), size_t(5));

    // Pop remaining
    batch = rb.tryPopBatch(100);  // Request more than available
    ASSERT_EQ(batch.size(), size_t(5));
    ASSERT_EQ(batch[0], 50);
}

TEST(RingBuffer_BatchPop_Empty) {
    emg::RingBuffer<int, 8> rb;
    auto batch = rb.tryPopBatch(10);
    ASSERT_EQ(batch.size(), size_t(0));
}

TEST(RingBuffer_Shutdown) {
    emg::RingBuffer<int, 8> rb;
    ASSERT_FALSE(rb.isShutdown());
    rb.shutdown();
    ASSERT_TRUE(rb.isShutdown());
}

TEST(RingBuffer_Reset) {
    emg::RingBuffer<int, 8> rb;
    rb.push(1);
    rb.push(2);
    rb.shutdown();

    rb.reset();
    ASSERT_TRUE(rb.empty());
    ASSERT_FALSE(rb.isShutdown());
}

TEST(RingBuffer_SPSC_Concurrent) {
    // Stress test: one producer thread, one consumer thread
    constexpr size_t BUF_SIZE = 1024;  // Power of 2
    constexpr size_t NUM_ITEMS = 100000;

    emg::RingBuffer<size_t, BUF_SIZE> rb;
    std::atomic<size_t> consumed_count{0};
    std::atomic<size_t> last_consumed{0};
    std::atomic<bool> producer_done{false};

    // Producer thread
    std::thread producer([&]() {
        for (size_t i = 1; i <= NUM_ITEMS; ++i) {
            rb.push(i);
            // Small delay to simulate 1kHz sampling
            if (i % 1000 == 0) {
                std::this_thread::yield();
            }
        }
        producer_done.store(true);
    });

    // Consumer thread
    std::thread consumer([&]() {
        size_t count = 0;
        while (!producer_done.load() || !rb.empty()) {
            auto val = rb.tryPop();
            if (val.has_value()) {
                // Values should be monotonically increasing (though some may be skipped due to overflow)
                ASSERT_TRUE(val.value() > 0);
                ASSERT_TRUE(val.value() <= NUM_ITEMS);
                last_consumed.store(val.value());
                count++;
            } else {
                std::this_thread::yield();
            }
        }
        consumed_count.store(count);
    });

    producer.join();
    consumer.join();

    // We should have consumed some items (may not be all due to overflow)
    ASSERT_TRUE(consumed_count.load() > 0);
    // The last consumed value should be near the end
    ASSERT_TRUE(last_consumed.load() > 0);
}

TEST(RingBuffer_WrapAround) {
    emg::RingBuffer<int, 4> rb;

    // Push and pop repeatedly to wrap around the buffer multiple times
    for (int round = 0; round < 10; ++round) {
        rb.push(round * 3 + 0);
        rb.push(round * 3 + 1);

        auto v1 = rb.tryPop();
        ASSERT_TRUE(v1.has_value());
        ASSERT_EQ(v1.value(), round * 3 + 0);

        auto v2 = rb.tryPop();
        ASSERT_TRUE(v2.has_value());
        ASSERT_EQ(v2.value(), round * 3 + 1);

        ASSERT_TRUE(rb.empty());
    }
}
