#pragma once
/// @file mock_actuator.h
/// @brief Mock actuator that logs commands to Serial for testing.

#include "actuator_controller.h"

class MockActuator : public ActuatorController {
public:
    void initialize() override {
#ifdef ARDUINO
        Serial.println("[MockActuator] Initialized");
#endif
    }

    void execute(Command cmd) override {
        last_command_ = cmd;
#ifdef ARDUINO
        Serial.print("[MockActuator] Execute: ");
        Serial.println(commandToString(cmd));
#endif
    }

    void safeState() override {
        last_command_ = Command::NONE;
#ifdef ARDUINO
        Serial.println("[MockActuator] SAFE STATE — stopped");
#endif
    }

    const char* name() const override {
        return "MockActuator";
    }

    Command lastCommand() const { return last_command_; }

private:
    Command last_command_ = Command::NONE;
};
