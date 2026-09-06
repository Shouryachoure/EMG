#include "signal_processor.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace emg {

static constexpr double PI = 3.14159265358979323846;

SignalProcessor::SignalProcessor(const ProcessingConfig& config)
    : config_(config) {
    computeFilterCoefficients();
}

void SignalProcessor::computeFilterCoefficients() {
    const double fs = config_.sample_rate_hz;
    const double fl = config_.filter_low_hz;
    const double fh = config_.filter_high_hz;

    if (fs <= 0.0 || fl <= 0.0 || fh <= 0.0 || fl >= fh || fh >= fs / 2.0) {
        filter_valid_ = false;
        return;
    }

    // Pre-warp frequencies
    const double wl = std::tan(PI * fl / fs);
    const double wh = std::tan(PI * fh / fs);
    const double bw = wh - wl;
    const double w0 = wl * wh;  // center frequency squared
    const double Q = std::sqrt(w0) / bw;
    const double w0_sqrt = std::sqrt(w0);

    // Digital center frequency
    const double w_center = 2.0 * std::atan(w0_sqrt);
    const double cos_w = std::cos(w_center);
    const double sin_w = std::sin(w_center);
    const double alpha2 = sin_w / (2.0 * Q);

    // Numerator & Denominator
    const double b0 = alpha2;
    const double b1 = 0.0;
    const double b2 = -alpha2;
    const double a0 = 1.0 + alpha2;
    const double a1 = -2.0 * cos_w;
    const double a2 = 1.0 - alpha2;

    // Normalized coefficients
    nb0_ = b0 / a0;
    nb1_ = b1 / a0;
    nb2_ = b2 / a0;
    na1_ = a1 / a0;
    na2_ = a2 / a0;
    filter_valid_ = true;
}

std::vector<double> SignalProcessor::process(const std::vector<double>& raw_samples) const {
    const size_t n = raw_samples.size();
    if (n == 0) return {};

    // Single-allocation fast pipeline:
    // Pass 1: Compute mean
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sum += raw_samples[i];
    }
    const double mean = sum / static_cast<double>(n);

    std::vector<double> output(n);
    double max_abs = 0.0;

    if (filter_valid_) {
        // Pass 2: Direct Form II Transposed IIR filter with inline DC subtraction & peak tracking
        double z1 = 0.0, z2 = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double x = raw_samples[i] - mean;
            const double y = nb0_ * x + z1;
            z1 = nb1_ * x - na1_ * y + z2;
            z2 = nb2_ * x - na2_ * y;
            output[i] = y;
            const double abs_y = std::abs(y);
            if (abs_y > max_abs) max_abs = abs_y;
        }
    } else {
        // Filter bypass
        for (size_t i = 0; i < n; ++i) {
            const double x = raw_samples[i] - mean;
            output[i] = x;
            const double abs_x = std::abs(x);
            if (abs_x > max_abs) max_abs = abs_x;
        }
    }

    // Pass 3 (optional): In-place peak normalization
    if (config_.normalize) {
        if (max_abs < 1e-10) {
            std::fill(output.begin(), output.end(), 0.0);
        } else {
            const double inv_max = 1.0 / max_abs;
            for (size_t i = 0; i < n; ++i) {
                output[i] *= inv_max;
            }
        }
    }

    return output;
}

std::vector<double> SignalProcessor::removeDC(const std::vector<double>& samples) {
    const size_t n = samples.size();
    if (n == 0) return {};

    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sum += samples[i];
    }
    const double mean = sum / static_cast<double>(n);

    std::vector<double> result(n);
    for (size_t i = 0; i < n; ++i) {
        result[i] = samples[i] - mean;
    }
    return result;
}

std::vector<double> SignalProcessor::bandpassFilter(const std::vector<double>& samples) const {
    const size_t n = samples.size();
    if (n == 0 || !filter_valid_) return samples;

    std::vector<double> output(n);
    double z1 = 0.0, z2 = 0.0;

    for (size_t i = 0; i < n; ++i) {
        const double x = samples[i];
        const double y = nb0_ * x + z1;
        z1 = nb1_ * x - na1_ * y + z2;
        z2 = nb2_ * x - na2_ * y;
        output[i] = y;
    }

    return output;
}

std::vector<double> SignalProcessor::normalize(const std::vector<double>& samples) {
    const size_t n = samples.size();
    if (n == 0) return {};

    double max_abs = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double abs_s = std::abs(samples[i]);
        if (abs_s > max_abs) max_abs = abs_s;
    }

    if (max_abs < 1e-10) {
        return std::vector<double>(n, 0.0);
    }

    const double inv_max = 1.0 / max_abs;
    std::vector<double> result(n);
    for (size_t i = 0; i < n; ++i) {
        result[i] = samples[i] * inv_max;
    }
    return result;
}

} // namespace emg
