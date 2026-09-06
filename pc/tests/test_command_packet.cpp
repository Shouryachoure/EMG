/// @file test_command_packet.cpp
/// @brief Unit tests for CommandPacket serialization and CRC-16.

#include "command_packet.h"
#include <stdexcept>
#include <cmath>

#include "test_framework.h"

using namespace emg;

TEST(command_packet_roundtrip) {
    CommandPacket src;
    src.version = 1;
    src.command = Command::OPEN;
    src.sequence_number = 1042;
    src.timestamp_ms = 987654;
    src.confidence = 0.88f;

    auto bytes = src.toBytes();
    ASSERT_EQ(bytes.size(), PACKET_SIZE);

    CommandPacket dest;
    bool success = dest.fromBytes(bytes);
    ASSERT_TRUE(success);

    ASSERT_EQ(dest.version, 1);
    ASSERT_EQ(dest.command, Command::OPEN);
    ASSERT_EQ(dest.sequence_number, 1042u);
    ASSERT_EQ(dest.timestamp_ms, 987654u);
    ASSERT_NEAR(dest.confidence, 0.88f, 1e-4f);
}

TEST(command_packet_crc_tamper_fails) {
    CommandPacket src;
    src.command = Command::GRASP;
    src.sequence_number = 5;
    src.confidence = 0.95f;

    auto bytes = src.toBytes();

    // Tamper with command byte
    bytes[1] = static_cast<uint8_t>(Command::CLOSE);

    CommandPacket dest;
    bool success = dest.fromBytes(bytes);
    ASSERT_FALSE(success); // CRC mismatch should fail
}

TEST(command_packet_short_buffer_fails) {
    uint8_t short_buf[10] = {0};
    CommandPacket dest;
    ASSERT_FALSE(dest.deserialize(short_buf, 10));
}
