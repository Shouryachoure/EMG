#include "udp_sender.h"
#include "logger.h"
#include <cstring>
#include <sstream>

namespace emg {

UDPSender::UDPSender(const UDPConfig& config)
    : config_(config)
    , socket_(INVALID_SOCK)
    , dest_addr_{}
    , sequence_number_(0)
    , initialized_(false) {}

UDPSender::~UDPSender() {
    shutdown();
}

bool UDPSender::platformInit() {
#ifdef _WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        LOG_ERROR("UDPSender", "WSAStartup failed with error: " + std::to_string(result));
        return false;
    }
#endif
    return true;
}

void UDPSender::platformCleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

bool UDPSender::initialize() {
    if (initialized_) return true;

    if (!platformInit()) return false;

    // Create UDP socket
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCK) {
        LOG_ERROR("UDPSender", "Failed to create UDP socket");
        platformCleanup();
        return false;
    }

    // Configure destination address
    std::memset(&dest_addr_, 0, sizeof(dest_addr_));
    dest_addr_.sin_family = AF_INET;
    dest_addr_.sin_port   = htons(config_.esp32_port);

    if (inet_pton(AF_INET, config_.esp32_ip.c_str(), &dest_addr_.sin_addr) <= 0) {
        LOG_ERROR("UDPSender", "Invalid IP address: " + config_.esp32_ip);
        shutdown();
        return false;
    }

    initialized_ = true;
    sequence_number_.store(0);

    std::ostringstream oss;
    oss << "UDP sender initialized → " << config_.esp32_ip << ":" << config_.esp32_port;
    LOG_INFO("UDPSender", oss.str());

    return true;
}

bool UDPSender::send(const Decision& decision) {
    if (config_.protocol_version == 2) {
        return sendV2(decision);
    }

    if (!initialized_) return false;

    CommandPacket packet;
    packet.version         = PROTOCOL_VERSION;
    packet.command         = decision.command;
    packet.sequence_number = sequence_number_.fetch_add(1) + 1;
    packet.timestamp_ms    = decision.timestamp_ms;
    packet.confidence      = static_cast<float>(decision.confidence);

    return sendPacket(packet);
}

bool UDPSender::sendV2(const Decision& decision) {
    if (!initialized_) return false;

    CommandPacketV2 packet;
    packet.version         = PROTOCOL_VERSION_V2;
    packet.command         = decision.command;
    packet.sequence_number = sequence_number_.fetch_add(1) + 1;
    packet.timestamp_ms    = decision.timestamp_ms;
    packet.confidence      = static_cast<float>(decision.confidence);
    packet.intensity       = decision.intensity;
    packet.finger_angles   = decision.finger_angles;

    return sendPacketV2(packet);
}

bool UDPSender::sendPacket(const CommandPacket& packet) {
    if (!initialized_) return false;

    auto bytes = packet.toBytes();
    int sent = sendto(socket_,
                      reinterpret_cast<const char*>(bytes.data()),
                      static_cast<int>(bytes.size()),
                      0,
                      reinterpret_cast<struct sockaddr*>(&dest_addr_),
                      sizeof(dest_addr_));

    if (sent < 0) {
        LOG_WARN("UDPSender", "sendto() failed");
        return false;
    }

    return true;
}

bool UDPSender::sendPacketV2(const CommandPacketV2& packet) {
    if (!initialized_) return false;

    auto bytes = packet.toBytes();
    int sent = sendto(socket_,
                      reinterpret_cast<const char*>(bytes.data()),
                      static_cast<int>(bytes.size()),
                      0,
                      reinterpret_cast<struct sockaddr*>(&dest_addr_),
                      sizeof(dest_addr_));

    if (sent < 0) {
        LOG_WARN("UDPSender", "sendto() v2 failed");
        return false;
    }

    return true;
}

bool UDPSender::sendCommand(Command cmd, double confidence) {
    Decision d;
    d.command = cmd;
    d.confidence = confidence;
    d.class_id = static_cast<int>(cmd);

    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    d.timestamp_ms = static_cast<uint32_t>(ms & 0xFFFFFFFF);

    return send(d);
}

uint32_t UDPSender::sequenceNumber() const {
    return sequence_number_.load();
}

void UDPSender::shutdown() {
    if (socket_ != INVALID_SOCK) {
#ifdef _WIN32
        closesocket(socket_);
#else
        close(socket_);
#endif
        socket_ = INVALID_SOCK;
    }
    if (initialized_) {
        platformCleanup();
        initialized_ = false;
        LOG_INFO("UDPSender", "UDP sender shut down");
    }
}

bool UDPSender::isInitialized() const {
    return initialized_;
}

} // namespace emg
