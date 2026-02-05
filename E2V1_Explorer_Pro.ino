/*
 * E2V1 Explorer Pro - Firmware v1.1
 * Author: YourName/Civilian
 * License: MIT
 */

#include <Arduino.h>
#define USE_NIMBLE 
#include <BleGamepad.h>

// --- Configuration ---
const int PIN_SENSOR = 14;
const int PIN_BTN_LEFT = 12;
const int PIN_BTN_RIGHT = 13;

const int AXIS_CENTER = 16384; 
const unsigned long TIMEOUT_MS = 3000;

// --- Global Objects ---
BleGamepadConfiguration bleGamepadConfig;
BleGamepad bleGamepad("E2V1 Explorer Pro", "Civilian", 100);

// --- State Variables ---
int lastSensorState = HIGH;
unsigned long lastPedalTime = 0;
unsigned long pedalInterval = 3000; 
float smoothedY = AXIS_CENTER;
int lastSentY = -1;

void setup() {
    pinMode(PIN_SENSOR, INPUT_PULLUP);
    pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
    pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);

    bleGamepadConfig.setAutoReport(false);
    bleGamepadConfig.setButtonCount(5);
    bleGamepadConfig.setHatSwitchCount(0);
    // Explicitly define axes to prevent Windows HID issues
    bleGamepadConfig.setWhichAxes(true, true, false, false, false, false, false, false);
    
    bleGamepadConfig.setVid(0xE502); 
    bleGamepadConfig.setPid(0x4444); 

    bleGamepad.begin(&bleGamepadConfig);
}

void loop() {
    if (bleGamepad.isConnected()) {
        int currentSensorState = digitalRead(PIN_SENSOR);
        unsigned long now = millis();
        int targetY = AXIS_CENTER;

        // 1. Cadence Calculation
        if (currentSensorState == LOW && lastSensorState == HIGH) {
            unsigned long currentInterval = now - lastPedalTime;
            if (currentInterval > 150) { // Debounce
                pedalInterval = currentInterval;
                lastPedalTime = now;
            }
        }

        // 2. Movement Logic
        unsigned long timeSinceLastPedal = now - lastPedalTime;
        if (timeSinceLastPedal > TIMEOUT_MS) {
            targetY = AXIS_CENTER;
            pedalInterval = TIMEOUT_MS;
        } else {
            // Map interval to Y-axis range (0-14000)
            int calculatedY = map(pedalInterval, 1000, 2500, 0, 14000);
            targetY = constrain(calculatedY, 0, 15000);
            
            // Fade out speed if pedaling stops
            if (timeSinceLastPedal > pedalInterval) {
                targetY = map(timeSinceLastPedal, pedalInterval, TIMEOUT_MS, targetY, AXIS_CENTER);
            }
        }

        // 3. Smoothing (Low-pass filter)
        smoothedY = (smoothedY * 0.92) + (targetY * 0.08);

        // 4. Input Handling & Reporting
        bool leftPressed = (digitalRead(PIN_BTN_LEFT) == LOW);
        bool rightPressed = (digitalRead(PIN_BTN_RIGHT) == LOW);
        static bool lastL = false, lastR = false;

        int reportY = (int)smoothedY;
        
        if (abs(reportY - lastSentY) > 15 || leftPressed != lastL || rightPressed != lastR) {
            bleGamepad.setX(AXIS_CENTER); 
            bleGamepad.setY(reportY);

            // Combo logic: Both buttons = Button 4
            if (leftPressed && rightPressed) {
                bleGamepad.release(BUTTON_1); bleGamepad.release(BUTTON_2);
                bleGamepad.press(BUTTON_4);
            } else {
                bleGamepad.release(BUTTON_4);
                leftPressed ? bleGamepad.press(BUTTON_1) : bleGamepad.release(BUTTON_1);
                rightPressed ? bleGamepad.press(BUTTON_2) : bleGamepad.release(BUTTON_2);
            }

            bleGamepad.sendReport();
            lastSentY = reportY; lastL = leftPressed; lastR = rightPressed;
        }
        lastSensorState = currentSensorState;
    }
    delay(15); 
}
