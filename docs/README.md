# BaBot - 3-DOF Ball-Balancing Robot

BaBot is a ball-balancing robot built on a 3-DOF Stewart platform. This repository targets the ESP32-S3 hardware revision and is arranged so the firmware compiles directly in Arduino IDE by default.

## Arduino IDE Setup

1. Install Arduino IDE and the `esp32:esp32` board platform.
2. Open the sketch folder [`../firmware/`](../firmware/).
3. Open [`../firmware/firmware.ino`](../firmware/firmware.ino).
4. Select the appropriate ESP32-S3 board target.
5. Compile directly in the IDE.

## Optional Arduino CLI

The CLI remains available as a convenience path, but it is no longer the primary workflow.

```powershell
.\scripts\compile.ps1
```

The helper script is the recommended CLI path because it injects the include roots for the integrated third-party source tree.

You can also compile the sketch directly without the helper script if you pass the same flags:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32s3 --build-property "build.extra_flags=-I<repo>\\firmware\\src -I<repo>\\firmware\\src\\third_party\\ArduinoJson\\src -I<repo>\\firmware\\src\\third_party\\CD74HC4067\\src -I<repo>\\firmware\\src\\third_party\\Servo\\src -I<repo>\\firmware\\src\\third_party\\WebSockets\\src" .\firmware
```

## Firmware Structure

- [`../firmware/firmware.ino`](../firmware/firmware.ino) keeps the Arduino `setup()` and `loop()` entrypoints plus high-level hardware composition.
- [`../firmware/src/`](../firmware/src/) contains project modules for button input, control loop, PID autotune, startup, mode handlers, UI command handling, drivers, and shared runtime state.
- [`../firmware/src/third_party/`](../firmware/src/third_party/) contains vendored third-party Arduino dependencies used by the default sketch build.
- [`../scripts/`](../scripts/) contains optional helper scripts for compile and dependency refresh.
- [`../hardware/`](../hardware/) contains board CAD, fabrication exports, enclosure assets, and hardware reference material.

## Hardware Revision

The default hardware documented in this repository is the newer main-board revision with a single `74HC4067` mux (`U1`) feeding a `4x4` sensor grid with `16` total sensors.

| Signal | GPIO | Notes |
|---|---:|---|
| `SERVO_A` | `41` | Servo output A |
| `SERVO_B` | `2` | Servo output B |
| `SERVO_C` | `1` | Servo output C |
| `U1_S0` | `40` | Remaining mux select line |
| `U1_S1` | `39` | Remaining mux select line |
| `U1_S2` | `48` | Remaining mux select line |
| `U1_S3` | `47` | Remaining mux select line |
| `U1_SIG` | `9` | Remaining mux ADC signal |
| `U12_INCREMENT` | `12` | X9C104 increment |
| `U12_UP/DOWN` | `13` | X9C104 direction |
| `U12_CS` | `14` | X9C104 chip select |
| `IR_LED_CONTROL` | `45` | IR illumination control |

The firmware pin map in [`../firmware/src/BoardConfig.h`](../firmware/src/BoardConfig.h) now matches this single-mux hardware revision.

## Third-Party Sources

- The required third-party Arduino dependencies live under [`../firmware/src/third_party/`](../firmware/src/third_party/).
- To refresh them from Arduino Library Manager, run `.\scripts\vendor-libraries.ps1`.
- Pinned versions, upstream homes, and retained licenses are tracked in [`third-party-sources.md`](third-party-sources.md).

## Runtime Notes

- The active hardware uses a single `U1` mux bank across `16` sensor positions.
- The sensor grid is `1.8x` larger than the legacy board, so the firmware now scales the mirrored legacy centroid output to preserve the old control logic on the new mechanics.
- The Boot button on GPIO0 is used as the runtime app button with `INPUT_PULLUP`. The firmware ignores boot-held state until the button has first been released after startup, so Boot+Reset still remains available for download mode entry.

- The default digital potentiometer tap is `5`, and the ESP32 12-bit `kBallThreshold` baseline is `60.0`. Both are expected to need hardware recalibration.
- The main loop uses fast-path button handling, a 30 Hz control path, signed IR readings, transient capture impulses for fast ball entries, velocity-scaled PID stabilization, and a persisted servo calibration routine.
- The Web UI **PID Autotune** routine uses safe relay feedback with capture active. It measures the noise floor, uses hysteresis to reject optical/electrical jitter, estimates conservative Ku/Pu-based PD gains, and tests small I values only after capture/PD succeeds.
- The firmware starts a Wi-Fi access point with SSID `12345678` and password `12345678`, serves a live web control page over HTTP, and streams telemetry through WebSocket from a task pinned to core `0`.
- Debug output uses the regular ESP32 serial monitor path. Log records are queued from the control path and flushed from the Wi-Fi/web task so serial printing does not run inline in `loop()`.
- Startup is now split into a safe boot plus deferred hardware stages. The board brings up serial, shared state, Wi-Fi, and the web page first, then initializes actuator, digipot, and IR subsystems one at a time.
- If the previous boot reset during actuator or digipot startup, the next boot automatically skips that risky stage, enables a dummy backend where needed, and reports the degraded state on both serial and the web page instead of looping on reset.

## Web UI Access

1. Power the robot and wait for the ESP32-S3 access point to start.
2. Join the Wi-Fi network named `12345678` using password `12345678`.
3. Open a browser and go to `http://192.168.4.1/`.
4. If `192.168.4.1` does not load, check the connected network details on your phone or computer and use the access point gateway/router IP shown there.
5. Once the page loads, the browser connects back to the robot over WebSocket on port `81` automatically.
6. Check the startup banner and subsystem list at the top of the control panel. On a bench boot with missing hardware, the page should still load and explicitly report any dummy or degraded subsystems.

## Bring-Up Checklist

- Compile in Arduino IDE, or with `arduino-cli compile --fqbn esp32:esp32:esp32s3 .\firmware` plus the project include flags shown above.
- Confirm the remaining `U1` mux bank returns distinct ADC values across all `16` sensor positions.
- Verify the X9C104 only moves when the target tap changes.
- Check neutral platform height, assembly mode, calibration mode, and servo limits.
- Connect to the `12345678` AP, open the hosted page, and verify live ball graphics plus bidirectional variable updates.
- On a bench test with no servos attached, confirm the page still loads, serial logs continue, and the actuator subsystem reports degraded or disabled state instead of rebooting the board.
- Re-tune the digipot default tap and ball threshold on real hardware.

## Reference Documents

- [`code-reference.md`](code-reference.md) - firmware reference for the modular ESP32-S3 codebase
- [`reference/how-it-works.pdf`](reference/how-it-works.pdf) - system theory
- [`reference/manual.pdf`](reference/manual.pdf) - assembly and usage manual
- [`hardware-guide.md`](hardware-guide.md) - hardware asset map for boards, fabrication, and enclosure files
