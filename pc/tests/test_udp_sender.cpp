/// @file test_udp_sender.cpp
/// @brief Unit tests for UDPSender on localhost.

#include "udp_sender.h"
#include <stdexcept>

#include "test_framework.h"

using namespace emg;

TEST(udp_sender_init_and_send) {
    UDPConfig cfg;
    cfg.esp32_ip = "127.0.0.1";
    cfg.esp32_port = 54321;

    UDPSender sender(cfg);
    ASSERT_TRUE(sender.initialize());
    ASSERT_TRUE(sender.isInitialized());
    ASSERT_EQ(sender.sequenceNumber(), 0u);

    // Send command
    bool ok1 = sender.sendCommand(Command::GRASP, 0.95);
    ASSERT_TRUE(ok1);
    ASSERT_EQ(sender.sequenceNumber(), 1u);

    // Send decision
    Decision d;
    d.command = Command::OPEN;
    d.class_id = 3;
    d.confidence = 0.88;
    d.timestamp_ms = 123456;

    bool ok2 = sender.send(d);
    ASSERT_TRUE(ok2);
    ASSERT_EQ(sender.sequenceNumber(), 2u);

    sender.shutdown();
    ASSERT_FALSE(sender.isInitialized());
}
