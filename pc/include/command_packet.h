#pragma once
/// @file command_packet.h
/// @brief UDP command packet structure with serialization and CRC-16 validation.
///
/// Packet format (16 bytes, little-endian):
///   [0]     version          uint8_t
///   [1]     command          uint8_t
///   [2-5]   sequence_number  uint32_t
///   [6-9]   timestamp_ms     uint32_t
///   [10-13] confidence       float (IEEE 754)
///   [14-15] checksum         uint16_t (CRC-16/CCITT-FALSE)

#include "command.h"
#include <cstdint>
#include <cstring>
#include <array>

namespace emg {

/// Current protocol version
static constexpr uint8_t PROTOCOL_VERSION = 1;

/// Fixed packet size
static constexpr size_t PACKET_SIZE = 16;

/// Command packet transmitted over UDP.
struct CommandPacket {
    uint8_t  version         = PROTOCOL_VERSION;
    Command  command         = Command::NONE;
    uint32_t sequence_number = 0;
    uint32_t timestamp_ms    = 0;
    float    confidence      = 0.0f;
    uint16_t checksum        = 0;

    /// Serialize to a 16-byte buffer.
    /// @param buffer Output buffer (must be at least PACKET_SIZE bytes).
    void serialize(uint8_t* buffer) const;

    /// Deserialize from a buffer and validate CRC.
    /// @param buffer Input buffer.
    /// @param len    Buffer length.
    /// @return true if deserialization and CRC validation succeeded.
    bool deserialize(const uint8_t* buffer, size_t len);

    /// Serialize to a std::array.
    std::array<uint8_t, PACKET_SIZE> toBytes() const;

    /// Deserialize from a std::array.
    bool fromBytes(const std::array<uint8_t, PACKET_SIZE>& data);
};

/// CRC-16/CCITT-FALSE calculation.
/// Polynomial: 0x1021, Init: 0xFFFF, No reflection, No final XOR.
uint16_t crc16(const uint8_t* data, size_t len);

} // namespace emg
