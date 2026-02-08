/*
 * E2V1 Explorer Pro - Firmware v1.4 (Advanced Smoothing Edition)
 * Author: Civilian
 * * Changelog v1.4:
 * - Added Moving Average (4 samples) for pedal stability
 * - Added Temporal Threshold (150ms) to ignore micro-variations
 * - Integrated Safe Watchdog & Light Sleep from v1.3
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

// NEW: Smoothing Constants
const uint8_t AVG_COUNT = 4;
const uint16_t INTERVAL_THRESHOLD_MS = 150; // Ignore changes smaller than this

// --- Global Objects ---
BleGamepadConfiguration bleGamepadConfig;
BleGamepad bleGamepad("E2V1 Explorer Pro", "Civilian", 100);

// --- State Variables ---
static volatile bool sensorTriggered = false;
static volatile uint32_t lastTriggerTime = 0;

static uint32_t lastPedalTime = 0;
static uint32_t pedalInterval = 3000; 
static uint32_t intervals[AVG_COUNT] = {2500, 2500, 2500, 2500};
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

float adaptiveSmoothing(float current, float target) {
    float delta = abs(target - current);
    float alpha = (delta > 2000) ? 0.25 : (delta > 1000) ? 0.15 : 0.08;
    return (current * (1.0 - alpha)) + (target * alpha);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    
    pinMode(PIN_SENSOR, INPUT_PULLUP);
    pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
    pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
    
    attachInterrupt(digitalPinToInterrupt(PIN_SENSOR), sensorISR, FALLING);

    bleGamepadConfig.setAutoReport(false);
    bleGamepadConfig.setButtonCount(5);
    bleGamepadConfig.setHatSwitchCount(0);
    // Explicitly defining axes as requested in v1.2 startup
    bleGamepadConfig.setWhichAxes(true, true, false, false, false, false, false, false);
    
    bleGamepadConfig.setVid(0xE502); 
    bleGamepadConfig.setPid(0x4444); 

    bleGamepad.begin(&bleGamepadConfig);
    delay(2000);
    bleInitialized = true;
    
    // Watchdog Setup
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = WATCHDOG_TIMEOUT_MS,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);
    
    Serial.println("E2V1 v1.4 Ready");
}

void loop() {
    esp_task_wdt_reset();
    bool isConnected = bleGamepad.isConnected();
    
    // Connection tracking
    if (isConnected && !wasConnected) {
        disconnectedTime = 0;
        wasConnected = true;
    } else if (!isConnected && wasConnected) {
        disconnectedTime = millis();
        wasConnected = false;
    }
    
    // Power Saving
    if (bleInitialized && !isConnected && disconnectedTime > 0) {
        if (millis() - disconnectedTime > DEEP_SLEEP_DELAY_MS) {
            esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION_US);
            esp_light_sleep_start();
            return;
        }
    }
    
    if (isConnected) {
        uint32_t now = millis();
        int targetY = AXIS_CENTER;

        // 1. Cadence Calculation with Moving Average & Threshold
        if (sensorTriggered) {
            uint32_t currentInterval = now - lastPedalTime;
            
            // Calculate current average to compare against
            uint32_t sum = 0;
            for(int i=0; i<AVG_COUNT; i++) sum += intervals[i];
            uint32_t currentAvg = sum / AVG_COUNT;

            // Only update if change is significant (Option 3)
            if (abs((int32_t)(currentInterval - currentAvg)) > INTERVAL_THRESHOLD_MS) {
                intervals[avgIndex] = currentInterval;
                avgIndex = (avgIndex + 1) % AVG_COUNT;
                
                // Recalculate average for the pedalInterval (Option 2)
                uint32_t newSum = 0;
                for(int i=0; i<AVG_COUNT; i++) newSum += intervals[i];
                pedalInterval = newSum / AVG_COUNT;
            }

            lastPedalTime = now;
            sensorTriggered = false;
        }

        // 2. Movement Logic
        uint32_t timeSinceLastPedal = now - lastPedalTime;
        
        if (timeSinceLastPedal > TIMEOUT_MS) {
            targetY = AXIS_CENTER;
            pedalInterval = TIMEOUT_MS;
            // Reset average buffer to prevent "memory" of old speed
            for(int i=0; i<AVG_COUNT; i++) intervals[i] = TIMEOUT_MS;
        } else {
            // Map interval to axis: 1000ms (fast) -> 0, 2500ms (slow) -> 14000
            int calculatedY = map(pedalInterval, 1000, 2500, 0, 14000);
            targetY = constrain(calculatedY, 0, 15000);
            
            if (timeSinceLastPedal > pedalInterval) {
                targetY = map(timeSinceLastPedal, pedalInterval, TIMEOUT_MS, targetY, AXIS_CENTER);
            }
        }

        // 3. Final Smoothing & Auto-center
        if (timeSinceLastPedal > AUTO_CENTER_DELAY) {
            smoothedY = AXIS_CENTER;
        } else {
            smoothedY = adaptiveSmoothing(smoothedY, targetY);
        }

        // 4. Buttons & Report
        bool leftPressed = (digitalRead(PIN_BTN_LEFT) == LOW);
        bool rightPressed = (digitalRead(PIN_BTN_RIGHT) == LOW);
        
        if (leftPressed && !lastL) leftPressTime = now;
        if (rightPressed && !lastR) rightPressTime = now;
        
        bool isCombo = leftPressed && rightPressed && 
                       (abs((int32_t)(leftPressTime - rightPressTime)) < COMBO_TOLERANCE_MS);

        int reportY = (int)smoothedY;
        bool significantChange = abs(reportY - lastSentY) > 15;
        bool buttonChanged = (leftPressed != lastL || rightPressed != lastR);
        
        if (buttonChanged || (significantChange && (now - lastReportTime >= MIN_REPORT_INTERVAL_MS))) {
            bleGamepad.setX(AXIS_CENTER); 
            bleGamepad.setY(reportY);

            if (isCombo) {
                bleGamepad.release(BUTTON_1); bleGamepad.release(BUTTON_2);
                bleGamepad.press(BUTTON_4);
            } else {
                bleGamepad.release(BUTTON_4);
                leftPressed ? bleGamepad.press(BUTTON_1) : bleGamepad.release(BUTTON_1);
                rightPressed ? bleGamepad.press(BUTTON_2) : bleGamepad.release(BUTTON_2);
            }

            bleGamepad.sendReport();
            lastSentY = reportY;
            lastL = leftPressed; lastR = rightPressed;
            lastReportTime = now;
        }
    }
    delay(5);
}
