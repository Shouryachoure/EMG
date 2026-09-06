# UDP Protocol Specification

## Overview

The PC application sends structured binary command packets to the ESP32 over UDP.
The protocol is designed for:
- **Low latency**: Fixed-size packets, no handshaking
- **Safety**: CRC validation, sequence numbers, staleness detection
- **Simplicity**: Stateless, unidirectional (PC → ESP32)

## Network Configuration

| Parameter | Default | Configurable |
|-----------|---------|-------------|
| ESP32 IP Address | `192.168.1.100` | Yes (`config.json`) |
| UDP Port | `8888` | Yes (`config.json`) |
| Protocol | UDP/IPv4 | No |
| Direction | PC → ESP32 (unidirectional) | No |

## Packet Format

**Total size: 16 bytes, little-endian byte order.**

```
Offset  Size  Field            Type       Description
──────────────────────────────────────────────────────────────
0       1     version          uint8_t    Protocol version (currently 1)
1       1     command          uint8_t    Command enum value
2       4     sequence_number  uint32_t   Monotonically increasing counter
6       4     timestamp_ms     uint32_t   Sender-side ms since epoch (truncated)
10      4     confidence       float      ML confidence [0.0, 1.0] (IEEE 754)
14      2     checksum         uint16_t   CRC-16/CCITT over bytes 0-13
──────────────────────────────────────────────────────────────
Total: 16 bytes
```

### Byte Layout (Hex Example)

```
 00  01  02 03 04 05  06 07 08 09  0A 0B 0C 0D  0E 0F
[VV][CC][SQ SQ SQ SQ][TS TS TS TS][CF CF CF CF][CK CK]
```

Example packet (hex, little-endian):
```
01 02 05 00 00 00 A0 86 01 00 CD CC 4C 3F 7B 3A
│  │  │           │           │           │
│  │  │           │           │           └── CRC-16 = 0x3A7B
│  │  │           │           └── confidence = 0.80 (IEEE 754)
│  │  │           └── timestamp = 100000 ms
│  │  └── sequence = 5
│  └── command = 2 (GRASP)
└── version = 1
```

## Command Values

| Value | Enum | Description |
|-------|------|-------------|
| 0 | `NONE` | No action / safe state |
| 1 | `RELAX` | Relax / release |
| 2 | `GRASP` | Grasp / grip |
| 3 | `OPEN` | Open hand |
| 4 | `CLOSE` | Close hand |
| 5–255 | — | Reserved (invalid, discard packet) |

## CRC-16 Calculation

Algorithm: **CRC-16/CCITT-FALSE**
- Polynomial: `0x1021`
- Initial value: `0xFFFF`
- Input reflected: No
- Output reflected: No
- Final XOR: `0x0000`

Computed over bytes 0–13 (all fields except checksum).

## Validation Rules (ESP32 Receiver)

The ESP32 MUST validate every received packet. Invalid packets are silently discarded.

### 1. Size Check
```
if (received_bytes != 16) → DISCARD
```

### 2. Version Check
```
if (packet.version != EXPECTED_VERSION) → DISCARD
```

### 3. CRC Check
```
computed_crc = crc16(packet_bytes[0..13])
if (computed_crc != packet.checksum) → DISCARD
```

### 4. Command Range Check
```
if (packet.command > 4) → DISCARD
```

### 5. Sequence Number Check
```
if (packet.sequence_number <= last_sequence_number) → DISCARD
(prevents replay and out-of-order packets)
```

### 6. Staleness Check
```
if (current_time - packet.timestamp_ms > STALE_THRESHOLD_MS) → DISCARD
(prevents acting on old commands)
```

Note: Since PC and ESP32 clocks are not synchronized, staleness is primarily
enforced via the ESP32's own watchdog timer (time since last valid packet).

## Timeout & Safety Behavior

### ESP32 Communication Watchdog

```
WATCHDOG_TIMEOUT_MS = 500  (configurable)

if (millis() - last_valid_packet_time > WATCHDOG_TIMEOUT_MS):
    enter_safe_state()    // Stop actuator, set NONE
```

### Packet Loss Behavior

UDP does not guarantee delivery. The system handles this by:

1. **No retransmission**: Stale commands are useless for real-time control
2. **Continuous stream**: PC sends commands at ~20–50 Hz, so occasional drops are tolerable
3. **Watchdog**: If too many consecutive packets are lost, ESP32 enters safe state
4. **Sequence gaps**: ESP32 accepts any packet with sequence > last_sequence (allows gaps)

### Invalid Packet Behavior

Invalid packets are:
- Silently discarded (no response sent)
- Optionally logged to ESP32 serial monitor for debugging
- Do NOT affect the current actuator state
- Do NOT reset the watchdog timer (only valid packets reset it)

## Serialization (C++ Reference)

### PC-Side (Serialize)
```cpp
void CommandPacket::serialize(uint8_t* buffer) const {
    buffer[0] = version;
    buffer[1] = static_cast<uint8_t>(command);
    memcpy(&buffer[2],  &sequence_number, 4);  // little-endian
    memcpy(&buffer[6],  &timestamp_ms,    4);
    memcpy(&buffer[10], &confidence,      4);
    uint16_t crc = crc16(buffer, 14);
    memcpy(&buffer[14], &crc, 2);
}
```

### ESP32-Side (Deserialize)
```cpp
bool CommandPacket::deserialize(const uint8_t* buffer, size_t len) {
    if (len != PACKET_SIZE) return false;
    version = buffer[0];
    command = static_cast<Command>(buffer[1]);
    memcpy(&sequence_number, &buffer[2],  4);
    memcpy(&timestamp_ms,    &buffer[6],  4);
    memcpy(&confidence,      &buffer[10], 4);
    memcpy(&checksum,        &buffer[14], 2);
    uint16_t computed = crc16(buffer, 14);
    return (computed == checksum);
}
```
