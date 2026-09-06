# System Architecture

## Overview

The EMG-Based Real-Time Actuator Control System is a multithreaded pipeline that transforms raw EMG sensor signals into discrete actuator commands, transmitted over UDP to an ESP32 microcontroller.

## High-Level Architecture

```mermaid
flowchart TD
    subgraph "EMG Sensor Hardware"
        SENSOR["EMG Sensor<br/>(Serial / USB)"]
    end

    subgraph PC["PC Application — C++17, 4 Threads"]
        subgraph T1["Thread 1: Acquisition"]
            EMG_READER["EMGReader<br/>(FakeEMGReader / SerialEMGReader)"]
        end

        RB[("Ring Buffer<br/>(SPSC, Lock-free, Bounded)")]

        subgraph T2["Thread 2: Processing"]
            SIG_PROC["SignalProcessor<br/>Bandpass filter, normalization"]
            FEAT["FeatureExtractor<br/>RMS, MAV, Var, WL, ZC, SSC"]
        end

        FQ[("Feature Queue<br/>(ThreadSafeQueue, Bounded)")]

        subgraph T3["Thread 3: ML + Decision"]
            ML["MLModel<br/>(MockMLModel / TFLite / ONNX)"]
            DEC["DecisionEngine<br/>Confidence threshold + mapping"]
        end

        DQ[("Decision Queue<br/>(ThreadSafeQueue, Bounded)")]

        subgraph T4["Thread 4: Communication"]
            UDP_SEND["UDPSender<br/>Structured binary packets"]
        end
    end

    subgraph NET["Network"]
        WIFI["UDP over Wi-Fi"]
    end

    subgraph ESP["ESP32 Firmware"]
        UDP_RX["UDPReceiver<br/>Packet validation"]
        WATCHDOG["SafetyWatchdog<br/>Timeout → safe state"]
        ACT_CTRL["ActuatorController<br/>(ServoActuator / MockActuator)"]
    end

    SENSOR -->|"raw samples"| EMG_READER
    EMG_READER -->|"push"| RB
    RB -->|"pop"| SIG_PROC
    SIG_PROC --> FEAT
    FEAT -->|"feature vector"| FQ
    FQ -->|"pop"| ML
    ML --> DEC
    DEC -->|"Command + confidence"| DQ
    DQ -->|"pop"| UDP_SEND
    UDP_SEND -->|"16-byte packet"| WIFI
    WIFI -->|"UDP"| UDP_RX
    UDP_RX --> ACT_CTRL
    ACT_CTRL --> WATCHDOG
```

## Component Descriptions

### PC-Side Components

| Component | Responsibility | Interface |
|-----------|---------------|-----------|
| **EMGReader** | Abstract interface for EMG data acquisition | `virtual std::vector<double> readSamples()` |
| **FakeEMGReader** | Generates synthetic EMG signals for testing | Implements `EMGReader` |
| **SerialEMGReader** | Reads from real serial-connected EMG sensor | Implements `EMGReader` (stub) |
| **RingBuffer\<T\>** | Lock-free SPSC bounded buffer for raw EMG data | `push()`, `pop()`, `tryPop()` |
| **SignalProcessor** | Bandpass filtering (20–450 Hz), DC removal, normalization | `process(raw_window) → filtered` |
| **FeatureExtractor** | Computes configurable EMG features | `extract(filtered) → feature_vector` |
| **MLModel** | Abstract interface for ML inference | `predict(features) → {class_id, confidence}` |
| **MockMLModel** | Deterministic classifier for testing | Implements `MLModel` |
| **DecisionEngine** | Maps ML output to commands with confidence gating | `decide(prediction) → Command` |
| **ThreadSafeQueue\<T\>** | Bounded, thread-safe queue with shutdown semantics | `push()`, `pop()`, `tryPop()`, `shutdown()` |
| **UDPSender** | Serializes and transmits command packets over UDP | `send(CommandPacket)` |
| **CommandPacket** | 16-byte binary packet with CRC-16 validation | `serialize()`, `deserialize()` |
| **Config** | JSON-based runtime configuration | Loads `config.json` |
| **Logger** | Thread-safe logging with levels and timestamps | `log(level, message)` |
| **Pipeline** | Top-level orchestrator — creates threads, manages lifecycle | `start()`, `stop()` |

### ESP32-Side Components

| Component | Responsibility |
|-----------|---------------|
| **WiFiManager** | Connects to Wi-Fi, handles reconnection |
| **UDPReceiver** | Receives and validates UDP command packets |
| **ActuatorController** | Abstract interface for actuator hardware |
| **ServoActuator** | PWM-based servo control |
| **MockActuator** | Serial-print mock for testing |
| **SafetyWatchdog** | Enters safe state if no valid command for N ms |

## Data Flow

```mermaid
sequenceDiagram
    participant S as EMG Sensor
    participant T1 as Thread 1<br/>(Acquisition)
    participant RB as Ring Buffer
    participant T2 as Thread 2<br/>(Processing)
    participant FQ as Feature Queue
    participant T3 as Thread 3<br/>(ML/Decision)
    participant DQ as Decision Queue
    participant T4 as Thread 4<br/>(UDP)
    participant E as ESP32
    participant A as Actuator

    loop Every sample (1 kHz)
        S->>T1: Raw EMG sample
        T1->>RB: push(sample)
    end

    loop Every window (e.g., 256 samples)
        RB->>T2: pop(window)
        T2->>T2: Filter + Extract features
        T2->>FQ: push(features)
    end

    loop Every feature vector
        FQ->>T3: pop(features)
        T3->>T3: ML predict → Decision
        T3->>DQ: push(decision)
    end

    loop Every decision
        DQ->>T4: pop(decision)
        T4->>E: UDP packet (16 bytes)
        E->>E: Validate packet
        E->>A: Execute command
    end
```

## Error Handling & Safety

```mermaid
flowchart TD
    A{ML Confidence ≥ threshold?}
    A -->|Yes| B[Map to Command]
    A -->|No| C["Command::NONE<br/>(safe, no action)"]

    D{Sensor connected?}
    D -->|No| E["Pipeline drains<br/>→ NONE"]

    F{UDP packet valid?}
    F -->|No| G["Discard packet<br/>Log warning"]
    F -->|Yes| H{Sequence fresh?}
    H -->|No| G
    H -->|Yes| I{Timestamp fresh?}
    I -->|No| G
    I -->|Yes| J[Execute command]

    K{Watchdog timeout?}
    K -->|Yes| L["SAFE STATE<br/>Stop actuator"]
```
