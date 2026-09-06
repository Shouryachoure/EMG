"""
=============================================================================
Raspberry Pi Pico W — Wireless Wi-Fi UDP EMG Streamer (MicroPython)
=============================================================================
Optional: Use this if you want the Pico W to transmit EMG data to your PC 
wirelessly over Wi-Fi (no USB cable attached to PC needed!).
"""

import network
import socket
import machine
import utime
import struct

WIFI_SSID = "YOUR_WIFI_SSID"
WIFI_PASSWORD = "YOUR_WIFI_PASSWORD"
PC_IP = "192.168.1.50"  # Your PC's local IP address
UDP_PORT = 9999

emg_adc = machine.ADC(26)

def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(WIFI_SSID, WIFI_PASSWORD)
    print("Connecting to Wi-Fi...")
    while not wlan.isconnected():
        utime.sleep(0.5)
        print(".", end="")
    print("\nConnected! Pico W IP:", wlan.ifconfig()[0])

def main():
    connect_wifi()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    dest_addr = (PC_IP, UDP_PORT)
    print(f"Streaming EMG samples to PC @ {PC_IP}:{UDP_PORT}...")

    batch = []
    next_time = utime.ticks_us()

    while True:
        now = utime.ticks_us()
        if utime.ticks_diff(now, next_time) >= 0:
            next_time = utime.ticks_add(next_time, 1000) # 1000 Hz
            val = emg_adc.read_u16()
            batch.append(val)

            # Transmit in batches of 20 samples (50 Hz network packet rate)
            if len(batch) >= 20:
                # Pack as 20 x unsigned 16-bit little-endian
                payload = struct.pack('<20H', *batch)
                sock.sendto(payload, dest_addr)
                batch.clear()

if __name__ == "__main__":
    main()
