#include "pipeline.h"
#include "fake_emg_reader.h"
#include "serial_emg_reader.h"
#include "mock_ml_model.h"
#include "logger.h"

#include <chrono>
#include <sstream>

namespace emg {

Pipeline::Pipeline(const SystemConfig& config)
    : config_(config)
    , signal_processor_(config.processing)
    , feature_extractor_(config.features)
    , decision_engine_(config.confidence_threshold)
    , udp_sender_(config.udp)
    , feature_queue_(config.feature_queue_capacity)
    , decision_queue_(config.decision_queue_capacity)
    , running_(false) {

    decision_engine_.setMapping(config.decision_mapping);
}

Pipeline::~Pipeline() {
    stop();
}

bool Pipeline::initialize() {
    LOG_INFO("Pipeline", "Initializing pipeline...");

    // Create EMG reader based on config
    if (config_.emg_source == "serial") {
        emg_reader_ = std::make_unique<SerialEMGReader>(
            config_.serial_port, config_.serial_baud, config_.sample_rate_hz);
    } else {
        emg_reader_ = std::make_unique<FakeEMGReader>(
            config_.sample_rate_hz, config_.batch_size, 2000);
    }

    if (!emg_reader_->initialize()) {
        LOG_ERROR("Pipeline", "Failed to initialize EMG reader: " + emg_reader_->name());
        return false;
    }
    LOG_INFO("Pipeline", "EMG reader initialized: " + emg_reader_->name());

    // Create ML model based on config
    if (config_.ml_model_type == "mock") {
        ml_model_ = std::make_unique<MockMLModel>();
    } else {
        // Future: TFLite, ONNX
        LOG_WARN("Pipeline", "Unknown ML model type '" + config_.ml_model_type + "', using mock");
        ml_model_ = std::make_unique<MockMLModel>();
    }

    if (!ml_model_->loadModel(config_.ml_model_path)) {
        LOG_ERROR("Pipeline", "Failed to load ML model");
        return false;
    }
    LOG_INFO("Pipeline", "ML model loaded: " + ml_model_->name());

    // Initialize UDP sender
    if (!udp_sender_.initialize()) {
        LOG_WARN("Pipeline", "UDP sender failed to initialize — running in offline mode");
        // Don't fail — allow pipeline to run without UDP for testing
    }

    LOG_INFO("Pipeline", "Pipeline initialized successfully");
    return true;
}

void Pipeline::start() {
    if (running_.load()) return;

    running_.store(true);
    LOG_INFO("Pipeline", "Starting 4 pipeline threads...");

    thread_acquisition_   = std::thread(&Pipeline::acquisitionThread, this);
    thread_processing_    = std::thread(&Pipeline::processingThread, this);
    thread_inference_     = std::thread(&Pipeline::inferenceThread, this);
    thread_communication_ = std::thread(&Pipeline::communicationThread, this);

    LOG_INFO("Pipeline", "All threads started");
}

void Pipeline::stop() {
    if (!running_.load()) return;

    LOG_INFO("Pipeline", "Stopping pipeline...");
    running_.store(false);

    // Shut down queues to unblock waiting threads
    raw_buffer_.shutdown();
    feature_queue_.shutdown();
    decision_queue_.shutdown();

    // Join all threads
    if (thread_acquisition_.joinable())   thread_acquisition_.join();
    if (thread_processing_.joinable())    thread_processing_.join();
    if (thread_inference_.joinable())     thread_inference_.join();
    if (thread_communication_.joinable()) thread_communication_.join();

    // Shutdown components
    if (emg_reader_) emg_reader_->shutdown();
    udp_sender_.shutdown();

    LOG_INFO("Pipeline", "Pipeline stopped cleanly");
}

bool Pipeline::isRunning() const {
    return running_.load();
}

// ============================================================
// Thread 1: EMG Acquisition
// ============================================================
void Pipeline::acquisitionThread() {
    LOG_INFO("Thread1-Acquisition", "Started");

    while (running_.load()) {
        if (!emg_reader_ || !emg_reader_->isConnected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        auto samples = emg_reader_->readSamples();
        for (const auto& sample : samples) {
            raw_buffer_.push(sample);
        }
    }

    LOG_INFO("Thread1-Acquisition", "Stopped");
}

// ============================================================
// Thread 2: Signal Processing + Feature Extraction
// ============================================================
void Pipeline::processingThread() {
    LOG_INFO("Thread2-Processing", "Started");

    const size_t window_size = config_.window_size;
    const size_t step_size   = window_size - config_.window_overlap;

    std::vector<double> window_buffer;
    window_buffer.reserve(window_size);

    while (running_.load()) {
        // Try to collect enough samples for a window
        auto batch = raw_buffer_.tryPopBatch(step_size);

        if (batch.empty()) {
            if (raw_buffer_.isShutdown()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        // Append new samples to the window buffer
        window_buffer.insert(window_buffer.end(), batch.begin(), batch.end());

        // Process complete windows
        while (window_buffer.size() >= window_size) {
            // Extract the current window
            std::vector<double> window(window_buffer.begin(),
                                       window_buffer.begin() + static_cast<long>(window_size));

            // Signal processing
            auto processed = signal_processor_.process(window);

            // Feature extraction
            auto features = feature_extractor_.extract(processed);

            // Push to feature queue
            feature_queue_.push(std::move(features));

            // Slide the window
            window_buffer.erase(window_buffer.begin(),
                                window_buffer.begin() + static_cast<long>(step_size));
        }
    }

    LOG_INFO("Thread2-Processing", "Stopped");
}

// ============================================================
// Thread 3: ML Inference + Decision
// ============================================================
void Pipeline::inferenceThread() {
    LOG_INFO("Thread3-Inference", "Started");

    while (running_.load()) {
        auto features_opt = feature_queue_.pop();  // Blocking

        if (!features_opt.has_value()) {
            break;  // Queue shut down
        }

        const auto& features = features_opt.value();

        if (!ml_model_ || !ml_model_->isReady()) {
            continue;
        }

        // ML inference
        auto prediction = ml_model_->predict(features);

        // Decision engine (confidence gating + mapping)
        auto decision = decision_engine_.decide(prediction);

        // Push to decision queue
        decision_queue_.push(decision);

        LOG_DEBUG("Thread3-Inference",
                  "class=" + std::to_string(decision.class_id) +
                  " cmd=" + commandToString(decision.command) +
                  " conf=" + std::to_string(decision.confidence));
    }

    LOG_INFO("Thread3-Inference", "Stopped");
}

// ============================================================
// Thread 4: UDP Communication
// ============================================================
void Pipeline::communicationThread() {
    LOG_INFO("Thread4-Communication", "Started");

    while (running_.load()) {
        auto decision_opt = decision_queue_.pop();  // Blocking

        if (!decision_opt.has_value()) {
            break;  // Queue shut down
        }

        const auto& decision = decision_opt.value();

        if (udp_sender_.isInitialized()) {
            bool sent = udp_sender_.send(decision);
            if (!sent) {
                LOG_WARN("Thread4-Communication", "Failed to send UDP packet");
            }
        }
    }

    // Send a final NONE command on shutdown for safety
    if (udp_sender_.isInitialized()) {
        Decision safe;
        safe.command = Command::NONE;
        safe.confidence = 1.0;
        auto now = std::chrono::steady_clock::now();
        safe.timestamp_ms = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count() & 0xFFFFFFFF);
        udp_sender_.send(safe);
        LOG_INFO("Thread4-Communication", "Sent final NONE (safe state) command");
    }

    LOG_INFO("Thread4-Communication", "Stopped");
}

} // namespace emg
