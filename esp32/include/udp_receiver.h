#pragma once
/// @file udp_receiver.h
/// @brief UDP packet receiver for ESP32 with validation and stats.

#include "command_packet.h"
#include <cstdint>

#ifdef ARDUINO
#include <WiFiUdp.h>
#endif

namespace emg_esp32 {

struct UDPReceiverConfig {
    uint16_t port = 5005;
    bool enforce_sequence = true;
};

class UDPReceiver {
public:
    explicit UDPReceiver(const UDPReceiverConfig& config = UDPReceiverConfig())
        : config_(config)
        , last_sequence_(0)
        , has_received_first_(false)
        , packets_received_(0)
        , packets_invalid_(0)
        , packets_out_of_order_(0) {}

    bool initialize() {
#ifdef ARDUINO
        return udp_.begin(config_.port) != 0;
#else
        return true;
#endif
    }

    /// Non-blocking check for a valid CommandPacket.
    /// Returns true if a valid, in-order packet was received and populated in out_packet.
    bool receive(CommandPacket& out_packet) {
#ifdef ARDUINO
        int packet_size = udp_.parsePacket();
        if (packet_size <= 0) {
            return false;
        }

        if (packet_size < static_cast<int>(COMMAND_PACKET_SIZE)) {
            // Incomplete packet
            udp_.flush();
            packets_invalid_++;
            return false;
        }

        uint8_t buffer[COMMAND_PACKET_SIZE];
        int bytes_read = udp_.read(buffer, COMMAND_PACKET_SIZE);
        udp_.flush();

        if (bytes_read != COMMAND_PACKET_SIZE) {
            packets_invalid_++;
            return false;
        }

        if (!deserializeCommandPacket(buffer, out_packet)) {
            packets_invalid_++;
            return false;
        }

        if (config_.enforce_sequence && has_received_first_) {
            if (out_packet.sequence_number <= last_sequence_) {
                // Out-of-order or duplicate packet
                packets_out_of_order_++;
                return false;
            }
        }

        last_sequence_ = out_packet.sequence_number;
        has_received_first_ = true;
        packets_received_++;
        return true;
#else
        (void)out_packet;
        return false;
#endif
    }

    uint32_t getPacketsReceived() const { return packets_received_; }
    uint32_t getPacketsInvalid() const { return packets_invalid_; }
    uint32_t getPacketsOutOfOrder() const { return packets_out_of_order_; }

private:
    UDPReceiverConfig config_;
#ifdef ARDUINO
    WiFiUDP udp_;
#endif
    uint32_t last_sequence_;
    bool has_received_first_;
    uint32_t packets_received_;
    uint32_t packets_invalid_;
    uint32_t packets_out_of_order_;
};

} // namespace emg_esp32
