/// @file test_neural_net.cpp
/// @brief Unit tests for NeuralNetModel (Option 1).

#include "neural_net_model.h"
#include "test_framework.h"
#include <vector>
#include <cmath>
#include <numeric>
#include <fstream>

using namespace emg;

TEST(neural_net_default_initialization) {
    NeuralNetModel model;
    ASSERT_TRUE(model.isReady());
    ASSERT_TRUE(model.loadModel(""));
    ASSERT_TRUE(model.isReady());
    ASSERT_EQ(model.name().substr(0, 14), "NeuralNetModel");
}

TEST(neural_net_predict_probabilities_valid) {
    NeuralNetModel model;
    ASSERT_TRUE(model.loadModel());

    // Feature vector: [RMS, MAV, VAR, WL, ZC, SSC]
    std::vector<double> features = {0.02, 0.015, 0.001, 5.0, 10.0, 15.0};
    auto pred = model.predict(features);

    ASSERT_TRUE(pred.class_id >= 1 && pred.class_id <= 4);
    ASSERT_TRUE(pred.confidence >= 0.0 && pred.confidence <= 1.0);
    ASSERT_NEAR(model.getConfidence(), pred.confidence, 1e-6);

    const auto& probs = model.getProbabilities();
    ASSERT_EQ(probs.size(), static_cast<size_t>(4));

    double sum = 0.0;
    for (double p : probs) {
        ASSERT_TRUE(p >= 0.0);
        sum += p;
    }
    ASSERT_NEAR(sum, 1.0, 1e-4);
    ASSERT_NEAR(probs[pred.class_id - 1], pred.confidence, 1e-6);
}

TEST(neural_net_rest_detection) {
    NeuralNetModel model;
    ASSERT_TRUE(model.loadModel());

    // Very low energy signal -> should predict Class 1 (RELAX)
    std::vector<double> rest_features = {0.01, 0.008, 0.0001, 2.0, 5.0, 8.0};
    auto pred = model.predict(rest_features);

    ASSERT_EQ(pred.class_id, 1);  // RELAX
    ASSERT_TRUE(pred.confidence >= 0.50);
}

TEST(neural_net_load_json_file) {
    NeuralNetModel model;
    std::vector<std::string> candidates = {
        "models/emg_mlp_weights.json",
        "../models/emg_mlp_weights.json",
        "../../models/emg_mlp_weights.json",
        "d:/EMG/models/emg_mlp_weights.json"
    };

    std::string chosen_path;
    for (const auto& path : candidates) {
        std::ifstream test_f(path);
        if (test_f.is_open()) {
            chosen_path = path;
            break;
        }
    }

    ASSERT_FALSE(chosen_path.empty());
    ASSERT_TRUE(model.loadModel(chosen_path));
    ASSERT_TRUE(model.isReady());

    // Test inference on low vs high energy
    std::vector<double> rest_features = {0.02, 0.015, 0.0005, 3.0, 8.0, 12.0};
    auto p_rest = model.predict(rest_features);
    ASSERT_TRUE(p_rest.class_id >= 1 && p_rest.class_id <= 4);
    ASSERT_TRUE(p_rest.confidence > 0.0);

    // Active features
    std::vector<double> active_features = {0.50, 0.40, 0.15, 60.0, 65.0, 110.0};
    auto p_active = model.predict(active_features);
    ASSERT_TRUE(p_active.class_id >= 1 && p_active.class_id <= 4);
    ASSERT_TRUE(p_active.confidence > 0.0);
}
