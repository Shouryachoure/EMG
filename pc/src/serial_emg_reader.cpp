#include "serial_emg_reader.h"
#include "logger.h"
#include <sstream>
#include <iostream>
#include <algorithm>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#endif

namespace emg {

SerialEMGReader::SerialEMGReader(const std::string& port,
                                 int baud_rate,
                                 double sample_rate_hz,
                                 size_t batch_size)
    : port_(port)
    , baud_rate_(baud_rate)
    , sample_rate_hz_(sample_rate_hz)
    , batch_size_(batch_size)
    , connected_(false)
#ifdef _WIN32
    , handle_(INVALID_HANDLE_VALUE)
#else
    , fd_(-1)
#endif
{
}

SerialEMGReader::~SerialEMGReader() {
    shutdown();
}

bool SerialEMGReader::initialize() {
    shutdown();

#ifdef _WIN32
    std::string device_path = port_;
    if (device_path.find("\\\\.\\") == std::string::npos) {
        device_path = "\\\\.\\" + port_;
    }

    handle_ = CreateFileA(device_path.c_str(),
                          GENERIC_READ,
                          0,
                          NULL,
                          OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL,
                          NULL);

    if (handle_ == INVALID_HANDLE_VALUE) {
        LOG_ERROR("SerialEMGReader", "Could not open serial port: " + port_);
        connected_ = false;
        return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(handle_, &dcb)) {
        LOG_ERROR("SerialEMGReader", "Failed to get DCB state for " + port_);
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    dcb.BaudRate = static_cast<DWORD>(baud_rate_);
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity   = NOPARITY;
    dcb.fBinary  = TRUE;

    if (!SetCommState(handle_, &dcb)) {
        LOG_ERROR("SerialEMGReader", "Failed to set baud rate " + std::to_string(baud_rate_) + " on " + port_);
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
        return false;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout         = 5;
    timeouts.ReadTotalTimeoutConstant     = 10;
    timeouts.ReadTotalTimeoutMultiplier   = 1;
    SetCommTimeouts(handle_, &timeouts);

    PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);

#else
    fd_ = open(port_.c_str(), O_RDONLY | O_NOCTTY | O_NDELAY);
    if (fd_ < 0) {
        LOG_ERROR("SerialEMGReader", "Could not open " + port_);
        return false;
    }

    struct termios tty{};
    tcgetattr(fd_, &tty);
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    tty.c_cflag |= (CLOCAL | CREAD | CS8);
    tcsetattr(fd_, TCSANOW, &tty);
#endif

    connected_ = true;
    rx_line_buffer_.clear();
    LOG_INFO("SerialEMGReader", "Successfully connected to hardware on " + port_ + " @ " + std::to_string(baud_rate_) + " baud");
    return true;
}

std::vector<double> SerialEMGReader::readSamples() {
    std::vector<double> samples;
    samples.reserve(batch_size_);

    if (!connected_) {
        return samples;
    }

    char buffer[256];
    size_t target_count = batch_size_;

    while (samples.size() < target_count) {
        DWORD bytes_read = 0;
#ifdef _WIN32
        if (!ReadFile(handle_, buffer, sizeof(buffer) - 1, &bytes_read, NULL) || bytes_read == 0) {
            break; // No more data currently available
        }
#else
        int res = read(fd_, buffer, sizeof(buffer) - 1);
        if (res <= 0) break;
        bytes_read = static_cast<DWORD>(res);
#endif

        buffer[bytes_read] = '\0';
        rx_line_buffer_.append(buffer, bytes_read);

        // Parse lines separated by '\n' or '\r'
        size_t newline_pos = 0;
        while ((newline_pos = rx_line_buffer_.find('\n')) != std::string::npos) {
            std::string line = rx_line_buffer_.substr(0, newline_pos);
            rx_line_buffer_.erase(0, newline_pos + 1);

            // Strip trailing '\r' or spaces
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
                line.pop_back();
            }

            if (!line.empty()) {
                double sample_val = 0.0;
                if (parseSample(line, sample_val)) {
                    samples.push_back(sample_val);
                    if (samples.size() >= target_count) {
                        break;
                    }
                }
            }
        }
    }

    return samples;
}

bool SerialEMGReader::parseSample(const std::string& str, double& out_val) {
    try {
        double raw = std::stod(str);

        // Normalize if it appears to be raw ADC integer (e.g. 10-bit 0..1023 from Arduino)
        if (raw >= 0.0 && raw <= 1024.0) {
            out_val = (raw - 512.0) / 512.0; // Center at 0.0, scale to [-1.0, 1.0]
        } else if (raw >= 0.0 && raw <= 4096.0) {
            // 12-bit ADC (e.g. RP2040 C++/Arduino, ESP32)
            out_val = (raw - 2048.0) / 2048.0;
        } else if (raw >= 0.0 && raw <= 65536.0) {
            // 16-bit ADC integer (Raspberry Pi Pico MicroPython read_u16())
            out_val = (raw - 32768.0) / 32768.0;
        } else {
            out_val = raw; // Assume already normalized / scaled float
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool SerialEMGReader::isConnected() const {
    return connected_;
}

std::string SerialEMGReader::name() const {
    return "SerialEMGReader(" + port_ + ")";
}

double SerialEMGReader::sampleRateHz() const {
    return sample_rate_hz_;
}

void SerialEMGReader::shutdown() {
    if (connected_) {
#ifdef _WIN32
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
#else
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
#endif
        connected_ = false;
        LOG_INFO("SerialEMGReader", "Closed port: " + port_);
    }
}

std::vector<std::string> SerialEMGReader::listAvailablePorts() {
    std::vector<std::string> ports;
#ifdef _WIN32
    char target_path[256];
    for (int i = 1; i <= 32; ++i) {
        std::string port_name = "COM" + std::to_string(i);
        DWORD result = QueryDosDeviceA(port_name.c_str(), target_path, sizeof(target_path));
        if (result != 0) {
            ports.push_back(port_name);
        }
    }
#endif
    return ports;
}

} // namespace emg
