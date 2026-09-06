# Comprehensive System Upgrade: Proportional Control, In-Browser Calibration, 5-Finger Bionics, BLE & Temporal Deep Learning

Elevate the EMG Actuator Control platform into a medical/bionic robotics workstation with continuous proportional force control, 1-click in-browser calibration, 5-finger multi-servo actuation, dual-mode BLE/Wi-Fi wireless communication, and 1D-CNN temporal deep learning.

---

## Overview of System Enhancements

### 1. Proportional Force & Exponential Easing Servo Control
- Continuous muscle contraction intensity (0–100%) mapped directly from signal RMS amplitude.
- Dual-mode packet support: Protocol v1 (16 bytes) and extended Protocol v2 (24 bytes).
- Exponential smoothing (`alpha = 0.2`) on the ESP32 to eliminate sudden mechanical snaps and servo jitter.

### 2. In-Browser 1-Click Calibration Workstation
- Integrated calibration engine running directly in `dashboard/server.py` over WebSockets.
- On-screen visual countdown with gesture prompts (Relax, Grasp, Open, Close).
- Live muscle exertion meter and automated Deep MLP re-training with instant hot-reload of weights.

### 3. 5-Finger Multi-Servo Prosthetic Actuation
- Independent finger tendon control across 5 ESP32 PWM pins:
  - Thumb: GPIO 18
  - Index: GPIO 19
  - Middle: GPIO 21
  - Ring: GPIO 22
  - Pinky: GPIO 23
- Gracefully falls back to single-servo mode if only GPIO 18 is wired.

### 4. Dual-Mode BLE (Bluetooth Low Energy) Wearable Firmware
- Adds Nordic UART Service (NUS) BLE server on the ESP32.
- Enables battery-powered portable operation outdoors without requiring a local Wi-Fi router.

### 5. Temporal 1D-CNN Deep Learning Model
- End-to-end 1D Temporal Convolutional Network in `ml/train_cnn.py`.
- Operates directly on raw 256-sample time windows to learn temporal muscle kinetics.

### 6. Multi-Channel EMG Processing
- Supports up to 4 simultaneous electrode channels (Flexor, Extensor, Pronator, Biceps).
