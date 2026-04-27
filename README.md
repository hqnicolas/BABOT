# BaBot: Advanced 3-DOF Stewart- Gough Ball-Balancing Robot

An autonomous high-precision ball-balancing instrument predicated on a 3-DOF Stewart-Gough parallel kinematic mechanism. Powered by an ESP32-S3 microcontroller, the system integrates custom KiCad PCB hardware, a high-density 16-channel IR sensory array, and an asynchronous WebSocket-driven control interface for real-time telemetry and PID optimization.

This project represents a modern realization of parallel robotics, bridging classical control theory with contemporary high-frequency embedded systems.

https://github.com/user-attachments/assets/6e9986bd-6b06-4d86-9c46-4f5fcbdaf4ec

**License:** [Creative Commons Attribution-NonCommercial 4.0 International](LICENSE)

---

## Quick Start

### Prerequisites

- Arduino IDE with ESP32 board support
- ESP32-S3 development board or compatible target
- Physical BaBot hardware assembled

### Compilation

1. Open `firmware/firmware.ino` in Arduino IDE.
2. Select **ESP32S3 Dev Module** or your specific ESP32-S3 variant.
3. Click **Compile**.

On Windows, the helper script is also available:

```powershell
.\scripts\compile.ps1
```

### First Boot

1. Power on the robot.
2. Wait for the ESP32-S3 Wi-Fi access point.
3. Connect to Wi-Fi network `12345678` using password `12345678`.
4. Open `http://192.168.4.1/`.
5. The live control dashboard loads with telemetry and runtime controls.

---

## Features

- **Precision 3-DOF Stewart Platform** - A high-rigidity parallel kinematic architecture utilizing three independent actuator branches for precise control of posture and stability.
- **Async Web Control Ecosystem** - Low-latency dual-core dashboard featuring real-time state visualization, ball trajectory overlays, and holistic system diagnostics via WebSockets.
- **High-Density IR Sensory Matrix** - 16-channel infrared detection grid employing signed signal processing and individual multiplexer channel compensation.
- **Synchronized 30Hz Control Loop** - Deterministic real-time control orchestrated by a dedicated FreeRTOS sensor task, ensuring minimal phase lag between optical acquisition and mechanical actuation.
- **Machine Learning PID Autotune** - Autonomous gain discovery utilizing a Multi-Stage CMA-ES (Covariance Matrix Adaptation Evolution Strategy) search algorithm for noise-tolerant feedback optimization.
- **Mathematical Kinematics Solver** - Native C++ real-time implementation of Stewart-Gough inverse kinematics, eliminating the need for legacy lookup tables or offline pre-computation.
- **Robust Fail-Safe Architecture** - Multi-stage subsystem health monitoring with automated recovery protocols and persistent NVS calibration storage.

---

## Firmware Modules

| Module | Purpose |
|---|---|
| `firmware.ino` | Arduino `setup()`/`loop()` orchestration, hardware object composition, and shared runtime state |
| `ButtonInput.h` | Boot-button press decoding and lightweight status LED stubs |
| `ControlLoop.h` | PID, capture impulse, motion grace, telemetry publication, and closed-loop reset helpers |
| `PidAutotune.h` | Safe relay PID autotune state machine, metrics, gain estimation, start/cancel/restore |
| `StartupManager.h` | Bench-safe startup, subsystem health, degraded-mode policy, and deferred hardware bring-up |
| `ModeHandlers.h` | ON/OFF/test/calibration/assembly/position-test mode behavior |
| `UiCommandHandler.h` | Web UI command dispatch, runtime patching, persistence commands, and MUX compensation |
| `BoardConfig.h` | Pin definitions, mux layout, timing, PID defaults, and firmware constants |
| `RuntimeState` | Shared telemetry/config state, UI command queue, status models, and NVS runtime persistence |
| `PlatformControl` | Stewart-platform inverse kinematics and servo commands |
| `IRSensorArray` | Single-mux IR reading, signed signal processing, MUX compensation, ball detection, and centroid calculation |
| `X9C104Digipot` | Digital potentiometer control for IR LED brightness tuning |
| `WebUiServer` | HTTP server, WebSocket telemetry, embedded Web UI, and UI command parsing |
| `DebugLogger` | Async serial logging queue flushed from the Wi-Fi task |

---

## Hardware Pinout

ESP32-S3 control pins are defined in `firmware/src/BoardConfig.h`.

| Signal | GPIO | Notes |
|---|---:|---|
| `SERVO_A` | 41 | Servo output A |
| `SERVO_B` | 2 | Servo output B |
| `SERVO_C` | 1 | Servo output C |
| `IR_LED_CONTROL` | 45 | IR illumination control |
| `BOOT_BUTTON` | 0 | Runtime app button, active-low |
| `U12_CS` | 14 | X9C104 chip select |
| `U12_INCREMENT` | 12 | X9C104 increment |
| `U12_UP/DOWN` | 13 | X9C104 direction |
| `U1_S0` | 40 | CD74HC4067 select line |
| `U1_S1` | 39 | CD74HC4067 select line |
| `U1_S2` | 48 | CD74HC4067 select line |
| `U1_S3` | 47 | CD74HC4067 select line |
| `U1_SIG` | 9 | Multiplexed ADC input |

GPIO 19 and 20 are reserved for ESP32-S3 USB D-/D+.

---

## Operation Modes

### OFF

Platform leveled, no active balancing.

### ON

- No ball detected: platform idles level.
- Fast ball entry or edge approach: capture impulse tilts toward center with normal ON-mode authority up to `20 deg`.
- Ball detected and slowed: velocity-scaled PID keeps the ball centered.
- Recent ball loss: firmware briefly holds the last tilt command, then levels and resets closed-loop state.

### ASSEMBLY

Manual one-servo-at-a-time diagnostic mode for validating mechanical assembly and range of motion.

### CALIBRATION

Interactive servo offset calibration. Offsets are persisted to NVS when calibration completes.

### TEST / INDIVIDUAL_TEST / POSITION_TEST

Open-loop platform diagnostics and synthetic ball-position tests for bring-up. `POSITION_TEST` injects a circular synthetic ball position and runs the normal ON controller, so it exercises the current runtime PID, capture behavior, setpoint, and tilt limits.

---

## Web Dashboard

Once connected to the Wi-Fi AP:

1. **Control Panel** - Tilt visualization, ball position overlay, and mode selector.
2. **Telemetry** - Real-time sensor readings, servo positions, PID state, and system status.
3. **PID Autotune** - Safe relay feedback with measured noise floor, Ku/Pu estimation, capture + PD validation, and small I-value tests.
4. **Configuration and runtime** - Digipot tap, ball threshold, setpoint, PID gains, Save Runtime, Load Runtime, and Reset Defaults.
5. **Tests and calibration** - Servo tests, position tests, assembly/calibration modes, and individual MUX compensation.

All changes sync bidirectionally over WebSocket.

---

## PID Autotune

The **PID Autotune** drawer uses safe relay feedback while actively trying to keep the ball on the tray.

1. It waits for a ball entry inside the safe operating radius.
2. It briefly levels the platform and measures optical/electrical jitter as a noise floor.
3. It runs relay feedback on X and Y with hysteresis greater than the measured noise.
4. It aborts an individual relay attempt if radius, velocity, or detection loss indicate risk, then recovers and waits for a new safe entry.
5. It estimates conservative `Ku`/`Pu`, calculates PD gains, and tests capture + PD.
6. It evaluates only tiny I values: `0.000`, `0.005`, `0.010`, and `0.020`.

Autotune relay tilt is limited to `15 deg`. ON mode can still use the normal `20 deg` platform authority. If relay response remains too noisy or unsafe after repeated safety aborts, autotune fails safely, levels the platform, and restores the previous runtime PID/capture settings. Successful autotune applies values in RAM only; **Save Runtime** remains the only NVS persistence step.

---

## Key Runtime Constants

| Constant | Current value | Notes |
|---|---:|---|
| `kControlIntervalUs` | `33333` | 30 Hz Nominal control periodicity |
| `kProportionalGain` | `3.4` | Optimized legacy proportional gain |
| `kIntegralGain` | `0.06` | Optimized legacy integral gain |
| `kDerivativeGain` | `12.0` | Optimized legacy derivative gain |
| `kPlateHeightMm` | `115.0` | Design vertical height (Nominal) |
| `kMaxPlateTiltDeg` | `15.0` | Symmetrical mechanical safety authority |
| `kBallThreshold` | `30.0` | Signed IR detection floor |

---

## Bring-Up Checklist

- [ ] Compile firmware without errors.
- [ ] Flash to ESP32-S3 via USB.
- [ ] Connect to the `12345678` Wi-Fi AP and open `http://192.168.4.1/`.
- [ ] Confirm subsystems report ready or clear degraded-state messages.
- [ ] Test `ASSEMBLY` and `CALIBRATION` modes before balancing.
- [ ] Run individual MUX compensation after sensor or lighting changes.
- [ ] Tune digipot tap and ball threshold on real hardware.
- [ ] Test ON mode with slow and moderate ball entries.
- [ ] Run PID Autotune with safe ball entries and verify noise floor, relay crossings, Ku/Pu, confidence, and estimated P/D before saving runtime values.

---

## Third-Party Libraries

Vendored libraries live in `firmware/src/third_party/`:

- ArduinoJson - JSON serialization for web communication under `firmware/src/third_party/ArduinoJson/src`
- Servo - ESP32 servo PWM output
- WebSockets - Bidirectional WebSocket communication
- CD74HC4067 - 16-channel multiplexer control

For versions, upstream sources, and licenses, see [docs/third-party-sources.md](docs/third-party-sources.md).

---

## Documentation

| Document | Content |
|---|---|
| Document | Content |
|---|---|
| [docs/kinematics-theory.md](docs/kinematics-theory.md) | **Maturity Thesis**: Stewart-Gough derivation and system geometry |
| [docs/code-reference.md](docs/code-reference.md) | Firmware architecture, CMA-ES Autotune model, and module responsibilities |
| [docs/hardware-guide.md](docs/hardware-guide.md) | Mechanical assembly and PCB asset organization |
| [docs/autotune-tutorial.md](docs/autotune-tutorial.md) | Procedural guide for autonomous PID optimization |

---

## Safety Notes

- The robot starts in `OFF` mode on power-up.
- Always test on a stable, non-slip surface.
- Keep hands clear of linkages while servos are active.
- Do not use PID Autotune on a partially assembled or mechanically loose platform.
- If autotune reports relay response too noisy or unsafe, improve sensing/mechanics before applying gains.

---

## Contact & Support

https://www.ba-bot.com
https://www.ba-bot.com/manual/

#  License
This project is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.

#  Contact
For inquiries or support, please contact: info@ba-bot.com


---
# BABOT

The open-source ball-balancing robot you build, program, and bring to life.
