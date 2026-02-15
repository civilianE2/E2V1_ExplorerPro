# E2V1 Explorer Pro - Bluetooth Cross-Trainer Controller

An ESP32-based HID controller that converts pedal strokes from a cross-trainer (elliptical) into precise joystick inputs. Specifically optimized for the Earth2.io metaverse and high-intensity exergaming.

## 🌟 The Vision

The **E2V1 Explorer Pro** was born from a desire to bridge the gap between physical exercise and the digital frontier. 

The original inspiration came from the **Earth2.io** metaverse. Traversing vast digital landscapes becomes a truly immersive adventure when your own muscle power moves your avatar. By mapping real-world effort to in-game velocity, E2V1 turns a standard cardio workout into a journey through the metaverse.

## 🚀 Key Features (v1.9.3 "Sprint Edition")

* **Median Filter Signal Processing:** Unlike standard averaging, our 9-sample median filter rejects sensor noise and physical jitters, ensuring a rock-solid speed even if your pedaling is irregular.
* **Accelerated Velocity Curve:** Calibrated for realistic immersion. 
    * **40 RPM** (Steady walk) translates to **~80%** joystick input.
    * **100+ RPM** (Full sprint) unlocks the final **20%** of power.
* **Dual-Action Shift Logic:** Built-in "Sprint" mode. Holding both buttons for 800ms triggers a virtual **Button 5**, ideal for mapping to 'Shift' or 'Sprint' in-game.
* **Low Latency & High Stability:** Uses the **NimBLE** library for a fast, stable, and energy-efficient Bluetooth LE connection.
* **Optimized HID Descriptor:** Clean implementation with 2 axes (X/Y) to ensure maximum compatibility with Windows, Android, and iOS without "ghost axes."

## 🛠 Hardware Requirements

* **Microcontroller:** ESP32 (DevKit V1 or similar).
* **Sensor:** Magnetic Reed switch or Hall effect sensor (**Optimized for 4 magnets**).
* **Buttons:** Two momentary tactile switches.
* **Logic:** Internal pull-ups are used (Connect sensors/buttons between GPIO and GND).

## 🔌 Wiring Diagram

| Component | ESP32 Pin (GPIO) | Connection |
| :--- | :--- | :--- |
| **Magnetic Sensor** | **GPIO 14** | Pin 14 <---> GND |
| **Left Button** | **GPIO 12** | Pin 12 <---> GND |
| **Right Button** | **GPIO 13** | Pin 13 <---> GND |

### Important Notes:
* **Sensor Placement:** For 1.9.3 firmware, mount **4 magnets** evenly on the rotating flywheel.
* **Debounce:** Software-level debounce is set to 40ms to support high-speed sprinting (150+ RPM).
* **Power:** Power via USB or a regulated 5V source to the VIN pin.

## 💻 Software Installation

1.  Install **Arduino IDE**.
2.  Install the following libraries:
    * `ESP32-BLE-Gamepad` (NimBLE-compatible version)
    * `NimBLE-Arduino`
3.  Select your ESP32 board and hit **Upload**.

## 🎮 HID Configuration

* **Device Name:** `E2V1_v193`
* **Axes:** X (Centered), Y (Velocity controlled)
* **Buttons:**
    * **Button 1:** Left physical button
    * **Button 2:** Right physical button
    * **Button 4:** Shortcut (Both buttons pressed simultaneously)
    * **Button 5:** **Sprint/Shift** (Long press both buttons for 800ms)

> **Pro Tip:** In applications like **Earth2.io E2V1**, map **Button 5** to the `Shift` key and **Button 4** to the `Spacebar` (Jump) for the ultimate hands-free exploration experience.

---
*Developed for the Earth2 Community by Civilian.*
