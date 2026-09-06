/// @file test_pipeline.cpp
/// @brief Integration tests for Pipeline orchestrator.

#include "pipeline.h"
#include <chrono>
#include <thread>
#include <stdexcept>

#include "test_framework.h"

using namespace emg;

TEST(pipeline_lifecycle) {
    SystemConfig cfg;
    cfg.udp.esp32_ip = "127.0.0.1";
    cfg.udp.esp32_port = 54322;
    cfg.emg_source = "fake";
    cfg.sample_rate_hz = 1000.0;
    cfg.batch_size = 50;

    Pipeline pipeline(cfg);
    ASSERT_TRUE(pipeline.initialize());
    ASSERT_FALSE(pipeline.isRunning());

    pipeline.start();
    ASSERT_TRUE(pipeline.isRunning());

    // Let the 4 threads run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    pipeline.stop();
    ASSERT_FALSE(pipeline.isRunning());
}
