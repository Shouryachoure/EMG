#pragma once
/// @file servo_actuator.h
/// @brief Servo-based actuator implementation.
///
/// Uses ESP32Servo library to control a PWM servo.
/// Maps commands to servo angles:
///   NONE  → 90° (neutral)
///   RELAX → 90° (neutral)
///   GRASP → 0°  (closed grip)
///   OPEN  → 180° (open)
///   CLOSE → 0°  (closed)
///
/// Angles are configurable.

#include "actuator_controller.h"

#ifdef ARDUINO
#include <ESP32Servo.h>
#endif

class ServoActuator : public ActuatorController {
public:
    /// @param pin       GPIO pin for servo signal
    /// @param neutral   Neutral angle (safe state)
    /// @param grasp     Angle for GRASP command
    /// @param open_angle  Angle for OPEN command
    /// @param close_angle Angle for CLOSE command
    ServoActuator(int pin = 18,
                  int neutral = 90,
                  int grasp = 0,
                  int open_angle = 180,
                  int close_angle = 0)
        : pin_(pin)
        , neutral_angle_(neutral)
        , grasp_angle_(grasp)
        , open_angle_(open_angle)
        , close_angle_(close_angle)
        , current_angle_(neutral) {}

    void initialize() override {
#ifdef ARDUINO
        servo_.attach(pin_);
        servo_.write(neutral_angle_);
#endif
        current_angle_ = neutral_angle_;
    }

    void execute(Command cmd) override {
        int target = neutral_angle_;

        switch (cmd) {
            case Command::NONE:
            case Command::RELAX:
                target = neutral_angle_;
                break;
            case Command::GRASP:
                target = grasp_angle_;
                break;
            case Command::OPEN:
                target = open_angle_;
                break;
            case Command::CLOSE:
                target = close_angle_;
                break;
        }

        if (target != current_angle_) {
#ifdef ARDUINO
            servo_.write(target);
#endif
            current_angle_ = target;
        }
    }

    void safeState() override {
        execute(Command::NONE);
    }

    const char* name() const override {
        return "ServoActuator";
    }

private:
#ifdef ARDUINO
    Servo servo_;
#endif
    int pin_;
    int neutral_angle_;
    int grasp_angle_;
    int open_angle_;
    int close_angle_;
    int current_angle_;
};
