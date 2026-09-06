/// @file test_signal_processor.cpp
/// @brief Unit tests for SignalProcessor.

#include "signal_processor.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

#include "test_framework.h"

using namespace emg;

TEST(signal_processor_remove_dc) {
    std::vector<double> input = {10.0, 12.0, 14.0, 16.0, 18.0}; // mean = 14.0
    auto out = SignalProcessor::removeDC(input);

    ASSERT_EQ(out.size(), input.size());
    ASSERT_NEAR(out[0], -4.0, 1e-6);
    ASSERT_NEAR(out[1], -2.0, 1e-6);
    ASSERT_NEAR(out[2], 0.0, 1e-6);
    ASSERT_NEAR(out[3], 2.0, 1e-6);
    ASSERT_NEAR(out[4], 4.0, 1e-6);

    double sum = 0.0;
    for (double v : out) sum += v;
    ASSERT_NEAR(sum, 0.0, 1e-6);
}

TEST(signal_processor_normalize) {
    std::vector<double> input = {-10.0, -5.0, 0.0, 5.0, 10.0};
    auto out = SignalProcessor::normalize(input);

    ASSERT_EQ(out.size(), input.size());
    ASSERT_NEAR(out[0], -1.0, 1e-6);
    ASSERT_NEAR(out[4], 1.0, 1e-6);
    ASSERT_NEAR(out[2], 0.0, 1e-6);

    // All zero case should not divide by zero
    std::vector<double> all_zero = {0.0, 0.0, 0.0};
    auto zero_out = SignalProcessor::normalize(all_zero);
    ASSERT_EQ(zero_out.size(), 3u);
    ASSERT_NEAR(zero_out[0], 0.0, 1e-6);
}

TEST(signal_processor_bandpass) {
    ProcessingConfig config;
    config.sample_rate_hz = 1000.0;
    config.filter_low_hz = 20.0;
    config.filter_high_hz = 450.0;
    config.normalize = false;

    SignalProcessor proc(config);

    // Passband sine wave at 100 Hz (sample rate 1000 Hz => 10 samples per cycle)
    std::vector<double> signal(100);
    for (size_t i = 0; i < signal.size(); ++i) {
        signal[i] = std::sin(2.0 * 3.1415926535 * 100.0 * i / 1000.0);
    }

    auto out = proc.process(signal);
    ASSERT_EQ(out.size(), signal.size());

    // Filter output should have reasonable amplitude
    double max_val = 0.0;
    for (size_t i = 20; i < out.size(); ++i) { // skip filter transient
        max_val = std::max(max_val, std::abs(out[i]));
    }
    ASSERT_TRUE(max_val > 0.1);
}
