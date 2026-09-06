#include "decision_engine.h"

namespace emg {

DecisionEngine::DecisionEngine(double confidence_threshold)
    : confidence_threshold_(confidence_threshold) {
    // Default mapping from whiteboard: 1=RELAX, 2=GRASP, 3=OPEN, 4=CLOSE
    class_to_command_ = {
        {1, Command::RELAX},
        {2, Command::GRASP},
        {3, Command::OPEN},
        {4, Command::CLOSE}
    };
}

void DecisionEngine::setMapping(const std::map<int, Command>& mapping) {
    class_to_command_ = mapping;
}

void DecisionEngine::setConfidenceThreshold(double threshold) {
    confidence_threshold_ = threshold;
}

Decision DecisionEngine::decide(const Prediction& prediction) const {
    Decision decision;
    decision.class_id    = prediction.class_id;
    decision.confidence  = prediction.confidence;
    decision.timestamp_ms = currentTimestampMs();

    // Safety: reject low-confidence predictions
    if (prediction.confidence < confidence_threshold_) {
        decision.command = Command::NONE;
        return decision;
    }

    // Look up the class in the mapping
    auto it = class_to_command_.find(prediction.class_id);
    if (it != class_to_command_.end()) {
        decision.command = it->second;
    } else {
        // Unknown class — safe default
        decision.command = Command::NONE;
    }

    return decision;
}

double DecisionEngine::confidenceThreshold() const {
    return confidence_threshold_;
}

const std::map<int, Command>& DecisionEngine::mapping() const {
    return class_to_command_;
}

uint32_t DecisionEngine::currentTimestampMs() {
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return static_cast<uint32_t>(ms & 0xFFFFFFFF);
}

} // namespace emg
