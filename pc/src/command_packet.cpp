#include "command_packet.h"

namespace emg {

uint16_t crc16(const uint8_t* data, size_t len) {
    // CRC-16/CCITT-FALSE
    // Polynomial: 0x1021
    // Initial: 0xFFFF
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

void CommandPacket::serialize(uint8_t* buffer) const {
    buffer[0] = version;
    buffer[1] = static_cast<uint8_t>(command);

    // Little-endian encoding
    std::memcpy(&buffer[2],  &sequence_number, sizeof(sequence_number));
    std::memcpy(&buffer[6],  &timestamp_ms,    sizeof(timestamp_ms));
    std::memcpy(&buffer[10], &confidence,       sizeof(confidence));

    // Compute CRC over bytes 0-13
    uint16_t crc = crc16(buffer, 14);
    std::memcpy(&buffer[14], &crc, sizeof(crc));
}

bool CommandPacket::deserialize(const uint8_t* buffer, size_t len) {
    if (len != PACKET_SIZE) return false;

    // Validate CRC first
    uint16_t received_crc;
    std::memcpy(&received_crc, &buffer[14], sizeof(received_crc));

    uint16_t computed_crc = crc16(buffer, 14);
    if (computed_crc != received_crc) return false;

    // Parse fields
    version = buffer[0];
    command = static_cast<Command>(buffer[1]);
    std::memcpy(&sequence_number, &buffer[2],  sizeof(sequence_number));
    std::memcpy(&timestamp_ms,    &buffer[6],  sizeof(timestamp_ms));
    std::memcpy(&confidence,      &buffer[10], sizeof(confidence));
    checksum = received_crc;

    return true;
}

std::array<uint8_t, PACKET_SIZE> CommandPacket::toBytes() const {
    std::array<uint8_t, PACKET_SIZE> data{};
    serialize(data.data());
    return data;
}

bool CommandPacket::fromBytes(const std::array<uint8_t, PACKET_SIZE>& data) {
    return deserialize(data.data(), data.size());
}

// ============================================================
// Protocol v2 Implementation (24 bytes)
// ============================================================

void CommandPacketV2::serialize(uint8_t* buffer) const {
    buffer[0] = version;
    buffer[1] = static_cast<uint8_t>(command);

    // Little-endian encoding
    std::memcpy(&buffer[2],  &sequence_number, sizeof(sequence_number));
    std::memcpy(&buffer[6],  &timestamp_ms,    sizeof(timestamp_ms));
    std::memcpy(&buffer[10], &confidence,      sizeof(confidence));

    buffer[14] = intensity;
    buffer[15] = finger_mask;
    std::memcpy(&buffer[16], finger_angles.data(), 5);
    buffer[21] = reserved;

    // Compute CRC over bytes 0-21 (22 bytes)
    uint16_t crc = crc16(buffer, 22);
    std::memcpy(&buffer[22], &crc, sizeof(crc));
}

bool CommandPacketV2::deserialize(const uint8_t* buffer, size_t len) {
    if (len != PACKET_SIZE_V2) return false;

    // Validate CRC first
    uint16_t received_crc;
    std::memcpy(&received_crc, &buffer[22], sizeof(received_crc));

    uint16_t computed_crc = crc16(buffer, 22);
    if (computed_crc != received_crc) return false;

    // Parse fields
    version = buffer[0];
    command = static_cast<Command>(buffer[1]);
    std::memcpy(&sequence_number, &buffer[2],  sizeof(sequence_number));
    std::memcpy(&timestamp_ms,    &buffer[6],  sizeof(timestamp_ms));
    std::memcpy(&confidence,      &buffer[10], sizeof(confidence));

    intensity   = buffer[14];
    finger_mask = buffer[15];
    std::memcpy(finger_angles.data(), &buffer[16], 5);
    reserved    = buffer[21];
    checksum    = received_crc;

    return true;
}

std::array<uint8_t, PACKET_SIZE_V2> CommandPacketV2::toBytes() const {
    std::array<uint8_t, PACKET_SIZE_V2> data{};
    serialize(data.data());
    return data;
}

bool CommandPacketV2::fromBytes(const std::array<uint8_t, PACKET_SIZE_V2>& data) {
    return deserialize(data.data(), data.size());
}

} // namespace emg
