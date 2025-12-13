# Aegis Sky: The Aura Perception Engine

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![Platform](https://img.shields.io/badge/platform-NVIDIA%20Jetson%20AGX%20Orin-green)
![Standard](https://img.shields.io/badge/std-C%2B%2B20-blue)
![CUDA](https://img.shields.io/badge/CUDA-12.x-76B900)
![License](https://img.shields.io/badge/license-Proprietary-red)

> **"In the era of asymmetric warfare, latency is the only metric that matters."**

**Aegis Sky** is the comprehensive software suite powering the next generation of autonomous Counter-UAS (C-UAS) defense systems. It is a monorepo containing both the flight-critical **Core Engine** and the physics-based **Simulator**.

Designed for the **NVIDIA Jetson AGX Orin**, Aegis Sky leverages a proprietary "Early Fusion" architecture to detect, track, and engage Class 1 & 2 drone threats with **<50ms end-to-end latency**.

---

## 🏗️ System Architecture

Aegis Sky is built on a high-performance **Service-Oriented Architecture (SOA)** with a strict separation between public contracts (Interfaces) and private logic (Plugins).

### The Data Pipeline
The system enforces a **Zero-Copy** data transport philosophy using pinned CUDA memory and lock-free ring buffers.

```mermaid
graph TD
    subgraph "Hardware Abstraction Layer"
        CAM[GigE Vision]
        RAD[4D Radar]
        PTU[Pan-Tilt Unit]
    end

    subgraph "Middleware (Zero-Copy Bus)"
        SHM[(Shared Memory RingBuffer)]
    end

    subgraph "Aegis Core Services"
        FUSION[Fusion Service<br/>(CUDA Projection)]
        PERC[Perception Service<br/>(xInfer Runtime)]
        TRACK[Tracking Service<br/>(Ext. Kalman Filter)]
        FIRE[Fire Control<br/>(Ballistics)]
    end

    CAM & RAD -->|DMA Write| SHM
    SHM -->|Ptr Read| FUSION
    FUSION -->|Fused Tensor| PERC
    PERC -->|Detections| TRACK
    TRACK -->|State Vector| FIRE
    FIRE -->|Slew Cmd| PTU
```

---

## 🧩 Repository Structure

This monorepo is organized to support simultaneous development of the flight software and the validation tools.

```text
aegis-sky/
├── core/                       # [PRODUCT] The Embedded Flight Software
│   ├── include/aegis/hal/      # Hardware Abstraction Contracts (ICamera, IRadar)
│   ├── src/services/fusion/    # The Aura Early-Fusion Engine
│   └── src/drivers/            # Dynamic Hardware Plugins (.so)
│
├── sim/                        # [TOOL] "The Matrix" HIL/SIL Simulator
│   ├── assets/                 # Scenarios and 3D Models
│   └── src/physics/            # Raytracing & RF Propagation Models
│
├── shared/                     # [COMMON] Shared Memory Layouts
│   └── include/aegis_ipc/      # Bridge definitions for Sim-to-Core comms
│
└── cmake/                      # Build Logic
    └── modules/FindxInfer.cmake # Linker logic for proprietary ML engine
```

---

## 🚀 Key Technical Features

### 1. The "Aura" Fusion Engine
Unlike legacy systems that fuse data *after* detection (Late Fusion), Aegis Sky projects raw 4D Radar Point Clouds (Range, Azimuth, Elevation, Doppler) directly onto the RGB image tensor using custom CUDA kernels. This allows the neural network to "see" speed and distance, enabling the detection of **radio-silent, camo-painted drones** that are invisible to single-sensor systems.

### 2. Powered by `xInfer`
Aegis Sky does not use standard, bloated runtimes. It is built on top of **[xInfer](https://github.com/your-org/xinfer)**, our internal high-performance inference engine.
*   **Custom Operators:** Fused Pre-processing and NMS kernels.
*   **INT8 Quantization:** Calibrated for max throughput on Jetson Orin DLA/GPU.

### 3. "The Bridge" Simulation
Validation is performed via **Hardware-in-the-Loop (HIL)** simulation. The `sim` module generates synthetic sensor data (including RF noise and lens distortion) and injects it directly into the `core` memory bus. The Flight Software cannot distinguish between the Simulator and real combat reality.

---

## 🛠️ Build Instructions

### Prerequisites
*   **NVIDIA JetPack 6.0+** (Orin) or Ubuntu 22.04 (x86_64 Dev)
*   **xInfer** & **xTorch** (Installed via internal registry)
*   **CMake 3.25+** & **Ninja**

### Building the Stack
You can build the Core, the Simulator, or both.

```bash
# 1. Configuration
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DAEGIS_PLATFORM=JETSON_ORIN \
    -DCMAKE_PREFIX_PATH="/opt/ignition/xinfer"

# 2. Build All Targets
cmake --build build --parallel $(nproc)

# 3. Install
sudo cmake --install build
```

---

## 🖥️ Usage

### Deployment Mode (Live Hardware)
Runs the Core with real hardware drivers loaded.
```bash
# Requires root for SCHED_FIFO priority
sudo aegis_core --config core/configs/deploy_live.yaml
```

### Simulation Mode (The Bridge)
Runs the Simulator and the Core in tandem, connected via Shared Memory.
```bash
# Terminal 1: The Matrix
./bin/aegis_sim --scenario sim/assets/scenarios/desert_swarm.json

# Terminal 2: The Core
# Loads 'driver_sim.so' instead of 'driver_gige.so'
./bin/aegis_core --config core/configs/deploy_sim.yaml
```

---

## 🛡️ License & Compliance

**Copyright © 2025 Aegis Sky, Inc.**

*   **Proprietary & Confidential:** This source code is the sole property of Aegis Sky, Inc. Unauthorized reproduction or distribution is strictly prohibited.
*   **Export Control:** This software is subject to US Export Control Regulations (EAR/ITAR). Transfer to foreign nationals may require a license.

---

**Maintained by the Aegis Sky Advanced Projects Group.**
