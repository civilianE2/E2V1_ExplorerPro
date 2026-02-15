/*
 * E2V1 Explorer Pro - Firmware v1.9.4 (OPTIMIZED) 
 * Author: Civilian
 * License: MIT
 * 
 * Changes in v1.9.4:
 * - Optimized median calculation (selection sort)
 * - Faster ISR using tick count
 * - Integer-based offset calculation (removed map)
 * - Optimized loop structure
 * - Reduced function call overhead
 * 
 * Hardware Configuration:
 * - ESP32 Dev Module @ 80MHz
 * - 4 magnets per flywheel revolution
 * - Reed/Hall sensor on GPIO 14
 * - Left button on GPIO 12
 * - Right button on GPIO 13
 */

#include <Arduino.h>
#include <cstring>  // for memcpy
#define USE_NIMBLE 
#include <BleGamepad.h>

// --- SPRINT CONFIGURATION ---
const uint16_t FAST_PULSE = 150;   // 100 RPM
const uint16_t SLOW_PULSE = 480;   // ~31 RPM (lower limit)
const float SMOOTHING = 0.12f;
const float ONE_MINUS_SMOOTHING = 0.88f;  // Pre-calculated
const int START_BOOST = 12000;     // Strong startup boost
const uint8_t AVG_COUNT = 9;       // Median buffer (must be ODD)
const uint32_t TIMEOUT_MS = 1500;
const uint32_t MIN_INTERVAL = 40;
const uint32_t REPORT_INTERVAL = 20;
const uint16_t SHIFT_DELAY = 800;
const uint32_t CONNECTION_CHECK_INTERVAL = 100;  // Check connection every 100ms

// Pin definitions
const uint8_t PIN_SENSOR = 14;
const uint8_t PIN_BTN_LEFT = 12;
const uint8_t PIN_BTN_RIGHT = 13;
const uint16_t AXIS_CENTER = 16384;

// Pre-calculated offset constants
const int16_t MAX_OFFSET = 16300;
const int16_t MIN_OFFSET = 1500;
const uint16_t OFFSET_RANGE = MAX_OFFSET - MIN_OFFSET;
const uint16_t PULSE_RANGE = SLOW_PULSE - FAST_PULSE;

BleGamepadConfiguration bleGamepadConfig;
BleGamepad bleGamepad("E2V1_v194", "E2V_Labs", 100);

// Sensor data
uint32_t intervals[AVG_COUNT];
uint8_t intervalIndex = 0;
uint32_t medianInterval = SLOW_PULSE;

// Volatile for ISR
volatile bool sensorTriggered = false;
volatile uint32_t lastTriggerTimeTicks = 0;
static const uint32_t MIN_INTERVAL_TICKS = MIN_INTERVAL / portTICK_PERIOD_MS;

// Timing variables
uint32_t lastPedalTime = 0;
uint32_t lastReportTime = 0;
uint32_t lastConnectionCheck = 0;

// State variables
float smoothedY = AXIS_CENTER;
int16_t lastSentY = -1;
uint8_t lastButtons = 0;
uint32_t buttonsBothStart = 0;
bool shiftActive = false;
bool isFirstPulse = true;

const uint8_t BUTTON_LOOKUP[2][2] = {{0, 2}, {1, 4}};

// Optimized ISR using tick count instead of millis()
void IRAM_ATTR sensorISR() {
    uint32_t now = xTaskGetTickCountFromISR();
    if (now - lastTriggerTimeTicks > MIN_INTERVAL_TICKS) {
        sensorTriggered = true;
        lastTriggerTimeTicks = now;
    }
}

// Fast median calculation using selection sort (only need middle element)
uint32_t getMedianInterval() {
    uint32_t sorted[AVG_COUNT];
    memcpy(sorted, intervals, sizeof(sorted));
    
    // Selection sort - only need to sort until median
    for (uint8_t i = 0; i <= AVG_COUNT / 2; i++) {
        uint8_t minIdx = i;
        for (uint8_t j = i + 1; j < AVG_COUNT; j++) {
            if (sorted[j] < sorted[minIdx]) {
                minIdx = j;
            }
        }
        if (minIdx != i) {
            uint32_t temp = sorted[i];
            sorted[i] = sorted[minIdx];
            sorted[minIdx] = temp;
        }
    }
    return sorted[AVG_COUNT / 2];
}

// Integer-based offset calculation (replaces map)
inline int16_t calculateOffset(uint32_t interval) {
    if (interval <= FAST_PULSE) return MAX_OFFSET;
    if (interval >= SLOW_PULSE) return MIN_OFFSET;
    
    // Linear interpolation using integers
    // offset = MAX_OFFSET - ((interval - FAST_PULSE) * OFFSET_RANGE) / PULSE_RANGE
    return MAX_OFFSET - ((interval - FAST_PULSE) * OFFSET_RANGE) / PULSE_RANGE;
}

// Handle sensor trigger
inline void handleSensor(uint32_t now) {
    uint32_t interval = now - lastPedalTime;
    lastPedalTime = now;
    
    if (interval > MIN_INTERVAL && interval < 1500) { 
        intervals[intervalIndex] = interval;
        intervalIndex = (intervalIndex + 1) % AVG_COUNT;
        medianInterval = getMedianInterval();
        
        if (isFirstPulse) {
            isFirstPulse = false;
            smoothedY = AXIS_CENTER - START_BOOST;
        }
    }
    sensorTriggered = false;
}

// Handle reporting
inline void handleReporting(uint32_t now) {
    uint32_t timeSinceLast = now - lastPedalTime;
    int16_t targetY;
    
    if (timeSinceLast > TIMEOUT_MS) {
        // Stopped
        targetY = AXIS_CENTER;
        for (uint8_t i = 0; i < AVG_COUNT; i++) intervals[i] = SLOW_PULSE;
        medianInterval = SLOW_PULSE;
        isFirstPulse = true;
    } else {
        // Calculate offset using integer math
        targetY = AXIS_CENTER - calculateOffset(medianInterval);
    }

    // Apply smoothing
    smoothedY = (smoothedY * ONE_MINUS_SMOOTHING) + (targetY * SMOOTHING);

    // Button logic
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

    // Send report only if changed
    int16_t currentY = (int16_t)smoothedY;
    bool btn5State = bleGamepad.isPressed(BUTTON_5);

    if (abs(currentY - lastSentY) > 2 || currentButtons != lastButtons || shiftActive != btn5State) {
        bleGamepad.setX(AXIS_CENTER);
        bleGamepad.setY(currentY);

        // Single button presses
        if (currentButtons == 1) bleGamepad.press(BUTTON_1); 
        else bleGamepad.release(BUTTON_1);
        
        if (currentButtons == 2) bleGamepad.press(BUTTON_2); 
        else bleGamepad.release(BUTTON_2);
        
        // Shift logic: Button 5 active -> Button 4 inactive
        if (shiftActive) {
            bleGamepad.press(BUTTON_5);
            bleGamepad.release(BUTTON_4);
        } else {
            bleGamepad.release(BUTTON_5);
            if (currentButtons == 4) bleGamepad.press(BUTTON_4); 
            else bleGamepad.release(BUTTON_4);
        }

        bleGamepad.sendReport();
        lastSentY = currentY;
        lastButtons = currentButtons;
    }
    lastReportTime = now;
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
    
    // Check connection periodically (not every loop)
    if (now - lastConnectionCheck > CONNECTION_CHECK_INTERVAL) {
        if (!bleGamepad.isConnected()) {
            delay(10);  // Save CPU when not connected
            return;
        }
        lastConnectionCheck = now;
    }

    // Handle sensor trigger (high priority)
    if (sensorTriggered) {
        handleSensor(now);
    }

    // Handle reporting (periodic)
    if (now - lastReportTime >= REPORT_INTERVAL) {
        handleReporting(now);
    }
    
    // Small delay to prevent watchdog issues
    delay(1);
}