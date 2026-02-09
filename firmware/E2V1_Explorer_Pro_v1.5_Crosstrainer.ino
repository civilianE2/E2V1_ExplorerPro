/*
 * E2V1 Explorer Pro - Firmware v1.5 (Crosstrainer Edition)
 * Author: Civilian
 * License: MIT
 * 
 * Hardware: 2 magnets per flywheel revolution on crosstrainer
 * 
 * Changelog v1.5 (Crosstrainer Optimized):
 * - Increased Moving Average: 4 → 8 samples (smoother for uneven motion)
 * - Increased Temporal Threshold: 150ms → 300ms (ignore micro-variations)
 * - Enhanced Smoothing at Low Speed: 0.08 → 0.03 alpha (much smoother)
 * - Fixed: 2-magnet correction applied
 * - Fixed: Map range adjusted for 2 magnets (2000-5000ms)
 * - All v1.3 features: Safe Watchdog, Deep Sleep, Auto-center
 */

#include <Arduino.h>
#define USE_NIMBLE 
#include <BleGamepad.h>
#include <esp_task_wdt.h>

// --- Configuration ---
const uint8_t PIN_SENSOR = 14;
const uint8_t PIN_BTN_LEFT = 12;
const uint8_t PIN_BTN_RIGHT = 13;

const uint16_t AXIS_CENTER = 16384; 
const uint32_t TIMEOUT_MS = 3000;
const uint32_t AUTO_CENTER_DELAY = 5000;
const uint8_t MIN_REPORT_INTERVAL_MS = 8;
const uint8_t DEBOUNCE_MS = 150;
const uint8_t COMBO_TOLERANCE_MS = 50;

// Watchdog & Sleep
const uint32_t WATCHDOG_TIMEOUT_MS = 30000; 
const uint32_t DEEP_SLEEP_DELAY_MS = 10000; 
const uint32_t DEEP_SLEEP_DURATION_US = 2000000;

// CROSSTRAINER SMOOTHING (Optimized!)
const uint8_t AVG_COUNT = 8;                     // 4 → 8 (more samples for stability)
const uint16_t INTERVAL_THRESHOLD_MS = 300;      // 150 → 300 (ignore small variations)

// --- Global Objects ---
BleGamepadConfiguration bleGamepadConfig;
BleGamepad bleGamepad("E2V1 Explorer Pro", "Civilian", 100);

// --- State Variables ---
static volatile bool sensorTriggered = false;
static volatile uint32_t lastTriggerTime = 0;

static uint32_t lastPedalTime = 0;
static uint32_t pedalInterval = 3000; 
static uint32_t intervals[AVG_COUNT] = {2500, 2500, 2500, 2500, 2500, 2500, 2500, 2500};
static uint8_t avgIndex = 0;

static float smoothedY = AXIS_CENTER;
static int16_t lastSentY = -1;

static bool lastL = false;
static bool lastR = false;
static uint32_t leftPressTime = 0;
static uint32_t rightPressTime = 0;
static uint32_t lastReportTime = 0;

static uint32_t disconnectedTime = 0;
static bool wasConnected = false;
static bool bleInitialized = false;

// --- Interrupt Handler ---
void IRAM_ATTR sensorISR() {
    uint32_t now = millis();
    if (now - lastTriggerTime > DEBOUNCE_MS) {
        sensorTriggered = true;
        lastTriggerTime = now;
    }
}

// --- Enhanced Adaptive Smoothing for Crosstrainer ---
float adaptiveSmoothing(float current, float target) {
    float delta = abs(target - current);
    
    float alpha;
    
    // CROSSTRAINER SPECIAL: Much stronger smoothing at low speeds
    if (pedalInterval > 2500) {
        alpha = 0.03;  // Very smooth at low speed (was 0.08) - reduces jerkiness
    } else if (delta > 2000) {
        alpha = 0.25;  // Fast reaction for large changes
    } else if (delta > 1000) {
        alpha = 0.15;  // Medium reaction
    } else {
        alpha = 0.08;  // Normal smoothing
    }
    
    return (current * (1.0 - alpha)) + (target * alpha);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== E2V1 Explorer Pro v1.5 (Crosstrainer Edition) ===");
    
    // GPIO Setup
    pinMode(PIN_SENSOR, INPUT_PULLUP);
    pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
    pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
    
    // Interrupt for sensor
    attachInterrupt(digitalPinToInterrupt(PIN_SENSOR), sensorISR, FALLING);
    Serial.println("GPIO initialized");

    // BLE Configuration
    bleGamepadConfig.setAutoReport(false);
    bleGamepadConfig.setButtonCount(5);
    bleGamepadConfig.setHatSwitchCount(0);
    bleGamepadConfig.setWhichAxes(true, true, false, false, false, false, false, false);
    
    bleGamepadConfig.setVid(0xE502); 
    bleGamepadConfig.setPid(0x4444); 

    Serial.println("Starting BLE...");
    bleGamepad.begin(&bleGamepadConfig);
    
    // Give BLE time to initialize properly
    delay(2000);
    bleInitialized = true;
    Serial.println("BLE initialized");
    
    // Watchdog timer (after BLE is ready)
    Serial.println("Initializing watchdog (30s timeout)...");
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WATCHDOG_TIMEOUT_MS,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);
    Serial.println("Watchdog active");
    
    Serial.println("=== Ready! Crosstrainer smoothing optimized ===\n");
}

void loop() {
    // Reset watchdog
    esp_task_wdt_reset();
    
    bool isConnected = bleGamepad.isConnected();
    
    // Connection tracking
    if (isConnected && !wasConnected) {
        Serial.println("BLE CONNECTED");
        disconnectedTime = 0;
        wasConnected = true;
    } else if (!isConnected && wasConnected) {
        Serial.println("BLE DISCONNECTED");
        disconnectedTime = millis();
        wasConnected = false;
    }
    
    // Deep Sleep (Battery Saving)
    if (bleInitialized && !isConnected && disconnectedTime > 0) {
        uint32_t disconnectedDuration = millis() - disconnectedTime;
        
        if (disconnectedDuration > DEEP_SLEEP_DELAY_MS) {
            // Reset state
            smoothedY = AXIS_CENTER;
            lastSentY = -1;
            pedalInterval = 3000;
            
            Serial.println("Entering deep sleep...");
            Serial.flush();
            
            esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION_US);
            esp_light_sleep_start();
            
            Serial.println("Woke from sleep");
            return;
        }
    }
    
    // === CONNECTED - NORMAL OPERATION ===
    if (isConnected) {
        uint32_t now = millis();
        int targetY = AXIS_CENTER;

        // 1. Cadence Calculation with Enhanced Smoothing
        if (sensorTriggered) {
            uint32_t currentInterval = now - lastPedalTime;
            
            // Calculate current average
            uint32_t sum = 0;
            for(int i=0; i<AVG_COUNT; i++) sum += intervals[i];
            uint32_t currentAvg = sum / AVG_COUNT;

            // Only update if change is significant (300ms threshold)
            if (abs((int32_t)(currentInterval - currentAvg)) > INTERVAL_THRESHOLD_MS) {
                intervals[avgIndex] = currentInterval;
                avgIndex = (avgIndex + 1) % AVG_COUNT;
                
                // Recalculate average with 2-magnet correction
                uint32_t newSum = 0;
                for(int i=0; i<AVG_COUNT; i++) newSum += intervals[i];
                pedalInterval = (newSum / AVG_COUNT) * 2;  // 2 magnets per revolution
            }

            lastPedalTime = now;
            sensorTriggered = false;
        }

        // 2. Movement Logic
        uint32_t timeSinceLastPedal = now - lastPedalTime;
        
        if (timeSinceLastPedal > TIMEOUT_MS) {
            // No movement for 3s
            targetY = AXIS_CENTER;
            pedalInterval = TIMEOUT_MS;
            // Reset average buffer
            for(int i=0; i<AVG_COUNT; i++) intervals[i] = TIMEOUT_MS;
        } else {
            // Map interval to axis (adjusted for 2 magnets)
            // Slow: 5000ms → Y=14000, Fast: 2000ms → Y=0
            int calculatedY = map(pedalInterval, 2000, 5000, 0, 14000);
            targetY = constrain(calculatedY, 0, 15000);
            
            // Fade out if pedaling stopped
            if (timeSinceLastPedal > pedalInterval) {
                targetY = map(timeSinceLastPedal, pedalInterval, TIMEOUT_MS, targetY, AXIS_CENTER);
            }
        }

        // 3. Auto-center & Final Smoothing
        if (timeSinceLastPedal > AUTO_CENTER_DELAY) {
            smoothedY = AXIS_CENTER;
        } else {
            // Apply enhanced adaptive smoothing (extra smooth at low speeds!)
            smoothedY = adaptiveSmoothing(smoothedY, targetY);
        }

        // 4. Button Handling with Combo Tolerance
        bool leftPressed = (digitalRead(PIN_BTN_LEFT) == LOW);
        bool rightPressed = (digitalRead(PIN_BTN_RIGHT) == LOW);
        
        if (leftPressed && !lastL) leftPressTime = now;
        if (rightPressed && !lastR) rightPressTime = now;
        
        bool isCombo = leftPressed && rightPressed && 
                       (abs((int32_t)(leftPressTime - rightPressTime)) < COMBO_TOLERANCE_MS);

        // 5. Report Throttling
        int reportY = (int)smoothedY;
        bool significantChange = abs(reportY - lastSentY) > 15;
        bool buttonChanged = (leftPressed != lastL || rightPressed != lastR);
        bool timeElapsed = (now - lastReportTime) >= MIN_REPORT_INTERVAL_MS;
        
        if (buttonChanged || (significantChange && timeElapsed)) {
            bleGamepad.setX(AXIS_CENTER); 
            bleGamepad.setY(reportY);

            // Combo logic
            if (isCombo) {
                bleGamepad.release(BUTTON_1); 
                bleGamepad.release(BUTTON_2);
                bleGamepad.press(BUTTON_4);
            } else {
                bleGamepad.release(BUTTON_4);
                leftPressed ? bleGamepad.press(BUTTON_1) : bleGamepad.release(BUTTON_1);
                rightPressed ? bleGamepad.press(BUTTON_2) : bleGamepad.release(BUTTON_2);
            }

            bleGamepad.sendReport();
            
            // Update state
            lastSentY = reportY;
            lastL = leftPressed;
            lastR = rightPressed;
            lastReportTime = now;
        }
    } else {
        // Not connected - reset state
        smoothedY = AXIS_CENTER;
        lastSentY = -1;
    }
    
    // Optimized loop delay
    delay(5);
}
