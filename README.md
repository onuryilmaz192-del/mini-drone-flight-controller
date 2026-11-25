# ESP32 Mini Drone

A custom-built mini quadcopter project combining a lightweight, crash-resistant 3D-printed frame with an **ESP32** flight controller written in C++ / Arduino framework. The project focuses on rapid iteration, practical engineering trade-offs, and end-to-end control—from CAD design to IMU sensor fusion, PID stabilization, and browser-based WebSocket piloting.

## Mechanical Design

For the mechanical design of this drone, my primary philosophy was to keep everything as simple and practical as possible. I intentionally avoided putting any cutouts or mounting holes in the center of the body. Even though this solid core would make the frame slightly heavier, I prioritized making the structure as strong as possible to survive inevitable crashes during testing.

The main challenge was finding the balance between structural strength and maintaining a good thrust-to-weight ratio. To compensate for the heavier solid core, I decided to avoid complex designs and dedicated mounting spaces for the ESP32, battery, and other electronics. Instead, I chose to secure all the components simply using tape.

This decision reduced the overall weight of the frame and kept the CAD design extremely straightforward. In this project, avoiding over-engineering in the mechanical phase helped me increase my productivity a lot, allowing me to move quickly from 3D printing directly to flight testing.

## Firmware & Control Architecture

For the software side of this project, my main goal was getting a working, responsive flight control and tuning loop running cleanly without getting into RTOS. I structured the code around small classes (`Websocket`, `MPU6050`, `PIDController`, and `Motor`) so that each part of the system has a clear responsibility and can be debugged on its own.

Instead of relying on a physical RC transmitter, I hosted a local Wi-Fi access point (`Mini_Drone_AP`) on the ESP32 and served a browser-based joystick UI over WebSocket (`/ws`). This let me control throttle, yaw, pitch, and roll from a phone or laptop, test PID adjustments live, and watch roll behavior on a chart.

On the control side, I read accelerometer and gyroscope data from an MPU6050 over I2C, fuse the angle estimate with a complementary filter, and run separate PID controllers for pitch, roll, and yaw with integral clamping and output limits. Because early flight and bench tests inevitably bring disconnects or dropped packets, I added basic fail-safe checks—such as zeroing throttle on disconnect or after a `500 ms` packet timeout.

## Hardware & Pinout

| Component | ESP32 Pin / Config | Notes |
| **MPU6050 SDA** | `GPIO 21` | I2C data |
| **MPU6050 SCL** | `GPIO 19` | I2C clock |
| **Top-Left Motor (`TLMotor`)** | `GPIO 32` | LEDC Channel `1`, 24 kHz, 8-bit PWM |
| **Top-Right Motor (`TRMotor`)** | `GPIO 27` | LEDC Channel `0`, 24 kHz, 8-bit PWM |
| **Bottom-Left Motor (`BLMotor`)** | `GPIO 25` | LEDC Channel `2`, 24 kHz, 8-bit PWM |
| **Bottom-Right Motor (`BRMotor`)** | `GPIO 12` | LEDC Channel `3`, 24 kHz, 8-bit PWM *(strapping pin — check boot behavior)* |

> **Safety Note:** Always remove propellers or secure the frame safely before testing motor spin directions and PID responses.

## Getting Started

1. Open the project in **PlatformIO** (VS Code) configured for an **ESP32** board.
2. Ensure required libraries (`ESPAsyncWebServer` and `ArduinoJson`) are listed in `platformio.ini`.
3. Build and upload the firmware to the ESP32.
4. Connect a phone or laptop to the `Mini_Drone_AP` Wi-Fi network and open `http://192.168.4.1` in a browser.

## Current Status & Next Steps

- [x] Solid-core 3D-printed frame design and assembly
- [x] SoftAP + WebSocket joystick control and `500 ms` packet fail-safe
- [x] MPU6050 I2C reading, calibration offsets, and complementary filter
- [x] Pitch, Roll, and Yaw PID calculation with motor mixing
- [ ] Enable live PID gain updates over WebSocket cleanly
- [ ] Stream telemetry JSON from ESP32 to the browser Roll chart
- [ ] Split classes into separate header and source files as the codebase grows
