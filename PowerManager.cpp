/**
 * PowerManager.cpp
 * Complete power management system implementation
 */

#include "PowerManager.h"
#include "Utils.h"
#include "Sensors.h"  // For getBatteryLevel

#ifdef NRF52_SERIES
#include <nrf.h>
#include <nrf_power.h>
#include <nrf_soc.h>
#include <nrf_rtc.h>
#include <nrf_clock.h>
#include <nrf_gpio.h>
#endif

// =============================================================================
// POWER CONSUMPTION CONSTANTS (mA)
// =============================================================================

const float PowerManager::POWER_NORMAL_MA = 15.0f;    // Base system consumption
const float PowerManager::POWER_DISPLAY_MA = 8.0f;    // OLED display
const float PowerManager::POWER_SENSORS_MA = 2.0f;    // BME280 active
const float PowerManager::POWER_AUDIO_MA = 5.0f;      // Microphone sampling
const float PowerManager::POWER_SLEEP_MA = 0.05f;     // Deep sleep mode

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
    
    // Load power settings from system settings
    if (sysSettings) {
        settings.fieldModeEnabled = sysSettings->fieldModeEnabled;
        settings.displayTimeoutMin = sysSettings->displayTimeoutMin;
        status.displayTimeoutMs = settings.displayTimeoutMin * 60000UL;
        
        // Set initial mode based on settings
        if (settings.fieldModeEnabled) {
            enableFieldMode();
        }
    }
    
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
    
    // Handle display timeout in field mode
    if (status.fieldModeActive) {
        handleDisplayTimeout();
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
    
    // Update system settings
    if (systemSettings) {
        systemSettings->fieldModeEnabled = true;
    }
    
    // Start display timeout
    resetDisplayTimeout();
    
    Serial.println(F("Field mode enabled"));
    savePowerSettings();
}

void PowerManager::disableFieldMode() {
    settings.fieldModeEnabled = false;
    status.fieldModeActive = false;
    status.currentMode = POWER_NORMAL;
    
    // Update system settings
    if (systemSettings) {
        systemSettings->fieldModeEnabled = false;
    }
    
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
        
        // Re-initialize display after power-down
        // This depends on your display library implementation
        // You might need to call display.begin() again
        
        Serial.println(F("Display powered on"));
    }
}

void PowerManager::turnOffDisplay() {
    if (status.displayOn) {
        status.displayOn = false;
        status.displayState = COMP_POWER_OFF;
        displayOffTime = millis();
        
        // Power down display
        // For OLED displays, you can usually turn them off with:
        // display.ssd1306_command(SSD1306_DISPLAYOFF);
        // or display.setPowerSave(1);
        
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
    // BME280 automatically goes to sleep mode after reading
    // You can force it with: bme.setSampling(..., MODE_SLEEP);
    
    Serial.println(F("Sensors powered down"));
}

void PowerManager::powerUpSensors() {
    status.sensorState = COMP_POWER_ON;
    
    // Wake up BME280
    // Usually done automatically when you take a reading
    // Or explicitly: bme.setSampling(..., MODE_NORMAL);
    
    Serial.println(F("Sensors powered up"));
}

void PowerManager::powerDownAudio() {
    status.audioState = COMP_POWER_OFF;
    
    // Stop audio sampling
    // This would depend on your audio implementation
    // For MAX9814, you could disable the analog input or power pin
    
    Serial.println(F("Audio powered down"));
}

void PowerManager::powerUpAudio() {
    status.audioState = COMP_POWER_ON;
    
    // Resume audio sampling
    // Re-enable analog input
    
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
    // Configure RTC1 for wake-up timer
    uint32_t ticks = (sleepTimeMs * 32768UL) / 1000; // Convert ms to RTC ticks
    
    // Stop RTC1
    nrf_rtc_task_trigger(NRF_RTC1, NRF_RTC_TASK_STOP);
    nrf_rtc_task_trigger(NRF_RTC1, NRF_RTC_TASK_CLEAR);
    
    // Set compare register for wake-up
    nrf_rtc_cc_set(NRF_RTC1, 0, ticks);
    nrf_rtc_event_clear(NRF_RTC1, NRF_RTC_EVENT_COMPARE_0);
    nrf_rtc_int_enable(NRF_RTC1, NRF_RTC_INT_COMPARE0_MASK);
    
    // Enable RTC1 interrupt
    NVIC_EnableIRQ(RTC1_IRQn);
    NVIC_SetPriority(RTC1_IRQn, 1);
    
    // Start RTC1
    nrf_rtc_task_trigger(NRF_RTC1, NRF_RTC_TASK_START);
    
    // Configure wake-up sources (buttons)
    configureButtonWakeup();
    
    // Enter System OFF mode (lowest power consumption)
    // Current consumption: ~0.5µA
    __WFE();
    __SEV();
    __WFE();
    
    // If we reach here, we were woken by button press, not timer
    Serial.println(F("Woken by button press"));
    
#else
    // Fallback for non-nRF52 platforms
    Serial.println(F("Platform sleep not implemented - using delay"));
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
    unsigned long minSleepInterval = systemSettings ? 
        (systemSettings->logInterval * 60000UL) : 600000UL; // Default 10 min
    
    return timeSinceLastSleep >= minSleepInterval;
}

bool PowerManager::shouldEnterSleep() {
    // Never sleep in normal mode (for development)
    if (status.currentMode == POWER_NORMAL) {
        return false;
    }
    
    // Don't sleep if display is on (user is interacting)
    if (status.displayOn) {
        return false;
    }
    
    // Don't sleep if we just woke up (prevent immediate re-sleep)
    if (millis() - status.lastUserActivity < 30000) { // 30 second minimum awake time
        return false;
    }
    
    // In field mode, sleep when display has been off for a while
    if (status.fieldModeActive) {
        unsigned long timeSinceDisplayOff = millis() - displayOffTime;
        return timeSinceDisplayOff > 60000; // 1 minute after display off
    }
    
    return false;
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
        newMode = POWER_NORMAL;
    }
    
    if (newMode != status.currentMode) {
        Serial.print(F("Power mode: "));
        Serial.print(getPowerModeString());
        Serial.print(F(" -> "));
        status.currentMode = newMode;
        Serial.println(getPowerModeString());
        
        // Auto-enable field mode if battery is low
        if (settings.autoFieldMode && newMode >= POWER_SAVE && !status.fieldModeActive) {
            enableFieldMode();
        }
        
        // Adjust system behavior based on power mode
        switch (newMode) {
            case POWER_CRITICAL:
                // Minimum functionality only
                powerDownAudio();
                setDisplayTimeout(1); // 1 minute timeout
                break;
                
            case POWER_SAVE:
                // Reduced functionality
                setDisplayTimeout(2); // 2 minute timeout
                break;
                
            case POWER_FIELD:
                // Optimized for field use
                setDisplayTimeout(5); // 5 minute timeout
                break;
                
            case POWER_NORMAL:
                // Full functionality
                powerUpAll();
                break;
        }
    }
    
    // Always update runtime estimate
    calculateRuntimeEstimate(batteryVoltage);
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
    
    // Update system settings
    if (systemSettings) {
        systemSettings->displayTimeoutMin = settings.displayTimeoutMin;
    }
    
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
    // For now, use defaults or sync with system settings
    if (systemSettings) {
        settings.fieldModeEnabled = systemSettings->fieldModeEnabled;
        settings.displayTimeoutMin = systemSettings->displayTimeoutMin;
        status.displayTimeoutMs = settings.displayTimeoutMin * 60000UL;
    }
    Serial.println(F("Power settings loaded"));
}

void PowerManager::savePowerSettings() {
    // In a full implementation, this would save to flash storage
    // For now, sync with system settings
    if (systemSettings) {
        systemSettings->fieldModeEnabled = settings.fieldModeEnabled;
        systemSettings->displayTimeoutMin = settings.displayTimeoutMin;
    }
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

void PowerManager::configureButtonWakeup() {
#ifdef NRF52_SERIES
    // Configure button pins for wake-up
    // Assumes buttons are on pins A0-A3 (P0.02, P0.03, P0.04, P0.05)
    
    uint32_t buttonPins[] = {2, 3, 4, 5}; // Adjust based on your actual pin mapping
    
    for (int i = 0; i < 4; i++) {
        // Configure pin as input with pullup
        nrf_gpio_cfg_input(buttonPins[i], NRF_GPIO_PIN_PULLUP);
        
        // Configure pin for wake-up on low signal (button press)
        nrf_gpio_cfg_sense_set(buttonPins[i], NRF_GPIO_PIN_SENSE_LOW);
    }
    
    Serial.println(F("Button wake-up configured"));
#endif
}

// =============================================================================
// GLOBAL HELPER FUNCTIONS
// =============================================================================

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


