#pragma once
/// @file config.h
/// @brief JSON configuration loader for the EMG control system.
///
/// Reads config.json and populates typed configuration structs.
/// Provides defaults for all parameters so the system runs without a config file.

#include "signal_processor.h"
#include "feature_extractor.h"
#include "decision_engine.h"
#include "udp_sender.h"
#include "command.h"
#include <string>
#include <map>

namespace emg {

/// Top-level system configuration.
struct SystemConfig {
    // EMG source
    std::string emg_source     = "fake";     // "fake" or "serial"
    std::string serial_port    = "COM3";
    int         serial_baud    = 115200;
    double      sample_rate_hz = 1000.0;
    size_t      batch_size     = 32;

    // Signal processing
    ProcessingConfig processing;

    // Feature extraction
    FeatureConfig features;

    // ML
    std::string ml_model_type  = "mock";
    std::string ml_model_path  = "";
    double      confidence_threshold = 0.6;

    // Decision mapping
    std::map<int, Command> decision_mapping = {
        {1, Command::RELAX},
        {2, Command::GRASP},
        {3, Command::OPEN},
        {4, Command::CLOSE}
    };

    // UDP
    UDPConfig udp;

    // Queue capacities
    size_t ring_buffer_capacity     = 4096;
    size_t feature_queue_capacity   = 64;
    size_t decision_queue_capacity  = 32;

    // Processing window
    size_t window_size    = 256;
    size_t window_overlap = 128;

    // Safety
    uint32_t watchdog_timeout_ms   = 500;
    uint32_t max_stale_command_ms  = 1000;

    // Logging
    std::string log_level  = "INFO";
    std::string log_file   = "";
    bool        log_console = true;
};

class Config {
public:
    /// Load configuration from a JSON file.
    /// @param filepath Path to config.json.
    /// @return true on success.
    static bool load(const std::string& filepath, SystemConfig& config);

    /// Save current configuration to a JSON file.
    static bool save(const std::string& filepath, const SystemConfig& config);

    /// Get default configuration.
    static SystemConfig defaults();
};

} // namespace emg
