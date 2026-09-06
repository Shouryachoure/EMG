# EMG-Based Real-Time Actuator Control System

A modular, multithreaded C++17 system that acquires EMG (electromyography) signals, processes them through a machine-learning pipeline, and sends classified motor commands to an ESP32 microcontroller over UDP.

## Architecture Overview

```mermaid
flowchart LR
    SENSOR["EMG Sensor"] --> ACQ["EMG Acquisition<br/>Thread 1"]
    ACQ -->|Ring Buffer| PROC["Signal Processing<br/>+ Feature Extraction<br/>Thread 2"]
    PROC -->|Feature Queue| ML["ML Inference<br/>Thread 3"]
    ML -->|Decision Queue| UDP["UDP Sender<br/>Thread 4"]
    UDP -->|Wi-Fi/UDP| ESP["ESP32"]
    ESP --> ACT["Actuator<br/>(Servo/Motor)"]
```

### Decision Mapping

| Class ID | Command | Description |
|----------|---------|-------------|
| 1 | RELAX | No action / safe state |
| 2 | GRASP | Close grip |
| 3 | OPEN | Open grip |
| 4 | CLOSE | Full close |

### Threading Model

| Thread | Responsibility | Input | Output |
|--------|---------------|-------|--------|
| Thread 1 | EMG Acquisition | Sensor (serial/mock) | Ring Buffer |
| Thread 2 | Signal Processing + Feature Extraction | Ring Buffer | Feature Queue |
| Thread 3 | ML Inference + Decision | Feature Queue | Decision Queue |
| Thread 4 | UDP Communication | Decision Queue | Network |

## Project Structure

```
emg-control/
├── pc/                  # PC-side C++17 application
│   ├── include/         # Header files
│   ├── src/             # Source files
│   ├── tests/           # Unit tests
│   ├── CMakeLists.txt   # Build system
│   └── config.json      # Runtime configuration
├── esp32/               # ESP32 firmware (PlatformIO)
│   ├── include/         # Header files
│   ├── src/             # Source files
│   └── platformio.ini   # PlatformIO config
├── docs/                # Documentation
│   ├── architecture.md  # System architecture
│   ├── protocol.md      # UDP protocol specification
│   ├── threading.md     # Threading model
│   └── testing.md       # Test strategy
└── README.md            # This file
```

## Building

### PC Application

**Prerequisites**: CMake 3.16+, C++17-capable compiler (MSVC 2019+, GCC 9+, Clang 10+)

```bash
cd pc
mkdir build && cd build
cmake ..
cmake --build .
```

### Running Unit Tests

```bash
cd pc/build
ctest --output-on-failure
```

### Running Hardware-Free ESP32 Simulator & Live Demo

You can test and observe the complete end-to-end real-time system without physical hardware:

**Terminal 1 (Simulated ESP32 Receiver & Servo):**
```bash
python tests/simulate_esp32.py 8888
```

**Terminal 2 (EMG Pipeline):**
```bash
# In pc/config.json, set "esp32_ip": "127.0.0.1", "esp32_port": 8888
./pc/build/emg_control.exe pc/config.json
```

### Automated End-to-End Integration Test

```bash
python tests/test_integration_e2e.py
```

### ESP32 Firmware (Hardware Deployment)

**Prerequisites**: [PlatformIO](https://platformio.org/)

```bash
cd esp32
pio run              # Compile
pio run -t upload    # Flash to ESP32
pio device monitor   # Serial monitor
```

## Configuration

Edit `pc/config.json` to configure:

- EMG acquisition parameters (sample rate, buffer size)
- Feature extraction settings (which features to compute)
- ML model parameters (confidence threshold)
- Decision mapping (class ID → command)
- UDP connection (ESP32 IP address, port)
- Safety timeouts

## Safety

This system implements conservative safety behavior:

- **Low ML confidence** → `Command::NONE` (no actuator action)
- **Sensor disconnect** → Pipeline drains, actuator stops
- **UDP timeout** → ESP32 enters safe state (actuator stops)
- **Invalid/stale packets** → Discarded, actuator unaffected
- **Queue overflow** → Oldest entries dropped (never blocks producer)

## License

Private project. All rights reserved.
