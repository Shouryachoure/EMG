#pragma once
/// @file ml_model.h
/// @brief Abstract interface for ML inference.
///
/// Concrete implementations:
///   - MockMLModel: deterministic classifier for testing
///   - (future) TFLiteModel: TensorFlow Lite inference
///   - (future) ONNXModel: ONNX Runtime inference
///
/// The interface returns a prediction with class_id and confidence.
/// The DecisionEngine uses confidence thresholding to produce safe commands.

#include <vector>
#include <string>
#include <cstdint>

namespace emg {

/// Result of ML prediction.
struct Prediction {
    int    class_id   = 0;      ///< Predicted gesture class (1-4)
    double confidence = 0.0;    ///< Confidence score [0.0, 1.0]
};

/// Abstract ML model interface.
class MLModel {
public:
    virtual ~MLModel() = default;

    /// Load a model from file (or initialize for mock).
    /// @param model_path Path to model file (empty for mock).
    /// @return true on success.
    virtual bool loadModel(const std::string& model_path = "") = 0;

    /// Run inference on a feature vector.
    /// @param features Feature vector from FeatureExtractor.
    /// @return Prediction with class_id and confidence.
    virtual Prediction predict(const std::vector<double>& features) = 0;

    /// Get the confidence of the last prediction.
    virtual double getConfidence() const = 0;

    /// Get a human-readable name for this model.
    virtual std::string name() const = 0;

    /// Check if the model is loaded and ready for inference.
    virtual bool isReady() const = 0;
};

} // namespace emg
