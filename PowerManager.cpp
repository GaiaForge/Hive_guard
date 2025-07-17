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
#include <nrf_soc.h>
#include <nrf_nvic.h>
#include <nrf_gpio.h>
#endif

extern RTC_PCF8523 rtc;  // Reference to global RTC object

// Global pointer for interrupt handler access
static PowerManager* powerManagerInstance = nullptr;

// Retained state in special RAM section that survives System OFF
__attribute__((section(".noinit"))) static RetainedState retainedState;

// =============================================================================
// POWER CONSUMPTION CONSTANTS (mA)
// =============================================================================

const float PowerManager::POWER_TESTING_MA = 15.0f;
const float PowerManager::POWER_DISPLAY_MA = 8.0f;
const float PowerManager::POWER_SENSORS_MA = 2.0f;
const float PowerManager::POWER_AUDIO_MA = 5.0f;
const float PowerManager::POWER_BLUETOOTH_MA = 12.0f;
const float PowerManager::POWER_SLEEP_MA = 1.0f;      // Polling sleep
const float PowerManager::POWER_DEEP_SLEEP_MA = 0.001f; // True deep sleep

// =============================================================================
// CONSTRUCTOR AND INITIALIZATION
// =============================================================================

PowerManager::PowerManager() {
    // Initialize status
    status.currentMode = POWER_TESTING;
    status.fieldModeActive = false;
    status.displayOn = true;
    status.displayTimeoutMs = 0;
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
    
    // NEW: Deep sleep variables
    status.deepSleepCapable = false;
    status.deepSleepCycles = 0;
    status.wakeFromDeepSleep = false;
    
    // Initialize settings
    settings.fieldModeEnabled = false;
    settings.displayTimeoutMin = 2;
    settings.sleepIntervalMin = 10;
    settings.autoFieldMode = false;
    settings.criticalBatteryLevel = 15;
    settings.useDeepSleep = true;  // Enable by default
    
    // Initialize timing
    lastPowerCheck = 0;
    displayOffTime = 0;
    lastSleepTime = 0;
    rtcInterruptWorking = false;
       
    systemStatus = nullptr;
    systemSettings = nullptr;
    bluetoothManager = nullptr;

    longPressStartTime = 0;
    longPressDetected = false;
    
    // Initialize wake-up variables
    wakeupFromRTC = false;
    wakeupFromButton = false;
    scheduledWakeTime = 0;
    
    powerManagerInstance = this;
}

void PowerManager::initialize(SystemStatus* sysStatus, SystemSettings* sysSettings) {
    systemStatus = sysStatus;
    systemSettings = sysSettings;
    
    // Initialize display power control hardware
    initializeDisplayPower();

    // Initialize retained state (clear on normal boots)
    // Note: This will be overridden by restoreRetainedState() if called later
    memset(&retainedState, 0, sizeof(retainedState));

    // Initialize Bluetooth button
    pinMode(BTN_BLUETOOTH, INPUT_PULLUP);
    Serial.println(F("Bluetooth button initialized on pin 13"));

    // Load display timeout settings from system settings
    if (sysSettings) {
        settings.fieldModeEnabled = sysSettings->fieldModeEnabled;
        settings.displayTimeoutMin = constrain(sysSettings->displayTimeoutMin, 1, 5);
        status.displayTimeoutMs = settings.displayTimeoutMin * 60000UL;
        status.bluetoothTimeoutMs = status.displayTimeoutMs;
        
        if (settings.fieldModeEnabled) {
            enableFieldMode();
        }
    }
    
    status.lastUserActivity = millis();
    status.lastBluetoothActivity = millis();
    status.totalUptime = millis();
    status.lastFlushTime = millis();
    
    // Initialize deep sleep capability
        
    Serial.println(F("PowerManager initialized"));
    Serial.print(F("  - Deep sleep capable: "));
    Serial.println(status.deepSleepCapable ? "YES" : "NO");
    Serial.print(F("  - Display timeout: "));
    Serial.print(settings.displayTimeoutMin);
    Serial.println(F(" minutes"));
}

// Set Bluetooth manager reference
void PowerManager::setBluetoothManager(BluetoothManager* btManager) {
    bluetoothManager = btManager;
    Serial.println(F("PowerManager: Bluetooth manager reference set"));
}

// =============================================================================
// RETAINED STATE MANAGEMENT
// =============================================================================

void PowerManager::saveRetainedState() {
    Serial.println(F("Saving state to retained memory..."));
    
    retainedState.magic = RETAINED_MAGIC;
    retainedState.fieldModeActive = status.fieldModeActive;
    retainedState.deepSleepWake = true;
    retainedState.wakeReason = WAKE_RTC;
    retainedState.logInterval = systemSettings ? systemSettings->logInterval : 10;
    retainedState.nextWakeTime = status.nextWakeTime;
    
    // Calculate checksum
    retainedState.checksum = calculateRetainedChecksum(retainedState);
    
    Serial.print(F("Retained state: fieldMode="));
    Serial.print(retainedState.fieldModeActive);
    Serial.print(F(", logInterval="));
    Serial.println(retainedState.logInterval);
}

bool PowerManager::restoreRetainedState() {
    Serial.println(F("Checking retained memory..."));
    
    // Validate magic number
    if (retainedState.magic != RETAINED_MAGIC) {
        Serial.println(F("No valid retained state found (magic mismatch)"));
        return false;
    }
    
    // Validate checksum
    uint16_t expectedChecksum = calculateRetainedChecksum(retainedState);
    if (retainedState.checksum != expectedChecksum) {
        Serial.println(F("Retained state corrupted (checksum mismatch)"));
        clearRetainedState();
        return false;
    }
    
    // Validate that this was actually a deep sleep wake
    if (!retainedState.deepSleepWake) {
        Serial.println(F("Not a deep sleep wake"));
        return false;
    }
    
    Serial.println(F("✓ Valid retained state found"));
    Serial.print(F("Restoring field mode: "));
    Serial.println(retainedState.fieldModeActive ? "ACTIVE" : "INACTIVE");
    
    // Restore field mode state
    if (retainedState.fieldModeActive) {
        // Restore field mode without going through full initialization
        status.fieldModeActive = true;
        status.currentMode = POWER_FIELD;
        settings.fieldModeEnabled = true;
        
        // Update system settings
        if (systemSettings) {
            systemSettings->fieldModeEnabled = true;
            systemSettings->logInterval = retainedState.logInterval;
        }
        
        // Keep display OFF since this is a scheduled wake
        turnOffDisplay();
        
        // Power down non-essential components
        powerDownNonEssential();
        
        Serial.println(F("Field mode restored from retained state"));
        return true;
    }
    
    return false;
}

void PowerManager::clearRetainedState() {
    Serial.println(F("Clearing retained state"));
    memset(&retainedState, 0, sizeof(retainedState));
}

uint16_t PowerManager::calculateRetainedChecksum(const RetainedState& state) {
    uint16_t checksum = 0;
    const uint8_t* data = (const uint8_t*)&state;
    
    // Calculate checksum for all fields except the checksum field itself
    for (size_t i = 0; i < sizeof(RetainedState) - sizeof(uint16_t); i++) {
        checksum += data[i];
    }
    
    return checksum;
}

// =============================================================================
// DEEP SLEEP INITIALIZATION
// =============================================================================

bool PowerManager::initializeDeepSleep() {
#ifdef NRF52_SERIES
    if (!systemStatus || !systemStatus->rtcWorking) {
        Serial.println(F("Deep sleep disabled: RTC not working"));
        return false;
    }
    
    // Setup wake-up pin for deep sleep
    setupWakeupPin();
    
    // IMPORTANT: Ensure PCF8523 is properly started
    extern RTC_PCF8523 rtc;
    if (!rtc.isrunning()) {
        Serial.println(F("Starting PCF8523 oscillator"));
        rtc.start();
        delay(100); // Let oscillator stabilize
    }
    
    Serial.println(F("Deep sleep initialization complete"));
    return true;
#else
    Serial.println(F("Deep sleep not supported on this platform"));
    return false;
#endif
}

void PowerManager::initializeWakeDetection(WakeUpSource bootReason) {
    status.lastWakeSource = bootReason;
    
    if (bootReason == WAKE_RTC) {
        status.wakeFromDeepSleep = true;
        status.deepSleepCycles++;
        Serial.println(F("PowerManager: Detected wake from deep sleep"));
    } else {
        status.wakeFromDeepSleep = false;
        Serial.println(F("PowerManager: Normal boot sequence"));
    }
}

// =============================================================================
// DEEP SLEEP MANAGEMENT
// =============================================================================

void PowerManager::setupRTCInterrupt() {
    // This is now fallback only
    pinMode(RTC_INT_PIN, INPUT_PULLUP);
    wakeupFromRTC = false;
    wakeupFromButton = false;
    rtcInterruptWorking = false;
    
    Serial.println(F("RTC interrupt setup (polling fallback mode)"));
}

void PowerManager::clearRTCAlarmFlag() {
    Serial.println(F("Clearing PCF8523 alarm flag"));
    
    // Read Control_2 register
    Wire.beginTransmission(0x68);
    Wire.write(0x01);  // Control_2 register
    Wire.endTransmission();
    
    Wire.requestFrom(0x68, 1);
    if (Wire.available()) {
        uint8_t control2 = Wire.read();
        
        Serial.print(F("Control_2 before clear: 0x"));
        Serial.println(control2, HEX);
        
        // Clear the alarm flag (AF) bit (bit 3)
        control2 &= ~0x08;
        
        Wire.beginTransmission(0x68);
        Wire.write(0x01);
        Wire.write(control2);
        Wire.endTransmission();
        
        Serial.print(F("Control_2 after clear: 0x"));
        Serial.println(control2, HEX);
    }
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
    Serial.println(F("Entering polling-based sleep"));
    Serial.flush();
    
    prepareSleep();
    
    // Calculate how many milliseconds until the next wake time
    DateTime now = rtc.now();
    DateTime nextWake(scheduledWakeTime);
    long secondsUntilWake = nextWake.unixtime() - now.unixtime();

    // If negative or too far in future, wake in 5 minutes as fallback
    if (secondsUntilWake <= 0 || secondsUntilWake > 3600) {
        secondsUntilWake = 300;  // 5 minutes fallback
    }

    unsigned long targetWakeTime = millis() + (secondsUntilWake * 1000UL);
    
    // DEBUG: Show timing info
    Serial.print(F("scheduledWakeTime (unix): "));
    Serial.println(scheduledWakeTime);
    Serial.print(F("Current millis(): "));
    Serial.println(millis());
    Serial.print(F("Target wake millis: "));
    Serial.println(targetWakeTime);
    
    // Check if target time is in the past
    if (targetWakeTime <= millis()) {
        Serial.println(F("ERROR: Target wake time is in the past!"));
        Serial.println(F("Waking immediately"));
        status.lastWakeSource = WAKE_TIMER;
        return;
    }
    
    Serial.print(F("Will wake in: "));
    Serial.print((targetWakeTime - millis()) / 1000);
    Serial.println(F(" seconds"));
    
    while (millis() < targetWakeTime) {
        // Show countdown every 30 seconds
        static unsigned long lastDebug = 0;
        if (millis() - lastDebug > 30000) {
            unsigned long remaining = (targetWakeTime - millis()) / 1000;
            Serial.print(F("Sleeping... wake in "));
            Serial.print(remaining);
            Serial.println(F(" seconds"));
            lastDebug = millis();
        }
        
        updateButtonStates();
        if (wasButtonPressed(0) || wasButtonPressed(1) || wasButtonPressed(2) || 
            wasButtonPressed(3) || wasBluetoothButtonPressed()) {
            Serial.println(F("Woke from button press"));
            status.lastWakeSource = WAKE_BUTTON;
            wakeupFromButton = true;
            return;
        }
        delay(100);
    }
    
    Serial.println(F("Woke from timer"));
    status.lastWakeSource = WAKE_TIMER;
}


// =============================================================================
// WAKE-UP STATUS FUNCTIONS
// =============================================================================

bool PowerManager::isWakeupFromScheduledTimer() const {
    return status.lastWakeSource == WAKE_RTC || status.lastWakeSource == WAKE_TIMER;
}

bool PowerManager::isWakeupFromButton() const {
    return status.lastWakeSource == WAKE_BUTTON || 
           status.lastWakeSource == WAKE_BLUETOOTH_BUTTON;
}

void PowerManager::clearWakeSource() {
    status.lastWakeSource = WAKE_UNKNOWN;
    status.wokenByTimer = false;
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
    Serial.println(F("Field Mode: Display timeout reached"));
    
    // Turn off display first
    turnOffDisplay();
    delay(500);
    
    // Power down non-essential components
    powerDownNonEssential();
    
    if (canUseDeepSleep()) {
        Serial.println(F("Using true deep sleep"));
        enterDeepSleepMode();  // This never returns
    } else {
        Serial.println(F("Using polling sleep fallback"));
        
        // Configure next wake time for polling
        if (systemSettings && systemStatus && systemStatus->rtcWorking) {
            DateTime now = rtc.now();
            uint8_t logInterval = systemSettings->logInterval;
            
            int nextMinute = ((now.minute() / logInterval) + 1) * logInterval;
            int nextHour = now.hour();
            
            if (nextMinute >= 60) {
                nextMinute = 0;
                nextHour++;
                if (nextHour >= 24) nextHour = 0;
            }
            
            DateTime nextWake(now.year(), now.month(), now.day(), nextHour, nextMinute, 0);
            configureRTCWakeup(nextWake.unixtime());
        }
        
        // Enter polling-based sleep
        enterNRF52Sleep();
    }
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

void PowerManager::setupWakeupPin() {
#ifdef NRF52_SERIES
    pinMode(RTC_INT_PIN, INPUT_PULLUP);
    
    // Configure A1 to generate DETECT signal on LOW level for System OFF wake
    nrf_gpio_cfg_sense_input(digitalPinToPinName(RTC_INT_PIN), 
                             NRF_GPIO_PIN_PULLUP, 
                             NRF_GPIO_PIN_SENSE_LOW);
    
    Serial.println(F("Wake-up pin A1 configured for deep sleep"));
    Serial.print(F("Pin state: "));
    Serial.println(digitalRead(RTC_INT_PIN) ? "HIGH" : "LOW");
#endif
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
    
    float currentConsumption = POWER_TESTING_MA;
    
    if (status.fieldModeActive) {
        float awakeTimeRatio = 2.0f / (systemSettings ? systemSettings->logInterval : 10.0f);
        float activePower = POWER_TESTING_MA + POWER_SENSORS_MA;
        
        if (systemStatus && systemStatus->pdmWorking) {
            activePower += POWER_AUDIO_MA;
        }
        if (status.displayOn) {
            activePower += POWER_DISPLAY_MA;
        }
        
        // Use appropriate sleep power based on deep sleep capability
        float sleepPower = canUseDeepSleep() ? POWER_DEEP_SLEEP_MA : POWER_SLEEP_MA;
        
        currentConsumption = (activePower * awakeTimeRatio) + (sleepPower * (1.0f - awakeTimeRatio));
    } else {
        currentConsumption += POWER_DISPLAY_MA + POWER_SENSORS_MA;
        if (systemStatus && systemStatus->pdmWorking) {
            currentConsumption += POWER_AUDIO_MA;
        }
    }
    
    float batteryLevel = getBatteryLevel(batteryVoltage);
    float remainingCapacity = BATTERY_CAPACITY_MAH * (batteryLevel / 100.0f);
    
    if (currentConsumption > 0) {
        status.estimatedRuntimeHours = remainingCapacity / currentConsumption;
    } else {
        status.estimatedRuntimeHours = 999.0f;
    }
    
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
// DEEP SLEEP IMPLEMENTATION
// =============================================================================

void PowerManager::enterDeepSleepMode() {
#ifdef NRF52_SERIES
    if (!canUseDeepSleep()) {
        Serial.println(F("Deep sleep not available - using polling fallback"));
        enterNRF52Sleep();
        return;
    }
    
    Serial.println(F("PowerManager: Preparing for System OFF deep sleep"));

    saveRetainedState();
    
    // Power down all peripherals
    prepareSleep();
    
    // Calculate next wake time
    extern RTC_PCF8523 rtc;
    DateTime now = rtc.now();
    uint8_t logInterval = systemSettings ? systemSettings->logInterval : 10;
    
    Serial.print(F("Current time: "));
    Serial.print(now.hour());
    Serial.print(F(":"));
    if (now.minute() < 10) Serial.print(F("0"));
    Serial.println(now.minute());
    
    // Calculate next interval boundary
    int nextMinute = ((now.minute() / logInterval) + 1) * logInterval;
    if (nextMinute >= 60) {
        nextMinute %= 60;
    }
    
    Serial.print(F("Next wake minute: "));
    Serial.println(nextMinute);
    
    // Program the PCF8523 hardware alarm
    programRTCAlarm(nextMinute);
    
    // Small delay to ensure alarm is set
    delay(100);
    
    Serial.println(F("=== ENTERING SYSTEM OFF DEEP SLEEP ==="));
    Serial.println(F("Device will reset on RTC alarm"));
    Serial.println(F("Next message will be from setup() after wake"));
    Serial.flush(); // Ensure all output is sent
    
    // Configure wake-up pin one more time
    nrf_gpio_cfg_sense_input(digitalPinToPinName(RTC_INT_PIN), 
                             NRF_GPIO_PIN_PULLUP, 
                             NRF_GPIO_PIN_SENSE_LOW);
    
    // Enter System OFF - this does not return
    // System will reset when RTC alarm pulls A1 low
    sd_power_system_off();
    
    // This line should never be reached
    Serial.println(F("ERROR: System OFF failed!"));
#else
    Serial.println(F("Deep sleep not supported - using polling"));
    enterNRF52Sleep();
#endif
}

void PowerManager::programRTCAlarm(uint8_t targetMinute) {
    extern RTC_PCF8523 rtc;
    (void)rtc; // Suppress unused variable warning
    
    Serial.print(F("Programming RTC alarm for minute: "));
    Serial.println(targetMinute);
    
    // Clear any existing alarm flag first
    clearRTCAlarmFlag();
    
    // PCF8523 Register Map (from datasheet):
    // 0x0A = Minute_alarm
    // 0x0B = Hour_alarm  
    // 0x0C = Day_alarm
    // 0x0D = Weekday_alarm
    // 0x01 = Control_2 (contains AIE bit)
    
    // Program alarm registers directly via I2C
    Wire.beginTransmission(0x68);  // PCF8523 I2C address
    Wire.write(0x0A);              // Start at Minute_alarm register
    Wire.write(decToBcd(targetMinute)); // Set target minute in BCD
    Wire.write(0x80);              // Hour_alarm disabled (bit 7 = 1)
    Wire.write(0x80);              // Day_alarm disabled (bit 7 = 1)
    Wire.write(0x80);              // Weekday_alarm disabled (bit 7 = 1)
    Wire.endTransmission();
    
    // Enable alarm interrupt in Control_2 register
    Wire.beginTransmission(0x68);
    Wire.write(0x01);              // Control_2 register address
    Wire.endTransmission();
    
    Wire.requestFrom(0x68, 1);
    uint8_t control2 = Wire.read();
    
    control2 |= 0x02;              // Set AIE bit (bit 1) to enable alarm interrupt
    control2 &= ~0x08;             // Clear AF bit (bit 3) - alarm flag
    
    Wire.beginTransmission(0x68);
    Wire.write(0x01);              // Control_2 register
    Wire.write(control2);
    Wire.endTransmission();
    
    Serial.println(F("RTC hardware alarm programmed"));
    
    // Verify the alarm was set correctly
    Wire.beginTransmission(0x68);
    Wire.write(0x0A);
    Wire.endTransmission();
    Wire.requestFrom(0x68, 1);
    uint8_t readBack = Wire.read();
    Serial.print(F("Alarm minute register: 0x"));
    Serial.print(readBack, HEX);
    Serial.print(F(" ("));
    Serial.print(bcdToDec(readBack));
    Serial.println(F(")"));
}



uint8_t PowerManager::decToBcd(uint8_t val) {
    return ((val / 10) << 4) + (val % 10);
}

uint8_t PowerManager::bcdToDec(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

bool PowerManager::canUseDeepSleep() const {
    Serial.println("=== Deep Sleep Capability Check ===");
    
    if (!status.deepSleepCapable) {
        Serial.println("Deep sleep: Not capable - initializeDeepSleep failed");
        return false;
    }
    
    if (!settings.useDeepSleep) {
        Serial.println("Deep sleep: Disabled in settings");
        return false;
    }
    
    if (!systemStatus || !systemStatus->rtcWorking) {
        Serial.println("Deep sleep: RTC not working");
        return false;
    }
    
    extern RTC_PCF8523 rtc;
    if (!rtc.isrunning()) {
        Serial.println("Deep sleep: RTC oscillator not running");
        return false;
    }
    
    Serial.println("Deep sleep: All checks passed - CAPABLE");
    return true;
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
    
    // Deep sleep status
    Serial.print(F("Deep Sleep: "));
    if (canUseDeepSleep()) {
        Serial.print(F("ENABLED ("));
        Serial.print(status.deepSleepCycles);
        Serial.println(F(" cycles)"));
    } else {
        Serial.println(F("DISABLED (using polling)"));
    }
    
    Serial.print(F("Wake from deep sleep: "));
    Serial.println(status.wakeFromDeepSleep ? "YES" : "NO");
    
    Serial.print(F("Est. Runtime: "));
    Serial.print(status.estimatedRuntimeHours, 1);
    Serial.println(F(" hours"));
    
    if (canUseDeepSleep()) {
        Serial.println(F("  (with true deep sleep)"));
    } else {
        Serial.println(F("  (with polling sleep)"));
    }
    
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