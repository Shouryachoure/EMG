#include "config.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace emg {

// ============================================================
// Minimal JSON parser (no external dependency)
// ============================================================
// This is a simplified parser that handles the flat/nested structure
// of our config.json. For production, consider nlohmann/json or similar.

namespace {

// Trim whitespace and quotes from a string value
std::string trimValue(const std::string& s) {
    std::string result = s;
    // Remove leading/trailing whitespace
    while (!result.empty() && (result.front() == ' ' || result.front() == '\t'))
        result.erase(result.begin());
    while (!result.empty() && (result.back() == ' ' || result.back() == '\t' ||
                                result.back() == ',' || result.back() == '\n' ||
                                result.back() == '\r'))
        result.pop_back();
    // Remove quotes
    if (result.size() >= 2 && result.front() == '"' && result.back() == '"') {
        result = result.substr(1, result.size() - 2);
    }
    return result;
}

// Simple key-value extractor from JSON-like text
// Returns a flat map of "section.key" → "value"
std::map<std::string, std::string> parseSimpleJSON(const std::string& content) {
    std::map<std::string, std::string> result;
    std::string current_section;
    std::string current_subsection;

    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        // Skip empty lines and braces
        std::string trimmed = line;
        while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
            trimmed.erase(trimmed.begin());

        if (trimmed.empty() || trimmed[0] == '{') continue;

        if (trimmed[0] == '}') {
            if (!current_subsection.empty()) {
                current_subsection.clear();
            } else if (!current_section.empty()) {
                current_section.clear();
            }
            continue;
        }

        // Check for section header: "section": {
        auto colon = trimmed.find(':');
        if (colon != std::string::npos) {
            std::string key = trimValue(trimmed.substr(0, colon));
            std::string val = trimValue(trimmed.substr(colon + 1));

            if (val == "{") {
                // New section
                if (current_section.empty()) {
                    current_section = key;
                } else {
                    current_subsection = key;
                }
            } else if (val == "}," || val == "}") {
                if (!current_subsection.empty()) {
                    current_subsection.clear();
                } else {
                    current_section.clear();
                }
            } else {
                // Key-value pair
                std::string full_key;
                if (!current_subsection.empty()) {
                    full_key = current_section + "." + current_subsection + "." + key;
                } else if (!current_section.empty()) {
                    full_key = current_section + "." + key;
                } else {
                    full_key = key;
                }
                result[full_key] = val;
            }
        }
    }
    return result;
}

double toDouble(const std::string& s, double def) {
    try { return std::stod(s); } catch (...) { return def; }
}

int toInt(const std::string& s, int def) {
    try { return std::stoi(s); } catch (...) { return def; }
}

bool toBool(const std::string& s, bool def) {
    if (s == "true") return true;
    if (s == "false") return false;
    return def;
}

} // anonymous namespace

bool Config::load(const std::string& filepath, SystemConfig& config) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_WARN("Config", "Could not open config file: " + filepath + " — using defaults");
        config = defaults();
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    file.close();

    auto kv = parseSimpleJSON(content);

    // EMG
    if (kv.count("emg.source"))          config.emg_source     = kv["emg.source"];
    if (kv.count("emg.serial_port"))     config.serial_port    = kv["emg.serial_port"];
    if (kv.count("emg.serial_baud_rate"))config.serial_baud    = toInt(kv["emg.serial_baud_rate"], 115200);
    if (kv.count("emg.sample_rate_hz"))  config.sample_rate_hz = toDouble(kv["emg.sample_rate_hz"], 1000);

    // Processing
    if (kv.count("processing.window_size"))    config.window_size = static_cast<size_t>(toInt(kv["processing.window_size"], 256));
    if (kv.count("processing.window_overlap")) config.window_overlap = static_cast<size_t>(toInt(kv["processing.window_overlap"], 128));
    if (kv.count("processing.filter_low_hz"))  config.processing.filter_low_hz  = toDouble(kv["processing.filter_low_hz"], 20.0);
    if (kv.count("processing.filter_high_hz")) config.processing.filter_high_hz = toDouble(kv["processing.filter_high_hz"], 450.0);
    if (kv.count("processing.normalize"))      config.processing.normalize = toBool(kv["processing.normalize"], true);
    config.processing.sample_rate_hz = config.sample_rate_hz;

    // Features
    if (kv.count("features.rms"))                config.features.rms                = toBool(kv["features.rms"], true);
    if (kv.count("features.mav"))                config.features.mav                = toBool(kv["features.mav"], true);
    if (kv.count("features.variance"))           config.features.variance           = toBool(kv["features.variance"], true);
    if (kv.count("features.waveform_length"))    config.features.waveform_length    = toBool(kv["features.waveform_length"], true);
    if (kv.count("features.zero_crossings"))     config.features.zero_crossings     = toBool(kv["features.zero_crossings"], true);
    if (kv.count("features.slope_sign_changes")) config.features.slope_sign_changes = toBool(kv["features.slope_sign_changes"], true);

    // ML
    if (kv.count("ml.model_type"))           config.ml_model_type  = kv["ml.model_type"];
    if (kv.count("ml.model_path"))           config.ml_model_path  = kv["ml.model_path"];
    if (kv.count("ml.confidence_threshold")) config.confidence_threshold = toDouble(kv["ml.confidence_threshold"], 0.6);

    // Decisions mapping
    if (kv.count("decisions.mapping.1")) config.decision_mapping[1] = stringToCommand(kv["decisions.mapping.1"]);
    if (kv.count("decisions.mapping.2")) config.decision_mapping[2] = stringToCommand(kv["decisions.mapping.2"]);
    if (kv.count("decisions.mapping.3")) config.decision_mapping[3] = stringToCommand(kv["decisions.mapping.3"]);
    if (kv.count("decisions.mapping.4")) config.decision_mapping[4] = stringToCommand(kv["decisions.mapping.4"]);

    // UDP
    if (kv.count("udp.esp32_ip"))   config.udp.esp32_ip   = kv["udp.esp32_ip"];
    if (kv.count("udp.esp32_port")) config.udp.esp32_port  = static_cast<uint16_t>(toInt(kv["udp.esp32_port"], 8888));

    // Queues
    if (kv.count("queues.ring_buffer_capacity"))    config.ring_buffer_capacity    = static_cast<size_t>(toInt(kv["queues.ring_buffer_capacity"], 4096));
    if (kv.count("queues.feature_queue_capacity"))  config.feature_queue_capacity  = static_cast<size_t>(toInt(kv["queues.feature_queue_capacity"], 64));
    if (kv.count("queues.decision_queue_capacity")) config.decision_queue_capacity = static_cast<size_t>(toInt(kv["queues.decision_queue_capacity"], 32));

    // Safety
    if (kv.count("safety.watchdog_timeout_ms"))  config.watchdog_timeout_ms  = static_cast<uint32_t>(toInt(kv["safety.watchdog_timeout_ms"], 500));
    if (kv.count("safety.max_stale_command_ms")) config.max_stale_command_ms = static_cast<uint32_t>(toInt(kv["safety.max_stale_command_ms"], 1000));

    // Logging
    if (kv.count("logging.level"))   config.log_level   = kv["logging.level"];
    if (kv.count("logging.file"))    config.log_file    = kv["logging.file"];
    if (kv.count("logging.console")) config.log_console = toBool(kv["logging.console"], true);

    LOG_INFO("Config", "Configuration loaded from: " + filepath);
    return true;
}

bool Config::save(const std::string& filepath, const SystemConfig& config) {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "    \"emg\": {\n";
    file << "        \"sample_rate_hz\": " << config.sample_rate_hz << ",\n";
    file << "        \"channels\": 1,\n";
    file << "        \"source\": \"" << config.emg_source << "\",\n";
    file << "        \"serial_port\": \"" << config.serial_port << "\",\n";
    file << "        \"serial_baud_rate\": " << config.serial_baud << "\n";
    file << "    },\n";
    file << "    \"processing\": {\n";
    file << "        \"window_size\": " << config.window_size << ",\n";
    file << "        \"window_overlap\": " << config.window_overlap << ",\n";
    file << "        \"filter_low_hz\": " << config.processing.filter_low_hz << ",\n";
    file << "        \"filter_high_hz\": " << config.processing.filter_high_hz << ",\n";
    file << "        \"normalize\": " << (config.processing.normalize ? "true" : "false") << "\n";
    file << "    },\n";
    file << "    \"ml\": {\n";
    file << "        \"model_type\": \"" << config.ml_model_type << "\",\n";
    file << "        \"model_path\": \"" << config.ml_model_path << "\",\n";
    file << "        \"confidence_threshold\": " << config.confidence_threshold << "\n";
    file << "    },\n";
    file << "    \"udp\": {\n";
    file << "        \"esp32_ip\": \"" << config.udp.esp32_ip << "\",\n";
    file << "        \"esp32_port\": " << config.udp.esp32_port << "\n";
    file << "    }\n";
    file << "}\n";

    file.close();
    return true;
}

SystemConfig Config::defaults() {
    return SystemConfig{};
}

} // namespace emg
