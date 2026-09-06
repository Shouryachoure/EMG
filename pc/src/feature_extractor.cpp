#include "feature_extractor.h"
#include <cmath>
#include <numeric>
#include <algorithm>

namespace emg {

FeatureExtractor::FeatureExtractor(const FeatureConfig& config)
    : config_(config) {
    buildFeatureList();
}

void FeatureExtractor::buildFeatureList() {
    enabled_features_.clear();

    if (config_.rms) {
        enabled_features_.push_back({"RMS", computeRMS});
    }
    if (config_.mav) {
        enabled_features_.push_back({"MAV", computeMAV});
    }
    if (config_.variance) {
        enabled_features_.push_back({"Variance", computeVariance});
    }
    if (config_.waveform_length) {
        enabled_features_.push_back({"WaveformLength", computeWaveformLength});
    }
    if (config_.zero_crossings) {
        enabled_features_.push_back({"ZeroCrossings", computeZeroCrossings});
    }
    if (config_.slope_sign_changes) {
        enabled_features_.push_back({"SlopeSignChanges", computeSlopeSignChanges});
    }
}

FeatureVector FeatureExtractor::extract(const std::vector<double>& samples) const {
    const size_t n = samples.size();
    if (n == 0) return FeatureVector(enabled_features_.size(), 0.0);

    // Fast-path: all 6 standard features enabled (standard production pipeline)
    if (config_.rms && config_.mav && config_.variance &&
        config_.waveform_length && config_.zero_crossings && config_.slope_sign_changes) {

        double sum = 0.0;
        double sum_sq = 0.0;
        double sum_abs = 0.0;
        double wl = 0.0;
        int zc = 0;
        int ssc = 0;
        constexpr double threshold = 0.01;

        sum += samples[0];
        sum_sq += samples[0] * samples[0];
        sum_abs += std::abs(samples[0]);

        for (size_t i = 1; i < n; ++i) {
            const double s = samples[i];
            const double prev = samples[i - 1];

            sum += s;
            sum_sq += s * s;
            sum_abs += std::abs(s);

            const double diff = std::abs(s - prev);
            wl += diff;

            if (((s > 0.0 && prev < 0.0) || (s < 0.0 && prev > 0.0)) && diff > threshold) {
                zc++;
            }

            if (i >= 2) {
                const double diff1 = s - prev;
                const double diff2 = prev - samples[i - 2];
                if (((diff1 > 0.0 && diff2 < 0.0) || (diff1 < 0.0 && diff2 > 0.0)) &&
                    (std::abs(diff1) > threshold || std::abs(diff2) > threshold)) {
                    ssc++;
                }
            }
        }

        const double inv_n = 1.0 / static_cast<double>(n);
        const double rms = std::sqrt(sum_sq * inv_n);
        const double mav = sum_abs * inv_n;
        const double mean = sum * inv_n;

        double sum_sq_diff = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double d = samples[i] - mean;
            sum_sq_diff += d * d;
        }
        const double var = (n > 1) ? (sum_sq_diff / static_cast<double>(n - 1)) : 0.0;

        return {rms, mav, var, wl, static_cast<double>(zc), static_cast<double>(ssc)};
    }

    // Fallback path for custom subsets
    FeatureVector features;
    features.reserve(enabled_features_.size());
    for (const auto& entry : enabled_features_) {
        features.push_back(entry.compute(samples));
    }
    return features;
}

std::vector<Feature> FeatureExtractor::extractNamed(const std::vector<double>& samples) const {
    std::vector<Feature> features;
    features.reserve(enabled_features_.size());

    for (const auto& entry : enabled_features_) {
        features.push_back({entry.name, entry.compute(samples)});
    }
    return features;
}

std::vector<std::string> FeatureExtractor::featureNames() const {
    std::vector<std::string> names;
    for (const auto& entry : enabled_features_) {
        names.push_back(entry.name);
    }
    return names;
}

size_t FeatureExtractor::featureCount() const {
    return enabled_features_.size();
}

// ============================================================
// Individual feature implementations
// ============================================================

double FeatureExtractor::computeRMS(const std::vector<double>& samples) {
    if (samples.empty()) return 0.0;

    double sum_sq = 0.0;
    for (const auto& s : samples) {
        sum_sq += s * s;
    }
    return std::sqrt(sum_sq / static_cast<double>(samples.size()));
}

double FeatureExtractor::computeMAV(const std::vector<double>& samples) {
    if (samples.empty()) return 0.0;

    double sum_abs = 0.0;
    for (const auto& s : samples) {
        sum_abs += std::abs(s);
    }
    return sum_abs / static_cast<double>(samples.size());
}

double FeatureExtractor::computeVariance(const std::vector<double>& samples) {
    if (samples.size() < 2) return 0.0;

    double mean = std::accumulate(samples.begin(), samples.end(), 0.0)
                  / static_cast<double>(samples.size());

    double sum_sq_diff = 0.0;
    for (const auto& s : samples) {
        double diff = s - mean;
        sum_sq_diff += diff * diff;
    }
    return sum_sq_diff / static_cast<double>(samples.size() - 1);  // Sample variance
}

double FeatureExtractor::computeWaveformLength(const std::vector<double>& samples) {
    if (samples.size() < 2) return 0.0;

    double wl = 0.0;
    for (size_t i = 1; i < samples.size(); ++i) {
        wl += std::abs(samples[i] - samples[i - 1]);
    }
    return wl;
}

double FeatureExtractor::computeZeroCrossings(const std::vector<double>& samples) {
    if (samples.size() < 2) return 0.0;

    // Threshold to avoid counting noise-induced crossings
    constexpr double threshold = 0.01;

    int count = 0;
    for (size_t i = 1; i < samples.size(); ++i) {
        if ((samples[i] > 0 && samples[i - 1] < 0) ||
            (samples[i] < 0 && samples[i - 1] > 0)) {
            if (std::abs(samples[i] - samples[i - 1]) > threshold) {
                count++;
            }
        }
    }
    return static_cast<double>(count);
}

double FeatureExtractor::computeSlopeSignChanges(const std::vector<double>& samples) {
    if (samples.size() < 3) return 0.0;

    // Threshold to avoid counting noise-induced changes
    constexpr double threshold = 0.01;

    int count = 0;
    for (size_t i = 2; i < samples.size(); ++i) {
        double diff1 = samples[i] - samples[i - 1];
        double diff2 = samples[i - 1] - samples[i - 2];

        if ((diff1 > 0 && diff2 < 0) || (diff1 < 0 && diff2 > 0)) {
            if (std::abs(diff1) > threshold || std::abs(diff2) > threshold) {
                count++;
            }
        }
    }
    return static_cast<double>(count);
}

} // namespace emg
