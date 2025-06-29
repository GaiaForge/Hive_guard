/**
 * DataStructures.cpp
 * Data structures utility functions implementation
 */

#include "DataStructures.h"
#include "Config.h"

// =============================================================================
// INITIALIZATION FUNCTIONS
// =============================================================================

void initializeSensorData(SensorData& data) {
    data.temperature = 0.0;
    data.humidity = 0.0;
    data.pressure = 0.0;
    data.batteryVoltage = 0.0;
    data.dominantFreq = 0;
    data.soundLevel = 0;
    data.beeState = BEE_UNKNOWN;
    data.alertFlags = ALERT_NONE;
    data.sensorsValid = false;
}

void initializeSystemStatus(SystemStatus& status) {
    status.systemReady = false;
    status.rtcWorking = false;
    status.displayWorking = false;
    status.bmeWorking = false;
    status.sdWorking = false;
    status.pdmWorking = false;
}

void initializeSystemSettings(SystemSettings& settings) {
    settings.tempOffset = DEFAULT_TEMP_OFFSET;
    settings.humidityOffset = DEFAULT_HUMIDITY_OFFSET;
    settings.audioSensitivity = DEFAULT_AUDIO_SENSITIVITY;
    settings.queenFreqMin = DEFAULT_QUEEN_FREQ_MIN;
    settings.queenFreqMax = DEFAULT_QUEEN_FREQ_MAX;
    settings.swarmFreqMin = DEFAULT_SWARM_FREQ_MIN;
    settings.swarmFreqMax = DEFAULT_SWARM_FREQ_MAX;
    settings.stressThreshold = DEFAULT_STRESS_THRESHOLD;
    settings.logInterval = DEFAULT_LOG_INTERVAL;
    settings.logEnabled = DEFAULT_LOG_ENABLED;
    settings.tempMin = DEFAULT_TEMP_MIN;
    settings.tempMax = DEFAULT_TEMP_MAX;
    settings.humidityMin = DEFAULT_HUMIDITY_MIN;
    settings.humidityMax = DEFAULT_HUMIDITY_MAX;
    settings.displayBrightness = DEFAULT_DISPLAY_BRIGHTNESS;
    settings.fieldModeEnabled = DEFAULT_FIELD_MODE_ENABLED;
    settings.displayTimeoutMin = DEFAULT_DISPLAY_TIMEOUT_MIN;
    settings.magicNumber = SETTINGS_MAGIC_NUMBER;
    settings.checksum = 0;
}

void initializeMenuState(MenuState& state) {
    state.settingsMenuActive = false;
    state.menuLevel = 0;
    state.selectedItem = 0;
    state.editingItem = 0;
    state.editFloatValue = 0.0;
    state.editIntValue = 0;
}

void initializeAbscondingIndicators(AbscondingIndicators& indicators) {
    indicators.queenSilent = false;
    indicators.increasedActivity = false;
    indicators.erraticPattern = false;
    indicators.riskLevel = 0;
    indicators.lastQueenDetected = 0;
}

void initializeDailyPattern(DailyPattern& pattern) {
    // Initialize hourly arrays
    for (int i = 0; i < 24; i++) {
        pattern.hourlyActivity[i] = 0;
        pattern.hourlyTemperature[i] = 0;
    }
    pattern.peakActivityTime = 12; // Default to noon
    pattern.quietestTime = 3;      // Default to 3 AM
    pattern.abnormalPattern = false;
}

// =============================================================================
// VALIDATION FUNCTIONS
// =============================================================================

bool isValidSensorData(const SensorData& data) {
    // Check temperature range
    if (data.temperature < -50.0 || data.temperature > 100.0) {
        return false;
    }
    
    // Check humidity range
    if (data.humidity < 0.0 || data.humidity > 100.0) {
        return false;
    }
    
    // Check pressure range (typical atmospheric pressure range)
    if (data.pressure < 300.0 || data.pressure > 1100.0) {
        return false;
    }
    
    // Check battery voltage range
    if (data.batteryVoltage < 0.0 || data.batteryVoltage > 5.0) {
        return false;
    }
    
    // Check frequency range
    if (data.dominantFreq > 2000) {
        return false;
    }
    
    // Check sound level range
    if (data.soundLevel > 100) {
        return false;
    }
    
    // Check bee state range
    if (data.beeState > BEE_UNKNOWN) {
        return false;
    }
    
    return true;
}

bool isValidSystemSettings(const SystemSettings& settings) {
    // Check temperature offset range
    if (settings.tempOffset < -10.0 || settings.tempOffset > 10.0) {
        return false;
    }
    
    // Check humidity offset range
    if (settings.humidityOffset < -20.0 || settings.humidityOffset > 20.0) {
        return false;
    }
    
    // Check audio sensitivity range
    if (settings.audioSensitivity > 10) {
        return false;
    }
    
    // Check frequency ranges
    if (settings.queenFreqMin >= settings.queenFreqMax) {
        return false;
    }
    
    if (settings.swarmFreqMin >= settings.swarmFreqMax) {
        return false;
    }
    
    // Check stress threshold
    if (settings.stressThreshold > 100) {
        return false;
    }
    
    // Check log interval
    if (settings.logInterval != 5 && settings.logInterval != 10 && 
        settings.logInterval != 30 && settings.logInterval != 60) {
        return false;
    }
    
    // Check temperature thresholds
    if (settings.tempMin >= settings.tempMax) {
        return false;
    }
    
    // Check humidity thresholds
    if (settings.humidityMin >= settings.humidityMax) {
        return false;
    }
    
    // Check display brightness
    if (settings.displayBrightness < 1 || settings.displayBrightness > 10) {
        return false;
    }
    
    // Check magic number
    if (settings.magicNumber != SETTINGS_MAGIC_NUMBER) {
        return false;
    }
    
    return true;
}

// =============================================================================
// DATA CONVERSION FUNCTIONS
// =============================================================================

void copyLogEntry(const SensorData& data, LogEntry& entry, uint32_t timestamp) {
    entry.unixTime = timestamp;
    entry.temperature = data.temperature;
    entry.humidity = data.humidity;
    entry.pressure = data.pressure;
    entry.dominantFreq = data.dominantFreq;
    entry.soundLevel = data.soundLevel;
    entry.beeState = data.beeState;
    entry.batteryVoltage = data.batteryVoltage;
    entry.alertFlags = data.alertFlags;
}

String sensorDataToString(const SensorData& data) {
    String result = "";
    
    result += "T:" + String(data.temperature, 1) + "C ";
    result += "H:" + String(data.humidity, 1) + "% ";
    result += "P:" + String(data.pressure, 1) + "hPa ";
    result += "F:" + String(data.dominantFreq) + "Hz ";
    result += "L:" + String(data.soundLevel) + "% ";
    result += "B:" + String(data.batteryVoltage, 2) + "V ";
    result += "State:" + String(data.beeState) + " ";
    result += "Alerts:0x" + String(data.alertFlags, HEX);
    
    return result;
}

String systemStatusToString(const SystemStatus& status) {
    String result = "Status: ";
    
    if (status.systemReady) result += "READY ";
    else result += "INIT ";
    
    result += "RTC:";
    result += status.rtcWorking ? "OK " : "FAIL ";
    
    result += "DISP:";
    result += status.displayWorking ? "OK " : "FAIL ";
    
    result += "BME:";
    result += status.bmeWorking ? "OK " : "FAIL ";
    
    result += "SD:";
    result += status.sdWorking ? "OK " : "FAIL ";
    
    result += "MIC:";
    result += status.pdmWorking ? "OK" : "FAIL";
    
    return result;
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

void resetSensorData(SensorData& data) {
    // Keep the last known valid readings but reset flags
    data.alertFlags = ALERT_NONE;
    data.sensorsValid = false;
    data.beeState = BEE_UNKNOWN;
}

void updateAbscondingRisk(AbscondingIndicators& indicators, uint32_t currentTime) {
    // Calculate time since last queen detection
    uint32_t timeSinceQueen = currentTime - indicators.lastQueenDetected;
    
    // Reset risk level calculation
    indicators.riskLevel = 0;
    
    // Queen silent for over 1 hour
    if (timeSinceQueen > 3600000) { // 1 hour in milliseconds
        indicators.queenSilent = true;
        indicators.riskLevel += 40;
    } else {
        indicators.queenSilent = false;
    }
    
    // Add other risk factors
    if (indicators.increasedActivity) {
        indicators.riskLevel += 30;
    }
    
    if (indicators.erraticPattern) {
        indicators.riskLevel += 30;
    }
    
    // Cap at 100%
    if (indicators.riskLevel > 100) {
        indicators.riskLevel = 100;
    }
}

void updateDailyPatternHour(DailyPattern& pattern, uint8_t hour, 
                           uint8_t activity, uint8_t temperature) {
    if (hour >= 24) return; // Invalid hour
    
    // Update with moving average (weight new reading at 25%)
    pattern.hourlyActivity[hour] = (pattern.hourlyActivity[hour] * 3 + activity) / 4;
    pattern.hourlyTemperature[hour] = (pattern.hourlyTemperature[hour] * 3 + temperature) / 4;
    
    // Find new peak and quiet times
    uint8_t maxActivity = 0;
    uint8_t minActivity = 255;
    
    for (int i = 0; i < 24; i++) {
        if (pattern.hourlyActivity[i] > maxActivity) {
            maxActivity = pattern.hourlyActivity[i];
            pattern.peakActivityTime = i;
        }
        if (pattern.hourlyActivity[i] < minActivity && pattern.hourlyActivity[i] > 0) {
            minActivity = pattern.hourlyActivity[i];
            pattern.quietestTime = i;
        }
    }
    
    // Check for abnormal patterns
    pattern.abnormalPattern = false;
    
    // Peak activity should be during day (9-17)
    if (pattern.peakActivityTime < 9 || pattern.peakActivityTime > 17) {
        pattern.abnormalPattern = true;
    }
    
    // Should be quiet at night (22-5)
    if ((hour >= 22 || hour <= 5) && activity > 50) {
        pattern.abnormalPattern = true;
    }
}

// =============================================================================
// DIAGNOSTIC FUNCTIONS
// =============================================================================

void printSensorData(const SensorData& data) {
    Serial.println(F("=== Sensor Data ==="));
    Serial.print(F("Temperature: ")); Serial.print(data.temperature); Serial.println(F("°C"));
    Serial.print(F("Humidity: ")); Serial.print(data.humidity); Serial.println(F("%"));
    Serial.print(F("Pressure: ")); Serial.print(data.pressure); Serial.println(F(" hPa"));
    Serial.print(F("Battery: ")); Serial.print(data.batteryVoltage); Serial.println(F("V"));
    Serial.print(F("Frequency: ")); Serial.print(data.dominantFreq); Serial.println(F(" Hz"));
    Serial.print(F("Sound Level: ")); Serial.print(data.soundLevel); Serial.println(F("%"));
    Serial.print(F("Bee State: ")); Serial.println(data.beeState);
    Serial.print(F("Alert Flags: 0x")); Serial.println(data.alertFlags, HEX);
    Serial.print(F("Sensors Valid: ")); Serial.println(data.sensorsValid ? "YES" : "NO");
    Serial.println(F("=================="));
}

void printSystemStatus(const SystemStatus& status) {
    Serial.println(F("=== System Status ==="));
    Serial.print(F("System Ready: ")); Serial.println(status.systemReady ? "YES" : "NO");
    Serial.print(F("RTC Working: ")); Serial.println(status.rtcWorking ? "YES" : "NO");
    Serial.print(F("Display Working: ")); Serial.println(status.displayWorking ? "YES" : "NO");
    Serial.print(F("BME Working: ")); Serial.println(status.bmeWorking ? "YES" : "NO");
    Serial.print(F("SD Working: ")); Serial.println(status.sdWorking ? "YES" : "NO");
    Serial.print(F("Microphone Working: ")); Serial.println(status.pdmWorking ? "YES" : "NO");
    Serial.println(F("====================="));
}



