#!/usr/bin/env python3
"""
ESP32 UDP Receiver & Actuator Simulator.
Mirrors the ESP32 firmware logic on the host for testing and verification without hardware.

Features:
- Binds UDP socket (port 8888 by default)
- Receives 16-byte binary CommandPacket
- Validates CRC-16/CCITT-FALSE checksum
- Enforces monotonically increasing sequence numbers
- Implements 500ms safety watchdog (enters safe state on timeout)
- Simulates Servo angles:
    NONE  / RELAX: 90° (neutral)
    GRASP / CLOSE: 0°  (closed grip)
    OPEN         : 180° (fully open)
"""

import socket
import struct
import time
import sys

COMMAND_NAMES = {
    0: "NONE",
    1: "RELAX",
    2: "GRASP",
    3: "OPEN",
    4: "CLOSE"
}

SERVO_ANGLES = {
    0: 90,   # Safe / neutral
    1: 90,   # Relax
    2: 0,    # Grasp
    3: 180,  # Open
    4: 0     # Close
}

def crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE matching PC and ESP32 implementations."""
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def run_simulator(port=8888, watchdog_timeout=0.5):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", port))
    sock.settimeout(0.1)  # 100ms non-blocking check for watchdog

    print("=" * 60)
    print(f"  ESP32 Actuator Simulator listening on UDP port {port}")
    print(f"  Safety Watchdog Timeout: {watchdog_timeout * 1000:.0f} ms")
    print("=" * 60)

    last_seq = 0
    has_first = False
    last_packet_time = time.time()
    current_command = 0
    current_angle = 90
    total_rx = 0
    total_bad_crc = 0
    total_out_of_order = 0

    try:
        while True:
            now = time.time()
            try:
                data, addr = sock.recvfrom(64)
                if len(data) != 16:
                    print(f"[ESP32] Invalid packet length: {len(data)} bytes (expected 16)")
                    total_bad_crc += 1
                    continue

                # Unpack: version(B), cmd(B), seq(I), timestamp(I), conf(f), crc(H)
                version, cmd_id, seq, ts, conf, rx_crc = struct.unpack("<BBIIfH", data)
                calc_crc = crc16(data[:14])

                if calc_crc != rx_crc:
                    print(f"[ESP32] CRC MISMATCH! Got 0x{rx_crc:04X}, expected 0x{calc_crc:04X}")
                    total_bad_crc += 1
                    continue

                if has_first and seq <= last_seq:
                    print(f"[ESP32] OUT-OF-ORDER packet dropped: seq {seq} <= last {last_seq}")
                    total_out_of_order += 1
                    continue

                # Valid packet
                last_seq = seq
                has_first = True
                last_packet_time = now
                total_rx += 1

                cmd_name = COMMAND_NAMES.get(cmd_id, f"UNKNOWN({cmd_id})")
                angle = SERVO_ANGLES.get(cmd_id, 90)

                if cmd_id != current_command:
                    current_command = cmd_id
                    current_angle = angle
                    print(f"[ACTUATOR] >>> GESTURE: {cmd_name:<6} | Servo: {current_angle:3d}° | Conf: {conf:.2f} | Seq: {seq}")

            except socket.timeout:
                pass

            # Watchdog check
            if has_first and (now - last_packet_time > watchdog_timeout):
                if current_command != 0:
                    print(f"[SAFETY] *** WATCHDOG TIMEOUT ({watchdog_timeout*1000:.0f}ms without packet) ***")
                    print("[SAFETY] >>> Actuator moved to SAFE STATE (Servo 90° Neutral)")
                    current_command = 0
                    current_angle = 90

    except KeyboardInterrupt:
        print("\nSimulator stopped.")
        print(f"Stats: {total_rx} valid packets, {total_bad_crc} bad CRC, {total_out_of_order} out-of-order.")
    finally:
        sock.close()

if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8888
    run_simulator(port=port)
