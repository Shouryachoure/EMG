/// @file test_thread_safe_queue.cpp
/// @brief Unit tests for the bounded ThreadSafeQueue.

#include "thread_safe_queue.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include "test_framework.h"

// ============================================================
// Tests
// ============================================================

TEST(TSQueue_PushPop_Basic) {
    emg::ThreadSafeQueue<int> q;

    ASSERT_TRUE(q.empty());
    ASSERT_EQ(q.size(), size_t(0));

    q.push(42);
    ASSERT_FALSE(q.empty());
    ASSERT_EQ(q.size(), size_t(1));

    auto val = q.tryPop();
    ASSERT_TRUE(val.has_value());
    ASSERT_EQ(val.value(), 42);
    ASSERT_TRUE(q.empty());
}

TEST(TSQueue_TryPop_Empty) {
    emg::ThreadSafeQueue<int> q;
    auto val = q.tryPop();
    ASSERT_FALSE(val.has_value());
}

TEST(TSQueue_FIFO_Order) {
    emg::ThreadSafeQueue<int> q;
    q.push(1);
    q.push(2);
    q.push(3);

    ASSERT_EQ(q.tryPop().value(), 1);
    ASSERT_EQ(q.tryPop().value(), 2);
    ASSERT_EQ(q.tryPop().value(), 3);
    ASSERT_TRUE(q.empty());
}

TEST(TSQueue_Bounded_DropsOldest) {
    emg::ThreadSafeQueue<int> q(3);  // Max capacity = 3
    ASSERT_EQ(q.capacity(), size_t(3));

    ASSERT_TRUE(q.push(1));   // size=1
    ASSERT_TRUE(q.push(2));   // size=2
    ASSERT_TRUE(q.push(3));   // size=3 (full)
    ASSERT_FALSE(q.push(4));  // Overflow: drops 1, pushes 4

    ASSERT_EQ(q.size(), size_t(3));
    ASSERT_EQ(q.tryPop().value(), 2);  // 1 was dropped
    ASSERT_EQ(q.tryPop().value(), 3);
    ASSERT_EQ(q.tryPop().value(), 4);
}

TEST(TSQueue_Unbounded) {
    emg::ThreadSafeQueue<int> q(0);  // Unbounded (capacity=0)

    for (int i = 0; i < 1000; ++i) {
        ASSERT_TRUE(q.push(i));  // Never overflows
    }
    ASSERT_EQ(q.size(), size_t(1000));
}

TEST(TSQueue_BlockingPop) {
    emg::ThreadSafeQueue<int> q;
    std::atomic<bool> popped{false};
    int result = -1;

    // Consumer thread blocks on pop()
    std::thread consumer([&]() {
        auto val = q.pop();
        if (val.has_value()) {
            result = val.value();
            popped.store(true);
        }
    });

    // Give consumer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_FALSE(popped.load());  // Should still be blocking

    // Push a value to unblock consumer
    q.push(99);

    consumer.join();
    ASSERT_TRUE(popped.load());
    ASSERT_EQ(result, 99);
}

TEST(TSQueue_Shutdown_UnblocksConsumer) {
    emg::ThreadSafeQueue<int> q;
    std::atomic<bool> finished{false};
    bool got_nullopt = false;

    std::thread consumer([&]() {
        auto val = q.pop();  // Blocks
        got_nullopt = !val.has_value();
        finished.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_FALSE(finished.load());

    q.shutdown();  // Should unblock the consumer

    consumer.join();
    ASSERT_TRUE(finished.load());
    ASSERT_TRUE(got_nullopt);  // pop() returned nullopt on shutdown
}

TEST(TSQueue_Shutdown_DrainRemaining) {
    emg::ThreadSafeQueue<int> q;
    q.push(1);
    q.push(2);
    q.shutdown();

    // Can still pop existing items after shutdown
    auto v1 = q.pop();
    ASSERT_TRUE(v1.has_value());
    ASSERT_EQ(v1.value(), 1);

    auto v2 = q.pop();
    ASSERT_TRUE(v2.has_value());
    ASSERT_EQ(v2.value(), 2);

    // Now empty + shutdown → nullopt
    auto v3 = q.pop();
    ASSERT_FALSE(v3.has_value());
}

TEST(TSQueue_PushAfterShutdown_Rejected) {
    emg::ThreadSafeQueue<int> q;
    q.shutdown();

    bool pushed = q.push(42);
    ASSERT_FALSE(pushed);
    ASSERT_TRUE(q.empty());
}

TEST(TSQueue_MultiProducer_SingleConsumer) {
    emg::ThreadSafeQueue<int> q;
    constexpr int NUM_PRODUCERS = 4;
    constexpr int ITEMS_PER_PRODUCER = 1000;

    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; ++p) {
        producers.emplace_back([&q, p]() {
            for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
                q.push(p * ITEMS_PER_PRODUCER + i);
            }
        });
    }

    // Wait for all producers to finish
    for (auto& t : producers) t.join();

    ASSERT_EQ(q.size(), size_t(NUM_PRODUCERS * ITEMS_PER_PRODUCER));

    // Drain and count
    int count = 0;
    while (auto val = q.tryPop()) {
        count++;
    }
    ASSERT_EQ(count, NUM_PRODUCERS * ITEMS_PER_PRODUCER);
}

TEST(TSQueue_ConcurrentProducerConsumer) {
    emg::ThreadSafeQueue<int> q(100);  // Bounded
    constexpr int TOTAL = 10000;
    std::atomic<int> consumed_count{0};
    std::atomic<bool> producer_done{false};

    std::thread producer([&]() {
        for (int i = 0; i < TOTAL; ++i) {
            q.push(i);
            if (i % 100 == 0) std::this_thread::yield();
        }
        producer_done.store(true);
    });

    std::thread consumer([&]() {
        while (!producer_done.load() || !q.empty()) {
            auto val = q.tryPop();
            if (val.has_value()) {
                consumed_count.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    // Some items may have been dropped due to bounded capacity,
    // but we should have consumed some
    ASSERT_TRUE(consumed_count.load() > 0);
}

TEST(TSQueue_Reset) {
    emg::ThreadSafeQueue<int> q;
    q.push(1);
    q.push(2);
    q.shutdown();

    q.reset();
    ASSERT_TRUE(q.empty());
    ASSERT_FALSE(q.isShutdown());

    // Should work again after reset
    ASSERT_TRUE(q.push(99));
    ASSERT_EQ(q.tryPop().value(), 99);
}
