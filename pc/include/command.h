#pragma once
/// @file command.h
/// @brief Command enum representing discrete actuator actions.
///
/// The mapping from ML class IDs to these commands is configurable.
/// Default mapping (from whiteboard):
///   Class 1 → RELAX
///   Class 2 → GRASP
///   Class 3 → OPEN
///   Class 4 → CLOSE

#include <cstdint>
#include <string>
#include <ostream>

namespace emg {

/// Discrete actuator commands.
/// NONE is the safe/default state — actuator takes no action.
enum class Command : uint8_t {
    NONE  = 0,   ///< No action / safe state
    RELAX = 1,   ///< Relax / release
    GRASP = 2,   ///< Grasp / grip
    OPEN  = 3,   ///< Open hand
    CLOSE = 4    ///< Close hand
};

/// Convert Command enum to human-readable string.
inline std::string commandToString(Command cmd) {
    switch (cmd) {
        case Command::NONE:  return "NONE";
        case Command::RELAX: return "RELAX";
        case Command::GRASP: return "GRASP";
        case Command::OPEN:  return "OPEN";
        case Command::CLOSE: return "CLOSE";
        default:             return "UNKNOWN";
    }
}

inline std::ostream& operator<<(std::ostream& os, const Command& cmd) {
    return os << commandToString(cmd);
}

/// Convert string to Command enum. Returns NONE for unrecognized strings.
inline Command stringToCommand(const std::string& str) {
    if (str == "NONE")  return Command::NONE;
    if (str == "RELAX") return Command::RELAX;
    if (str == "GRASP") return Command::GRASP;
    if (str == "OPEN")  return Command::OPEN;
    if (str == "CLOSE") return Command::CLOSE;
    return Command::NONE;
}

/// Check if a command value is valid (within defined range).
inline bool isValidCommand(uint8_t value) {
    return value <= static_cast<uint8_t>(Command::CLOSE);
}

} // namespace emg
