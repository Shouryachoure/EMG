#include "signal_processor.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace emg {

static constexpr double PI = 3.14159265358979323846;

SignalProcessor::SignalProcessor(const ProcessingConfig& config)
    : config_(config) {}

std::vector<double> SignalProcessor::process(const std::vector<double>& raw_samples) const {
    if (raw_samples.empty()) return {};

    // Step 1: Remove DC offset
    auto dc_removed = removeDC(raw_samples);

    // Step 2: Bandpass filter
    auto filtered = bandpassFilter(dc_removed);

    // Step 3: Normalize (optional)
    if (config_.normalize) {
        filtered = normalize(filtered);
    }

    return filtered;
}

std::vector<double> SignalProcessor::removeDC(const std::vector<double>& samples) {
    if (samples.empty()) return {};

    double mean = std::accumulate(samples.begin(), samples.end(), 0.0)
                  / static_cast<double>(samples.size());

    std::vector<double> result(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        result[i] = samples[i] - mean;
    }
    return result;
}

std::vector<double> SignalProcessor::bandpassFilter(const std::vector<double>& samples) const {
    if (samples.empty()) return {};

    // 2nd-order IIR Butterworth bandpass filter
    // Designed using bilinear transform
    //
    // This is a simplified implementation suitable for EMG preprocessing.
    // For production, consider using a proper DSP library.

    const double fs = config_.sample_rate_hz;
    const double fl = config_.filter_low_hz;
    const double fh = config_.filter_high_hz;

    if (fs <= 0.0 || fl <= 0.0 || fh <= 0.0 || fl >= fh || fh >= fs / 2.0) {
        // Invalid filter parameters — return input unchanged
        return samples;
    }

    // Pre-warp frequencies
    const double wl = std::tan(PI * fl / fs);
    const double wh = std::tan(PI * fh / fs);
    const double bw = wh - wl;
    const double w0 = wl * wh;  // center frequency squared (geometric mean)

    // 2nd-order bandpass coefficients (Butterworth)
    // H(s) = s*bw / (s^2 + s*bw + w0) transformed via bilinear
    const double Q = std::sqrt(w0) / bw;
    const double w0_sqrt = std::sqrt(w0);

    // Bilinear transform coefficients for a second-order bandpass section
    const double alpha = std::sin(2.0 * PI * w0_sqrt / (2.0 * fs)) / (2.0 * Q);

    // Use center frequency for the digital filter
    const double w_center = 2.0 * std::atan(w0_sqrt);  // digital center frequency

    const double cos_w = std::cos(w_center);
    const double sin_w = std::sin(w_center);
    const double alpha2 = sin_w / (2.0 * Q);

    // Numerator coefficients (bandpass: b0 = alpha, b1 = 0, b2 = -alpha)
    const double b0 = alpha2;
    const double b1 = 0.0;
    const double b2 = -alpha2;

    // Denominator coefficients
    const double a0 = 1.0 + alpha2;
    const double a1 = -2.0 * cos_w;
    const double a2 = 1.0 - alpha2;

    // Normalize
    const double nb0 = b0 / a0;
    const double nb1 = b1 / a0;
    const double nb2 = b2 / a0;
    const double na1 = a1 / a0;
    const double na2 = a2 / a0;

    // Apply filter (Direct Form II Transposed)
    std::vector<double> output(samples.size());
    double z1 = 0.0, z2 = 0.0;

    for (size_t i = 0; i < samples.size(); ++i) {
        double x = samples[i];
        double y = nb0 * x + z1;
        z1 = nb1 * x - na1 * y + z2;
        z2 = nb2 * x - na2 * y;
        output[i] = y;
    }

    return output;
}

std::vector<double> SignalProcessor::normalize(const std::vector<double>& samples) {
    if (samples.empty()) return {};

    double max_abs = 0.0;
    for (const auto& s : samples) {
        max_abs = std::max(max_abs, std::abs(s));
    }

    if (max_abs < 1e-10) {
        // Signal is essentially zero — return zeros
        return std::vector<double>(samples.size(), 0.0);
    }

    std::vector<double> result(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
        result[i] = samples[i] / max_abs;
    }
    return result;
}

} // namespace emg
