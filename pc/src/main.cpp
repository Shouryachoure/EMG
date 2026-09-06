/// @file main.cpp
/// @brief Entry point for the EMG control system PC application.

#include "pipeline.h"
#include "config.h"
#include "logger.h"

#include <iostream>
#include <csignal>
#include <atomic>

static std::atomic<bool> g_shutdown_requested{false};

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n[SIGNAL] Shutdown requested (signal " << signal << ")\n";
        g_shutdown_requested.store(true);
    }
}

int main(int argc, char* argv[]) {
    // Register signal handlers for clean shutdown
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Determine config file path
    std::string config_path = "config.json";
    if (argc > 1) {
        config_path = argv[1];
    }

    // Load configuration
    emg::SystemConfig config;
    emg::Config::load(config_path, config);

    // Initialize logger
    emg::LogLevel log_level = emg::LogLevel::INFO;
    if (config.log_level == "DEBUG") log_level = emg::LogLevel::DEBUG;
    else if (config.log_level == "WARN") log_level = emg::LogLevel::WARN;
    else if (config.log_level == "ERROR") log_level = emg::LogLevel::ERR;

    emg::Logger::instance().init(log_level, config.log_file, config.log_console);

    LOG_INFO("Main", "========================================");
    LOG_INFO("Main", "  EMG Control System v1.0.0");
    LOG_INFO("Main", "========================================");
    LOG_INFO("Main", "Config: " + config_path);
    LOG_INFO("Main", "EMG source: " + config.emg_source);
    LOG_INFO("Main", "ML model: " + config.ml_model_type);
    LOG_INFO("Main", "UDP target: " + config.udp.esp32_ip + ":" + std::to_string(config.udp.esp32_port));

    // Create and initialize pipeline
    emg::Pipeline pipeline(config);

    if (!pipeline.initialize()) {
        LOG_FATAL("Main", "Pipeline initialization failed — aborting");
        return 1;
    }

    // Start pipeline
    pipeline.start();
    LOG_INFO("Main", "Pipeline running. Press Ctrl+C to stop.");

    // Wait for shutdown signal
    while (!g_shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Clean shutdown
    LOG_INFO("Main", "Shutting down...");
    pipeline.stop();
    LOG_INFO("Main", "Shutdown complete");

    return 0;
}
