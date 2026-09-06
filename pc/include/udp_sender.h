#pragma once
/// @file udp_sender.h
/// @brief UDP packet sender for transmitting commands to ESP32.
///
/// Uses Winsock2 on Windows, POSIX sockets on Linux.
/// Sends structured CommandPacket (16 bytes) with automatic sequence numbering.

#include "command_packet.h"
#include "decision_engine.h"
#include <string>
#include <cstdint>
#include <atomic>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "ws2_32.lib")
    #endif
    using socket_t = SOCKET;
    constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    using socket_t = int;
    constexpr socket_t INVALID_SOCK = -1;
#endif

namespace emg {

/// Configuration for UDP sender.
struct UDPConfig {
    std::string esp32_ip         = "192.168.1.100";
    uint16_t    esp32_port       = 8888;
    uint8_t     protocol_version = 1; ///< 1 = 16-byte v1, 2 = 24-byte v2
};

class UDPSender {
public:
    explicit UDPSender(const UDPConfig& config = {});
    ~UDPSender();

    // Non-copyable
    UDPSender(const UDPSender&) = delete;
    UDPSender& operator=(const UDPSender&) = delete;

    /// Initialize the UDP socket.
    /// @return true on success.
    bool initialize();

    /// Send a decision as a UDP command packet (uses config.protocol_version).
    /// @param decision The decision to send.
    /// @return true if the packet was sent successfully.
    bool send(const Decision& decision);

    /// Explicitly send as Protocol v2 (24 bytes with intensity and 5-finger angles).
    bool sendV2(const Decision& decision);

    /// Send raw CommandPacket (Protocol v1).
    bool sendPacket(const CommandPacket& packet);

    /// Send raw CommandPacketV2 (Protocol v2).
    bool sendPacketV2(const CommandPacketV2& packet);

    /// Send a raw command (convenience method).
    bool sendCommand(Command cmd, double confidence = 1.0);

    /// Get the current sequence number.
    uint32_t sequenceNumber() const;

    /// Shutdown the socket.
    void shutdown();

    /// Check if the sender is initialized.
    bool isInitialized() const;

private:
    UDPConfig              config_;
    socket_t               socket_;
    struct sockaddr_in     dest_addr_;
    std::atomic<uint32_t>  sequence_number_;
    bool                   initialized_;

    /// Platform-specific socket initialization.
    static bool platformInit();
    static void platformCleanup();
};

} // namespace emg
