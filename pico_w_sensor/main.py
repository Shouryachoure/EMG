"""
=============================================================================
Raspberry Pi Pico W — MicroPython EMG Sensor Transmitter (USB Serial)
=============================================================================

Hardware Wiring:
  - EMG Sensor SIG / OUT -> Pico W GP26 (Pin 31 / ADC0)
  - EMG Sensor 3V3 / VCC -> Pico W 3V3(OUT) (Pin 36)
  - EMG Sensor GND       -> Pico W AGND / GND (Pin 33 or Pin 38)

IMPORTANT SAFETY NOTE:
  - Pico W ADC pins accept MAX 3.3V. Ensure your EMG module is powered from 
    Pico's 3.3V pin (Pin 36), NOT 5V!

How to Run with Thonny:
  1. Open Thonny IDE.
  2. Select interpreter: "MicroPython (Raspberry Pi Pico)".
  3. Paste this code and save it as `main.py` onto your Raspberry Pi Pico.
  4. Run the script.
  5. Close Thonny (or disconnect its serial port) so the PC pipeline can read the COM port.
  6. In `pc/config.json`, set:
       "emg_source": "serial",
       "serial_port": "COMX",  <-- Your Pico W COM port (e.g. COM4)
       "serial_baud_rate": 115200
"""

import machine
import utime

# ADC0 is GP26 (Physical pin 31)
emg_adc = machine.ADC(26)

# 1000 Hz target rate -> 1,000 microseconds (1 ms) per sample
SAMPLE_INTERVAL_US = 1000

print("[Pico W] Starting EMG Sensor Stream @ 1000 Hz on GP26...")

def run_stream():
    next_sample_time = utime.ticks_us()

    while True:
        now = utime.ticks_us()
        if utime.ticks_diff(now, next_sample_time) >= 0:
            next_sample_time = utime.ticks_add(next_sample_time, SAMPLE_INTERVAL_US)

            # read_u16 returns an unsigned 16-bit int (0 to 65535)
            # The PC SerialEMGReader automatically auto-centers and scales this.
            val = emg_adc.read_u16()
            print(val)

if __name__ == "__main__":
    run_stream()
