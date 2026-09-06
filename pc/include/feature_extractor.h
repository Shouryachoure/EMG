#pragma once
/// @file feature_extractor.h
/// @brief Configurable EMG feature extraction.
///
/// Supported features (from whiteboard / spec):
///   - RMS (Root Mean Square)
///   - MAV (Mean Absolute Value)
///   - Variance
///   - WL  (Waveform Length)
///   - ZC  (Zero Crossings)
///   - SSC (Slope Sign Changes)
///
/// Feature selection is configurable — not all features need to be active.
/// New features can be added without modifying the rest of the pipeline.

#include <vector>
#include <string>
#include <functional>
#include <map>

namespace emg {

/// Configuration: which features to compute.
struct FeatureConfig {
    bool rms                = true;
    bool mav                = true;
    bool variance           = true;
    bool waveform_length    = true;
    bool zero_crossings     = true;
    bool slope_sign_changes = true;
};

/// A named feature value.
struct Feature {
    std::string name;
    double      value;
};

/// Result of feature extraction: an ordered vector of feature values.
using FeatureVector = std::vector<double>;

class FeatureExtractor {
public:
    explicit FeatureExtractor(const FeatureConfig& config = {});

    /// Extract features from a processed signal window.
    /// @param samples Processed (filtered) EMG samples.
    /// @return Feature vector (ordered list of enabled feature values).
    FeatureVector extract(const std::vector<double>& samples) const;

    /// Extract features with names (for debugging/logging).
    std::vector<Feature> extractNamed(const std::vector<double>& samples) const;

    /// Get the names of the enabled features (in order).
    std::vector<std::string> featureNames() const;

    /// Get the number of enabled features.
    size_t featureCount() const;

    // ---- Individual feature functions (public for testing) ----

    /// Root Mean Square
    static double computeRMS(const std::vector<double>& samples);

    /// Mean Absolute Value
    static double computeMAV(const std::vector<double>& samples);

    /// Variance
    static double computeVariance(const std::vector<double>& samples);

    /// Waveform Length (sum of absolute differences between consecutive samples)
    static double computeWaveformLength(const std::vector<double>& samples);

    /// Zero Crossings (number of times the signal crosses zero)
    static double computeZeroCrossings(const std::vector<double>& samples);

    /// Slope Sign Changes (number of times the slope changes sign)
    static double computeSlopeSignChanges(const std::vector<double>& samples);

private:
    FeatureConfig config_;

    /// Internal registry of enabled feature functions.
    struct FeatureEntry {
        std::string name;
        std::function<double(const std::vector<double>&)> compute;
    };
    std::vector<FeatureEntry> enabled_features_;

    void buildFeatureList();
};

} // namespace emg
