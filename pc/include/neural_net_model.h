#pragma once
/// @file neural_net_model.h
/// @brief Native C++ Deep Multi-Layer Perceptron (MLP) for EMG gesture classification.
///
/// Implements emg::MLModel with zero external runtime dependencies.
/// Architecture: 6 inputs (RMS, MAV, VAR, WL, ZC, SSC) -> Dense(16, ReLU) -> Dense(12, ReLU) -> Dense(4, Softmax).
///
/// Can load custom weights exported by ml/train_my_arm.py (JSON),
/// or seamlessly fall back to built-in pre-calibrated default weights.

#include "ml_model.h"
#include <vector>
#include <string>

namespace emg {

/// Weights and biases for a single fully connected layer.
struct DenseLayerWeights {
    int in_features  = 0;
    int out_features = 0;
    std::vector<std::vector<double>> weights;  ///< [out_features][in_features]
    std::vector<double>              biases;   ///< [out_features]
};

/// Deep Neural Network (MLP) Classifier implementing emg::MLModel.
class NeuralNetModel : public MLModel {
public:
    NeuralNetModel();
    ~NeuralNetModel() override = default;

    /// Load weights from a JSON file, or fall back to default weights if empty/missing.
    /// @param model_path Path to JSON weights file (e.g. "models/emg_mlp_weights.json").
    /// @return true if loaded successfully.
    bool loadModel(const std::string& model_path = "") override;

    /// Run real-time forward pass inference on a 6-element feature vector.
    /// Execution time: < 10 microseconds.
    /// @param features Feature vector [RMS, MAV, VAR, WL, ZC, SSC].
    /// @return Prediction with class_id (1-4) and confidence [0.0, 1.0].
    Prediction predict(const std::vector<double>& features) override;

    /// Get confidence score of the last prediction.
    double getConfidence() const override;

    /// Model name descriptor.
    std::string name() const override;

    /// Returns true if model weights are loaded and ready.
    bool isReady() const override;

    /// Get normalized class probabilities from the last inference pass.
    const std::vector<double>& getProbabilities() const;

private:
    void initDefaultWeights();
    bool loadWeightsFromJson(const std::string& filepath);

    std::vector<double> forwardDense(const std::vector<double>& input,
                                     const DenseLayerWeights& layer,
                                     bool use_relu);
    std::vector<double> softmax(const std::vector<double>& z);

    DenseLayerWeights layer1_;  ///< 6 -> 16
    DenseLayerWeights layer2_;  ///< 16 -> 12
    DenseLayerWeights layer3_;  ///< 12 -> 4

    std::vector<double> feature_mean_;
    std::vector<double> feature_std_;

    bool ready_ = false;
    Prediction last_prediction_{0, 0.0};
    std::vector<double> last_probabilities_;
    std::string model_name_ = "NeuralNetModel (MLP 6-16-12-4)";
};

} // namespace emg
