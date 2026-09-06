#pragma once
/// @file decision_engine.h
/// @brief Maps ML predictions to actuator commands with confidence gating.
///
/// Decision logic:
///   1. If ML confidence < threshold → Command::NONE (safe, no action)
///   2. Look up class_id in configurable mapping → Command
///   3. If class_id is not in mapping → Command::NONE
///
/// The mapping from the whiteboard:
///   1 = RELAX, 2 = GRASP, 3 = OPEN, 4 = CLOSE

#include "command.h"
#include "ml_model.h"
#include <map>
#include <chrono>
#include <array>
#include <cstdint>

namespace emg {

/// A decision produced by the engine — includes the command and metadata.
struct Decision {
    Command command        = Command::NONE;
    int     class_id       = 0;
    double  confidence     = 0.0;
    uint32_t timestamp_ms  = 0;   ///< Milliseconds since epoch (truncated to 32 bits)
    uint8_t  intensity     = 0;   ///< 0-100% proportional contraction force
    std::array<uint8_t, 5> finger_angles = {90, 90, 90, 90, 90}; ///< Individual finger articulation angles
};

class DecisionEngine {
public:
    /// @param confidence_threshold Minimum confidence to act (default 0.6)
    explicit DecisionEngine(double confidence_threshold = 0.6);

    /// Set the class-to-command mapping.
    void setMapping(const std::map<int, Command>& mapping);

    /// Set the confidence threshold.
    void setConfidenceThreshold(double threshold);

    /// Produce a decision from an ML prediction.
    /// @param prediction ML output (class_id + confidence).
    /// @return Decision with command, class_id, confidence, and timestamp.
    Decision decide(const Prediction& prediction) const;

    /// Get the current confidence threshold.
    double confidenceThreshold() const;

    /// Get the current mapping.
    const std::map<int, Command>& mapping() const;

private:
    double                 confidence_threshold_;
    std::map<int, Command> class_to_command_;

    /// Get current time in milliseconds.
    static uint32_t currentTimestampMs();
};

} // namespace emg
