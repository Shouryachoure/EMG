#!/usr/bin/env python3
"""
EMG Muscle Gesture Classifier - Training & Recording Suite
===========================================================
Option 2: Records arm EMG data (via Serial from Pico W/Arduino/ESP32 or Synthetic),
extracts the 6 time-domain features, trains a Deep Multi-Layer Perceptron (MLP),
and exports the weights to models/emg_mlp_weights.json for native C++ inference.

Architecture:
  Input (6 features) -> Dense(16, ReLU) -> Dense(12, ReLU) -> Dense(4, Softmax)

Usage:
  # Quick training with synthetic EMG data (works immediately without hardware):
  python ml/train_my_arm.py --synthetic --epochs 80

  # Train on your actual arm via Raspberry Pi Pico W / Arduino Serial:
  python ml/train_my_arm.py --port COM3 --baud 115200 --duration 6
"""

import sys
import os
import math
import time
import json
import random
import argparse

# ==============================================================================
# Feature Extraction (matches pc/src/feature_extractor.cpp exactly)
# ==============================================================================

def extract_features(window, deadzone=0.01):
    """
    Computes 6 time-domain EMG features from a sample window.
    Features: [RMS, MAV, Variance, Waveform Length, Zero Crossings, Slope Sign Changes]
    """
    n = len(window)
    if n == 0:
        return [0.0] * 6

    # 1. RMS & MAV & Mean
    sum_sq = 0.0
    sum_abs = 0.0
    sum_val = 0.0
    for x in window:
        sum_sq += x * x
        sum_abs += abs(x)
        sum_val += x
    
    rms = math.sqrt(sum_sq / n)
    mav = sum_abs / n
    mean = sum_val / n

    # 2. Variance
    sum_var = sum((x - mean) ** 2 for x in window)
    variance = sum_var / n

    # 3. Waveform Length (WL)
    wl = sum(abs(window[i] - window[i - 1]) for i in range(1, n))

    # 4. Zero Crossings (ZC)
    zc = 0
    for i in range(1, n):
        diff = abs(window[i] - window[i - 1])
        if ((window[i] > 0 and window[i - 1] < 0) or (window[i] < 0 and window[i - 1] > 0)) and diff >= deadzone:
            zc += 1

    # 5. Slope Sign Changes (SSC)
    ssc = 0
    for i in range(1, n - 1):
        d1 = window[i] - window[i - 1]
        d2 = window[i] - window[i + 1]
        if (d1 * d2) > 0 and (abs(d1) >= deadzone or abs(d2) >= deadzone):
            ssc += 1

    return [rms, mav, variance, wl, float(zc), float(ssc)]

# ==============================================================================
# Data Generation & Acquisition
# ==============================================================================

GESTURES = [
    (1, "RELAX", "Rest arm completely relaxed on table"),
    (2, "GRASP", "Form a tight fist (flex flexor carpi muscles)"),
    (3, "OPEN",  "Spread fingers wide open (extensor muscles)"),
    (4, "CLOSE", "Pinch thumb and index together firmly")
]

def generate_synthetic_emg(gesture_id, duration_sec=5.0, sample_rate=1000, window_size=256, hop_size=128):
    """
    Generates realistic biological EMG patterns for testing without hardware.
    """
    num_samples = int(duration_sec * sample_rate)
    samples = []

    # Characteristic amplitude & frequency per gesture
    params = {
        1: {"amp": 0.03, "muap_freq": 20.0,  "noise": 0.015},  # RELAX: background noise
        2: {"amp": 0.55, "muap_freq": 90.0,  "noise": 0.08},   # GRASP: high amplitude flex
        3: {"amp": 0.35, "muap_freq": 140.0, "noise": 0.06},   # OPEN: rapid extensor activity
        4: {"amp": 0.42, "muap_freq": 110.0, "noise": 0.07},   # CLOSE: localized pinch
    }[gesture_id]

    dt = 1.0 / sample_rate
    for i in range(num_samples):
        t = i * dt
        # Superposition of motor unit action potential (MUAP) bursts + Gaussian noise
        s = (params["amp"] * math.sin(2 * math.pi * params["muap_freq"] * t) *
             (0.8 + 0.4 * math.sin(2 * math.pi * 3.5 * t)) +
             random.gauss(0, params["noise"]))
        # Clip to [-1.0, 1.0]
        samples.append(max(-1.0, min(1.0, s)))

    # Slice into windows and extract features
    dataset = []
    for start in range(0, len(samples) - window_size, hop_size):
        w = samples[start:start + window_size]
        feat = extract_features(w)
        dataset.append((feat, gesture_id - 1)) # 0-indexed for network
    return dataset

def record_from_serial(port, baud, duration_sec=5.0, sample_rate=1000, window_size=256, hop_size=128):
    """
    Reads live ADC samples from a serial port (Pico W / Arduino).
    """
    try:
        import serial
    except ImportError:
        print("[ERROR] 'pyserial' is required for live recording. Run: pip install pyserial")
        print("Falling back to synthetic data for this run.")
        return None

    try:
        ser = serial.Serial(port, baud, timeout=1.0)
        time.sleep(1.5)  # Allow Arduino/Pico reboot
    except Exception as e:
        print(f"[ERROR] Failed to open serial port {port}: {e}")
        return None

    print(f"  Recording for {duration_sec} seconds... Hold gesture!")
    samples = []
    start_time = time.time()
    while time.time() - start_time < duration_sec:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if not line:
            continue
        try:
            val = float(line)
            # Normalize depending on ADC range
            if val > 1024:
                val = (val / 32768.0) - 1.0  # 16-bit
            elif val > 1.0:
                val = (val / 512.0) - 1.0    # 10-bit
            samples.append(val)
        except ValueError:
            pass

    ser.close()
    print(f"  Captured {len(samples)} samples.")
    if len(samples) < window_size:
        print("  [WARN] Not enough samples captured!")
        return []

    dataset = []
    for start in range(0, len(samples) - window_size, hop_size):
        w = samples[start:start + window_size]
        dataset.append((extract_features(w), 0))
    return dataset

# ==============================================================================
# Pure Python Deep Neural Network (MLP 6 -> 16 -> 12 -> 4)
# ==============================================================================

class DenseLayer:
    def __init__(self, in_features, out_features, activation="relu"):
        self.in_features = in_features
        self.out_features = out_features
        self.activation = activation
        
        # He initialization for ReLU, Xavier for Softmax/Linear
        scale = math.sqrt(2.0 / in_features) if activation == "relu" else math.sqrt(1.0 / in_features)
        self.weights = [[random.gauss(0, scale) for _ in range(in_features)] for _ in range(out_features)]
        self.biases = [0.0] * out_features

        # Gradients
        self.grad_w = [[0.0] * in_features for _ in range(out_features)]
        self.grad_b = [0.0] * out_features

        # Adam optimizer state
        self.m_w = [[0.0] * in_features for _ in range(out_features)]
        self.v_w = [[0.0] * in_features for _ in range(out_features)]
        self.m_b = [0.0] * out_features
        self.v_b = [0.0] * out_features

    def forward(self, x):
        self.last_input = x
        self.z = []
        for j in range(self.out_features):
            val = self.biases[j]
            for i in range(self.in_features):
                val += self.weights[j][i] * x[i]
            self.z.append(val)

        if self.activation == "relu":
            self.a = [max(0.0, z_val) for z_val in self.z]
        elif self.activation == "softmax":
            max_z = max(self.z)
            exp_z = [math.exp(val - max_z) for val in self.z]
            sum_exp = sum(exp_z)
            self.a = [val / sum_exp for val in exp_z]
        else:
            self.a = list(self.z)
        return self.a

    def backward(self, delta):
        # delta is dL/dz
        # Compute grad w and grad b
        for j in range(self.out_features):
            self.grad_b[j] += delta[j]
            for i in range(self.in_features):
                self.grad_w[j][i] += delta[j] * self.last_input[i]

        # Compute dL/dx for previous layer
        dx = [0.0] * self.in_features
        for i in range(self.in_features):
            val = 0.0
            for j in range(self.out_features):
                val += self.weights[j][i] * delta[j]
            dx[i] = val
        return dx

    def update_adam(self, lr=0.005, beta1=0.9, beta2=0.999, eps=1e-8, t=1):
        for j in range(self.out_features):
            # Biases
            self.m_b[j] = beta1 * self.m_b[j] + (1.0 - beta1) * self.grad_b[j]
            self.v_b[j] = beta2 * self.v_b[j] + (1.0 - beta2) * (self.grad_b[j] ** 2)
            m_hat = self.m_b[j] / (1.0 - beta1 ** t)
            v_hat = self.v_b[j] / (1.0 - beta2 ** t)
            self.biases[j] -= lr * m_hat / (math.sqrt(v_hat) + eps)
            self.grad_b[j] = 0.0

            # Weights
            for i in range(self.in_features):
                self.m_w[j][i] = beta1 * self.m_w[j][i] + (1.0 - beta1) * self.grad_w[j][i]
                self.v_w[j][i] = beta2 * self.v_w[j][i] + (1.0 - beta2) * (self.grad_w[j][i] ** 2)
                m_hat_w = self.m_w[j][i] / (1.0 - beta1 ** t)
                v_hat_w = self.v_w[j][i] / (1.0 - beta2 ** t)
                self.weights[j][i] -= lr * m_hat_w / (math.sqrt(v_hat_w) + eps)
                self.grad_w[j][i] = 0.0


class EMGNeuralNetwork:
    def __init__(self):
        # 6 inputs -> 16 -> 12 -> 4 classes
        self.layer1 = DenseLayer(6, 16, activation="relu")
        self.layer2 = DenseLayer(16, 12, activation="relu")
        self.layer3 = DenseLayer(12, 4, activation="softmax")
        self.feature_mean = [0.0] * 6
        self.feature_std  = [1.0] * 6

    def compute_normalization(self, X):
        n = len(X)
        if n == 0:
            return
        self.feature_mean = [sum(x[i] for x in X) / n for i in range(6)]
        self.feature_std = []
        for i in range(6):
            variance = sum((x[i] - self.feature_mean[i]) ** 2 for x in X) / n
            std = math.sqrt(variance)
            self.feature_std.append(std if std > 1e-6 else 1.0)

    def normalize(self, x):
        return [(x[i] - self.feature_mean[i]) / self.feature_std[i] for i in range(6)]

    def forward(self, x_raw):
        x = self.normalize(x_raw)
        h1 = self.layer1.forward(x)
        h2 = self.layer2.forward(h1)
        out = self.layer3.forward(h2)
        return out

    def train(self, X_train, y_train, epochs=80, lr=0.005, batch_size=16):
        self.compute_normalization(X_train)
        n = len(X_train)
        indices = list(range(n))
        step = 0

        for epoch in range(1, epochs + 1):
            random.shuffle(indices)
            total_loss = 0.0
            correct = 0

            for i in indices:
                step += 1
                x_norm = self.normalize(X_train[i])
                target = y_train[i]

                # Forward
                h1 = self.layer1.forward(x_norm)
                h2 = self.layer2.forward(h1)
                probs = self.layer3.forward(h2)

                # Cross-entropy loss: -log(probs[target])
                p_target = max(1e-12, probs[target])
                total_loss += -math.log(p_target)

                # Prediction check
                pred = probs.index(max(probs))
                if pred == target:
                    correct += 1

                # Backward: Softmax + Cross-Entropy gradient is simply (probs - y_one_hot)
                delta3 = list(probs)
                delta3[target] -= 1.0

                dh2 = self.layer3.backward(delta3)
                delta2 = [dh2[k] if self.layer2.z[k] > 0 else 0.0 for k in range(12)]

                dh1 = self.layer2.backward(delta2)
                delta1 = [dh1[k] if self.layer1.z[k] > 0 else 0.0 for k in range(16)]

                self.layer1.backward(delta1)

                # Mini-batch update
                if step % batch_size == 0 or i == indices[-1]:
                    self.layer1.update_adam(lr=lr, t=step)
                    self.layer2.update_adam(lr=lr, t=step)
                    self.layer3.update_adam(lr=lr, t=step)

            if epoch % 10 == 0 or epoch == epochs:
                acc = (correct / n) * 100.0
                avg_loss = total_loss / n
                print(f"  Epoch {epoch:3d}/{epochs} | Loss: {avg_loss:.4f} | Train Accuracy: {acc:5.1f}%")

    def evaluate(self, X_test, y_test):
        correct = 0
        confusion = [[0] * 4 for _ in range(4)]
        for x, target in zip(X_test, y_test):
            probs = self.forward(x)
            pred = probs.index(max(probs))
            confusion[target][pred] += 1
            if pred == target:
                correct += 1
        acc = (correct / len(X_test)) * 100.0 if X_test else 0.0
        return acc, confusion

    def export_json(self, filepath):
        """
        Exports the trained network weights and feature scaling to JSON for C++.
        """
        data = {
            "model_name": "EMG_Deep_MLP",
            "version": "1.0",
            "architecture": [6, 16, 12, 4],
            "feature_names": ["RMS", "MAV", "VAR", "WL", "ZC", "SSC"],
            "classes": {
                "1": "RELAX",
                "2": "GRASP",
                "3": "OPEN",
                "4": "CLOSE"
            },
            "feature_mean": self.feature_mean,
            "feature_std": self.feature_std,
            "layers": [
                {
                    "name": "layer1",
                    "in_features": self.layer1.in_features,
                    "out_features": self.layer1.out_features,
                    "weights": self.layer1.weights,
                    "biases": self.layer1.biases
                },
                {
                    "name": "layer2",
                    "in_features": self.layer2.in_features,
                    "out_features": self.layer2.out_features,
                    "weights": self.layer2.weights,
                    "biases": self.layer2.biases
                },
                {
                    "name": "layer3",
                    "in_features": self.layer3.in_features,
                    "out_features": self.layer3.out_features,
                    "weights": self.layer3.weights,
                    "biases": self.layer3.biases
                }
            ]
        }
        os.makedirs(os.path.dirname(filepath), exist_ok=True)
        with open(filepath, "w") as f:
            json.dump(data, f, indent=2)
        print(f"\n[OK] Model exported successfully to: {filepath}")

# ==============================================================================
# Main Interactive Flow
# ==============================================================================

def main():
    parser = argparse.ArgumentParser(description="EMG Muscle Gesture Classifier Trainer")
    parser.add_argument("--synthetic", action="store_true", help="Generate synthetic EMG dataset without hardware")
    parser.add_argument("--port", type=str, default=None, help="Serial port for live recording (e.g. COM3 or /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    parser.add_argument("--duration", type=float, default=6.0, help="Recording duration per gesture in seconds")
    parser.add_argument("--epochs", type=int, default=80, help="Number of training epochs")
    parser.add_argument("--lr", type=float, default=0.008, help="Learning rate")
    parser.add_argument("--out", type=str, default="models/emg_mlp_weights.json", help="Output JSON weights path")
    args = parser.parse_args()

    print("=" * 65)
    print("      EMG DEEP NEURAL NETWORK (MLP) TRAINING SUITE")
    print("=" * 65)

    all_data = []

    if args.synthetic or args.port is None:
        if args.port is None and not args.synthetic:
            print("[INFO] No --port specified. Defaulting to high-fidelity synthetic mode.")
            print("       (To record your physical arm, pass --port COM3)")
        print("\n[1/3] Generating synthetic EMG training dataset for 4 gestures...")
        for gid, name, desc in GESTURES:
            print(f"  Simulating: Class {gid} ({name}) - {desc}")
            ds = generate_synthetic_emg(gid, duration_sec=args.duration)
            all_data.extend(ds)
    else:
        print(f"\n[1/3] Interactive Arm Recording Mode on port {args.port}...")
        for gid, name, desc in GESTURES:
            print("\n" + "-" * 55)
            print(f"  GET READY FOR GESTURE {gid}: {name}")
            print(f"  Instruction: {desc}")
            print("-" * 55)
            for countdown in [3, 2, 1]:
                print(f"  Starting in {countdown}...", end="\r")
                time.sleep(1.0)
            print("  >>> RECORDING NOW! HOLD GESTURE! <<<       ")
            recorded = record_from_serial(args.port, args.baud, duration_sec=args.duration)
            if not recorded:
                print("  [WARN] Using synthetic backup for this gesture.")
                recorded = generate_synthetic_emg(gid, duration_sec=args.duration)
            for feat, _ in recorded:
                all_data.append((feat, gid - 1))

    # Split Train / Test (80 / 20)
    random.seed(42)
    random.shuffle(all_data)
    split_idx = int(len(all_data) * 0.8)
    train_set = all_data[:split_idx]
    test_set  = all_data[split_idx:]

    X_train = [d[0] for d in train_set]
    y_train = [d[1] for d in train_set]
    X_test  = [d[0] for d in test_set]
    y_test  = [d[1] for d in test_set]

    print(f"\n[2/3] Training Deep Neural Network (MLP: 6 -> 16 -> 12 -> 4)...")
    print(f"  Dataset size: {len(all_data)} feature windows (Train: {len(train_set)}, Test: {len(test_set)})")

    nn = EMGNeuralNetwork()
    nn.train(X_train, y_train, epochs=args.epochs, lr=args.lr)

    # Evaluation
    test_acc, confusion = nn.evaluate(X_test, y_test)
    print(f"\n[3/3] Evaluation Results:")
    print(f"  Final Test Accuracy: {test_acc:.2f}%\n")
    print("  Confusion Matrix:")
    print("  True \\ Pred | RELAX  GRASP   OPEN  CLOSE")
    print("  ------------+----------------------------")
    names = ["RELAX", "GRASP", "OPEN ", "CLOSE"]
    for i in range(4):
        row = "  " + names[i] + "     | " + "  ".join(f"{confusion[i][j]:5d}" for j in range(4))
        print(row)

    # Export
    nn.export_json(args.out)
    print("\nNext step: Run 'pc/build/emg_control.exe' to execute live native C++ inference!")
    print("=" * 65)

if __name__ == "__main__":
    main()
