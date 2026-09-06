#pragma once
/// @file command_packet.h (ESP32 version)
/// @brief UDP command packet — shared protocol with PC-side.

#include "command.h"
#include <cstdint>
#include <cstring>

static constexpr uint8_t PROTOCOL_VERSION = 1;
static constexpr size_t  PACKET_SIZE = 16;

struct CommandPacket {
    uint8_t  version         = PROTOCOL_VERSION;
    Command  command         = Command::NONE;
    uint32_t sequence_number = 0;
    uint32_t timestamp_ms    = 0;
    float    confidence      = 0.0f;
    uint16_t checksum        = 0;

    /// Deserialize from a buffer and validate CRC.
    bool deserialize(const uint8_t* buffer, size_t len) {
        if (len != PACKET_SIZE) return false;

        // Validate CRC first
        uint16_t received_crc;
        memcpy(&received_crc, &buffer[14], sizeof(received_crc));

        uint16_t computed_crc = crc16(buffer, 14);
        if (computed_crc != received_crc) return false;

        // Parse fields
        version = buffer[0];
        command = static_cast<Command>(buffer[1]);
        memcpy(&sequence_number, &buffer[2],  sizeof(sequence_number));
        memcpy(&timestamp_ms,    &buffer[6],  sizeof(timestamp_ms));
        memcpy(&confidence,      &buffer[10], sizeof(confidence));
        checksum = received_crc;

        return true;
    }

    /// CRC-16/CCITT-FALSE (must match PC-side implementation)
    static uint16_t crc16(const uint8_t* data, size_t len) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; ++i) {
            crc ^= static_cast<uint16_t>(data[i]) << 8;
            for (int j = 0; j < 8; ++j) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ 0x1021;
                } else {
                    crc <<= 1;
                }
            }
        }
        return crc;
    }
};
