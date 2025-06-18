/**
 * PowerManager.cpp
 * Power management system implementation
 */

#include "PowerManager.h"
#include "Utils.h"

#ifdef NRF52_SERIES
#include <nrf.h>
#include <nrf_power.h>
#include <nrf_soc.h>
#endif

// =============================================================================
// POWER CONSUMPTION CONSTANTS (mA)
// =============================================================================

const float PowerManager::POWER_NORMAL_MA = 15.0f;    // Base system consumption
const float PowerManager::POWER_DISPLAY_MA = 8.0f;    // OLED display
const float PowerManager::POWER_SENSORS_MA = 2.0f;    // BME280 active
const float PowerManager::POWER_AUDIO_MA = 5.0f;      // Microphone sampling
const float PowerManager::POWER_SLEEP_MA = 0.05f;     // Deep sleep mode

// Global power manager instance
PowerManager globalPowerManager;

// =============================================================================
// CONSTRUCTOR AND INITIALIZATION
// =============================================================================

PowerManager::PowerManager() {
    // Initialize status
    status.currentMode = POWER_NORMAL;
    status.fieldModeActive = false;
    status.displayOn = true;
    status.displayTimeoutMs = 5 * 60 * 1000; // 5 minutes default
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
    
    // Initialize settings
    settings.fieldModeEnabled = false;
    settings.displayTimeoutMin = 5;
    settings.sleepIntervalMin = 10;
    settings.autoFieldMode = true;
    settings.criticalBatteryLevel = 15; // 15% battery
    
    // Initialize timing
    lastPowerCheck = 0;
    displayOffTime = 0;
    lastSleepTime = 0;
    
    systemStatus = nullptr;
    systemSettings = nullptr;
}

void PowerManager::initialize(SystemStatus* sysStatus, SystemSettings* sysSettings) {
    systemStatus = sysStatus;
    systemSettings = sysSettings;
    
    loadPowerSettings();
    
    // Configure initial power mode
    status.lastUserActivity = millis();
    
    // Configure wake-up sources
    configureWakeupSources();
    
    Serial.println(F("PowerManager initialized"));
}

// =============================================================================
// CORE POWER MANAGEMENT
// =============================================================================

void PowerManager::update() {
    unsigned long currentTime = millis();
    
    // Update uptime
    status.totalUptime = currentTime;
    
    // Check power mode every 10 seconds
    if (currentTime - lastPowerCheck >= 10000) {
        // This would need battery voltage from sensor data
        // For now, use a placeholder - this should be called with actual voltage
        updatePowerMode(3.7f); // Placeholder voltage
        calculateRuntimeEstimate(3.7f);
        lastPowerCheck = currentTime;
    }
    
    // Handle display timeout in field mode
    if (status.fieldModeActive) {
        handleDisplayTimeout();
    }
    
    // Check if we should enter sleep
    if (shouldEnterSleep()) {
        unsigned long sleepTime = systemSettings->logInterval * 60000UL;
        prepareSleep();
        enterDeepSleep(sleepTime);
    }
}

void PowerManager::handleUserActivity() {
    status.lastUserActivity = millis();
    status.buttonPresses++;
    
    // Turn on display if it's off and we're in field mode
    if (status.fieldModeActive && !status.displayOn) {
        turnOnDisplay();
    }
    
    // Reset display timeout
    resetDisplayTimeout();
}

void PowerManager::handleWakeUp(WakeUpSource source) {
    status.lastWakeSource = source;
    
    if (source == WAKE_TIMER || source == WAKE_RTC) {
        status.sleepCycles++;
    }
    
    wakeFromSleep();
    
    Serial.print(F("Woke up from: "));
    Serial.println(getWakeSourceString());
}

// =============================================================================
// FIELD MODE MANAGEMENT
// =============================================================================

void PowerManager::enableFieldMode() {
    settings.fieldModeEnabled = true;
    status.fieldModeActive = true;
    status.currentMode = POWER_FIELD;
    
    // Start display timeout
    resetDisplayTimeout();
    
    Serial.println(F("Field mode enabled"));
    savePowerSettings();
}

void PowerManager::disableFieldMode() {
    settings.fieldModeEnabled = false;
    status.fieldModeActive = false;
    status.currentMode = POWER_NORMAL;
    
    // Ensure display stays on
    turnOnDisplay();
    
    Serial.println(F("Field mode disabled"));
    savePowerSettings();
}

bool PowerManager::isFieldModeActive() const {
    return status.fieldModeActive;
}

// =============================================================================
// DISPLAY MANAGEMENT
// =============================================================================

void PowerManager::turnOnDisplay() {
    if (!status.displayOn) {
        status.displayOn = true;
        status.displayState = COMP_POWER_ON;
        
        // Power up display - this would interface with the actual display
        // For now, just set the flag
        
        Serial.println(F("Display powered on"));
    }
}

void PowerManager::turnOffDisplay() {
    if (status.displayOn) {
        status.displayOn = false;
        status.displayState = COMP_POWER_OFF;
        displayOffTime = millis();
        
        // Power down display - this would interface with the actual display
        // For now, just set the flag
        
        Serial.println(F("Display powered off"));
    }
}

bool PowerManager::isDisplayOn() const {
    return status.displayOn;
}

void PowerManager::resetDisplayTimeout() {
    status.lastUserActivity = millis();
}

unsigned long PowerManager::getDisplayTimeRemaining() const {
    if (!status.fieldModeActive || !status.displayOn) {
        return 0;
    }
    
    unsigned long elapsed = millis() - status.lastUserActivity;
    if (elapsed >= status.displayTimeoutMs) {
        return 0;
    }
    
    return status.displayTimeoutMs - elapsed;
}

void PowerManager::handleDisplayTimeout() {
    if (!status.displayOn) return;
    
    unsigned long timeSinceActivity = millis() - status.lastUserActivity;
    
    if (timeSinceActivity >= status.displayTimeoutMs) {
        turnOffDisplay();
    }
}

// =============================================================================
// COMPONENT POWER CONTROL
// =============================================================================

void PowerManager::powerDownSensors() {
    status.sensorState = COMP_POWER_SLEEP;
    
    // Put BME280 in sleep mode
    // This would interface with the actual sensor
    
    Serial.println(F("Sensors powered down"));
}

void PowerManager::powerUpSensors() {
    status.sensorState = COMP_POWER_ON;
    
    // Wake up BME280
    // This would interface with the actual sensor
    
    Serial.println(F("Sensors powered up"));
}

void PowerManager::powerDownAudio() {
    status.audioState = COMP_POWER_OFF;
    
    // Stop audio sampling
    // This would interface with the audio system
    
    Serial.println(F("Audio powered down"));
}

void PowerManager::powerUpAudio() {
    status.audioState = COMP_POWER_ON;
    
    // Resume audio sampling
    // This would interface with the audio system
    
    Serial.println(F("Audio powered up"));
}

void PowerManager::powerDownNonEssential() {
    if (status.displayOn && status.fieldModeActive) {
        turnOffDisplay();
    }
    
    powerDownAudio();
    powerDownSensors();
}

void PowerManager::powerUpAll() {
    powerUpSensors();
    powerUpAudio();
    
    if (status.fieldModeActive) {
        // Don't automatically turn on display in field mode
        // Wait for user activity
    } else {
        turnOnDisplay();
    }
}

// =============================================================================
// SLEEP MANAGEMENT
// =============================================================================

void PowerManager::enterDeepSleep(uint32_t sleepTimeMs) {
    Serial.print(F("Entering deep sleep for "));
    Serial.print(sleepTimeMs / 1000);
    Serial.println(F(" seconds"));
    
    delay(100); // Allow serial to flush
    
#ifdef NRF52_SERIES
    // Configure RTC for wake-up
    // This is simplified - full implementation would configure RTC properly
    
    // Enter system off mode (lowest power)
    // sd_power_system_off();
    
    // For now, use regular delay (in real implementation this would be deep sleep)
    delay(sleepTimeMs);
#else
    // For other platforms, use regular delay
    delay(sleepTimeMs);
#endif
    
    // Wake up handling
    handleWakeUp(WAKE_TIMER);
}

void PowerManager::prepareSleep() {
    Serial.println(F("Preparing for sleep..."));
    
    // Power down non-essential components
    powerDownNonEssential();
    
    // Save any pending data
    if (systemStatus && systemStatus->sdWorking) {
        // Ensure SD operations are complete
    }
    
    lastSleepTime = millis();
}

void PowerManager::wakeFromSleep() {
    Serial.println(F("Waking from sleep..."));
    
    // Power up essential components
    powerUpSensors();
    
    // Don't automatically power up display in field mode
    if (!status.fieldModeActive) {
        turnOnDisplay();
    }
}

bool PowerManager::canEnterSleep() const {
    // Don't sleep if display is on (user is interacting)
    if (status.displayOn && status.fieldModeActive) {
        return false;
    }
    
    // Don't sleep in normal mode
    if (status.currentMode == POWER_NORMAL) {
        return false;
    }
    
    // Check if enough time has passed since last sleep
    unsigned long timeSinceLastSleep = millis() - lastSleepTime;
    unsigned long minSleepInterval = systemSettings->logInterval * 60000UL;
    
    return timeSinceLastSleep >= minSleepInterval;
}

bool PowerManager::shouldEnterSleep() {
    return status.fieldModeActive && canEnterSleep();
}

// =============================================================================
// POWER MODE MANAGEMENT
// =============================================================================

void PowerManager::updatePowerMode(float batteryVoltage) {
    PowerMode newMode = status.currentMode;
    
    if (batteryVoltage <= BATTERY_CRITICAL) {
        newMode = POWER_CRITICAL;
    } else if (batteryVoltage <= BATTERY_LOW) {
        newMode = POWER_SAVE;
    } else if (settings.fieldModeEnabled) {
        newMode = POWER_FIELD;
    } else {
        newMode = POWER_NORMAL;
    }
    
    if (newMode != status.currentMode) {
        Serial.print(F("Power mode changed: "));
        Serial.print(getPowerModeString());
        Serial.print(F(" -> "));
        status.currentMode = newMode;
        Serial.println(getPowerModeString());
        
        // Auto-enable field mode if battery is low and auto mode is enabled
        if (settings.autoFieldMode && newMode >= POWER_SAVE && !status.fieldModeActive) {
            enableFieldMode();
        }
    }
}

void PowerManager::calculateRuntimeEstimate(float batteryVoltage) {
    // Assume 2000mAh battery capacity (typical for field deployment)
    const float BATTERY_CAPACITY_MAH = 2000.0f;
    
    // Calculate current consumption based on active components
    float currentConsumption = POWER_NORMAL_MA;
    
    if (status.displayOn) {
        currentConsumption += POWER_DISPLAY_MA;
    }
    
    if (status.sensorState == COMP_POWER_ON) {
        currentConsumption += POWER_SENSORS_MA;
    }
    
    if (status.audioState == COMP_POWER_ON) {
        currentConsumption += POWER_AUDIO_MA;
    }
    
    // In field mode, factor in sleep time
    if (status.fieldModeActive) {
        float activeTimeRatio = 0.1f; // Assume 10% active time in field mode
        currentConsumption = (currentConsumption * activeTimeRatio) + 
                           (POWER_SLEEP_MA * (1.0f - activeTimeRatio));
    }
    
    // Calculate remaining capacity based on voltage
    float batteryLevel = getBatteryLevel(batteryVoltage);
    float remainingCapacity = BATTERY_CAPACITY_MAH * (batteryLevel / 100.0f);
    
    // Estimate runtime
    if (currentConsumption > 0) {
        status.estimatedRuntimeHours = remainingCapacity / currentConsumption;
    } else {
        status.estimatedRuntimeHours = 999.0f; // Practically infinite
    }
    
    // Calculate daily usage estimate
    status.dailyUsageEstimateMah = currentConsumption * 24.0f;
}

// =============================================================================
// SETTINGS MANAGEMENT
// =============================================================================

void PowerManager::setDisplayTimeout(uint8_t minutes) {
    settings.displayTimeoutMin = constrain(minutes, 1, 30);
    status.displayTimeoutMs = settings.displayTimeoutMin * 60000UL;
    savePowerSettings();
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
    savePowerSettings();
}

void PowerManager::loadPowerSettings() {
    // In a full implementation, this would load from flash storage
    // For now, use defaults
    Serial.println(F("Power settings loaded (defaults)"));
}

void PowerManager::savePowerSettings() {
    // In a full implementation, this would save to flash storage
    Serial.println(F("Power settings saved"));
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
    return status.totalUptime;
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
    Serial.println(F("\n=== Power Status ==="));
    Serial.print(F("Mode: ")); Serial.println(getPowerModeString());
    Serial.print(F("Field Mode: ")); Serial.println(status.fieldModeActive ? "ON" : "OFF");
    Serial.print(F("Display: ")); Serial.println(status.displayOn ? "ON" : "OFF");
    Serial.print(F("Runtime Est: ")); Serial.print(status.estimatedRuntimeHours, 1); Serial.println(F(" hours"));
    Serial.print(F("Daily Usage: ")); Serial.print(status.dailyUsageEstimateMah, 1); Serial.println(F(" mAh"));
    Serial.print(F("Sleep Cycles: ")); Serial.println(status.sleepCycles);
    Serial.print(F("Button Presses: ")); Serial.println(status.buttonPresses);
    Serial.print(F("Uptime: ")); Serial.print(status.totalUptime / 1000); Serial.println(F(" seconds"));
    Serial.println(F("==================\n"));
}

void PowerManager::resetStatistics() {
    status.sleepCycles = 0;
    status.buttonPresses = 0;
    status.totalUptime = millis(); // Reset uptime reference
    Serial.println(F("Power statistics reset"));
}

// =============================================================================
// WAKE-UP SOURCE CONFIGURATION
// =============================================================================

void PowerManager::configureWakeupSources() {
#ifdef NRF52_SERIES
    // Configure button pins as wake-up sources
    // This would set up GPIO interrupts for the button pins
    
    // Configure RTC for periodic wake-up
    // This would set up RTC interrupts
    
    Serial.println(F("Wake-up sources configured"));
#else
    Serial.println(F("Wake-up sources not implemented for this platform"));
#endif
}

// =============================================================================
// GLOBAL HELPER FUNCTIONS
// =============================================================================

void initializePowerManager() {
    // This would be called from main setup()
    // globalPowerManager.initialize(...);
}

void updatePowerManager() {
    globalPowerManager.update();
}

bool shouldSystemSleep() {
    return globalPowerManager.shouldEnterSleep();
}

void handleSystemWakeup() {
    globalPowerManager.handleWakeUp(WAKE_BUTTON);
}

const char* powerModeToString(PowerMode mode) {
    switch (mode) {
        case POWER_NORMAL: return "Normal";
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
        return POWER_NORMAL;
    }
}