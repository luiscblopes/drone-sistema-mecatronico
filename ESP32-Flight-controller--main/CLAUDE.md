# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

An Arduino/C++ firmware project for a quadcopter flight controller running on an **ESP32**. The drone flies in **angle (self-leveling) mode** using an **MPU6050** IMU over I2C, reads a 6-channel RC receiver via PWM interrupts, and drives 4 ESCs/motors. There is no host-side application, build script, or test framework — every `.ino` is a standalone Arduino sketch flashed to an ESP32.

## Build / flash / run

There are no Makefiles or CI. Sketches are compiled and uploaded with the **Arduino IDE** (or `arduino-cli`) targeting an ESP32 board. Serial monitor runs at **115200 baud**.

Required libraries (install via Library Manager / `arduino-cli lib install`):
- `ESP32Servo` — ESC PWM via the `Servo` class (the main FC depends only on this + the bundled `Wire`)
- For the web-tuning sketch only: `ESPAsyncWebServer`, `AsyncTCP`, and the ESP32 `SPIFFS` filesystem

Each sketch must live in a folder whose name matches the `.ino` (Arduino requirement) — already the case here. Flash one sketch at a time; they are mutually exclusive programs, not modules.

The web-tuning sketch additionally needs HTML/CSS assets uploaded to the ESP32 **SPIFFS** partition (Arduino IDE "ESP32 Sketch Data Upload" tool) and Wi-Fi credentials set in the `ssid`/`password` constants near the top of the file.

## Bring-up order (do not skip — props can injure)

The intended workflow, in order. Most steps map to a sketch under `test/`:
1. `test/reciever_pwm_esp32/` — verify RC receiver channels.
2. `test/measure_angles_from_mpu/` (and `test/complementry_filter_webserver/`) — verify IMU angle output.
3. `test/motor_calibration_esp32/` — calibrate ESCs, confirm motor spin direction matches the hand-drawn diagram in `resources/`.
4. `src/Gyro_accelerometer_calibration.ino` — print IMU offsets with the quad level.
5. **Copy those printed offsets into `setup()` of `src/Anglemode_flightcontroller_ver3.1.ino`** (the `RateCalibration*` / `Acc*Calibration` assignments) before flashing the main FC.
6. `test/anglemode_flightcontroller_ver3.1_PID_values_tuning_webserver/` — tune PID gains live over Wi-Fi.

`src/Anglemode_flightcontroller_ver3.1.ino` is the **production flight code**. The `test/` sketches are diagnostics and the tuning variant — keep changes to control logic in sync between the main FC and the web-tuning sketch, since they share the same control pipeline.

## Control architecture (the part that needs reading several files)

The whole controller is a single fixed-rate loop with **no scheduler/RTOS task** — timing is enforced by busy-waiting at the end of `loop()`:

```
while (micros() - LoopTimer < t*1e6);   // t = 0.004 s  → 250 Hz loop
```

Pipeline each cycle:
1. **IMU read** — raw I2C burst from MPU6050 (addr `0x68`); gyro scaled `/65.5` (±500 °/s), accel `/4096` (±8 g). In the main FC the read is **inlined in `loop()`**, not via the `gyro_signals()` helper (that helper still exists but is unused on the hot path). Calibration offsets are then subtracted.
2. **Attitude estimate** — accelerometer roll/pitch via `atan`, then fused with gyro rate through a **complementary filter** (`0.991`/`0.009`). A 1-D **Kalman filter** (`kalman_1d`, and an inlined version) is present but **commented out** — complementary is the active estimator. Estimated angles are clamped to **±20°**.
3. **Cascaded PID** — outer **angle** loop (setpoint from RC sticks `0.1*(ch-1500)`) produces a desired **rate**, which feeds the inner **rate** loop. Roll/pitch/yaw each run the same trapezoidal-integral PID (`pid_equation`, but again **inlined** in the main loop). Integral term and total output are both clamped to **±400**.
4. **Motor mixing** — `MotorInput1..4 = Throttle ∓ Roll ∓ Pitch ∓ Yaw` (see the X-quad sign table at the mixer; motor pins 13/12/14/27). Outputs clamped to `[ThrottleIdle=1170, 1999]`.
5. **Arming/failsafe** — throttle channel `ReceiverValue[2] < 1030` forces all motors to `ThrottleCutOff=1000` and **zeroes all PID integrator/previous-error state** (anti-windup on disarm). Throttle is also capped at 1800 to preserve control authority.

### Shared conventions across all sketches
- **RC input**: one ISR `channelInterruptHandler` on `CHANGE` for all 6 channels measures pulse width into `volatile int ReceiverValue[6]` (1000–2000 µs). Pins: ch1–6 = `34,35,32,33,25,26`.
- **ESCs**: `ESP32Servo` with `attach(pin,1000,2000)` and `setPeriodHertz(ESCfreq=500)`; commanded with `writeMicroseconds`.
- **State** touched in both ISR and loop is `volatile`. Tuning constants (`P*Rate*`, `*Angle*`, `ESCfreq`, `t`) are globals at the top of each sketch — editing gains means editing those literals (or, for the web sketch, the SPIFFS `*.txt` files written by `/get` handlers and read at boot).
- **MPU6050 register writes** configure DLPF/full-scale every read in some sketches; addresses `0x1A,0x1B,0x1C,0x3B,0x43,0x6B` recur — treat them as the fixed device protocol, not magic numbers to "clean up."

## Editing guidance specific to this repo
- Loop time `t` is hard-coupled to the busy-wait, the PID integral/derivative terms, the complementary-filter weights, and the Kalman variance constants. Changing the loop rate means revisiting all of them, not just `t`.
- Calibration offsets in the main FC are **hardcoded in `setup()`** and are board/IMU-specific; never copy one unit's values to another.
- `resources/` holds the schematic, hand-drawn wiring diagram, BOM, and Gerber files — consult the hand-drawn diagram for motor/prop direction and pin mapping before changing pins or the mixer.
