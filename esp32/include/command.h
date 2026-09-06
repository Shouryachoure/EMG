#pragma once
/// @file command.h (ESP32 version)
/// @brief Command enum — shared with PC-side (must be identical).

#include <cstdint>

enum class Command : uint8_t {
    NONE  = 0,
    RELAX = 1,
    GRASP = 2,
    OPEN  = 3,
    CLOSE = 4
};

inline const char* commandToString(Command cmd) {
    switch (cmd) {
        case Command::NONE:  return "NONE";
        case Command::RELAX: return "RELAX";
        case Command::GRASP: return "GRASP";
        case Command::OPEN:  return "OPEN";
        case Command::CLOSE: return "CLOSE";
        default:             return "UNKNOWN";
    }
}

inline bool isValidCommand(uint8_t value) {
    return value <= static_cast<uint8_t>(Command::CLOSE);
}
