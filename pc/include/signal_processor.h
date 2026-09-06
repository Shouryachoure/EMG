#pragma once
/// @file signal_processor.h
/// @brief EMG signal preprocessing: filtering, DC removal, normalization.

#include <vector>

namespace emg {

/// Configuration for signal processing.
struct ProcessingConfig {
    double filter_low_hz  = 20.0;    ///< Bandpass low cutoff (Hz)
    double filter_high_hz = 450.0;   ///< Bandpass high cutoff (Hz)
    double sample_rate_hz = 1000.0;  ///< Sample rate (Hz)
    bool   normalize      = true;    ///< Normalize output to [-1, 1]
};

class SignalProcessor {
public:
    explicit SignalProcessor(const ProcessingConfig& config = {});

    /// Process a window of raw EMG samples.
    /// Applies: DC removal → bandpass filter → normalization (optional).
    /// @param raw_samples Input raw samples.
    /// @return Processed samples.
    std::vector<double> process(const std::vector<double>& raw_samples) const;

    /// Remove DC offset (subtract mean).
    static std::vector<double> removeDC(const std::vector<double>& samples);

    /// Apply a simple IIR bandpass filter (Butterworth approximation).
    std::vector<double> bandpassFilter(const std::vector<double>& samples) const;

    /// Normalize samples to [-1, 1] range.
    static std::vector<double> normalize(const std::vector<double>& samples);

private:
    void computeFilterCoefficients();

    ProcessingConfig config_;
    bool   filter_valid_ = false;
    double nb0_ = 0.0;
    double nb1_ = 0.0;
    double nb2_ = 0.0;
    double na1_ = 0.0;
    double na2_ = 0.0;
};

} // namespace emg
