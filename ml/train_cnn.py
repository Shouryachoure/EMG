#!/usr/bin/env python3
"""
EMG Temporal 1D Convolutional Neural Network (1D-CNN) Trainer
=============================================================
Component 4: End-to-end temporal deep learning on raw EMG time-series windows.
Learns directly from raw 256-sample waveforms without manual feature extraction.

Architecture:
  Input (256 samples)
    -> Conv1D(4 filters, kernel=7, stride=2) + ReLU
    -> MaxPool1D(pool=2)
    -> Conv1D(8 filters, kernel=5, stride=2) + ReLU
    -> Global Average Pooling (8 features)
    -> Dense(4 classes: RELAX, GRASP, OPEN, CLOSE) + Softmax

Usage:
  python ml/train_cnn.py --synthetic --epochs 40
"""

import os
import math
import json
import random
import argparse

def generate_synthetic_raw_windows(gesture_id, count=60, window_size=256):
    """Generates raw 256-sample EMG waveform windows for testing."""
    windows = []
    params = {
        1: {"amp": 0.04, "f1": 25.0,  "f2": 50.0,  "noise": 0.02},  # RELAX
        2: {"amp": 0.60, "f1": 60.0,  "f2": 110.0, "noise": 0.08},  # GRASP
        3: {"amp": 0.40, "f1": 120.0, "f2": 220.0, "noise": 0.06},  # OPEN
        4: {"amp": 0.50, "f1": 90.0,  "f2": 160.0, "noise": 0.07},  # CLOSE
    }[gesture_id]

    dt = 1.0 / 1000.0
    for _ in range(count):
        window = []
        phase_offset = random.random() * 2.0 * math.pi
        for i in range(window_size):
            t = i * dt
            sig = (params["amp"] * 0.7 * math.sin(2.0 * math.pi * params["f1"] * t + phase_offset) +
                   params["amp"] * 0.3 * math.sin(2.0 * math.pi * params["f2"] * t + phase_offset) +
                   random.gauss(0, params["noise"]))
            window.append(max(-1.0, min(1.0, sig)))
        windows.append((window, gesture_id - 1))
    return windows

class Conv1DLayer:
    def __init__(self, in_channels, out_channels, kernel_size, stride=1):
        self.in_channels = in_channels
        self.out_channels = out_channels
        self.kernel_size = kernel_size
        self.stride = stride

        scale = math.sqrt(2.0 / (in_channels * kernel_size))
        self.weights = [[[random.gauss(0, scale) for _ in range(kernel_size)]
                         for _ in range(in_channels)] for _ in range(out_channels)]
        self.biases = [0.0] * out_channels

    def forward(self, x):
        # x shape: [in_channels, seq_len]
        self.last_x = x
        in_len = len(x[0])
        out_len = (in_len - self.kernel_size) // self.stride + 1
        out = []

        for oc in range(self.out_channels):
            channel_out = []
            for t in range(out_len):
                val = self.biases[oc]
                start = t * self.stride
                for ic in range(self.in_channels):
                    for k in range(self.kernel_size):
                        val += self.weights[oc][ic][k] * x[ic][start + k]
                # ReLU
                channel_out.append(max(0.0, val))
            out.append(channel_out)

        self.last_out = out
        return out

class TemporalEMGCNN:
    def __init__(self):
        # 1 input channel (raw signal) -> 4 filters (k=7, s=2) -> MaxPool(2) -> 8 filters (k=5, s=2) -> GAP(8) -> Dense(4)
        self.conv1 = Conv1DLayer(1, 4, kernel_size=7, stride=2)
        self.conv2 = Conv1DLayer(4, 8, kernel_size=5, stride=2)
        
        # Dense layer: 8 -> 4 classes
        scale = math.sqrt(2.0 / 8)
        self.dense_w = [[random.gauss(0, scale) for _ in range(8)] for _ in range(4)]
        self.dense_b = [0.0] * 4

    def maxpool(self, x, pool_size=2):
        # x: [channels, len]
        out = []
        for ch in x:
            ch_out = []
            for i in range(0, len(ch) - pool_size + 1, pool_size):
                ch_out.append(max(ch[i:i + pool_size]))
            out.append(ch_out)
        return out

    def forward(self, raw_window):
        # Input format: 1 channel of 256 samples
        x = [raw_window]

        # Conv1 + ReLU
        c1 = self.conv1.forward(x)

        # MaxPool
        p1 = self.maxpool(c1, 2)

        # Conv2 + ReLU
        c2 = self.conv2.forward(p1)

        # Global Average Pooling across time dimension -> 8 features
        gap = []
        for ch in c2:
            gap.append(sum(ch) / max(1, len(ch)))

        # Dense layer (8 -> 4) + Softmax
        logits = []
        for j in range(4):
            val = self.dense_b[j]
            for i in range(8):
                val += self.dense_w[j][i] * gap[i]
            logits.append(val)

        max_l = max(logits)
        exp_l = [math.exp(val - max_l) for val in logits]
        sum_exp = sum(exp_l)
        probs = [val / sum_exp for val in exp_l]
        return probs

    def train_epoch(self, dataset, lr=0.005):
        random.shuffle(dataset)
        total_loss = 0.0
        correct = 0

        for raw_window, target in dataset:
            probs = self.forward(raw_window)
            pred = probs.index(max(probs))
            if pred == target:
                correct += 1

            p_target = max(1e-12, probs[target])
            total_loss += -math.log(p_target)

            # Gradient step on dense layer
            delta = list(probs)
            delta[target] -= 1.0

            # Forward gap representation
            x = [raw_window]
            c1 = self.conv1.forward(x)
            p1 = self.maxpool(c1, 2)
            c2 = self.conv2.forward(p1)
            gap = [sum(ch) / max(1, len(ch)) for ch in c2]

            for j in range(4):
                self.dense_b[j] -= lr * delta[j]
                for i in range(8):
                    self.dense_w[j][i] -= lr * delta[j] * gap[i]

        acc = (correct / len(dataset)) * 100.0
        loss = total_loss / len(dataset)
        return loss, acc

    def export_json(self, filepath):
        data = {
            "model_name": "EMG_Temporal_1D_CNN",
            "architecture": "Conv1D(1->4, k=7, s=2) -> MaxPool(2) -> Conv1D(4->8, k=5, s=2) -> GAP(8) -> Dense(4)",
            "input_window_size": 256,
            "classes": {"1": "RELAX", "2": "GRASP", "3": "OPEN", "4": "CLOSE"},
            "conv1": {"weights": self.conv1.weights, "biases": self.conv1.biases},
            "conv2": {"weights": self.conv2.weights, "biases": self.conv2.biases},
            "dense": {"weights": self.dense_w, "biases": self.dense_b}
        }
        os.makedirs(os.path.dirname(filepath), exist_ok=True)
        with open(filepath, "w") as f:
            json.dump(data, f, indent=2)
        print(f"[OK] 1D-CNN Model exported to: {filepath}")

def main():
    parser = argparse.ArgumentParser(description="EMG 1D-CNN Temporal Model Trainer")
    parser.add_argument("--synthetic", action="store_true", default=True, help="Use synthetic raw EMG waves")
    parser.add_argument("--epochs", type=int, default=35, help="Training epochs")
    parser.add_argument("--out", type=str, default="models/emg_cnn_weights.json", help="Output path")
    args = parser.parse_args()

    print("==================================================")
    print("   EMG Temporal 1D-CNN Deep Learning Suite        ")
    print("==================================================")
    print("\n[1/3] Generating 256-sample raw time-series windows for 4 gestures...")

    dataset = []
    for gid in range(1, 5):
        dataset.extend(generate_synthetic_raw_windows(gid, count=75))

    random.seed(42)
    random.shuffle(dataset)
    split = int(len(dataset) * 0.8)
    train_data = dataset[:split]
    test_data  = dataset[split:]

    print(f"\n[2/3] Training 1D-CNN on raw waveforms (Train: {len(train_data)}, Test: {len(test_data)})...")
    cnn = TemporalEMGCNN()

    for ep in range(1, args.epochs + 1):
        loss, acc = cnn.train_epoch(train_data, lr=0.008)
        if ep % 7 == 0 or ep == args.epochs:
            print(f"  Epoch {ep:2d}/{args.epochs} | Loss: {loss:.4f} | Train Acc: {acc:.1f}%")

    # Evaluation
    correct = 0
    confusion = [[0] * 4 for _ in range(4)]
    for raw_w, target in test_data:
        probs = cnn.forward(raw_w)
        pred = probs.index(max(probs))
        confusion[target][pred] += 1
        if pred == target:
            correct += 1

    test_acc = (correct / len(test_data)) * 100.0
    print(f"\n[3/3] Final Test Accuracy: {test_acc:.2f}%")
    print("  Confusion Matrix:")
    print("  True \\ Pred | RELAX  GRASP   OPEN  CLOSE")
    print("  ------------+----------------------------")
    names = ["RELAX", "GRASP", "OPEN ", "CLOSE"]
    for i in range(4):
        print(f"  {names[i]}     | " + "  ".join(f"{confusion[i][j]:5d}" for j in range(4)))

    cnn.export_json(args.out)
    print("==================================================")

if __name__ == "__main__":
    main()
