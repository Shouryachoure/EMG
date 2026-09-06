#include "fake_emg_reader.h"
#include <cmath>
#include <random>
#include <chrono>
#include <thread>

namespace emg {

// Constants for signal generation
static constexpr double PI = 3.14159265358979323846;

FakeEMGReader::FakeEMGReader(double sample_rate_hz,
                             size_t batch_size,
                             uint32_t pattern_duration_ms)
    : sample_rate_hz_(sample_rate_hz)
    , batch_size_(batch_size)
    , pattern_duration_ms_(pattern_duration_ms)
    , connected_(false)
    , forced_class_(0)
    , sample_counter_(0) {}

bool FakeEMGReader::initialize() {
    connected_ = true;
    sample_counter_ = 0;
    return true;
}

std::vector<double> FakeEMGReader::readSamples() {
    if (!connected_) return {};

    std::vector<double> samples(batch_size_);

    // Simulate real-time delay based on batch size and sample rate
    auto delay_us = static_cast<long long>(
        (batch_size_ / sample_rate_hz_) * 1'000'000.0);
    std::this_thread::sleep_for(std::chrono::microseconds(delay_us));

    int gesture_class = currentClass();

    for (size_t i = 0; i < batch_size_; ++i) {
        double t = static_cast<double>(sample_counter_) / sample_rate_hz_;
        samples[i] = generateSample(gesture_class, t);
        sample_counter_++;
    }

    return samples;
}

bool FakeEMGReader::isConnected() const {
    return connected_;
}

std::string FakeEMGReader::name() const {
    return "FakeEMGReader";
}

double FakeEMGReader::sampleRateHz() const {
    return sample_rate_hz_;
}

void FakeEMGReader::shutdown() {
    connected_ = false;
}

void FakeEMGReader::setForcedClass(int class_id) {
    forced_class_ = class_id;
}

int FakeEMGReader::currentClass() const {
    if (forced_class_ >= 1 && forced_class_ <= 4) {
        return forced_class_;
    }

    // Auto-cycle through classes 1→2→3→4→1→...
    double total_time_ms = (static_cast<double>(sample_counter_) / sample_rate_hz_) * 1000.0;
    int cycle_index = static_cast<int>(total_time_ms / pattern_duration_ms_) % 4;
    return cycle_index + 1;  // 1-based
}

double FakeEMGReader::generateSample(int gesture_class, double t) const {
    // Thread-local random engine for noise generation
    thread_local std::mt19937 gen(
        static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
    thread_local std::normal_distribution<double> noise(0.0, 1.0);

    double n = noise(gen);

    switch (gesture_class) {
        case 1:  // RELAX — low-amplitude noise only
            return 0.05 * n;

        case 2:  // GRASP — medium-amplitude sine bursts + noise
            return 0.4 * std::sin(2.0 * PI * 50.0 * t)  // 50 Hz component
                 + 0.15 * std::sin(2.0 * PI * 120.0 * t) // 120 Hz component
                 + 0.08 * n;

        case 3:  // OPEN — high-amplitude mixed frequency + noise
            return 0.5 * std::sin(2.0 * PI * 80.0 * t)
                 + 0.25 * std::sin(2.0 * PI * 200.0 * t)
                 + 0.1 * n;

        case 4:  // CLOSE — strong sine dominant + noise
            return 0.7 * std::sin(2.0 * PI * 60.0 * t)
                 + 0.3 * std::sin(2.0 * PI * 150.0 * t)
                 + 0.1 * n;

        default: // Unknown — treat as RELAX
            return 0.05 * n;
    }
}

} // namespace emg
