#pragma once
/// @file safety_watchdog.h
/// @brief Communication timeout watchdog for ESP32.
///
/// If no valid command packet is received for WATCHDOG_TIMEOUT_MS,
/// the actuator enters safe state. This prevents stale commands
/// from running indefinitely.

#include <cstdint>

#ifndef WATCHDOG_TIMEOUT_MS
#define WATCHDOG_TIMEOUT_MS 500
#endif

class SafetyWatchdog {
public:
    explicit SafetyWatchdog(uint32_t timeout_ms = WATCHDOG_TIMEOUT_MS)
        : timeout_ms_(timeout_ms)
        , last_valid_time_(0)
        , timed_out_(false) {}

    /// Call this when a valid packet is received to reset the watchdog.
    void feed(uint32_t current_time_ms) {
        last_valid_time_ = current_time_ms;
        timed_out_ = false;
    }

    /// Check if the watchdog has timed out.
    /// @param current_time_ms Current time in milliseconds.
    /// @return true if timed out (no valid packet for timeout_ms_).
    bool check(uint32_t current_time_ms) {
        if (last_valid_time_ == 0) {
            // Not yet initialized — don't trigger immediately
            return false;
        }

        uint32_t elapsed = current_time_ms - last_valid_time_;
        if (elapsed > timeout_ms_) {
            if (!timed_out_) {
                timed_out_ = true;
            }
            return true;
        }
        return false;
    }

    /// Check if the watchdog has already triggered.
    bool isTimedOut() const { return timed_out_; }

    /// Get the timeout duration.
    uint32_t timeoutMs() const { return timeout_ms_; }

    /// Set a new timeout duration.
    void setTimeout(uint32_t ms) { timeout_ms_ = ms; }

private:
    uint32_t timeout_ms_;
    uint32_t last_valid_time_;
    bool     timed_out_;
};
