#!/usr/bin/env python3
"""
Automated End-to-End Integration Test:
PC emg_control.exe -> UDP Socket -> Simulated ESP32 Receiver
"""

import socket
import struct
import subprocess
import time
import sys
import os

def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

def main():
    test_port = 8889
    print("==================================================")
    print("  EMG Control System — End-to-End Integration Test")
    print("==================================================")

    # 1. Bind UDP receiver on test port
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("127.0.0.1", test_port))
    sock.settimeout(0.5)

    # 2. Prepare test config.json
    test_config_path = "d:/EMG/pc/build/test_e2e_config.json"
    with open("d:/EMG/pc/config.json", "r") as f:
        config_content = f.read()

    # Point to localhost:8889
    config_content = config_content.replace('"esp32_ip": "192.168.1.100"', '"esp32_ip": "127.0.0.1"')
    config_content = config_content.replace('"esp32_port": 8888', f'"esp32_port": {test_port}')

    with open(test_config_path, "w") as f:
        f.write(config_content)

    # 3. Launch emg_control.exe as a subprocess
    exe_path = "d:/EMG/pc/build/emg_control.exe"
    env = os.environ.copy()
    env["PATH"] = "C:\\msys64\\ucrt64\\bin;" + env.get("PATH", "")

    proc = subprocess.Popen([exe_path, test_config_path], env=env)
    print(f"[TEST] Spawned emg_control PID {proc.pid}")

    received_packets = []
    start_time = time.time()

    try:
        # Collect packets for ~3 seconds
        while time.time() - start_time < 3.0:
            try:
                data, _ = sock.recvfrom(64)
                if len(data) == 16:
                    version, cmd_id, seq, ts, conf, rx_crc = struct.unpack("<BBIIfH", data)
                    calc_crc = crc16(data[:14])
                    assert calc_crc == rx_crc, f"CRC mismatch: expected {calc_crc}, got {rx_crc}"
                    received_packets.append({
                        "version": version,
                        "command": cmd_id,
                        "seq": seq,
                        "timestamp": ts,
                        "conf": conf
                    })
            except socket.timeout:
                pass
    finally:
        # Terminate emg_control
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()
        sock.close()

    print(f"[TEST] Received {len(received_packets)} valid UDP command packets")
    assert len(received_packets) >= 5, f"Expected at least 5 packets, got {len(received_packets)}"

    # Verify sequence monotonicity
    seqs = [p["seq"] for p in received_packets]
    for i in range(1, len(seqs)):
        assert seqs[i] > seqs[i-1], f"Sequence numbers not monotonic: {seqs[i-1]} -> {seqs[i]}"

    # Verify commands and confidence values
    for p in received_packets:
        assert p["version"] == 1, f"Expected version 1, got {p['version']}"
        assert 0 <= p["command"] <= 4, f"Invalid command ID {p['command']}"
        assert 0.0 <= p["conf"] <= 1.0, f"Confidence out of range: {p['conf']}"

    print("[TEST] Sequence monotonicity: VERIFIED")
    print("[TEST] CRC-16 integrity on all packets: VERIFIED")
    print("[TEST] Command & confidence values: VERIFIED")
    print("==================================================")
    print("  ALL INTEGRATION CHECKS PASSED!")
    print("==================================================")
    return 0

if __name__ == "__main__":
    sys.exit(main())
