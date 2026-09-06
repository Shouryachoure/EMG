#pragma once
/// @file wifi_manager.h
/// @brief Wi-Fi connection management for ESP32.

#ifdef ARDUINO
#include <WiFi.h>
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YOUR_PASSWORD"
#endif

class WiFiManager {
public:
    /// Connect to Wi-Fi with configured SSID and password.
    /// Blocks until connected or timeout.
    /// @param timeout_ms Maximum time to wait for connection.
    /// @return true if connected.
    bool connect(uint32_t timeout_ms = 10000) {
#ifdef ARDUINO
        Serial.print("[WiFi] Connecting to ");
        Serial.println(WIFI_SSID);

        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - start > timeout_ms) {
                Serial.println("\n[WiFi] Connection FAILED (timeout)");
                return false;
            }
            delay(250);
            Serial.print(".");
        }

        Serial.println();
        Serial.print("[WiFi] Connected! IP: ");
        Serial.println(WiFi.localIP());
        return true;
#else
        (void)timeout_ms;
        return false;
#endif
    }

    /// Check if still connected.
    bool isConnected() const {
#ifdef ARDUINO
        return WiFi.status() == WL_CONNECTED;
#else
        return false;
#endif
    }

    /// Attempt to reconnect if disconnected.
    bool ensureConnected() {
        if (isConnected()) return true;
#ifdef ARDUINO
        Serial.println("[WiFi] Reconnecting...");
#endif
        return connect();
    }
};
