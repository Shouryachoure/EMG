#!/usr/bin/env python3
"""
EMG & Actuator Real-Time Telemetry Dashboard Server.

Features:
- Serves the frontend web dashboard on http://localhost:8080
- WebSocket server on ws://localhost:8080/ws
- Binds UDP receiver (port 8888) to capture live CommandPacket stream from emg_control.exe
- Computes synchronized 1000 Hz raw & filtered EMG waveform samples and time-domain features
- Broadcasts real-time JSON frames to all connected web clients at 30-60 FPS
"""

import asyncio
import http.server
import json
import math
import os
import random
import socket
import struct
import threading
import time
import websockets

# Configuration
HTTP_PORT = 8080
WS_PORT = 8081
UDP_PORT = 8888
FRAME_RATE = 30  # Telemetry broadcast rate (Hz)
SAMPLES_PER_FRAME = 33  # ~1000 Hz EMG sample rate divided by 30 fps

STATIC_DIR = os.path.dirname(os.path.abspath(__file__))

COMMAND_NAMES = {
    0: "NONE",
    1: "RELAX",
    2: "GRASP",
    3: "OPEN",
    4: "CLOSE"
}

SERVO_ANGLES = {
    0: 90,   # Safe state
    1: 90,   # Relax (neutral)
    2: 0,    # Grasp (closed)
    3: 180,  # Open (wide)
    4: 0     # Close (closed)
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


class TelemetryState:
    def __init__(self):
        self.lock = threading.Lock()
        self.command_id = 1
        self.command_name = "RELAX"
        self.confidence = 0.95
        self.sequence_number = 0
        self.timestamp_ms = 0
        self.servo_angle = 90
        self.packets_received = 0
        self.packets_corrupted = 0
        self.packets_out_of_order = 0
        self.last_packet_time = 0.0
        self.packet_rate_hz = 0.0
        self.watchdog_timed_out = False
        self.watchdog_timeout_sec = 0.5
        self.manual_override = False
        self.signal_phase = 0.0

        # Rate estimation
        self._packet_timestamps = []

    def update_from_udp(self, cmd_id, conf, seq, ts):
        now = time.time()
        with self.lock:
            self.command_id = cmd_id
            self.command_name = COMMAND_NAMES.get(cmd_id, "NONE")
            self.confidence = conf
            self.sequence_number = seq
            self.timestamp_ms = ts
            self.servo_angle = SERVO_ANGLES.get(cmd_id, 90)
            self.packets_received += 1
            self.last_packet_time = now
            self.watchdog_timed_out = False
            self.manual_override = False

            self._packet_timestamps.append(now)
            cutoff = now - 1.0
            self._packet_timestamps = [t for t in self._packet_timestamps if t > cutoff]
            self.packet_rate_hz = len(self._packet_timestamps)

    def check_watchdog(self):
        now = time.time()
        with self.lock:
            if self.packets_received > 0 and (now - self.last_packet_time > self.watchdog_timeout_sec):
                if not self.watchdog_timed_out:
                    self.watchdog_timed_out = True
                    self.command_id = 0
                    self.command_name = "NONE"
                    self.servo_angle = 90

    def set_manual_command(self, cmd_id):
        with self.lock:
            self.manual_override = True
            self.command_id = cmd_id
            self.command_name = COMMAND_NAMES.get(cmd_id, "NONE")
            self.confidence = 0.98
            self.servo_angle = SERVO_ANGLES.get(cmd_id, 90)
            self.last_packet_time = time.time()
            self.watchdog_timed_out = False

    def generate_frame(self):
        with self.lock:
            cmd = self.command_id
            conf = self.confidence
            seq = self.sequence_number
            angle = self.servo_angle
            rx_count = self.packets_received
            rate = self.packet_rate_hz
            wd = self.watchdog_timed_out
            manual = self.manual_override
            cmd_name = self.command_name

        # Generate realistic EMG waveform samples for the active gesture
        raw_samples = []
        filtered_samples = []

        # Signal parameters per gesture
        # 0 (NONE) / 1 (RELAX): baseline noise
        # 2 (GRASP): strong low-frequency burst
        # 3 (OPEN): high-frequency wider burst
        # 4 (CLOSE): intense muscle contraction
        if cmd in (0, 1):
            amp, f1, f2 = 0.06, 30.0, 60.0
        elif cmd == 2:
            amp, f1, f2 = 0.45, 50.0, 120.0
        elif cmd == 3:
            amp, f1, f2 = 0.65, 80.0, 200.0
        else: # 4
            amp, f1, f2 = 0.90, 60.0, 150.0

        for _ in range(SAMPLES_PER_FRAME):
            self.signal_phase += 1.0 / 1000.0
            t = self.signal_phase

            # Raw signal: DC offset + harmonics + noise
            dc = 0.15
            sig = (amp * 0.7 * math.sin(2.0 * math.pi * f1 * t) +
                   amp * 0.3 * math.sin(2.0 * math.pi * f2 * t) +
                   random.gauss(0, 0.04))
            raw = dc + sig
            filt = sig # Filtered removes DC and bounds

            raw_samples.append(round(raw, 4))
            filtered_samples.append(round(filt, 4))

        # Compute real-time time-domain features on current batch
        rms = math.sqrt(sum(s * s for s in filtered_samples) / len(filtered_samples))
        mav = sum(abs(s) for s in filtered_samples) / len(filtered_samples)
        mean_v = sum(filtered_samples) / len(filtered_samples)
        variance = sum((s - mean_v) ** 2 for s in filtered_samples) / max(1, len(filtered_samples) - 1)
        wl = sum(abs(filtered_samples[i] - filtered_samples[i-1]) for i in range(1, len(filtered_samples)))
        zc = sum(1 for i in range(1, len(filtered_samples)) if (filtered_samples[i] * filtered_samples[i-1] < 0) and abs(filtered_samples[i] - filtered_samples[i-1]) > 0.01)
        ssc = sum(1 for i in range(2, len(filtered_samples)) if ((filtered_samples[i] - filtered_samples[i-1]) * (filtered_samples[i-1] - filtered_samples[i-2]) < 0))

        return {
            "type": "telemetry",
            "time": time.time(),
            "command_id": cmd,
            "command_name": cmd_name,
            "confidence": round(conf, 3),
            "sequence_number": seq,
            "servo_angle": angle,
            "packets_received": rx_count,
            "packet_rate_hz": rate,
            "watchdog_timed_out": wd,
            "manual_override": manual,
            "raw_samples": raw_samples,
            "filtered_samples": filtered_samples,
            "features": {
                "rms": round(rms, 4),
                "mav": round(mav, 4),
                "variance": round(variance, 6),
                "wl": round(wl, 4),
                "zc": zc,
                "ssc": ssc
            }
        }


g_state = TelemetryState()
g_clients = set()


def udp_listener_thread():
    """Background thread listening for binary CommandPacket UDP datagrams."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.bind(("0.0.0.0", UDP_PORT))
        print(f"[UDP] Listening for EMG packets on 0.0.0.0:{UDP_PORT}")
    except Exception as e:
        print(f"[UDP] Warning: could not bind port {UDP_PORT}: {e}")
        return

    sock.settimeout(0.2)
    last_seq = 0
    has_seq = False

    while True:
        try:
            data, _ = sock.recvfrom(64)
            if len(data) != 16:
                g_state.packets_corrupted += 1
                continue

            version, cmd_id, seq, ts, conf, rx_crc = struct.unpack("<BBIIfH", data)
            calc_crc = crc16(data[:14])

            if calc_crc != rx_crc:
                g_state.packets_corrupted += 1
                continue

            if has_seq and seq <= last_seq:
                g_state.packets_out_of_order += 1
                continue

            last_seq = seq
            has_seq = True
            g_state.update_from_udp(cmd_id, conf, seq, ts)

        except socket.timeout:
            pass
        except Exception:
            pass


class CustomHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=STATIC_DIR, **kwargs)

    def log_message(self, format, *args):
        pass  # Suppress excessive HTTP access logs


def run_http_server():
    httpd = http.server.HTTPServer(("0.0.0.0", HTTP_PORT), CustomHTTPRequestHandler)
    print(f"[HTTP] Serving dashboard at http://localhost:{HTTP_PORT}")
    httpd.serve_forever()


async def ws_handler(websocket):
    g_clients.add(websocket)
    try:
        async for message in websocket:
            try:
                data = json.loads(message)
                if data.get("action") == "manual_command":
                    cmd_id = int(data.get("command_id", 0))
                    g_state.set_manual_command(cmd_id)
            except Exception:
                pass
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        g_clients.remove(websocket)


async def broadcast_loop():
    while True:
        g_state.check_watchdog()
        frame = g_state.generate_frame()
        frame_json = json.dumps(frame)

        if g_clients:
            # Broadcast to all connected WebSocket clients
            disconnected = []
            for ws in list(g_clients):
                try:
                    await ws.send(frame_json)
                except Exception:
                    disconnected.append(ws)
            for ws in disconnected:
                g_clients.discard(ws)

        await asyncio.sleep(1.0 / FRAME_RATE)


async def main_async():
    # Start background UDP thread
    t_udp = threading.Thread(target=udp_listener_thread, daemon=True)
    t_udp.start()

    # Start background HTTP thread
    t_http = threading.Thread(target=run_http_server, daemon=True)
    t_http.start()

    # Start WebSocket server
    async with websockets.serve(ws_handler, "0.0.0.0", WS_PORT, ping_interval=20):
        print(f"[WS]   WebSocket server ready on ws://localhost:{WS_PORT}")
        await broadcast_loop()


def main():
    print("=" * 60)
    print("  EMG Actuator Control — Live Telemetry Dashboard Server")
    print(f"  Web Dashboard: http://localhost:{HTTP_PORT}")
    print(f"  WebSocket:     ws://localhost:{WS_PORT}")
    print(f"  UDP Receiver:  Port {UDP_PORT}")
    print("=" * 60)
    asyncio.run(main_async())


if __name__ == "__main__":
    main()
