/**
 * =========================================================================
 * EMG Actuator Controller — Arduino IDE Sketch for ESP32
 * =========================================================================
 * 
 * Hardware Wiring:
 *   - Servo Signal (Orange/Yellow) -> ESP32 GPIO 18
 *   - Servo VCC (Red)             -> External 5V or ESP32 VIN
 *   - Servo GND (Brown/Black)     -> ESP32 GND (Common Ground)
 * 
 * Required Arduino Libraries (Install via Arduino Library Manager):
 *   - ESP32Servo by Kevin Harrington
 * 
 * Instructions:
 *   1. Update WIFI_SSID and WIFI_PASSWORD below.
 *   2. Select Board: "ESP32 Dev Module" (or your specific ESP32 variant).
 *   3. Upload and open Serial Monitor @ 115200 baud.
 *   4. Note the printed IP address (e.g. 192.168.1.150).
 *   5. Put that IP into d:\EMG\pc\config.json ("esp32_ip").
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>

// ==========================================
// 1. CONFIGURATION (Edit for your setup)
// ==========================================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const uint16_t UDP_PORT   = 8888;
const int SERVO_PIN       = 18;
const uint32_t WATCHDOG_TIMEOUT_MS = 500;

// Servo Angles (Degrees)
const int ANGLE_NEUTRAL = 90;   // RELAX / NONE (Safe State)
const int ANGLE_GRASP   = 0;    // GRASP (Closed Grip)
const int ANGLE_OPEN    = 180;  // OPEN (Wide Open)
const int ANGLE_CLOSE   = 0;    // CLOSE (Full Fist)

// ==========================================
// 2. PROTOCOL DEFINITIONS (16-byte Packet)
// ==========================================
enum class Command : uint8_t {
    NONE  = 0,
    RELAX = 1,
    GRASP = 2,
    OPEN  = 3,
    CLOSE = 4
};

struct __attribute__((packed)) CommandPacket {
    uint8_t  version;          // 1
    uint8_t  command;          // Command enum (0-4)
    uint32_t sequence_number;  // Monotonic
    uint32_t timestamp_ms;     // Sender timestamp
    float    confidence;       // 0.0 - 1.0
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
// 3. HARDWARE & STATE
// ==========================================
WiFiUDP udp;
Servo servo;

uint32_t last_packet_time_ms = 0;
uint32_t last_sequence_number = 0;
bool has_received_first = false;
Command current_command = Command::NONE;
int current_angle = ANGLE_NEUTRAL;

uint32_t stat_rx = 0;
uint32_t stat_bad_crc = 0;
uint32_t stat_out_of_order = 0;
uint32_t last_telemetry_ms = 0;

void setServoAngle(int target_angle) {
    if (target_angle != current_angle) {
        servo.write(target_angle);
        current_angle = target_angle;
    }
}

void enterSafeState() {
    if (current_command != Command::NONE) {
        Serial.println("[SAFETY] WATCHDOG TIMEOUT! Returning actuator to SAFE STATE (90 deg).");
        current_command = Command::NONE;
        setServoAngle(ANGLE_NEUTRAL);
    }
}

// ==========================================
// 4. SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("==================================================");
    Serial.println("   EMG Actuator Controller (ESP32 Firmware)       ");
    Serial.println("==================================================");

    // Attach Servo & move to safe state immediately
    servo.attach(SERVO_PIN);
    setServoAngle(ANGLE_NEUTRAL);
    Serial.print("[Actuator] Attached to GPIO ");
    Serial.print(SERVO_PIN);
    Serial.println(" (Safe State: 90 deg)");

    // Connect to Wi-Fi
    Serial.print("[WiFi] Connecting to: ");
    Serial.println(WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 25) {
        delay(400);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("==================================================");
        Serial.print("   CONNECTED! ESP32 IP ADDRESS: ");
        Serial.println(WiFi.localIP());
        Serial.println("==================================================");
    } else {
        Serial.println("\n[WiFi] Warning: Connection pending. Retrying in background...");
    }

    // Start UDP Receiver
    udp.begin(UDP_PORT);
    Serial.print("[UDP] Listening on port ");
    Serial.println(UDP_PORT);
    Serial.println("[System] Ready. Waiting for commands from PC...");
}

// ==========================================
// 5. MAIN LOOP
// ==========================================
void loop() {
    uint32_t now = millis();

    // 1. Maintain Wi-Fi
    if (WiFi.status() != WL_CONNECTED) {
        enterSafeState();
        delay(100);
        return;
    }

    // 2. Poll for incoming UDP packet
    int packet_size = udp.parsePacket();
    if (packet_size >= 16) {
        uint8_t buffer[16];
        int bytes_read = udp.read(buffer, 16);
        udp.flush();

        if (bytes_read == 16) {
            uint16_t rx_crc = (static_cast<uint16_t>(buffer[15]) << 8) | buffer[14];
            uint16_t calc_crc = crc16(buffer, 14);

            if (rx_crc != calc_crc) {
                stat_bad_crc++;
            } else {
                // CRC is valid
                uint8_t cmd_raw = buffer[1];
                uint32_t seq = (static_cast<uint32_t>(buffer[5]) << 24) |
                               (static_cast<uint32_t>(buffer[4]) << 16) |
                               (static_cast<uint32_t>(buffer[3]) << 8)  |
                               (static_cast<uint32_t>(buffer[2]));

                if (has_received_first && seq <= last_sequence_number) {
                    stat_out_of_order++;
                } else {
                    // Valid in-order packet
                    last_sequence_number = seq;
                    has_received_first = true;
                    last_packet_time_ms = now;
                    stat_rx++;

                    Command cmd = static_cast<Command>(cmd_raw);
                    if (cmd != current_command) {
                        current_command = cmd;

                        int target_angle = ANGLE_NEUTRAL;
                        const char* cmd_str = "NONE";
                        switch (cmd) {
                            case Command::RELAX: target_angle = ANGLE_NEUTRAL; cmd_str = "RELAX"; break;
                            case Command::GRASP: target_angle = ANGLE_GRASP;   cmd_str = "GRASP"; break;
                            case Command::OPEN:  target_angle = ANGLE_OPEN;    cmd_str = "OPEN";  break;
                            case Command::CLOSE: target_angle = ANGLE_CLOSE;   cmd_str = "CLOSE"; break;
                            default:             target_angle = ANGLE_NEUTRAL; cmd_str = "NONE";  break;
                        }

                        setServoAngle(target_angle);
                        Serial.print("[ACTUATOR] Cmd: ");
                        Serial.print(cmd_str);
                        Serial.print(" | Angle: ");
                        Serial.print(target_angle);
                        Serial.print(" deg | Seq: ");
                        Serial.println(seq);
                    }
                }
            }
        }
    }

    // 3. Safety Watchdog Check
    if (has_received_first && (now - last_packet_time_ms > WATCHDOG_TIMEOUT_MS)) {
        enterSafeState();
    }

    // 4. Diagnostic Telemetry (Every 3 seconds)
    if (now - last_telemetry_ms >= 3000) {
        last_telemetry_ms = now;
        Serial.print("[Telemetry] Rx: ");
        Serial.print(stat_rx);
        Serial.print(" | Bad CRC: ");
        Serial.print(stat_bad_crc);
        Serial.print(" | Servo: ");
        Serial.print(current_angle);
        Serial.println(" deg");
    }

    delay(2);
}
