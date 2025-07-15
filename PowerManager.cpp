/**
 * PowerManager.cpp - UPDATED: Display power control moved here
 */

#include "PowerManager.h"
#include "Utils.h"
#include "Sensors.h"  // For getBatteryLevel
#include "Bluetooth.h"
// =============================================================================
// POWER CONSUMPTION CONSTANTS (mA)
// =============================================================================

const float PowerManager::POWER_TESTING_MA = 15.0f;
const float PowerManager::POWER_DISPLAY_MA = 8.0f;
const float PowerManager::POWER_SENSORS_MA = 2.0f;
const float PowerManager::POWER_AUDIO_MA = 5.0f;
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
    
    // Field mode timing variables
    status.wokenByTimer = false;
    status.lastLogTime = 0;
    status.nextWakeTime = 0;
    status.lastFlushTime = 0;
    
    // Initialize settings
    settings.fieldModeEnabled = false;
    settings.displayTimeoutMin = 5;     // Default 5 minutes
    settings.sleepIntervalMin = 10;
    settings.autoFieldMode = false;
    settings.criticalBatteryLevel = 15;
    
    // Initialize timing
    lastPowerCheck = 0;
    displayOffTime = 0;
    lastSleepTime = 0;
       
    systemStatus = nullptr;
    systemSettings = nullptr;

    longPressStartTime = 0;
    longPressDetected = false;
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
        
        // If field mode is enabled in settings, activate it
        if (settings.fieldModeEnabled) {
            enableFieldMode();
        }
    }
    
    status.lastUserActivity = millis();
    status.lastBluetoothActivity = millis();
    status.totalUptime = millis();  // Set uptime reference
    status.lastFlushTime = millis();
    
    Serial.println(F("PowerManager initialized with display power control"));
    Serial.println(F("  - Power monitoring: ENABLED"));
    Serial.println(F("  - Field mode: ENABLED"));
    Serial.println(F("  - Display control: ENABLED (Pin 12)"));
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
        if (!status.bluetoothOn) {
            Serial.println(F("PowerManager: Bluetooth button pressed - activating Bluetooth"));
            activateBluetooth();
        } else {
            Serial.println(F("PowerManager: Bluetooth button pressed - Bluetooth already on"));
            // Reset the timer
            status.lastBluetoothActivity = millis();
        }
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
        
        
        // Turn display on and reset timeout
        turnOnDisplay();
        resetDisplayTimeout();
        
        // NOTE: This does NOT reset Bluetooth timer - only display timer
        
    } else {
                
        turnOnDisplay();
    }
}

// NEW: Handle Bluetooth activity separately
void PowerManager::handleBluetoothActivity() {
    status.lastBluetoothActivity = millis();
    
    if (status.fieldModeActive && status.bluetoothOn) {
        Serial.println(F("PowerManager: Bluetooth activity - Bluetooth timer reset"));
    }
}

void PowerManager::update() {
    unsigned long currentTime = millis();
    
    // Field mode logic: check both display and Bluetooth timeouts
    if (status.fieldModeActive) {
        checkFieldModeTimeout(currentTime);
        checkBluetoothTimeout(currentTime);  // NEW: Check Bluetooth timeout separately
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
// FIELD MODE LOGIC
// =============================================================================

void PowerManager::enterFieldSleep() {
    Serial.println(F("Field Mode: Display timeout reached - entering sleep"));
    
    // Turn off display to save power
    turnOffDisplay();
    
    // Power down non-essential components
    powerDownNonEssential();
    
    Serial.println(F("=== FIELD SLEEP MODE ==="));
    Serial.println(F("Sleeping until next scheduled reading or button press"));
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
    
    // NEW: Turn off Bluetooth in field mode (will be manually activated)
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

    // NEW: Turn Bluetooth back on for testing mode
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
            turnOnDisplay();
            resetDisplayTimeout();
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
    
    unsigned long currentTime = millis();
    return (currentTime >= status.nextWakeTime);
}

void PowerManager::updateNextWakeTime(uint8_t logIntervalMinutes) {
    status.lastLogTime = millis();
    
    // Round to next full minute boundary
    if (systemStatus && systemStatus->rtcWorking) {
        extern RTC_PCF8523 rtc;
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
    if (!status.fieldModeActive) {
        return false; // Never sleep in testing mode
    }
    
    // Sleep if we've taken a reading and have time before next reading
    unsigned long currentTime = millis();
    unsigned long timeSinceLastReading = currentTime - status.lastLogTime;
    
    // Wait at least 30 seconds after taking a reading before sleeping
    if (timeSinceLastReading < 30000) {
        return false;
    }
    
    // Don't sleep if next reading is due soon (within 1 minute)
    if (currentTime + 60000 >= status.nextWakeTime) {
        return false;
    }
    
    return true;
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
    Serial.println(F("Powering down non-essential components for field sleep"));
    status.sensorState = COMP_POWER_SLEEP;
    status.audioState = COMP_POWER_SLEEP;
}

void PowerManager::powerUpAll() {
    Serial.println(F("Powering up all components after field sleep"));
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
    Serial.println(F("PowerManager: Deep sleep called but using delay-based sleep"));
}

void PowerManager::prepareSleep() {
    Serial.println(F("PowerManager: Preparing for sleep"));
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
    if (source == WAKE_TIMER) {
        status.sleepCycles++;
    }
    wakeFromSleep();
}

void PowerManager::configureWakeupSources() {
    Serial.println(F("PowerManager: Wake-up sources configured for field mode"));
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
    
    if (status.fieldModeActive) {
        Serial.print(F("Next Reading: "));
        if (millis() >= status.nextWakeTime) {
            Serial.println(F("DUE NOW"));
        } else {
            unsigned long timeToNext = status.nextWakeTime - millis();
            Serial.print(timeToNext / 60000);
            Serial.print(F("m "));
            Serial.print((timeToNext % 60000) / 1000);
            Serial.println(F("s"));
        }
        
        Serial.print(F("Sleep Cycles: ")); Serial.println(status.sleepCycles);
        Serial.print(F("Last Wake: ")); Serial.println(getWakeSourceString());
    }
    
    Serial.print(F("Runtime Est: ")); Serial.print(status.estimatedRuntimeHours, 1); Serial.println(F(" hours"));
    Serial.print(F("Daily Usage: ")); Serial.print(status.dailyUsageEstimateMah, 1); Serial.println(F(" mAh"));
    Serial.print(F("Button Presses: ")); Serial.println(status.buttonPresses);
    Serial.print(F("Uptime: ")); Serial.print(getUptime() / 1000); Serial.println(F(" seconds"));
    
    // Memory health check
    Serial.print(F("Memory Usage: ")); Serial.print(getMemoryUsagePercent()); Serial.println(F("%"));
    Serial.print(F("Memory Health: ")); Serial.println(isMemoryHealthy() ? "OK" : "WARNING");
    
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