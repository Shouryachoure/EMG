# Complete Step-by-Step Hardware Setup & Deployment Guide

This guide takes you through the full end-to-end setup of the **EMG-Based Real-Time Actuator Control System** using:
1. **Input**: Analog EMG Sensor + **Raspberry Pi Pico W**
2. **Brain**: PC (C++17 Pipeline + Live Web Dashboard)
3. **Output**: **ESP32** + Servo Motor (Wi-Fi UDP Actuator)

---

## Stage 1: Hardware Wiring

### Part A: EMG Sensor to Raspberry Pi Pico W

Connect your EMG sensor (MyoWare 2.0, AD8232, SEN0240, Gravity, etc.) to the Pico W:

| EMG Sensor Pin | Pico W Pin Name | Physical Pin Number | Wire Color Suggestion |
|---|---|---|---|
| **SIG / OUT** (Analog Signal) | **GP26 (ADC0)** | **Pin 31** | Yellow / Blue |
| **VCC / 3V3** (Power) | **3V3(OUT)** | **Pin 36** | Red |
| **GND** (Ground) | **AGND** (Analog GND) | **Pin 33** (or Pin 38) | Black |

> [!CAUTION]
> **Voltage Warning**: The Pico W ADC accepts a **maximum of 3.3V**. Power your EMG sensor strictly from **Pin 36 (3V3 OUT)**. Connecting 5V to GP26 can permanently damage the Pico ADC.

---

### Part B: Servo Motor to ESP32

Connect your standard PWM servo (SG90, MG996R, etc.) to the ESP32:

| Servo Wire | ESP32 Pin | Details |
|---|---|---|
| **Signal** (Orange / Yellow) | **GPIO 18** | PWM control signal |
| **Power VCC** (Red) | **VIN** (or External 5V) | 5V power (ESP32 VIN pin when powered via USB) |
| **Ground** (Brown / Black) | **GND** | Must share common ground with ESP32 |

> [!TIP]
> If using a heavy-duty servo (e.g. MG996R metal gear), power the servo from an external 5V 2A power adapter, and connect the adapter's GND to the ESP32 GND.

---

### Part C: Electrode Placement on Forearm

For wrist / hand motion (grasp, open, relax):
1. **Clean Skin**: Wipe skin with rubbing alcohol or water to remove natural skin oils.
2. **Electrode 1 (Red / IN+)**: Center of the inner forearm muscle belly (flexor digitorum superficialis).
3. **Electrode 2 (Blue / IN-)**: 2–3 cm away along the same muscle fiber direction.
4. **Reference (Black / REF)**: On a bony prominence with zero muscle activity (elbow bone or wrist bone).

---

## Stage 2: Program the Microcontrollers

### Part A: Program the Raspberry Pi Pico W (EMG Acquisition)

#### Using MicroPython (via Thonny IDE) — Recommended:
1. Plug your Pico W into the PC via micro-USB.
2. Open **Thonny IDE**. Set the interpreter to **MicroPython (Raspberry Pi Pico)** (bottom right).
3. Open `d:\EMG\pico_w_sensor\main.py`:
   ```python
   import machine, utime
   emg_adc = machine.ADC(26) # GP26 (Pin 31)

   while True:
       val = emg_adc.read_u16() # 0 to 65535
       print(val)
       utime.sleep_us(1000)      # 1000 Hz sample rate
   ```
4. Click **File $\rightarrow$ Save as... $\rightarrow$ Raspberry Pi Pico** and name it **`main.py`**.
5. **IMPORTANT**: Close Thonny (or click the red Stop/Disconnect button) so Windows frees the COM port for the C++ pipeline.

---

### Part B: Program the ESP32 (Actuator Controller)

#### Using Arduino IDE:
1. Connect the ESP32 to your PC via USB.
2. Open `d:\EMG\esp32\esp32_emg_actuator\esp32_emg_actuator.ino`.
3. In Arduino Library Manager (`Ctrl+Shift+I`), search for and install **`ESP32Servo`** by Kevin Harrington.
4. Near line 24 of the sketch, set your Wi-Fi name and password:
   ```cpp
   const char* WIFI_SSID     = "YOUR_WIFI_NAME";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   ```
5. Select your ESP32 board and port in the **Tools** menu, then click **Upload**.
6. Open **Serial Monitor** at **115200 baud**. You will see:
   ```text
   ==================================================
      CONNECTED! ESP32 IP ADDRESS: 192.168.1.150
   ==================================================
   [UDP] Listening on port 8888
   ```
7. 👉 **Write down the IP address** (e.g. `192.168.1.150`).

---

## Stage 3: Find Your COM Port & Configure PC

1. **Find Pico W COM Port**:
   - In Windows, press `Win + X` $\rightarrow$ **Device Manager** $\rightarrow$ **Ports (COM & LPT)**.
   - Look for **USB Serial Device** (e.g. `COM4`).
2. **Edit `d:\EMG\pc\config.json`**:
   Open `d:\EMG\pc\config.json` and set your Pico's COM port and your ESP32's IP address:
   ```json
   {
       "emg": {
           "source": "serial",
           "serial_port": "COM4",
           "serial_baud_rate": 115200,
           "sample_rate_hz": 1000
       },
       "udp": {
           "esp32_ip": "192.168.1.150",
           "esp32_port": 8888
       }
   }
   ```

---

## Stage 4: Run the Complete System

Open two PowerShell windows:

### Terminal 1: Start the Web Dashboard
```powershell
python d:\EMG\dashboard\server.py
```
Open your browser to: **`http://localhost:8080`**

### Terminal 2: Start the C++ EMG Processing Pipeline
```powershell
$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
d:\EMG\pc\build\emg_control.exe d:\EMG\pc\config.json
```

---

## Stage 5: Live Operation & Muscle Gestures

| Your Muscle Action | EMG Signal Behavior | ML Classification | Servo Motion |
|---|---|---|---|
| **Relax Forearm** | Low amplitude baseline noise (< 0.2 RMS) | **`RELAX`** | **90°** (Neutral Safe Position) |
| **Make a Tight Fist** | Moderate sustained burst (0.2–0.5 RMS) | **`GRASP`** | **0°** (Closed Grip) |
| **Extend Fingers Out** | High frequency sharp burst (0.5–0.8 RMS) | **`OPEN`** | **180°** (Full Hand Open) |
| **Hard Squeeze** | Maximum voluntary contraction (> 0.8 RMS) | **`CLOSE`** | **0°** (Full Fist) |

---

## Stage 6: Safety Watchdog Verification

1. While the system is actively running, press `Ctrl+C` in Terminal 2 (killing the PC pipeline).
2. Look at the servo: within **500 milliseconds**, the ESP32 safety watchdog triggers and **automatically returns the servo to the 90° neutral safe state**.
3. Re-running `emg_control.exe` instantly restores control.
