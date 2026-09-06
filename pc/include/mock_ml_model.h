#pragma once
/// @file mock_ml_model.h
/// @brief Deterministic mock ML model for testing the pipeline without real ML.
///
/// Classification logic (based on feature magnitude):
///   RMS < 0.2  → class 1 (RELAX), confidence 0.95
///   RMS < 0.5  → class 2 (GRASP), confidence 0.85
///   RMS < 0.8  → class 3 (OPEN),  confidence 0.80
///   RMS >= 0.8 → class 4 (CLOSE), confidence 0.90

#include "ml_model.h"

namespace emg {

class MockMLModel : public MLModel {
public:
    MockMLModel() = default;

    bool loadModel(const std::string& model_path = "") override;
    Prediction predict(const std::vector<double>& features) override;
    double getConfidence() const override;
    std::string name() const override;
    bool isReady() const override;

private:
    bool       ready_ = false;
    Prediction last_prediction_;
};

} // namespace emg
