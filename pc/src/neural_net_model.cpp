#include "neural_net_model.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>

namespace emg {

// ============================================================
// Lightweight JSON Parser for Network Weights
// ============================================================
namespace {

void skipWhitespace(const std::string& s, size_t& pos) {
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r' || s[pos] == '\n' || s[pos] == ':')) {
        pos++;
    }
}

bool findKey(const std::string& s, size_t& pos, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t found = s.find(needle, pos);
    if (found == std::string::npos) return false;
    pos = found + needle.size();
    return true;
}

std::vector<double> parse1DArray(const std::string& s, size_t& pos) {
    std::vector<double> result;
    skipWhitespace(s, pos);
    if (pos >= s.size() || s[pos] != '[') return result;
    pos++; // skip '['

    while (pos < s.size()) {
        skipWhitespace(s, pos);
        if (pos < s.size() && s[pos] == ']') {
            pos++;
            break;
        }
        // Parse double
        char* end = nullptr;
        double val = std::strtod(&s[pos], &end);
        if (end != &s[pos]) {
            result.push_back(val);
            pos += (end - &s[pos]);
        }
        skipWhitespace(s, pos);
        if (pos < s.size() && s[pos] == ',') {
            pos++;
        }
    }
    return result;
}

std::vector<std::vector<double>> parse2DArray(const std::string& s, size_t& pos) {
    std::vector<std::vector<double>> result;
    skipWhitespace(s, pos);
    if (pos >= s.size() || s[pos] != '[') return result;
    pos++; // skip outer '['

    while (pos < s.size()) {
        skipWhitespace(s, pos);
        if (pos < s.size() && s[pos] == ']') {
            pos++;
            break;
        }
        if (pos < s.size() && s[pos] == '[') {
            result.push_back(parse1DArray(s, pos));
        }
        skipWhitespace(s, pos);
        if (pos < s.size() && s[pos] == ',') {
            pos++;
        }
    }
    return result;
}

} // anonymous namespace

// ============================================================
// NeuralNetModel Implementation
// ============================================================

NeuralNetModel::NeuralNetModel() {
    initDefaultWeights();
}

void NeuralNetModel::initDefaultWeights() {
    // Default architecture: 6 -> 16 -> 12 -> 4
    layer1_.in_features = 6;
    layer1_.out_features = 16;
    layer1_.weights.assign(16, std::vector<double>(6, 0.0));
    layer1_.biases.assign(16, 0.0);

    layer2_.in_features = 16;
    layer2_.out_features = 12;
    layer2_.weights.assign(12, std::vector<double>(16, 0.0));
    layer2_.biases.assign(12, 0.0);

    layer3_.in_features = 12;
    layer3_.out_features = 4;
    layer3_.weights.assign(4, std::vector<double>(12, 0.0));
    layer3_.biases.assign(4, 0.0);

    feature_mean_ = {0.20, 0.16, 0.05, 30.0, 50.0, 80.0};
    feature_std_  = {0.10, 0.08, 0.04, 15.0, 12.0, 25.0};

    // Pre-calibrate weights so that:
    // Class 1 (RELAX): dominant when features are near zero / low energy
    // Class 2 (GRASP): dominant when RMS/MAV/VAR are moderately high
    // Class 3 (OPEN): dominant when high frequency (ZC, SSC) are elevated
    // Class 4 (CLOSE): dominant when strong amplitude with high waveform length
    layer1_.weights[0][0] = 2.0;  // RMS
    layer1_.weights[0][1] = 1.5;  // MAV
    layer1_.weights[1][3] = 1.8;  // WL
    layer1_.weights[2][4] = 2.2;  // ZC
    layer1_.weights[3][5] = 2.0;  // SSC

    for (size_t j = 0; j < 12; ++j) {
        layer2_.weights[j][j % 4] = 1.5;
    }

    // Class 1: Relax (favors low normalized RMS)
    layer3_.biases[0] = 1.0;
    layer3_.weights[0][0] = -2.5;

    // Class 2: Grasp (favors positive normalized RMS/MAV)
    layer3_.biases[1] = -0.5;
    layer3_.weights[1][0] = 2.8;

    // Class 3: Open (favors ZC/SSC)
    layer3_.biases[2] = -0.8;
    layer3_.weights[2][2] = 3.0;

    // Class 4: Close (favors WL + high amplitude)
    layer3_.biases[3] = -1.0;
    layer3_.weights[3][1] = 2.5;
    layer3_.weights[3][3] = 1.5;

    ready_ = true;
    model_name_ = "NeuralNetModel (MLP 6-16-12-4 [Default Pre-calibrated])";
}

bool NeuralNetModel::loadModel(const std::string& model_path) {
    if (model_path.empty()) {
        LOG_INFO("NeuralNetModel", "No model path specified, using pre-calibrated default weights");
        initDefaultWeights();
        return true;
    }

    if (loadWeightsFromJson(model_path)) {
        LOG_INFO("NeuralNetModel", "Successfully loaded neural network weights from: " + model_path);
        model_name_ = "NeuralNetModel (MLP 6-16-12-4 [" + model_path + "])";
        return true;
    }

    LOG_WARN("NeuralNetModel", "Failed to load '" + model_path + "', falling back to default weights");
    initDefaultWeights();
    return true;
}

bool NeuralNetModel::loadWeightsFromJson(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_WARN("NeuralNetModel", "Could not open weights file: " + filepath);
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string json = ss.str();
    file.close();

    size_t pos = 0;

    // 1. Feature normalization vectors
    if (findKey(json, pos, "feature_mean")) {
        auto mean = parse1DArray(json, pos);
        if (mean.size() == 6) feature_mean_ = mean;
    }
    pos = 0;
    if (findKey(json, pos, "feature_std")) {
        auto std_dev = parse1DArray(json, pos);
        if (std_dev.size() == 6) feature_std_ = std_dev;
    }

    // Helper lambda to load a layer
    auto loadLayer = [&](const std::string& layer_name, DenseLayerWeights& layer, int expected_in, int expected_out) -> bool {
        size_t lpos = 0;
        if (!findKey(json, lpos, layer_name)) return false;

        // Find weights
        size_t wpos = lpos;
        if (!findKey(json, wpos, "weights")) return false;
        auto w = parse2DArray(json, wpos);
        if (w.size() != static_cast<size_t>(expected_out)) return false;
        for (const auto& row : w) {
            if (row.size() != static_cast<size_t>(expected_in)) return false;
        }

        // Find biases
        size_t bpos = lpos;
        if (!findKey(json, bpos, "biases")) return false;
        auto b = parse1DArray(json, bpos);
        if (b.size() != static_cast<size_t>(expected_out)) return false;

        layer.in_features  = expected_in;
        layer.out_features = expected_out;
        layer.weights      = std::move(w);
        layer.biases       = std::move(b);
        return true;
    };

    if (!loadLayer("layer1", layer1_, 6, 16)) {
        LOG_WARN("NeuralNetModel", "Failed parsing layer1 (expected 6 -> 16)");
        return false;
    }
    if (!loadLayer("layer2", layer2_, 16, 12)) {
        LOG_WARN("NeuralNetModel", "Failed parsing layer2 (expected 16 -> 12)");
        return false;
    }
    if (!loadLayer("layer3", layer3_, 12, 4)) {
        LOG_WARN("NeuralNetModel", "Failed parsing layer3 (expected 12 -> 4)");
        return false;
    }

    ready_ = true;
    return true;
}

std::vector<double> NeuralNetModel::forwardDense(const std::vector<double>& input,
                                                 const DenseLayerWeights& layer,
                                                 bool use_relu) {
    std::vector<double> output(layer.out_features, 0.0);
    for (int j = 0; j < layer.out_features; ++j) {
        double val = layer.biases[j];
        for (int i = 0; i < layer.in_features; ++i) {
            val += layer.weights[j][i] * input[i];
        }
        output[j] = use_relu ? std::max(0.0, val) : val;
    }
    return output;
}

std::vector<double> NeuralNetModel::softmax(const std::vector<double>& z) {
    std::vector<double> probs(z.size(), 0.0);
    if (z.empty()) return probs;

    double max_z = *std::max_element(z.begin(), z.end());
    double sum_exp = 0.0;
    for (size_t i = 0; i < z.size(); ++i) {
        probs[i] = std::exp(z[i] - max_z);
        sum_exp += probs[i];
    }
    if (sum_exp > 1e-12) {
        for (size_t i = 0; i < z.size(); ++i) {
            probs[i] /= sum_exp;
        }
    }
    return probs;
}

Prediction NeuralNetModel::predict(const std::vector<double>& features) {
    if (!ready_ || features.empty()) {
        last_prediction_ = {0, 0.0};
        last_probabilities_ = {0.0, 0.0, 0.0, 0.0};
        return last_prediction_;
    }

    // 1. Feature normalization (z-score scaling)
    std::vector<double> x_norm(6, 0.0);
    for (size_t i = 0; i < 6; ++i) {
        double raw_val = (i < features.size()) ? features[i] : 0.0;
        double mean = (i < feature_mean_.size()) ? feature_mean_[i] : 0.0;
        double std_dev = (i < feature_std_.size() && feature_std_[i] > 1e-6) ? feature_std_[i] : 1.0;
        x_norm[i] = (raw_val - mean) / std_dev;
    }

    // 2. Forward pass: Layer 1 (Dense 6 -> 16 + ReLU)
    auto h1 = forwardDense(x_norm, layer1_, true);

    // 3. Forward pass: Layer 2 (Dense 16 -> 12 + ReLU)
    auto h2 = forwardDense(h1, layer2_, true);

    // 4. Forward pass: Layer 3 (Dense 12 -> 4 Linear)
    auto z3 = forwardDense(h2, layer3_, false);

    // 5. Activation: Softmax over 4 gesture classes
    last_probabilities_ = softmax(z3);

    // 6. Classification: Argmax (1-indexed: 1=RELAX, 2=GRASP, 3=OPEN, 4=CLOSE)
    int best_class = 1;
    double max_prob = last_probabilities_[0];
    for (size_t i = 1; i < last_probabilities_.size(); ++i) {
        if (last_probabilities_[i] > max_prob) {
            max_prob = last_probabilities_[i];
            best_class = static_cast<int>(i) + 1;
        }
    }

    last_prediction_ = {best_class, max_prob};
    return last_prediction_;
}

double NeuralNetModel::getConfidence() const {
    return last_prediction_.confidence;
}

std::string NeuralNetModel::name() const {
    return model_name_;
}

bool NeuralNetModel::isReady() const {
    return ready_;
}

const std::vector<double>& NeuralNetModel::getProbabilities() const {
    return last_probabilities_;
}

} // namespace emg
