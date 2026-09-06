#pragma once
/// @file serial_emg_reader.h
/// @brief Hardware serial reader for EMG sensor streaming over USB/COM.

#include "emg_reader.h"
#include <string>
#include <vector>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

namespace emg {

class SerialEMGReader : public EMGReader {
public:
    explicit SerialEMGReader(const std::string& port = "COM3",
                             int baud_rate = 115200,
                             double sample_rate_hz = 1000.0,
                             size_t batch_size = 32);
    ~SerialEMGReader() override;

    bool initialize() override;
    std::vector<double> readSamples() override;
    bool isConnected() const override;
    std::string name() const override;
    double sampleRateHz() const override;
    void shutdown() override;

    /// List active COM ports detected on the system.
    static std::vector<std::string> listAvailablePorts();

private:
    std::string port_;
    int         baud_rate_;
    double      sample_rate_hz_;
    size_t      batch_size_;
    bool        connected_;

#ifdef _WIN32
    HANDLE      handle_;
#else
    int         fd_;
#endif
    std::string rx_line_buffer_;

    bool parseSample(const std::string& str, double& out_val);
};

} // namespace emg
