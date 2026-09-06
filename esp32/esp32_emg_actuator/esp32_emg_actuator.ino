/**
 * =========================================================================
 * EMG Bionic Actuator Controller — Arduino IDE Sketch for ESP32
 * =========================================================================
 * 
 * Advanced Features:
 *   - Proportional Continuous Control & S-Curve Exponential Easing (smooth glide, no snaps)
 *   - 5-Finger Bionic Multi-Servo Support (Thumb, Index, Middle, Ring, Pinky)
 *   - Dual-Mode Protocol Support (v1 16-byte discrete & v2 24-byte proportional)
 *   - Dual-Mode Wireless: Wi-Fi UDP (Port 8888) + Optional BLE UART (Nordic NUS)
 *   - 500ms Hardware Safety Watchdog (auto-failsafe to 90° neutral)
 * 
 * Hardware Wiring:
 *   - Thumb / Master Servo Signal -> ESP32 GPIO 18
 *   - Index Finger Servo Signal   -> ESP32 GPIO 19 (Optional)
 *   - Middle Finger Servo Signal  -> ESP32 GPIO 21 (Optional)
 *   - Ring Finger Servo Signal    -> ESP32 GPIO 22 (Optional)
 *   - Pinky Finger Servo Signal   -> ESP32 GPIO 23 (Optional)
 *   - Servo VCC (Red)             -> External +5V 2A+ Power Supply
 *   - Servo GND (Brown/Black)     -> ESP32 GND & 5V Supply GND (Common Ground)
 * 
 * Required Arduino Libraries:
 *   - ESP32Servo by Kevin Harrington
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

// Set to 1 to enable Bluetooth Low Energy (BLE) alongside Wi-Fi
#define ENABLE_BLE 0

#if ENABLE_BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#define BLE_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_CHAR_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#endif

// ==========================================
// 1. CONFIGURATION (Edit for your setup)
// ==========================================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const uint16_t UDP_PORT   = 8888;
const uint32_t WATCHDOG_TIMEOUT_MS = 500;

// Servo Pins (5-Finger Bionic Hand)
const int PIN_THUMB  = 18;  // Master / Gripper
const int PIN_INDEX  = 19;
const int PIN_MIDDLE = 21;
const int PIN_RING   = 22;
const int PIN_PINKY  = 23;

// Smoothing factor for exponential S-curve interpolation [0.1 = slow glide, 0.4 = fast]
const float SMOOTHING_FACTOR = 0.28f;

// Preset Angles (Degrees)
const int ANGLE_NEUTRAL = 90;   // RELAX (Safe State)
const int ANGLE_GRASP   = 0;    // GRASP (Closed Grip)
const int ANGLE_OPEN    = 180;  // OPEN (Wide Open)
const int ANGLE_CLOSE   = 0;    // CLOSE (Full Fist)

// ==========================================
// 2. PROTOCOL PACKETS (v1: 16-byte, v2: 24-byte)
// ==========================================
enum class Command : uint8_t {
    NONE  = 0,
    RELAX = 1,
    GRASP = 2,
    OPEN  = 3,
    CLOSE = 4
};

// Protocol v1 (16 bytes)
struct __attribute__((packed)) PacketV1 {
    uint8_t  version;          // 1
    uint8_t  command;          // Command enum (0-4)
    uint32_t sequence_number;  // Monotonic
    uint32_t timestamp_ms;     // Sender timestamp
    float    confidence;       // 0.0 - 1.0
    uint16_t checksum;         // CRC-16/CCITT-FALSE
};

// Protocol v2 (24 bytes - Proportional & 5-Finger Articulation)
struct __attribute__((packed)) PacketV2 {
    uint8_t  version;          // 2
    uint8_t  command;          // Command enum (0-4)
    uint32_t sequence_number;  // Monotonic
    uint32_t timestamp_ms;     // Sender timestamp
    float    confidence;       // 0.0 - 1.0
    uint8_t  intensity;        // 0 - 100% muscle contraction force
    uint8_t  finger_mask;      // Active finger bits (0x1F for all 5)
    uint8_t  finger_angles[5]; // Thumb, Index, Middle, Ring, Pinky (0-180°)
    uint8_t  reserved;
    uint16_t checksum;         // CRC-16/CCITT-FALSE
};

uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= (static_cast<uint16_t>(data[i]) << 8);
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

// ==========================================
// 3. HARDWARE STATE & MULTI-SERVO INSTANCES
// ==========================================
WiFiUDP udp;
Servo servo_thumb;
Servo servo_index;
Servo servo_middle;
Servo servo_ring;
Servo servo_pinky;

uint32_t last_packet_time_ms = 0;
uint32_t last_sequence_number = 0;
bool has_received_first = false;
Command current_command = Command::NONE;

// Target and smoothed positions for all 5 fingers
float target_angles[5]  = { 90.0f, 90.0f, 90.0f, 90.0f, 90.0f };
float current_angles[5] = { 90.0f, 90.0f, 90.0f, 90.0f, 90.0f };

uint32_t stat_rx = 0;
uint32_t stat_bad_crc = 0;
uint32_t stat_out_of_order = 0;
uint32_t last_telemetry_ms = 0;

void setAllTargetAngles(int angle) {
    for (int i = 0; i < 5; ++i) {
        target_angles[i] = static_cast<float>(angle);
    }
}

void enterSafeState() {
    if (current_command != Command::NONE) {
        Serial.println("[SAFETY] WATCHDOG TIMEOUT! Failsafe triggered → 90° NEUTRAL");
        current_command = Command::NONE;
        setAllTargetAngles(ANGLE_NEUTRAL);
    }
}

// Process validated command packet
void handleCommand(Command cmd, float conf, uint8_t intensity = 0, const uint8_t* custom_angles = nullptr) {
    current_command = cmd;

    if (custom_angles != nullptr) {
        // Direct individual finger control from Protocol v2
        for (int i = 0; i < 5; ++i) {
            target_angles[i] = custom_angles[i];
        }
    } else {
        // Standard discrete / proportional command mapping
        switch (cmd) {
            case Command::RELAX:
                setAllTargetAngles(ANGLE_NEUTRAL);
                break;

            case Command::GRASP:
                // If proportional intensity is provided, scale grasp between 45° and 0°
                if (intensity > 0) {
                    int dynamic_angle = 45 - static_cast<int>((intensity / 100.0f) * 45.0f);
                    setAllTargetAngles(constrain(dynamic_angle, 0, 90));
                } else {
                    setAllTargetAngles(ANGLE_GRASP);
                }
                break;

            case Command::OPEN:
                setAllTargetAngles(ANGLE_OPEN);
                break;

            case Command::CLOSE:
                // Precision pinch (Thumb + Index close, others neutral)
                target_angles[0] = ANGLE_CLOSE;
                target_angles[1] = ANGLE_CLOSE;
                target_angles[2] = ANGLE_NEUTRAL;
                target_angles[3] = ANGLE_NEUTRAL;
                target_angles[4] = ANGLE_NEUTRAL;
                break;

            case Command::NONE:
            default:
                setAllTargetAngles(ANGLE_NEUTRAL);
                break;
        }
    }
}

// Dispatch raw packet buffer (from either Wi-Fi UDP or BLE)
void processIncomingPacket(const uint8_t* buf, size_t len) {
    if (len == sizeof(PacketV1)) {
        // Protocol v1 (16 bytes)
        const PacketV1* pkt = reinterpret_cast<const PacketV1*>(buf);
        uint16_t expected_crc = crc16(buf, sizeof(PacketV1) - 2);
        if (pkt->checksum != expected_crc) {
            stat_bad_crc++;
            return;
        }

        if (has_received_first && pkt->sequence_number <= last_sequence_number) {
            stat_out_of_order++;
            return;
        }

        last_sequence_number = pkt->sequence_number;
        has_received_first = true;
        last_packet_time_ms = millis();
        stat_rx++;

        handleCommand(static_cast<Command>(pkt->command), pkt->confidence);

    } else if (len == sizeof(PacketV2)) {
        // Protocol v2 (24 bytes)
        const PacketV2* pkt = reinterpret_cast<const PacketV2*>(buf);
        uint16_t expected_crc = crc16(buf, sizeof(PacketV2) - 2);
        if (pkt->checksum != expected_crc) {
            stat_bad_crc++;
            return;
        }

        if (has_received_first && pkt->sequence_number <= last_sequence_number) {
            stat_out_of_order++;
            return;
        }

        last_sequence_number = pkt->sequence_number;
        has_received_first = true;
        last_packet_time_ms = millis();
        stat_rx++;

        handleCommand(static_cast<Command>(pkt->command), pkt->confidence, pkt->intensity, pkt->finger_angles);
    }
}

// ==========================================
// 4. SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==================================================");
    Serial.println("  Bionic EMG Actuator Controller (ESP32 Firmware) ");
    Serial.println("==================================================");

    // Attach all 5 Finger Servos
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    servo_thumb.setPeriodHertz(50);
    servo_index.setPeriodHertz(50);
    servo_middle.setPeriodHertz(50);
    servo_ring.setPeriodHertz(50);
    servo_pinky.setPeriodHertz(50);

    servo_thumb.attach(PIN_THUMB, 500, 2400);
    servo_index.attach(PIN_INDEX, 500, 2400);
    servo_middle.attach(PIN_MIDDLE, 500, 2400);
    servo_ring.attach(PIN_RING, 500, 2400);
    servo_pinky.attach(PIN_PINKY, 500, 2400);

    // Initial safe position
    setAllTargetAngles(ANGLE_NEUTRAL);
    for (int i = 0; i < 5; ++i) current_angles[i] = ANGLE_NEUTRAL;
    servo_thumb.write(ANGLE_NEUTRAL);
    servo_index.write(ANGLE_NEUTRAL);
    servo_middle.write(ANGLE_NEUTRAL);
    servo_ring.write(ANGLE_NEUTRAL);
    servo_pinky.write(ANGLE_NEUTRAL);

    // Connect Wi-Fi
    Serial.printf("[WIFI] Connecting to '%s'...", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 25) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WIFI] Connected successfully!");
        Serial.print("[WIFI] Assigned IP: ");
        Serial.println(WiFi.localIP());
        udp.begin(UDP_PORT);
        Serial.printf("[UDP] Listening on port %d\n", UDP_PORT);
    } else {
        Serial.println("\n[WIFI] Wi-Fi connection timed out. Running in offline/BLE mode.");
    }

#if ENABLE_BLE
    BLEDevice::init("EMG-Bionic-Actuator");
    BLEServer* pServer = BLEDevice::createServer();
    BLEService* pService = pServer->createService(BLE_SERVICE_UUID);
    BLECharacteristic* pRxChar = pService->createCharacteristic(
        BLE_CHAR_RX_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxChar->setCallbacks(new class : public BLECharacteristicCallbacks {
        void onWrite(BLECharacteristic* pChar) override {
            std::string val = pChar->getValue();
            if (val.length() > 0) {
                processIncomingPacket(reinterpret_cast<const uint8_t*>(val.data()), val.length());
            }
        }
    });
    pService->start();
    pServer->getAdvertising()->start();
    Serial.println("[BLE] Nordic UART Service started & advertising as 'EMG-Bionic-Actuator'");
#endif

    Serial.println("[READY] Safety Watchdog Armed (500ms). Awaiting packets...\n");
}

// ==========================================
// 5. MAIN LOOP (Smooth 50 Hz Easing Loop)
// ==========================================
void loop() {
    // 1. Read Wi-Fi UDP packets
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
        uint8_t packetBuffer[32];
        int len = udp.read(packetBuffer, sizeof(packetBuffer));
        if (len > 0) {
            processIncomingPacket(packetBuffer, len);
        }
    }

    // 2. Safety Watchdog Check
    if (has_received_first && (millis() - last_packet_time_ms > WATCHDOG_TIMEOUT_MS)) {
        enterSafeState();
    }

    // 3. Smooth Exponential S-Curve Easing (50 Hz motor refresh)
    static uint32_t last_servo_update_ms = 0;
    if (millis() - last_servo_update_ms >= 20) {
        last_servo_update_ms = millis();

        for (int i = 0; i < 5; ++i) {
            // Easing equation: current += alpha * (target - current)
            current_angles[i] += SMOOTHING_FACTOR * (target_angles[i] - current_angles[i]);
        }

        servo_thumb.write(static_cast<int>(round(current_angles[0])));
        servo_index.write(static_cast<int>(round(current_angles[1])));
        servo_middle.write(static_cast<int>(round(current_angles[2])));
        servo_ring.write(static_cast<int>(round(current_angles[3])));
        servo_pinky.write(static_cast<int>(round(current_angles[4])));
    }

    // 4. Periodic Telemetry Print
    if (millis() - last_telemetry_ms >= 1000) {
        last_telemetry_ms = millis();
        if (stat_rx > 0) {
            Serial.printf("[STATS] RX: %u pkts | Angle: %.0f° | Bad CRC: %u | OOO: %u\n",
                stat_rx, current_angles[0], stat_bad_crc, stat_out_of_order);
        }
    }
}
