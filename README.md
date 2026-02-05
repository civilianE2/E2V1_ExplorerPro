# E2V1 Explorer Pro - Bluetooth Cross-Trainer Controller

An ESP32-based HID controller that converts pedal strokes from a cross-trainer (elliptical) into joystick inputs. Designed for immersive gaming and fitness tracking via Bluetooth LE.

## 🚀 Features
* **Adaptive Speed Mapping:** Converts pedal cadence into smooth Y-axis movement.
* **Dual-Button Interface:** Dedicated buttons for navigation or actions, including a combined "Button 4" shortcut.
* **Optimized HID Descriptor:** Clean implementation with only 2 axes (X/Y) to ensure compatibility with Windows/Android without "ghost axes."
* **Low Latency:** Uses the NimBLE library for a stable, low-energy connection and fast response times.

## 🛠 Hardware Requirements
* **Microcontroller:** ESP32 (DevKit V1 or similar).
* **Sensor:** Reed switch or Hall effect sensor (connected to Pin 14).
* **Buttons:** Two momentary tactile switches (Pins 12 & 13).
* **Pull-ups:** Internal pull-ups are used, so buttons/sensors should pull to GND.

## 💻 Software Installation
1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Install the following libraries:
   * [ESP32-BLE-Gamepad](https://github.com/lemmingDev/ESP32-BLE-Gamepad)
   * [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)
3. Ensure you have the `USE_NIMBLE` define active (included in the source).
4. Select your ESP32 board and hit **Upload**.

## 🎮 HID Configuration
To ensure maximum compatibility, the device identifies as:
- **Name:** E2V1 Explorer Pro
- **Manufacturer:** Civilian
- **Axes:** X (Centered), Y (Speed-controlled)
- **Buttons:** 5 available (1, 2, and 4 in use)
