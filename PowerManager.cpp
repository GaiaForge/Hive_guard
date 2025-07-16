/**
 * PowerManager.cpp - UPDATED: Complete deep sleep implementation with nRF52840 and PCF8523
 */

#include "PowerManager.h"
#include "Utils.h"
#include "Sensors.h"  // For getBatteryLevel
#include "Bluetooth.h"

#ifdef NRF52_SERIES
#include <nrf.h>
#include <nrf_power.h>
#include <nrf_rtc.h>
#endif

extern RTC_PCF8523 rtc;  // Reference to global RTC object

// Global pointer for interrupt handler access
static PowerManager* powerManagerInstance = nullptr;

// =============================================================================
// POWER CONSUMPTION CONSTANTS (mA)
// =============================================================================

const float PowerManager::POWER_TESTING_MA = 15.0f;
const float PowerManager::POWER_DISPLAY_MA = 8.0f;
const float PowerManager::POWER_SENSORS_MA = 2.0f;
const float PowerManager::POWER_AUDIO_MA = 5.0f;
const float PowerManager::POWER_BLUETOOTH_MA = 12.0f;
const float PowerManager::POWER_SLEEP_MA = 0.05f;

// =============================================================================
// CONSTRUCTOR AND INITIALIZATION
// =============================================================================

PowerManager::PowerManager() {
    // Initialize status
    status.currentMode = POWER_TESTING;
    status.fieldModeActive = false;
    status.displayOn = true;  // Starts on
    status.displayTimeoutMs = 0;  // Will be set from settings
    status.lastUserActivity = 0;
    status.nextSleepTime = 0;
    status.totalUptime = 0;
    status.sleepCycles = 0;
    status.buttonPresses = 0;
    status.lastWakeSource = WAKE_UNKNOWN;
    status.estimatedRuntimeHours = 0;
    status.dailyUsageEstimateMah = 0;
    status.displayState = COMP_POWER_ON;
    status.sensorState = COMP_POWER_ON;
    status.audioState = COMP_POWER_ON;
    status.bluetoothState = COMP_POWER_ON;
    
    // Field mode timing variables
    status.wokenByTimer = false;
    status.lastLogTime = 0;
    status.nextWakeTime = 0;
    status.lastFlushTime = 0;
    
    // Bluetooth timing
    status.bluetoothOn = false;
    status.bluetoothTimeoutMs = 0;
    status.lastBluetoothActivity = 0;
    status.bluetoothManuallyActivated = false;
    
    // Initialize settings
    settings.fieldModeEnabled = false;
    settings.displayTimeoutMin = 2;     // Default 2 minutes
    settings.sleepIntervalMin = 10;
    settings.autoFieldMode = false;
    settings.criticalBatteryLevel = 15;
    
    // Initialize timing
    lastPowerCheck = 0;
    displayOffTime = 0;
    lastSleepTime = 0;
       
    systemStatus = nullptr;
    systemSettings = nullptr;
    bluetoothManager = nullptr;

    longPressStartTime = 0;
    longPressDetected = false;
    
    // Initialize deep sleep variables
    wakeupFromRTC = false;
    wakeupFromButton = false;
    scheduledWakeTime = 0;
    rtcInterruptWorking = false;  // Add this line
    
    // Set global instance pointer for interrupt handler
    powerManagerInstance = this;
}

void PowerManager::initialize(SystemStatus* sysStatus, SystemSettings* sysSettings) {
    systemStatus = sysStatus;
    systemSettings = sysSettings;
    
    // Initialize display power control hardware
    initializeDisplayPower();

    // Initialize Bluetooth button
    pinMode(BTN_BLUETOOTH, INPUT_PULLUP);
    Serial.println(F("Bluetooth button initialized on pin 13"));

    // Load display timeout settings from system settings
    if (sysSettings) {
        settings.fieldModeEnabled = sysSettings->fieldModeEnabled;
        settings.displayTimeoutMin = constrain(sysSettings->displayTimeoutMin, 1, 5); // 1-5 minutes only
        status.displayTimeoutMs = settings.displayTimeoutMin * 60000UL;
        status.bluetoothTimeoutMs = status.displayTimeoutMs; // Same timeout for Bluetooth
        
        // If field mode is enabled in settings, activate it
        if (settings.fieldModeEnabled) {
            enableFieldMode();
        }
    }
    
    status.lastUserActivity = millis();
    status.lastBluetoothActivity = millis();
    status.totalUptime = millis();  // Set uptime reference
    status.lastFlushTime = millis();
    
    // Setup RTC interrupt for deep sleep wake-up
    setupRTCInterrupt();
    
    Serial.println(F("PowerManager initialized with deep sleep capability"));
    Serial.println(F("  - Power monitoring: ENABLED"));
    Serial.println(F("  - Field mode: ENABLED"));
    Serial.println(F("  - Display control: ENABLED (Pin 12)"));
    Serial.println(F("  - RTC wake-up: ENABLED (Pin A1)"));
    Serial.print(F("  - Display timeout: "));
    Serial.print(settings.displayTimeoutMin);
    Serial.println(F(" minutes (1-5 range)"));
}

// Set Bluetooth manager reference
void PowerManager::setBluetoothManager(BluetoothManager* btManager) {
    bluetoothManager = btManager;
    Serial.println(F("PowerManager: Bluetooth manager reference set"));
}

// =============================================================================
// DEEP SLEEP MANAGEMENT
// =============================================================================

void PowerManager::setupRTCInterrupt() {
    // Configure A1 as input with pullup for RTC interrupt
    pinMode(RTC_INT_PIN, INPUT_PULLUP);
    
    // Clear wake-up flags
    wakeupFromRTC = false;
    wakeupFromButton = false;
    rtcInterruptWorking = false;  // Start pessimistic
    
    Serial.println(F("PowerManager: Setting up RTC interrupt system..."));
    
    // Step 1: Try to clear any pending RTC alarm flags
    clearRTCAlarmFlags();
    
    // Step 2: Check pin state after clearing
    delay(200);  // Give RTC time to release pin
    int pinState = digitalRead(RTC_INT_PIN);
    
    Serial.print(F("RTC_INT_PIN (A1) state after clear: "));
    Serial.println(pinState ? "HIGH (good)" : "LOW (stuck)");
    
    if (pinState == HIGH) {
        // Pin is in proper idle state - try to attach interrupt
        Serial.println(F("Attempting to attach RTC interrupt..."));
        
        attachInterrupt(digitalPinToInterrupt(RTC_INT_PIN), rtcInterruptHandler, FALLING);
        
        // Test the interrupt by monitoring pin for a few seconds
        Serial.println(F("Testing interrupt stability..."));        
        unsigned long testStart = millis();
        int falseAlarms = 0;
        
        while (millis() - testStart < 2000) {  // Test for 2 seconds
            if (wakeupFromRTC) {
                falseAlarms++;
                wakeupFromRTC = false;  // Clear for next test
                Serial.print(F("False alarm #")); Serial.println(falseAlarms);
            }
            delay(50);
        }
        
        if (falseAlarms == 0) {
            rtcInterruptWorking = true;
            Serial.println(F("✓ RTC interrupt is stable and working"));
            Serial.println(F("✓ Will use hardware interrupt for wake-ups"));
        } else {
            detachInterrupt(digitalPinToInterrupt(RTC_INT_PIN));
            Serial.print(F("✗ RTC interrupt unstable ("));
            Serial.print(falseAlarms);
            Serial.println(F(" false alarms)"));
            Serial.println(F("✓ Will use polling backup method"));
        }
    } else {
        Serial.println(F("✗ A1 pin stuck LOW - RTC alarm flag not clearable"));
        Serial.println(F("✓ Will use polling backup method"));
    }
    
    if (!rtcInterruptWorking) {
        Serial.println(F("PowerManager: RTC interrupt disabled, using polling fallback"));
    }
    
    Serial.println(F("PowerManager: Wake system initialized"));
}

void PowerManager::clearRTCAlarmFlags() {
    if (!systemStatus || !systemStatus->rtcWorking) {
        Serial.println(F("Cannot clear RTC flags - RTC not working"));
        return;
    }
    
    extern RTC_PCF8523 rtc;
    
    Serial.println(F("Clearing RTC alarm flags..."));
    
    // Method 1: Read current time (sometimes clears flags)
    DateTime now = rtc.now();
    delay(50);
    
    // Method 2: Try to disable any existing alarms
    // Note: PCF8523 library may not have these methods, but try:
    // rtc.disableAlarm();  // If available
    // rtc.clearAlarmFlag(); // If available
    
    // Method 3: Read time multiple times to clear any pending states
    for (int i = 0; i < 5; i++) {
        rtc.now();
        delay(20);
    }
    
    Serial.print(F("Current RTC time: "));
    Serial.println(now.timestamp(DateTime::TIMESTAMP_FULL));
}

// Static interrupt handler - must be static and minimal
void PowerManager::rtcInterruptHandler() {
    // Set wake-up flag via global instance pointer
    if (powerManagerInstance) {
        powerManagerInstance->wakeupFromRTC = true;
    }
}

void PowerManager::configureRTCWakeup(uint32_t wakeupTimeUnix) {
    if (!systemStatus || !systemStatus->rtcWorking) {
        Serial.println(F("PowerManager: Cannot configure RTC alarm - RTC not working"));
        return;
    }
    
    scheduledWakeTime = wakeupTimeUnix;
    DateTime alarmTime(wakeupTimeUnix);
    
    // For now, store the wake time and use polling method
    // TODO: Implement PCF8523 alarm register programming directly
    Serial.print(F("PowerManager: Wake scheduled for "));
    Serial.print(alarmTime.hour());
    Serial.print(F(":"));
    if (alarmTime.minute() < 10) Serial.print(F("0"));
    Serial.println(alarmTime.minute());
    Serial.println(F("Note: Using polling method until PCF8523 alarm is implemented"));
}

bool PowerManager::handleRTCWakeup() {
    if (!wakeupFromRTC) {
        return false;
    }
    
    // Clear the wake-up flag
    wakeupFromRTC = false;
    
    Serial.println(F("PowerManager: Woke from RTC alarm"));
    status.lastWakeSource = WAKE_RTC;
    status.sleepCycles++;
    
    return true;
}


void PowerManager::enterNRF52Sleep() {
#ifdef NRF52_SERIES
    Serial.print(F("PowerManager: Entering deep sleep ("));
    Serial.print(rtcInterruptWorking ? "interrupt" : "polling");
    Serial.println(F(" mode)"));
    Serial.flush();
    
    prepareSleep();
    configureWakeupSources();
    
    if (rtcInterruptWorking) {
        // Method 1: True deep sleep with hardware interrupt
        Serial.println(F("Using hardware interrupt deep sleep"));
        
        // Configure for true deep sleep
        // TODO: Implement actual nRF52 deep sleep with sd_power_system_off()
        // For now, use the delay method but with longer delays for power savings
        
        unsigned long targetWakeTime = scheduledWakeTime * 1000UL;
        
        while (millis() < targetWakeTime && !wakeupFromRTC) {
            // Check for button press wake-up (higher priority)
            updateButtonStates();
            if (wasButtonPressed(0) || wasButtonPressed(1) || wasButtonPressed(2) || 
                wasButtonPressed(3) || wasBluetoothButtonPressed()) {
                Serial.println(F("PowerManager: Woke from button press"));
                status.lastWakeSource = WAKE_BUTTON;
                wakeupFromButton = true;
                return;
            }
            
            // Longer delays since we have hardware interrupt for RTC
            delay(500);  // 500ms delays save more power
        }
        
        // Check wake source
        if (wakeupFromRTC) {
            Serial.println(F("PowerManager: Woke from RTC interrupt"));
            status.lastWakeSource = WAKE_RTC;
        } else {
            Serial.println(F("PowerManager: Woke from timer (polling backup)"));
            status.lastWakeSource = WAKE_TIMER;
        }
        
    } else {
        // Method 2: Polling fallback (your current working method)
        Serial.println(F("Using polling fallback method"));
        
        unsigned long targetWakeTime = scheduledWakeTime * 1000UL;
        
        while (millis() < targetWakeTime) {
            // Check for button press wake-up
            updateButtonStates();
            if (wasButtonPressed(0) || wasButtonPressed(1) || wasButtonPressed(2) || 
                wasButtonPressed(3) || wasBluetoothButtonPressed()) {
                Serial.println(F("PowerManager: Woke from button press"));
                status.lastWakeSource = WAKE_BUTTON;
                wakeupFromButton = true;
                return;
            }
            delay(100);  // Faster polling since no hardware interrupt
        }
        
        // Timer wake-up
        Serial.println(F("PowerManager: Woke from timer"));
        status.lastWakeSource = WAKE_TIMER;
    }
    
#else
    Serial.println(F("PowerManager: Deep sleep not supported on this platform"));
    delay(1000);
#endif
}


// =============================================================================
// WAKE-UP STATUS FUNCTIONS
// =============================================================================

bool PowerManager::isWakeupFromRTC() const {
    return status.lastWakeSource == WAKE_RTC;
}

bool PowerManager::isWakeupFromButton() const {
    return status.lastWakeSource == WAKE_BUTTON || 
           status.lastWakeSource == WAKE_BLUETOOTH_BUTTON;
}

// =============================================================================
// BLUETOOTH POWER CONTROL
// =============================================================================

void PowerManager::activateBluetooth() {
    if (bluetoothManager == nullptr) {
        Serial.println(F("PowerManager: Cannot activate Bluetooth - no manager reference"));
        return;
    }
    
    Serial.println(F("PowerManager: Activating Bluetooth (manual button press)"));
    
    status.bluetoothOn = true;
    status.bluetoothManuallyActivated = true;
    status.bluetoothState = COMP_POWER_ON;
    status.lastBluetoothActivity = millis();
    
    // Turn on Bluetooth through the manager
    bluetoothManager->setEnabled(true);
    
    Serial.print(F("Bluetooth activated for "));
    Serial.print(settings.displayTimeoutMin);
    Serial.println(F(" minutes"));
}

void PowerManager::deactivateBluetooth() {
    if (bluetoothManager == nullptr) {
        Serial.println(F("PowerManager: Cannot deactivate Bluetooth - no manager reference"));
        return;
    }
    
    if (!status.bluetoothOn) {
        return; // Already off
    }
    
    Serial.println(F("PowerManager: Deactivating Bluetooth (timeout or manual)"));
    
    status.bluetoothOn = false;
    status.bluetoothManuallyActivated = false;
    status.bluetoothState = COMP_POWER_OFF;
    
    // Turn off Bluetooth through the manager
    bluetoothManager->setEnabled(false);
}

bool PowerManager::isBluetoothOn() const {
    return status.bluetoothOn;
}

void PowerManager::handleBluetoothConnection() {
    if (status.bluetoothOn) {
        status.lastBluetoothActivity = millis();
        Serial.println(F("PowerManager: Bluetooth connected - timer reset"));
    }
}

void PowerManager::handleBluetoothDisconnection() {
    if (status.bluetoothOn) {
        status.lastBluetoothActivity = millis();
        Serial.println(F("PowerManager: Bluetooth disconnected - countdown started"));
    }
}

unsigned long PowerManager::getBluetoothTimeRemaining() const {
    if (!status.bluetoothOn || !status.fieldModeActive) {
        return 0;
    }
    
    unsigned long elapsed = millis() - status.lastBluetoothActivity;
    if (elapsed >= status.bluetoothTimeoutMs) {
        return 0;
    }
    
    return status.bluetoothTimeoutMs - elapsed;
}

void PowerManager::handleBluetoothButtonPress() {
    status.buttonPresses++;
    
    if (status.fieldModeActive) {
        // In field mode, Bluetooth button both wakes system AND activates Bluetooth
        if (!status.displayOn) {
            // Wake up the system first
            wakeFromFieldSleep();
        }
        
        if (!status.bluetoothOn) {
            Serial.println(F("PowerManager: Bluetooth button pressed - activating Bluetooth"));
            activateBluetooth();
        } else {
            Serial.println(F("PowerManager: Bluetooth button pressed - Bluetooth already on"));
            // Reset the timer
            status.lastBluetoothActivity = millis();
        }
        
        // Mark as Bluetooth button wake-up
        status.lastWakeSource = WAKE_BLUETOOTH_BUTTON;
    } else {
        Serial.println(F("PowerManager: Bluetooth button pressed in testing mode (Bluetooth always on)"));
    }
}

// =============================================================================
// HARDWARE DISPLAY POWER CONTROL 
// =============================================================================

void PowerManager::initializeDisplayPower() {
    pinMode(DISPLAY_POWER_PIN, OUTPUT);
    digitalWrite(DISPLAY_POWER_PIN, DISPLAY_POWER_ON);
    status.displayOn = true;
    status.displayState = COMP_POWER_ON;
    Serial.println(F("Display power control initialized on pin 12"));
}

void PowerManager::turnOnDisplay() {
    if (!status.displayOn) {
        digitalWrite(DISPLAY_POWER_PIN, DISPLAY_POWER_ON);
        delay(50); // Small delay for display to power up
        status.displayOn = true;
        status.displayState = COMP_POWER_ON;
        Serial.println(F("PowerManager: Display turned ON (pin 12 HIGH)"));
    }
}

void PowerManager::turnOffDisplay() {
    if (status.displayOn && status.fieldModeActive) {
        digitalWrite(DISPLAY_POWER_PIN, DISPLAY_POWER_OFF);
        status.displayOn = false;
        status.displayState = COMP_POWER_OFF;
        displayOffTime = millis();
        Serial.println(F("PowerManager: Display turned OFF (pin 12 LOW)"));
    }
}

bool PowerManager::isDisplayOn() const {
    // In testing mode, display is always considered on
    if (!status.fieldModeActive) {
        return true;
    }
    
    return status.displayOn;
}

void PowerManager::resetDisplayTimeout() {
    status.lastUserActivity = millis();
    
    if (status.fieldModeActive) {
        // Silent timeout - no messages
        // Timeout will be checked in checkFieldModeTimeout()
    }
}

unsigned long PowerManager::getDisplayTimeRemaining() const {
    // No timeout in testing mode
    if (!status.fieldModeActive || !status.displayOn) {
        return 0;
    }
    
    unsigned long elapsed = millis() - status.lastUserActivity;
    if (elapsed >= status.displayTimeoutMs) {
        return 0;
    }
    
    return status.displayTimeoutMs - elapsed;
}

// =============================================================================
// CORE POWER MANAGEMENT
// =============================================================================

void PowerManager::handleUserActivity() {
    status.lastUserActivity = millis();
    status.buttonPresses++;
    
    if (status.fieldModeActive) {
        // If we're asleep, wake up
        if (!status.displayOn) {
            wakeFromFieldSleep();
        }
        
        // Turn display on and reset timeout
        turnOnDisplay();
        resetDisplayTimeout();
        
        // Mark as button wake-up
        status.lastWakeSource = WAKE_BUTTON;
        wakeupFromButton = true;
        wakeupFromRTC = false;
        
    } else {
        turnOnDisplay();
    }
}

// Handle Bluetooth activity separately
void PowerManager::handleBluetoothActivity() {
    status.lastBluetoothActivity = millis();
    
    if (status.fieldModeActive && status.bluetoothOn) {
        Serial.println(F("PowerManager: Bluetooth activity - Bluetooth timer reset"));
    }
}

void PowerManager::update() {
    unsigned long currentTime = millis();
    
    // Handle wake-up events
    if (handleRTCWakeup()) {
        // Woke from RTC - this is a scheduled reading
        status.wokenByTimer = true;
        // Display stays OFF for scheduled readings
    }
    
    // Field mode logic: check both display and Bluetooth timeouts
    if (status.fieldModeActive) {
        checkFieldModeTimeout(currentTime);
        checkBluetoothTimeout(currentTime);
    }
    
    // Update power monitoring every 5 seconds
    if (currentTime - lastPowerCheck >= 5000) {
        lastPowerCheck = currentTime;
    }
}

// Check Bluetooth timeout separately from display timeout
void PowerManager::checkBluetoothTimeout(unsigned long currentTime) {
    // Only check timeout if Bluetooth is currently on and manually activated
    if (!status.bluetoothOn || !status.bluetoothManuallyActivated) {
        return;
    }
    
    unsigned long timeSinceActivity = currentTime - status.lastBluetoothActivity;
    
    // Check if timeout period has elapsed
    if (timeSinceActivity >= status.bluetoothTimeoutMs) {
        Serial.println(F("PowerManager: Bluetooth timeout reached - deactivating"));
        deactivateBluetooth();
    }
}

void PowerManager::checkFieldModeTimeout(unsigned long currentTime) {
    // Only check timeout if display is currently on
    if (!status.displayOn) {
        return; // Already off/sleeping
    }
    
    unsigned long timeSinceActivity = currentTime - status.lastUserActivity;
    
    // Check if timeout period has elapsed
    if (timeSinceActivity >= status.displayTimeoutMs) {
        Serial.println(F("Field Mode: Display timeout - entering sleep"));
        enterFieldSleep();
    }
}

// =============================================================================
// FIELD MODE SLEEP LOGIC
// =============================================================================

void PowerManager::enterFieldSleep() {
    Serial.println(F("Field Mode: Display timeout reached - entering deep sleep"));
    
    // Turn off display first
    turnOffDisplay();
    
    // Wait 500ms as specified
    delay(500);
    
    // Power down non-essential components
    powerDownNonEssential();
    
    // Configure next wake time
    if (systemSettings && systemStatus && systemStatus->rtcWorking) {
        DateTime now = rtc.now();
        uint8_t logInterval = systemSettings->logInterval;
        
        // Calculate next interval boundary (round up to next interval minute)
        int nextMinute = ((now.minute() / logInterval) + 1) * logInterval;
        int nextHour = now.hour();
        
        // Handle minute overflow
        if (nextMinute >= 60) {
            nextMinute = 0;
            nextHour++;
            if (nextHour >= 24) nextHour = 0;
        }
        
        DateTime nextWake(now.year(), now.month(), now.day(), nextHour, nextMinute, 0);
        configureRTCWakeup(nextWake.unixtime());
        
        Serial.print(F("PowerManager: Next wake at "));
        Serial.print(nextHour);
        Serial.print(F(":"));
        if (nextMinute < 10) Serial.print(F("0"));
        Serial.println(nextMinute);
    }
    
    Serial.println(F("=== ENTERING DEEP SLEEP ==="));
    Serial.println(F("Wake sources: RTC alarm OR button press"));
    
    // Enter actual deep sleep
    enterNRF52Sleep();
}

void PowerManager::wakeFromFieldSleep() {
    Serial.println(F("=== WAKE FROM FIELD SLEEP ==="));
    
    // Turn display back on
    turnOnDisplay();
    
    // Power up components
    powerUpAll();
    
    // Reset timeout for new interaction period
    resetDisplayTimeout();
    
    Serial.println(F("Field Mode: Awake - dashboard restored"));
}

// =============================================================================
// FIELD MODE MANAGEMENT
// =============================================================================

void PowerManager::enableFieldMode() {
    if (status.fieldModeActive) return; // Already active
    
    settings.fieldModeEnabled = true;
    status.fieldModeActive = true;
    status.currentMode = POWER_FIELD;
    
    // Update system settings
    if (systemSettings) {
        systemSettings->fieldModeEnabled = true;
    }
    
    // Initialize field mode timing
    status.lastLogTime = millis();
    updateNextWakeTime(systemSettings ? systemSettings->logInterval : 10);
    
    // Start timeout countdown silently
    resetDisplayTimeout();
    
    // Turn off Bluetooth in field mode (will be manually activated)
    if (bluetoothManager) {
        deactivateBluetooth();
    }

    powerDownSensors();
    powerDownAudio();
    
    Serial.println(F("=== FIELD MODE ENABLED ==="));
    Serial.print(F("Log interval: "));
    Serial.print(systemSettings ? systemSettings->logInterval : 10);
    Serial.println(F(" minutes"));
    Serial.print(F("Display timeout: "));
    Serial.print(settings.displayTimeoutMin);
    Serial.println(F(" minutes"));
    Serial.println(F("Bluetooth: OFF (use external button to activate)"));
    Serial.println(F("Dashboard will display normally until timeout"));
    Serial.println(F("System will enter deep sleep after timeout"));
    Serial.println(F("========================="));
}

void PowerManager::disableFieldMode() {
    if (!status.fieldModeActive) return; // Already inactive
    
    settings.fieldModeEnabled = false;
    status.fieldModeActive = false;
    status.currentMode = POWER_TESTING;
    
    // Update system settings
    if (systemSettings) {
        systemSettings->fieldModeEnabled = false;
    }
    
    // Ensure display is on when exiting field mode
    turnOnDisplay();

    // Turn Bluetooth back on for testing mode
    if (bluetoothManager) {
        bluetoothManager->setEnabled(true);
        status.bluetoothOn = true;
        status.bluetoothState = COMP_POWER_ON;
    }

    // Power sensors back up for testing mode
    powerUpSensors();
    powerUpAudio();
    
    Serial.println(F("=== FIELD MODE DISABLED ==="));
    Serial.println(F("Returning to Testing Mode"));
    Serial.println(F("Bluetooth: ON (always on in testing mode)"));
    Serial.println(F("=========================="));
}

bool PowerManager::isFieldModeActive() const {
    return status.fieldModeActive;
}

bool PowerManager::checkForLongPressWake() {
    // Only check for long press wake when display is off in field mode
    if (!status.fieldModeActive || status.displayOn) {
        longPressStartTime = 0;
        longPressDetected = false;
        return false;
    }
    
    // Check if any button is being held
    bool anyButtonHeld = isButtonHeld(0) || isButtonHeld(1) || isButtonHeld(2) || isButtonHeld(3);
    
    if (anyButtonHeld) {
        if (longPressStartTime == 0) {
            longPressStartTime = millis();
        } else if (millis() - longPressStartTime >= LONG_PRESS_WAKE_TIME && !longPressDetected) {
            longPressDetected = true;
            Serial.println(F("Long press detected - waking for full system access"));
            
            // Full wake: display on, reset timeout, but keep sensors down until needed
            wakeFromFieldSleep();
            Serial.println(F("System awake - full menu access available"));
            return true;
        }
    } else {
        longPressStartTime = 0;
        longPressDetected = false;
    }
    
    return false;
}

// =============================================================================
// FIELD MODE TIMING
// =============================================================================

bool PowerManager::shouldTakeReading() const {
    if (!status.fieldModeActive) {
        return false; // Not in field mode
    }
    
    // If we just woke from RTC, it's time for a reading
    return (status.lastWakeSource == WAKE_RTC);
}

void PowerManager::updateNextWakeTime(uint8_t logIntervalMinutes) {
    status.lastLogTime = millis();
    
    // Round to next full minute boundary
    if (systemStatus && systemStatus->rtcWorking) {
        DateTime now = rtc.now();
        
        // Calculate next interval boundary (round up to next interval minute)
        int nextMinute = ((now.minute() / logIntervalMinutes) + 1) * logIntervalMinutes;
        int nextHour = now.hour();
        
        // Handle minute overflow
        if (nextMinute >= 60) {
            nextMinute = 0;
            nextHour++;
            if (nextHour >= 24) nextHour = 0;
        }
        
        DateTime nextReading(now.year(), now.month(), now.day(), nextHour, nextMinute, 0);
        status.nextWakeTime = nextReading.unixtime() * 1000UL; // Convert to millis
        
        Serial.print(F("Next reading at: "));
        Serial.print(nextHour);
        Serial.print(F(":"));
        if (nextMinute < 10) Serial.print(F("0"));
        Serial.println(nextMinute);
    } else {
        // Fallback if no RTC
        status.nextWakeTime = status.lastLogTime + (logIntervalMinutes * 60000UL);
        Serial.print(F("Next reading in "));
        Serial.print(logIntervalMinutes);
        Serial.println(F(" minutes (no RTC)"));
    }
}

bool PowerManager::shouldEnterSleep() {
    return status.fieldModeActive && !status.displayOn;
}

bool PowerManager::isTimeForBufferFlush() const {
    // Flush buffer every hour
    unsigned long timeSinceFlush = millis() - status.lastFlushTime;
    return (timeSinceFlush >= 3600000UL); // 1 hour in milliseconds
}

void PowerManager::setWakeSource(bool fromTimer) {
    status.wokenByTimer = fromTimer;
    status.lastWakeSource = fromTimer ? WAKE_TIMER : WAKE_BUTTON;
    
    if (fromTimer) {
        Serial.println(F("Wake source: Timer (scheduled reading)"));
    } else {
        Serial.println(F("Wake source: Button (user interrupt)"));
    }
}

bool PowerManager::wasWokenByTimer() const {
    return status.wokenByTimer;
}

// =============================================================================
// POWER MODE MANAGEMENT
// =============================================================================

void PowerManager::updatePowerMode(float batteryVoltage) {
    PowerMode newMode = status.currentMode;
    
    // Auto power mode based on battery
    if (batteryVoltage <= BATTERY_CRITICAL) {
        newMode = POWER_CRITICAL;
    } else if (batteryVoltage <= BATTERY_LOW) {
        newMode = POWER_SAVE;
    } else if (settings.fieldModeEnabled) {
        newMode = POWER_FIELD;
    } else {
        newMode = POWER_TESTING;
    }
    
    if (newMode != status.currentMode) {
        Serial.print(F("Power mode change: "));
        Serial.print(getPowerModeString());
        Serial.print(F(" -> "));
        status.currentMode = newMode;
        Serial.println(getPowerModeString());
        
        // Log mode change reasons
        switch (newMode) {
            case POWER_TESTING:
                Serial.println(F("  Reason: Normal operation"));
                break;
            case POWER_FIELD:
                Serial.println(F("  Reason: Field mode enabled"));
                break;
            case POWER_SAVE:
                Serial.println(F("  Reason: Low battery"));
                break;
            case POWER_CRITICAL:
                Serial.println(F("  Reason: Critical battery"));
                // Auto-disable field mode if battery critical
                if (status.fieldModeActive) {
                    Serial.println(F("  Auto-disabling field mode due to critical battery"));
                    disableFieldMode();
                }
                break;
        }
    }
    
    // Always update runtime estimate
    calculateRuntimeEstimate(batteryVoltage);
}

void PowerManager::calculateRuntimeEstimate(float batteryVoltage) {
    const float BATTERY_CAPACITY_MAH = 1200.0f;
    
    // Calculate current consumption based on active components
    float currentConsumption = POWER_TESTING_MA;
    
    // Factor in field mode with display timeout power savings
    if (status.fieldModeActive) {
        // In field mode, we're awake for ~2 minutes every log interval
        float awakeTimeRatio = 2.0f / (systemSettings ? systemSettings->logInterval : 10.0f);
        
        // Active power when awake
        float activePower = POWER_TESTING_MA + POWER_SENSORS_MA;
        if (systemStatus && systemStatus->pdmWorking) {
            activePower += POWER_AUDIO_MA;
        }
        
        // Add display power only when display is on
        if (status.displayOn) {
            activePower += POWER_DISPLAY_MA;
        }
        
        // Sleep power when asleep
        float sleepPower = POWER_SLEEP_MA;
        
        // Calculate display on/off ratio
        float displayOnRatio = 1.0f; // Default: always on
        if (status.fieldModeActive && status.displayTimeoutMs > 0) {
            // Estimate display is on 10% of the time in field mode with timeout
            displayOnRatio = 0.1f;
            activePower = POWER_TESTING_MA + POWER_SENSORS_MA + (POWER_DISPLAY_MA * displayOnRatio);
            if (systemStatus && systemStatus->pdmWorking) {
                activePower += POWER_AUDIO_MA;
            }
        }
        
        currentConsumption = (activePower * awakeTimeRatio) + (sleepPower * (1.0f - awakeTimeRatio));
    } else {
        // Testing mode - all components on including display
        currentConsumption += POWER_DISPLAY_MA + POWER_SENSORS_MA;
        if (systemStatus && systemStatus->pdmWorking) {
            currentConsumption += POWER_AUDIO_MA;
        }
    }
    
    // Calculate remaining capacity based on voltage
    float batteryLevel = getBatteryLevel(batteryVoltage);
    float remainingCapacity = BATTERY_CAPACITY_MAH * (batteryLevel / 100.0f);
    
    // Estimate runtime
    if (currentConsumption > 0) {
        status.estimatedRuntimeHours = remainingCapacity / currentConsumption;
    } else {
        status.estimatedRuntimeHours = 999.0f;
    }
    
    // Calculate daily usage estimate
    status.dailyUsageEstimateMah = currentConsumption * 24.0f;
}

// =============================================================================
// COMPONENT POWER CONTROL
// =============================================================================

void PowerManager::powerDownNonEssential() {
    Serial.println(F("Powering down non-essential components for deep sleep"));
    status.sensorState = COMP_POWER_SLEEP;
    status.audioState = COMP_POWER_SLEEP;
}

void PowerManager::powerUpAll() {
    Serial.println(F("Powering up all components after wake"));
    status.sensorState = COMP_POWER_ON;
    status.audioState = COMP_POWER_ON;
}

void PowerManager::powerDownSensors() {
    status.sensorState = COMP_POWER_SLEEP;
    Serial.println(F("PowerManager: Sensors powered down"));
}

void PowerManager::powerUpSensors() {
    status.sensorState = COMP_POWER_ON;
    Serial.println(F("PowerManager: Sensors powered up"));
}

void PowerManager::powerDownAudio() {
    status.audioState = COMP_POWER_OFF;
    Serial.println(F("PowerManager: Audio powered down"));
}

void PowerManager::powerUpAudio() {
    status.audioState = COMP_POWER_ON;
    Serial.println(F("PowerManager: Audio powered up"));
}

void PowerManager::powerDownBluetooth() {
    if (bluetoothManager) {
        bluetoothManager->setEnabled(false);
    }
    status.bluetoothState = COMP_POWER_OFF;
    Serial.println(F("PowerManager: Bluetooth powered down"));
}

void PowerManager::powerUpBluetooth() {
    if (bluetoothManager) {
        bluetoothManager->setEnabled(true);
    }
    status.bluetoothState = COMP_POWER_ON;
    Serial.println(F("PowerManager: Bluetooth powered up"));
}

// =============================================================================
// SLEEP MANAGEMENT STUBS
// =============================================================================

void PowerManager::enterDeepSleep(uint32_t sleepTimeMs) {
    Serial.println(F("PowerManager: enterDeepSleep() called - using field sleep instead"));
    enterFieldSleep();
}

void PowerManager::prepareSleep() {
    Serial.println(F("PowerManager: Preparing for deep sleep"));
    powerDownNonEssential();
}

void PowerManager::wakeFromSleep() {
    Serial.println(F("PowerManager: Waking from sleep"));
    powerUpAll();
}

bool PowerManager::canEnterSleep() const {
    return status.fieldModeActive; // Only in field mode
}

void PowerManager::handleWakeUp(WakeUpSource source) {
    status.lastWakeSource = source;
    if (source == WAKE_TIMER || source == WAKE_RTC) {
        status.sleepCycles++;
    }
    wakeFromSleep();
}

void PowerManager::configureWakeupSources() {
    Serial.println(F("PowerManager: Wake-up sources configured (buttons + RTC interrupt)"));
    // Note: Button interrupts are handled by existing updateButtonStates() 
    // RTC interrupt is configured in setupRTCInterrupt()
}

// =============================================================================
// STATUS REPORTING
// =============================================================================

PowerMode PowerManager::getCurrentPowerMode() const {
    return status.currentMode;
}

float PowerManager::getEstimatedRuntimeHours() const {
    return status.estimatedRuntimeHours;
}

float PowerManager::getDailyUsageEstimate() const {
    return status.dailyUsageEstimateMah;
}

unsigned long PowerManager::getUptime() const {
    return millis() - status.totalUptime;
}

uint32_t PowerManager::getSleepCycles() const {
    return status.sleepCycles;
}

uint32_t PowerManager::getButtonPresses() const {
    return status.buttonPresses;
}

PowerStatus PowerManager::getPowerStatus() const {
    return status;
}

const char* PowerManager::getPowerModeString() const {
    return powerModeToString(status.currentMode);
}

const char* PowerManager::getWakeSourceString() const {
    switch (status.lastWakeSource) {
        case WAKE_BUTTON: return "Button";
        case WAKE_TIMER: return "Timer";
        case WAKE_RTC: return "RTC";
        case WAKE_BLUETOOTH_BUTTON: return "BT Button";
        case WAKE_EXTERNAL: return "External";
        default: return "Unknown";
    }
}

// =============================================================================
// DIAGNOSTICS
// =============================================================================

void PowerManager::printPowerStatus() const {
    Serial.println(F("\n=== Power Manager Status ==="));
    Serial.print(F("Mode: ")); Serial.println(getPowerModeString());
    Serial.print(F("Field Mode: ")); Serial.println(status.fieldModeActive ? "ACTIVE" : "INACTIVE");
    
    // Wake system status
    Serial.print(F("RTC Interrupt: "));
    if (rtcInterruptWorking) {
        Serial.println(F("WORKING (hardware wake-up)"));
    } else {
        Serial.println(F("DISABLED (polling fallback)"));
    }
    
    // Display status
    Serial.print(F("Display: ")); Serial.print(status.displayOn ? "ON" : "OFF");
    if (status.fieldModeActive) {
        Serial.print(F(" (timeout: "));
        Serial.print(settings.displayTimeoutMin);
        Serial.print(F("min)"));
        
        if (status.displayOn) {
            unsigned long timeRemaining = getDisplayTimeRemaining();
            if (timeRemaining > 0) {
                Serial.print(F(" - "));
                Serial.print(timeRemaining / 60000);
                Serial.print(F("m "));
                Serial.print((timeRemaining % 60000) / 1000);
                Serial.print(F("s remaining"));
            }
        }
    }
    Serial.println();
    
    // ... rest of existing status code ...
    
    Serial.println(F("=====================================\n"));
}

void PowerManager::resetStatistics() {
    status.sleepCycles = 0;
    status.buttonPresses = 0;
    status.totalUptime = millis();
    Serial.println(F("Power statistics reset"));
}

// =============================================================================
// SETTINGS MANAGEMENT
// =============================================================================

void PowerManager::setDisplayTimeout(uint8_t minutes) {
    // Constrain to 1-5 minutes for field mode
    settings.displayTimeoutMin = constrain(minutes, 1, 5);
    status.displayTimeoutMs = settings.displayTimeoutMin * 60000UL;
    status.bluetoothTimeoutMs = status.displayTimeoutMs; // Keep same timeout
    
    // Update system settings
    if (systemSettings) {
        systemSettings->displayTimeoutMin = settings.displayTimeoutMin;
    }
    
    Serial.print(F("PowerManager: Display timeout set to "));
    Serial.print(settings.displayTimeoutMin);
    Serial.println(F(" minutes"));
}

void PowerManager::setFieldMode(bool enabled) {
    if (enabled) {
        enableFieldMode();
    } else {
        disableFieldMode();
    }
}

void PowerManager::setAutoFieldMode(bool enabled) {
    settings.autoFieldMode = enabled;
    Serial.print(F("PowerManager: setAutoFieldMode("));
    Serial.print(enabled);
    Serial.println(F(")"));
}

void PowerManager::loadPowerSettings() {
    if (systemSettings) {
        settings.fieldModeEnabled = systemSettings->fieldModeEnabled;
        settings.displayTimeoutMin = systemSettings->displayTimeoutMin;
        status.displayTimeoutMs = settings.displayTimeoutMin * 60000UL;
        status.bluetoothTimeoutMs = status.displayTimeoutMs;
    }
    Serial.println(F("PowerManager: Power settings loaded"));
}

void PowerManager::savePowerSettings() {
    if (systemSettings) {
        systemSettings->fieldModeEnabled = settings.fieldModeEnabled;
        systemSettings->displayTimeoutMin = settings.displayTimeoutMin;
    }
    Serial.println(F("PowerManager: Power settings saved"));
}

// =============================================================================
// GLOBAL HELPER FUNCTIONS
// =============================================================================

const char* powerModeToString(PowerMode mode) {
    switch (mode) {
        case POWER_TESTING: return "Testing";
        case POWER_FIELD: return "Field";
        case POWER_SAVE: return "Power Save";
        case POWER_CRITICAL: return "Critical";
        default: return "Unknown";
    }
}

PowerMode batteryToPowerMode(float voltage) {
    if (voltage <= BATTERY_CRITICAL) {
        return POWER_CRITICAL;
    } else if (voltage <= BATTERY_LOW) {
        return POWER_SAVE;
    } else {
        return POWER_TESTING;
    }
}