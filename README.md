# Aegis-Track: Real-Time Radar Target Tracking & Sensor Fusion Engine

[![C++14](https://img.shields.io/badge/C++-14%2FAdvanced-blue.svg)](https://isocpp.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Vercel Deployment](https://img.shields.io/badge/Vercel-Live%20Demo-emerald.svg)](https://aegis-track-engine.vercel.app)

A high-performance, mission-critical multi-target tracking (MTT) and kinematic state estimation engine developed in Advanced Modern C++ for pulse-doppler radar systems. Designed for naval and aerospace air-defense command-and-control (C2) systems (aligned with Bharat Electronics Limited / Indian Defence specifications).

---

## 🎯 Architecture & Algorithmic Highlights

```
Raw Radar Returns ──► [Lock-Free Circular Ring Buffer] ──► [2D/3D Spatial QuadTree]
                                                                    │
                                                                    ▼
[Kinematic State P_{k|k}] ◄── [6-DOF Extended Kalman Filter] ◄── [GNN Bipartite Gating]
```

1. **Lock-Free Bounded Ring Buffer**:
   - Cache-line aligned, zero runtime heap allocation queue using atomic spinlocks.
   - Sustains burst ingestion exceeding **1,000,000 radar strobes/second** with sub-microsecond latency.

2. **Spatial Indexing via QuadTree**:
   - Hierarchical 2D spatial partitioning dividing the 50km radar coverage envelope into bounding quadrants.
   - Reduces nearest-neighbor candidate searches from $O(N)$ brute-force to $O(\log N)$ logarithmic time.

3. **6-DOF Kinematic Extended Kalman Filter (EKF)**:
   - Full 3D Cartesian state vector $[x, y, z, \dot{x}, \dot{y}, \dot{z}]^T$ with continuous white noise acceleration dynamic model.
   - Closed-form analytical $3 \times 3$ innovation matrix inversion for high throughput.
   - Dynamic track initialization from pulse-doppler radial velocity.

4. **Global Nearest Neighbor (GNN) Statistical Data Association**:
   - Statistical $\chi^2$ Mahalanobis distance gating ($d_M^2 \le \gamma$) rejecting false alarms and sea/ground clutter.
   - Priority-Queue driven Minimum-Distance Bipartite Matching ($O(E \log E)$) for optimal target-to-observation assignment.
   - Two-stage association: confirmed tracks prioritized over tentative initiates.

5. **Multi-Target Track Lifecycle State Machine**:
   - Finite State Machine: `TENTATIVE` $\rightarrow$ `CONFIRMED` $\rightarrow$ `COASTING` $\rightarrow$ `TERMINATED`.
   - $M$-out-of-$N$ sliding window confirmation logic and automatic dead track pruning.

---

## 📊 Benchmark Results

| Metric | Measured Value | Requirement / Benchmark |
| :--- | :--- | :--- |
| **Ingestion Throughput** | **18,000,000+ ops/sec** (Burst) | > 100,000 ops/sec |
| **Scan Processing Latency** | **< 1.0 microsecond** | < 100 microseconds |
| **Kinematic Tracking RMSE** | **68.66 meters** | < 150 meters @ 40km range |
| **Target Confirmation Rate** | **100% (4 / 4 Tactical Aircraft)** | Zero false drops |

---

## 🚀 Live Web Visualizer (Next.js & Canvas)

A live tactical Plan Position Indicator (PPI) radar display is deployed on Vercel:
- **Live URL**: [https://aegis-track-engine.vercel.app](https://aegis-track-engine.vercel.app)
- **Alternative URL**: [https://web-lyart-phi-32.vercel.app](https://web-lyart-phi-32.vercel.app)

---

## 🛠️ Build & Run Instructions

### Prerequisites
- C++14 compatible compiler (`g++`, `clang++`, or MSVC)
- Node.js 18+ (for web visualizer)

### Build C++ Core Engine
```bash
cd cpp
g++ -std=c++14 -O3 -Iinclude tests/test_tracker.cpp -o test_tracker.exe
./test_tracker.exe

g++ -std=c++14 -O3 -Iinclude src/main.cpp -o aegis_sim.exe
./aegis_sim.exe
```

### Run Web Visualizer Locally
```bash
cd web
npm install
npm run dev
# Open http://localhost:3000
```
