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

## 🔌 Wiring Diagram

The project uses the ESP32's internal pull-up resistors, so all sensors and buttons are connected between the **GPIO pin** and **GND**.

| Component | ESP32 Pin (GPIO) | Connection |
| :--- | :--- | :--- |
| **Magnetic Reed Sensor** | GPIO 14 | Pin 14 <---> GND |
| **Left Button** | GPIO 12 | Pin 12 <---> GND |
| **Right Button** | GPIO 13 | Pin 13 <---> GND |

### Important Notes:
* **Sensor Placement:** Mount the magnet on the rotating part of the cross-trainer and the reed switch on the frame so they pass within 5-10mm of each other.
* **Debounce:** The software includes a 150ms lockout to prevent double-triggering from a single pedal stroke.
* **Power:** You can power the ESP32 via the micro-USB/USB-C port or a regulated 5V source to the VIN pin.

## 💻 Software Installation
1. Install [Arduino IDE](https://www.arduino.cc/en/software).
2. Install the following libraries:
   * [ESP32-BLE-Gamepad](https://github.com/lemmingDev/ESP32-BLE-Gamepad)
   * [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)
3. Ensure you have the `USE_NIMBLE` define active (included in the source).
4. Select your ESP32 board and hit **Upload**.

## 🎮 HID Configuration
- **Name:** E2V1 Explorer Pro
- **Manufacturer:** Civilian
- **Axes:** X (Centered), Y (Speed-controlled)
- **Buttons:** 5 available (1, 2, and 4 in use)

[![watch demo](https://img.youtube.com/vi/343EgGPdnpc/0.jpg)](https://www.youtube.com/watch?v=343EgGPdnpc)
