#include "mock_ml_model.h"
#include <cmath>
#include <numeric>

namespace emg {

bool MockMLModel::loadModel(const std::string& /*model_path*/) {
    ready_ = true;
    return true;
}

Prediction MockMLModel::predict(const std::vector<double>& features) {
    if (!ready_ || features.empty()) {
        last_prediction_ = {0, 0.0};
        return last_prediction_;
    }

    // Use the first feature (typically RMS) as the primary discriminator.
    // If multiple features are present, use the mean magnitude.
    double magnitude = 0.0;
    if (features.size() == 1) {
        magnitude = std::abs(features[0]);
    } else {
        // Compute mean absolute value of all features
        double sum = 0.0;
        for (const auto& f : features) {
            sum += std::abs(f);
        }
        magnitude = sum / static_cast<double>(features.size());
    }

    // Deterministic classification based on magnitude thresholds
    if (magnitude < 0.2) {
        last_prediction_ = {1, 0.95};  // RELAX — very confident (low activity)
    } else if (magnitude < 0.5) {
        last_prediction_ = {2, 0.85};  // GRASP — medium activity
    } else if (magnitude < 0.8) {
        last_prediction_ = {3, 0.80};  // OPEN — higher activity
    } else {
        last_prediction_ = {4, 0.90};  // CLOSE — strong activity
    }

    return last_prediction_;
}

double MockMLModel::getConfidence() const {
    return last_prediction_.confidence;
}

std::string MockMLModel::name() const {
    return "MockMLModel";
}

bool MockMLModel::isReady() const {
    return ready_;
}

} // namespace emg
