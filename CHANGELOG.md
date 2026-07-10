# Changelog

## v2.0.0 — 2026-07-10

### Added

- C8T6-safe Flash parameter journal with CRC32 and append-only wear reduction.
- Runtime PID, speed, ramp, line, odometry, battery, and stall configuration.
- Acceleration ramp, encoder odometry, MOVE/TURN commands, and an 8-step route queue.
- Motor-stall latch, reset-cause reporting, fault history, and FreeRTOS diagnostics.
- OLED odometry page, comprehensive hardware/software documentation, pin map, and validation scripts.

### Validation

- Built for STM32F103C8T6 and STM32F103RCT6 with Arm GNU Toolchain 14.2.1.
- Host PID/protocol tests, project consistency checks, and prebuilt-artifact checksums passed.
- Hardware-in-the-loop validation remains required; see `docs/05_验收清单.md`.
