/// @file main.cpp
/// @brief ESP32 firmware entry point for EMG Actuator Control.
///
/// Features:
///   - Connects to Wi-Fi
///   - Binds UDP receiver to port (default 8888)
///   - Receives & validates binary CommandPacket (16 bytes, CRC-16)
///   - Enforces monotonic sequence numbers
///   - Feeds safety watchdog on each valid packet
///   - Automatically drives actuator to safeState() on watchdog timeout
///   - Drives configured actuator (Servo or Mock)

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "command.h"
#include "command_packet.h"
#include "actuator_controller.h"
#include "servo_actuator.h"
#include "mock_actuator.h"
#include "safety_watchdog.h"
#include "wifi_manager.h"
#include "udp_receiver.h"

#ifndef UDP_PORT
#define UDP_PORT 8888
#endif

#ifndef WATCHDOG_TIMEOUT_MS
#define WATCHDOG_TIMEOUT_MS 500
#endif

#ifndef SERVO_PIN
#define SERVO_PIN 18
#endif

#ifndef USE_MOCK_ACTUATOR
#define USE_MOCK_ACTUATOR 0
#endif

#ifdef ARDUINO

// System objects
static WiFiManager   g_wifi;
static emg_esp32::UDPReceiver g_udp({UDP_PORT, true});
static SafetyWatchdog g_watchdog(WATCHDOG_TIMEOUT_MS);

#if USE_MOCK_ACTUATOR
static MockActuator  g_actuator_impl;
#else
static ServoActuator g_actuator_impl(SERVO_PIN);
#endif
static ActuatorController* g_actuator = &g_actuator_impl;

// Telemetry & diagnostics
static uint32_t g_last_telemetry_ms = 0;
static Command  g_current_command = Command::NONE;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("==================================================");
    Serial.println("   EMG Actuator Controller (ESP32 Firmware)       ");
    Serial.println("==================================================");

    // 1. Initialize Actuator into safe state immediately
    g_actuator->initialize();
    g_actuator->safeState();
    Serial.print("[Actuator] Initialized: ");
    Serial.println(g_actuator->name());

    // 2. Connect to Wi-Fi
    if (!g_wifi.connect()) {
        Serial.println("[WiFi] Warning: Could not connect to Wi-Fi. Retrying in loop...");
    }

    // 3. Initialize UDP receiver
    if (g_udp.initialize()) {
        Serial.print("[UDP] Listening on port ");
        Serial.println(UDP_PORT);
    } else {
        Serial.println("[UDP] FAILED to bind port!");
    }

    Serial.print("[Watchdog] Timeout configured to ");
    Serial.print(WATCHDOG_TIMEOUT_MS);
    Serial.println(" ms");

    Serial.println("[System] Ready and waiting for commands.");
}

void loop() {
    uint32_t now = millis();

    // 1. Maintain Wi-Fi connection
    if (!g_wifi.isConnected()) {
        g_actuator->safeState();
        g_wifi.ensureConnected();
        return;
    }

    // 2. Poll for UDP packets
    CommandPacket packet;
    if (g_udp.receive(packet)) {
        // Valid packet received
        g_watchdog.feed(now);

        if (packet.command != g_current_command) {
            g_current_command = packet.command;
            g_actuator->execute(packet.command);

            Serial.print("[Command] Seq=");
            Serial.print(packet.sequence_number);
            Serial.print(" Cmd=");
            Serial.print(static_cast<int>(packet.command));
            Serial.print(" Conf=");
            Serial.println(packet.confidence, 2);
        }
    }

    // 3. Check Safety Watchdog
    if (g_watchdog.check(now)) {
        if (g_current_command != Command::NONE) {
            Serial.println("[Safety] WATCHDOG TIMEOUT! Reverting to SAFE STATE.");
            g_current_command = Command::NONE;
            g_actuator->safeState();
        }
    }

    // 4. Periodic Telemetry (every 3 seconds)
    if (now - g_last_telemetry_ms >= 3000) {
        g_last_telemetry_ms = now;
        Serial.print("[Telemetry] Rx=");
        Serial.print(g_udp.getPacketsReceived());
        Serial.print(" Bad=");
        Serial.print(g_udp.getPacketsInvalid());
        Serial.print(" OutOfOrder=");
        Serial.print(g_udp.getPacketsOutOfOrder());
        Serial.print(" Watchdog=");
        Serial.println(g_watchdog.isTimedOut() ? "TRIGGERED" : "OK");
    }

    delay(2);
}

#else

int main() {
    // Non-Arduino stub for host compile verification
    return 0;
}

#endif
