#pragma once
/// @file pipeline.h
/// @brief Top-level pipeline orchestrator — creates 4 threads, wires queues, manages lifecycle.

#include "config.h"
#include "emg_reader.h"
#include "ring_buffer.h"
#include "thread_safe_queue.h"
#include "signal_processor.h"
#include "feature_extractor.h"
#include "ml_model.h"
#include "decision_engine.h"
#include "udp_sender.h"

#include <thread>
#include <atomic>
#include <memory>

namespace emg {

class Pipeline {
public:
    explicit Pipeline(const SystemConfig& config);
    ~Pipeline();

    // Non-copyable, non-movable
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    /// Initialize all components.
    /// @return true if all components initialized successfully.
    bool initialize();

    /// Start the 4 processing threads.
    void start();

    /// Stop all threads and shut down cleanly.
    void stop();

    /// Check if the pipeline is running.
    bool isRunning() const;

private:
    // Thread entry points
    void acquisitionThread();    // Thread 1
    void processingThread();     // Thread 2
    void inferenceThread();      // Thread 3
    void communicationThread();  // Thread 4

    // Configuration
    SystemConfig config_;

    // Components
    std::unique_ptr<EMGReader>       emg_reader_;
    SignalProcessor                  signal_processor_;
    FeatureExtractor                 feature_extractor_;
    std::unique_ptr<MLModel>         ml_model_;
    DecisionEngine                   decision_engine_;
    UDPSender                        udp_sender_;

    // Inter-thread communication
    // RingBuffer uses compile-time capacity. We use 4096 as the standard.
    RingBuffer<double, 4096>                     raw_buffer_;
    ThreadSafeQueue<std::vector<double>>         feature_queue_;
    ThreadSafeQueue<Decision>                    decision_queue_;

    // Threading
    std::thread      thread_acquisition_;
    std::thread      thread_processing_;
    std::thread      thread_inference_;
    std::thread      thread_communication_;
    std::atomic<bool> running_;
};

} // namespace emg
