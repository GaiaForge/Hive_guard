/**
 * PowerManager.cpp - PHASE 3: Display Timeout Implementation
 * Added display timeout control for major power savings in field mode
 */

#include "PowerManager.h"
#include "Utils.h"
#include "Sensors.h"  // For getBatteryLevel

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
}

// =============================================================================
// REPLACE PowerManager::initialize() - CLEAN VERSION
// =============================================================================

void PowerManager::initialize(SystemStatus* sysStatus, SystemSettings* sysSettings) {
    systemStatus = sysStatus;
    systemSettings = sysSettings;
    
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
    status.totalUptime = millis();  // Set uptime reference
    status.lastFlushTime = millis();
    // REMOVED: lastUserInteraction = millis(); // Don't initialize deleted variable
    
    Serial.println(F("PowerManager initialized - Simple Field Mode"));
    Serial.println(F("  - Power monitoring: ENABLED"));
    Serial.println(F("  - Field mode: ENABLED"));
    Serial.println(F("  - Display control: ENABLED"));
    Serial.print(F("  - Display timeout: "));
    Serial.print(settings.displayTimeoutMin);
    Serial.println(F(" minutes (1-5 range)"));
}

// =============================================================================
// CORE POWER MANAGEMENT
// =============================================================================

void PowerManager::handleUserActivity() {
    status.lastUserActivity = millis();
    status.buttonPresses++;
    
    if (status.fieldModeActive) {
        Serial.print(F("PowerManager: User activity in field mode (button #"));
        Serial.print(status.buttonPresses);
        Serial.println(F(")"));
        
        // Turn display on and reset timeout
        turnOnDisplay();
        resetDisplayTimeout();
        
    } else {
        Serial.print(F("PowerManager: User activity (button #"));
        Serial.print(status.buttonPresses);
        Serial.println(F(")"));
        
        turnOnDisplay();
    }
}

void PowerManager::update() {
    unsigned long currentTime = millis();
    
    // Simple field mode logic: just check for display timeout
    if (status.fieldModeActive) {
        checkFieldModeTimeout(currentTime);
    }
    
    // Update power monitoring every 5 seconds
    if (currentTime - lastPowerCheck >= 5000) {
        lastPowerCheck = currentTime;
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
    
    Serial.println(F("=== FIELD MODE ENABLED ==="));
    Serial.print(F("Log interval: "));
    Serial.print(systemSettings ? systemSettings->logInterval : 10);
    Serial.println(F(" minutes"));
    Serial.print(F("Display timeout: "));
    Serial.print(settings.displayTimeoutMin);
    Serial.println(F(" minutes"));
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
    
    Serial.println(F("=== FIELD MODE DISABLED ==="));
    Serial.println(F("Returning to Testing Mode"));
    Serial.println(F("=========================="));
}

bool PowerManager::isFieldModeActive() const {
    return status.fieldModeActive;
}

// =============================================================================
// DISPLAY MANAGEMENT
// =============================================================================

void PowerManager::turnOnDisplay() {
    if (!status.displayOn) {
        // FIXED: Check if setDisplayPower exists before calling
        #ifdef DISPLAY_POWER_PIN
        extern void setDisplayPower(bool powerOn);
        setDisplayPower(true);  // Hardware power on
        delay(100);  // Wait for display to initialize
        #endif
        
        status.displayOn = true;
        status.displayState = COMP_POWER_ON;
        
        Serial.println(F("PowerManager: Display turned ON"));
    }
}

void PowerManager::turnOffDisplay() {
    if (status.displayOn && status.fieldModeActive) {
        // FIXED: Check if setDisplayPower exists before calling
        #ifdef DISPLAY_POWER_PIN
        extern void setDisplayPower(bool powerOn);
        setDisplayPower(false);  // Hardware power off
        #endif
        
        status.displayOn = false;
        status.displayState = COMP_POWER_OFF;
        displayOffTime = millis();
        
        Serial.println(F("PowerManager: Display turned OFF"));
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
    status.nextWakeTime = status.lastLogTime + (logIntervalMinutes * 60000UL);
    
    Serial.print(F("Next reading scheduled in "));
    Serial.print(logIntervalMinutes);
    Serial.println(F(" minutes"));
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