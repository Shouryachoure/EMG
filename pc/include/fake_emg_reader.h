#pragma once
/// @file fake_emg_reader.h
/// @brief Synthetic EMG signal generator for hardware-free testing.
///
/// Generates distinguishable signal patterns for each gesture class:
///   - RELAX: low-amplitude noise (~0.05 RMS)
///   - GRASP: medium-amplitude sine bursts (~0.5 RMS)
///   - OPEN:  high-amplitude mixed frequency (~0.7 RMS)
///   - CLOSE: high-amplitude strong sine (~0.9 RMS)
///
/// The pattern cycles through classes automatically at a configurable rate.

#include "emg_reader.h"
#include <cstddef>
#include <cstdint>

namespace emg {

class FakeEMGReader : public EMGReader {
public:
    /// @param sample_rate_hz  Simulated sample rate (default 1000 Hz)
    /// @param batch_size      Number of samples returned per readSamples() call
    /// @param pattern_duration_ms  How long each gesture pattern lasts before switching
    explicit FakeEMGReader(double sample_rate_hz = 1000.0,
                           size_t batch_size = 32,
                           uint32_t pattern_duration_ms = 2000);

    bool initialize() override;
    std::vector<double> readSamples() override;
    bool isConnected() const override;
    std::string name() const override;
    double sampleRateHz() const override;
    void shutdown() override;

    /// Force a specific gesture class (1-4). 0 = resume auto-cycling.
    void setForcedClass(int class_id);

    /// Get the current gesture class being generated.
    int currentClass() const;

private:
    double generateSample(int gesture_class, double t) const;

    double   sample_rate_hz_;
    size_t   batch_size_;
    uint32_t pattern_duration_ms_;
    bool     connected_;
    int      forced_class_;        // 0 = auto-cycle
    size_t   sample_counter_;      // Total samples generated
};

} // namespace emg
