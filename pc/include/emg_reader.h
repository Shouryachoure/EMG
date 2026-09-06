#pragma once
/// @file emg_reader.h
/// @brief Abstract interface for EMG data acquisition.
///
/// Concrete implementations:
///   - FakeEMGReader: synthetic signals for testing (no hardware)
///   - SerialEMGReader: reads from a serial-connected EMG sensor (stub)
///
/// The whiteboard shows "Serial Comm" / "PySerial" as the original interface.
/// This abstract interface decouples the pipeline from any specific sensor.

#include <vector>
#include <string>
#include <cstddef>

namespace emg {

/// Abstract EMG data source.
class EMGReader {
public:
    virtual ~EMGReader() = default;

    /// Initialize the reader (open port, configure sensor, etc.).
    /// @return true on success, false on failure.
    virtual bool initialize() = 0;

    /// Read a batch of raw EMG samples.
    /// @return Vector of raw samples (doubles). Empty if no data available.
    virtual std::vector<double> readSamples() = 0;

    /// Check if the reader is connected and producing data.
    virtual bool isConnected() const = 0;

    /// Return a human-readable name for this reader (for logging).
    virtual std::string name() const = 0;

    /// Get the sample rate in Hz.
    virtual double sampleRateHz() const = 0;

    /// Shut down the reader (close port, release resources).
    virtual void shutdown() = 0;
};

} // namespace emg
