#pragma once
/// @file actuator_controller.h
/// @brief Abstract actuator interface for ESP32.
///
/// Supports: Servo, DC motor, stepper, robotic hand, etc.
/// Implement safeState() to stop the actuator when communication is lost.

#include "command.h"

class ActuatorController {
public:
    virtual ~ActuatorController() = default;

    /// Initialize the actuator hardware (set pin modes, attach servos, etc.).
    virtual void initialize() = 0;

    /// Execute a command.
    virtual void execute(Command cmd) = 0;

    /// Enter safe state — stop the actuator, move to neutral position.
    /// Called when communication is lost or an error occurs.
    virtual void safeState() = 0;

    /// Get the name of this actuator (for logging).
    virtual const char* name() const = 0;
};
