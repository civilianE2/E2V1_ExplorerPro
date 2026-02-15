/*
 * E2V1 Explorer Pro - Firmware v1.9.3 (ACCELERATED CURVE)
 * Author: Civilian
 * License: MIT
 * 
 * New in v1.9.3:
 * - Dynamics: 40 RPM (375ms) -> -0.8 axis value (STABLE!)
 * - Dynamics: 100 RPM (150ms) -> -1.0 axis value
 * - Filter: 9-sample MEDIAN for maximum stability
 * - Buttons: Long press Shift (Button 5) with proper Button 4 deactivation
 * - Simplified speed curve (removed slowdown logic)
 * 
 * Hardware Configuration:
 * - ESP32 Dev Module @ 80MHz
 * - 4 magnets per flywheel revolution
 * - Reed/Hall sensor on GPIO 14
 * - Left button on GPIO 12
 * - Right button on GPIO 13
 */

#include <Arduino.h>
#define USE_NIMBLE 
#include <BleGamepad.h>

// --- SPRINT CONFIGURATION ---
const uint16_t FAST_PULSE = 150;   // 100 RPM
const uint16_t SLOW_PULSE = 480;   // ~31 RPM (lower limit)
const float SMOOTHING = 0.12f;     // Slightly increased for steeper curve
const float INV_SMOOTHING = 0.88f;
const int START_BOOST = 12000;     // Strong startup boost
const uint8_t AVG_COUNT = 9;       // Median buffer (must be ODD)
const uint32_t TIMEOUT_MS = 1500;
const uint32_t MIN_INTERVAL = 40;
const uint32_t REPORT_INTERVAL = 20;
const uint16_t SHIFT_DELAY = 800;

const uint8_t PIN_SENSOR = 14;
const uint8_t PIN_BTN_LEFT = 12;
const uint8_t PIN_BTN_RIGHT = 13;
const uint16_t AXIS_CENTER = 16384;

BleGamepadConfiguration bleGamepadConfig;
BleGamepad bleGamepad("E2V1_v193", "E2V_Labs", 100);

uint32_t intervals[AVG_COUNT];
uint8_t intervalIndex = 0;
uint32_t medianInterval = 480;

volatile bool sensorTriggered = false;
volatile uint32_t lastTriggerTime = 0;

uint32_t lastPedalTime = 0;
uint32_t lastReportTime = 0;
float smoothedY = AXIS_CENTER;
int16_t lastSentY = -1;
uint8_t lastButtons = 0;
uint32_t buttonsBothStart = 0;
bool shiftActive = false;
bool isFirstPulse = true;

const uint8_t BUTTON_LOOKUP[2][2] = {{0, 2}, {1, 4}};

void IRAM_ATTR sensorISR() {
    uint32_t now = millis();
    if (now - lastTriggerTime > MIN_INTERVAL) {
        sensorTriggered = true;
        lastTriggerTime = now;
    }
}

// Calculate MEDIAN of intervals (rejects outliers)
uint32_t getMedianInterval() {
    uint32_t sorted[AVG_COUNT];
    for (uint8_t i = 0; i < AVG_COUNT; i++) sorted[i] = intervals[i];
    
    // Bubble sort
    for (uint8_t i = 0; i < AVG_COUNT - 1; i++) {
        for (uint8_t j = 0; j < AVG_COUNT - i - 1; j++) {
            if (sorted[j] > sorted[j + 1]) {
                uint32_t temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
    return sorted[AVG_COUNT / 2];
}

void setup() {
    setCpuFrequencyMhz(80);
    
    bleGamepadConfig.setAutoReport(false);
    bleGamepadConfig.setButtonCount(6);
    bleGamepadConfig.setHatSwitchCount(0);
    bleGamepadConfig.setWhichAxes(true, true, false, false, false, false, false, false);
    bleGamepadConfig.setWhichSimulationControls(false, false, false, false, false);
    bleGamepadConfig.setWhichSpecialButtons(false, false, false, false, false, false, false, false);
    
    bleGamepad.begin(&bleGamepadConfig);

    // Initialize interval buffer
    for (uint8_t i = 0; i < AVG_COUNT; i++) intervals[i] = SLOW_PULSE;

    pinMode(PIN_SENSOR, INPUT_PULLUP);
    pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
    pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_SENSOR), sensorISR, FALLING);

    bleGamepad.setX(AXIS_CENTER);
    bleGamepad.setY(AXIS_CENTER);
    bleGamepad.sendReport();
}

void loop() {
    uint32_t now = millis();
    if (!bleGamepad.isConnected()) return;

    // --- SENSOR PROCESSING ---
    if (sensorTriggered) {
        uint32_t interval = now - lastPedalTime;
        lastPedalTime = now;
        
        if (interval > MIN_INTERVAL && interval < 1500) { 
            // Add to circular buffer
            intervals[intervalIndex] = interval;
            intervalIndex = (intervalIndex + 1) % AVG_COUNT;
            
            // Calculate median (stable!)
            medianInterval = getMedianInterval();
            
            if (isFirstPulse) {
                isFirstPulse = false;
                smoothedY = AXIS_CENTER - START_BOOST;
            }
        }
        sensorTriggered = false;
    }

    // --- REPORTING ---
    if (now - lastReportTime >= REPORT_INTERVAL) {
        uint32_t timeSinceLast = now - lastPedalTime;
        int targetY;
        
        if (timeSinceLast > TIMEOUT_MS) {
            // Stopped
            targetY = AXIS_CENTER;
            for (uint8_t i = 0; i < AVG_COUNT; i++) intervals[i] = SLOW_PULSE;
            medianInterval = SLOW_PULSE;
            isFirstPulse = true;
        } else {
            // Aggressive Curve:
            // 150ms (100 RPM) -> 16300 offset -> Y=84 -> normalized -1.0
            // 375ms (40 RPM) -> ~13100 offset -> Y=3284 -> normalized -0.8
            // 480ms (31 RPM) -> 1500 offset -> Y=14884 -> normalized -0.09
            int speedOffset = map(constrain(medianInterval, FAST_PULSE, SLOW_PULSE), 
                                 FAST_PULSE, SLOW_PULSE, 16300, 1500);
            targetY = AXIS_CENTER - speedOffset;
        }

        // Apply smoothing
        smoothedY = (smoothedY * INV_SMOOTHING) + (targetY * SMOOTHING);

        // --- BUTTON LOGIC ---
        bool L = (digitalRead(PIN_BTN_LEFT) == LOW);
        bool R = (digitalRead(PIN_BTN_RIGHT) == LOW);
        uint8_t currentButtons = BUTTON_LOOKUP[L][R];

        // Long press detection for Shift (Button 5)
        if (L && R) {
            if (buttonsBothStart == 0) buttonsBothStart = now;
            else if (now - buttonsBothStart > SHIFT_DELAY) shiftActive = true;
        } else {
            buttonsBothStart = 0;
            shiftActive = false;
        }

        // --- SEND REPORT ---
        int currentY = (int)smoothedY;
        bool btn5State = bleGamepad.isPressed(BUTTON_5);

        if (abs(currentY - lastSentY) > 2 || currentButtons != lastButtons || shiftActive != btn5State) {
            bleGamepad.setX(AXIS_CENTER);
            bleGamepad.setY(currentY);

            // Single button presses
            if (currentButtons == 1) bleGamepad.press(BUTTON_1); else bleGamepad.release(BUTTON_1);
            if (currentButtons == 2) bleGamepad.press(BUTTON_2); else bleGamepad.release(BUTTON_2);
            
            // Shift logic: Button 5 active -> Button 4 inactive
            if (shiftActive) {
                bleGamepad.press(BUTTON_5);
                bleGamepad.release(BUTTON_4);
            } else {
                bleGamepad.release(BUTTON_5);
                if (currentButtons == 4) bleGamepad.press(BUTTON_4); else bleGamepad.release(BUTTON_4);
            }

            bleGamepad.sendReport();
            lastSentY = currentY;
            lastButtons = currentButtons;
            lastReportTime = now;
        }
    }
    delay(1);
}
