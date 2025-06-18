/**
 * Alerts.cpp
 * Alert system implementation
 */

#include "Alerts.h"
#include "Utils.h"

// Alert history for preventing spam
static unsigned long lastAlertTime[7] = {0, 0, 0, 0, 0, 0, 0};  
const unsigned long ALERT_COOLDOWN = 300000; // 5 minutes

// =============================================================================
// ALERT CHECKING
// =============================================================================

void checkAlerts(SensorData& data, SystemSettings& settings, SystemStatus& status) {
    data.alertFlags = ALERT_NONE;
    
    // Environmental alerts (only if sensors are valid)
    if (data.sensorsValid) {
        // Temperature alerts
        if (data.temperature > settings.tempMax) {
            data.alertFlags |= ALERT_TEMP_HIGH;
        }
        if (data.temperature < settings.tempMin) {
            data.alertFlags |= ALERT_TEMP_LOW;
        }
        
        // Humidity alerts
        if (data.humidity > settings.humidityMax) {
            data.alertFlags |= ALERT_HUMIDITY_HIGH;
        }
        if (data.humidity < settings.humidityMin) {
            data.alertFlags |= ALERT_HUMIDITY_LOW;
        }
    }
    
    // Bee state alerts
    if (data.beeState == BEE_QUEEN_MISSING) {
        data.alertFlags |= ALERT_QUEEN_ISSUE;
    }
    if (data.beeState == BEE_PRE_SWARM) {
        data.alertFlags |= ALERT_SWARM_RISK;
    }
    
    // System alerts
    if (data.batteryVoltage < BATTERY_LOW && data.batteryVoltage > 0) {
        data.alertFlags |= ALERT_LOW_BATTERY;
    }
    
    // SD card error check (single location)
    if (!status.sdWorking) {
        data.alertFlags |= ALERT_SD_ERROR;
    }
}

// =============================================================================
// ALERT PRIORITY
// =============================================================================

uint8_t getAlertPriority(uint8_t alertFlag) {
    // Return priority level (higher number = higher priority)
    switch (alertFlag) {
        case ALERT_SWARM_RISK:
            return 5;
        case ALERT_QUEEN_ISSUE:
            return 4;
        case ALERT_TEMP_HIGH:
        case ALERT_TEMP_LOW:
            return 3;
        case ALERT_HUMIDITY_HIGH:
        case ALERT_HUMIDITY_LOW:
            return 2;
        case ALERT_LOW_BATTERY:
        case ALERT_SD_ERROR:
            return 1;
        default:
            return 0;
    }
}

// =============================================================================
// ALERT DESCRIPTIONS
// =============================================================================

const char* getAlertDescription(uint8_t alertFlag) {
    switch (alertFlag) {
        case ALERT_TEMP_HIGH:
            return "Temperature too high";
        case ALERT_TEMP_LOW:
            return "Temperature too low";
        case ALERT_HUMIDITY_HIGH:
            return "Humidity too high";
        case ALERT_HUMIDITY_LOW:
            return "Humidity too low";
        case ALERT_QUEEN_ISSUE:
            return "Queen problem detected";
        case ALERT_SWARM_RISK:
            return "Swarm behavior detected";
        case ALERT_LOW_BATTERY:
            return "Battery low - charge soon";
        case ALERT_SD_ERROR:
            return "SD card error";
        default:
            return "Unknown alert";
    }
}

// =============================================================================
// ALERT STRING FORMATTING
// =============================================================================

const char* getAlertString(uint8_t alertFlags) {
    static char alertStr[100];
    
    if (alertFlags == ALERT_NONE) {
        return "NONE";
    }
    
    alertStr[0] = '\0';
    
    // Build alert string with abbreviated names
    if (alertFlags & ALERT_TEMP_HIGH) strcat(alertStr, "TEMP_HIGH ");
    if (alertFlags & ALERT_TEMP_LOW) strcat(alertStr, "TEMP_LOW ");
    if (alertFlags & ALERT_HUMIDITY_HIGH) strcat(alertStr, "HUM_HIGH ");
    if (alertFlags & ALERT_HUMIDITY_LOW) strcat(alertStr, "HUM_LOW ");
    if (alertFlags & ALERT_QUEEN_ISSUE) strcat(alertStr, "QUEEN ");
    if (alertFlags & ALERT_SWARM_RISK) strcat(alertStr, "SWARM ");
    if (alertFlags & ALERT_LOW_BATTERY) strcat(alertStr, "LOW_BAT ");
    if (alertFlags & ALERT_SD_ERROR) strcat(alertStr, "SD_ERR");
    
    // Remove trailing space
    int len = strlen(alertStr);
    if (len > 0 && alertStr[len-1] == ' ') {
        alertStr[len-1] = '\0';
    }
    
    return alertStr;
}

// =============================================================================
// ALERT ACTIONS
// =============================================================================

void handleAlertActions(SensorData& data, SystemSettings& settings) {
    // Check each alert type and perform actions
    
    // Critical alerts
    if (data.alertFlags & ALERT_SWARM_RISK) {
        handleSwarmAlert();
    }
    
    if (data.alertFlags & ALERT_QUEEN_ISSUE) {
        handleQueenAlert();
    }
    
    // Environmental alerts
    if (data.alertFlags & (ALERT_TEMP_HIGH | ALERT_TEMP_LOW)) {
        handleTemperatureAlert(data.temperature, settings);
    }
    
    if (data.alertFlags & (ALERT_HUMIDITY_HIGH | ALERT_HUMIDITY_LOW)) {
        handleHumidityAlert(data.humidity, settings);
    }
    
    // System alerts
    if (data.alertFlags & ALERT_LOW_BATTERY) {
        handleLowBatteryAlert(data.batteryVoltage);
    }
    
    if (data.alertFlags & ALERT_SD_ERROR) {
        handleSDErrorAlert();
    }
}

// =============================================================================
// SPECIFIC ALERT HANDLERS
// =============================================================================

void handleSwarmAlert() {
    unsigned long currentTime = millis();
    
    if (currentTime - lastAlertTime[5] > ALERT_COOLDOWN) {
        Serial.println(F("!!! SWARM ALERT !!!"));
        Serial.println(F("Pre-swarm behavior detected"));
        Serial.println(F("Check hive immediately"));
        
        lastAlertTime[5] = currentTime;
        
        // Could trigger external notification here
        // e.g., LED flash pattern, buzzer, or wireless alert
    }
}

void handleQueenAlert() {
    unsigned long currentTime = millis();
    
    if (currentTime - lastAlertTime[4] > ALERT_COOLDOWN) {
        Serial.println(F("!!! QUEEN ALERT !!!"));
        Serial.println(F("Queen may be missing or in distress"));
        Serial.println(F("Inspect hive for queen presence"));
        
        lastAlertTime[4] = currentTime;
    }
}

void handleTemperatureAlert(float temp, SystemSettings& settings) {
    unsigned long currentTime = millis();
    
    if (temp > settings.tempMax && currentTime - lastAlertTime[0] > ALERT_COOLDOWN) {
        Serial.print(F("!!! HIGH TEMPERATURE: "));
        Serial.print(temp);
        Serial.println(F("°C !!!"));
        Serial.println(F("Ensure adequate ventilation"));
        
        lastAlertTime[0] = currentTime;
    }
    
    if (temp < settings.tempMin && currentTime - lastAlertTime[1] > ALERT_COOLDOWN) {
        Serial.print(F("!!! LOW TEMPERATURE: "));
        Serial.print(temp);
        Serial.println(F("°C !!!"));
        Serial.println(F("Check hive insulation"));
        
        lastAlertTime[1] = currentTime;
    }
}

void handleHumidityAlert(float humidity, SystemSettings& settings) {
    unsigned long currentTime = millis();
    
    if (humidity > settings.humidityMax && currentTime - lastAlertTime[2] > ALERT_COOLDOWN) {
        Serial.print(F("!!! HIGH HUMIDITY: "));
        Serial.print(humidity);
        Serial.println(F("% !!!"));
        Serial.println(F("Improve ventilation"));
        
        lastAlertTime[2] = currentTime;
    }
    
    if (humidity < settings.humidityMin && currentTime - lastAlertTime[3] > ALERT_COOLDOWN) {
        Serial.print(F("!!! LOW HUMIDITY: "));
        Serial.print(humidity);
        Serial.println(F("% !!!"));
        Serial.println(F("Consider water source"));
        
        lastAlertTime[3] = currentTime;
    }
}

void handleLowBatteryAlert(float voltage) {
    unsigned long currentTime = millis();
    
    if (currentTime - lastAlertTime[6] > ALERT_COOLDOWN) {
        Serial.print(F("!!! LOW BATTERY: "));
        Serial.print(voltage);
        Serial.println(F("V !!!"));
        Serial.println(F("Charge or replace battery soon"));
        
        lastAlertTime[6] = currentTime;
    }
}

void handleSDErrorAlert() {
    unsigned long currentTime = millis();
    
    if (currentTime - lastAlertTime[7] > ALERT_COOLDOWN) {
        Serial.println(F("!!! SD CARD ERROR !!!"));
        Serial.println(F("Data logging unavailable"));
        Serial.println(F("Check SD card connection"));
        
        lastAlertTime[7] = currentTime;
    }
}

// =============================================================================
// ALERT STATISTICS
// =============================================================================

void getAlertStatistics(uint32_t& totalAlerts, uint32_t alertCounts[8]) {
    // This would typically read from stored data
    // For now, return example data
    totalAlerts = 0;
    
    for (int i = 0; i < 8; i++) {
        alertCounts[i] = 0;
        if (lastAlertTime[i] > 0) {
            alertCounts[i] = 1; // Simple count
            totalAlerts++;
        }
    }
}

// =============================================================================
// ALERT LOG
// =============================================================================

void logAlert(uint8_t alertType, float value, RTC_DS3231& rtc, SystemStatus& status) {
    if (!status.sdWorking || !status.rtcWorking) return;
    
    SDLib::File alertLog = SD.open("/alerts.log", FILE_WRITE);
    
    if (alertLog) {
        DateTime now = rtc.now();
        
        // Write timestamp
        alertLog.print(now.timestamp(DateTime::TIMESTAMP_FULL));
        alertLog.print(F(","));
        
        // Write alert type
        alertLog.print(getAlertDescription(alertType));
        alertLog.print(F(","));
        
        // Write value if applicable
        if (value != 0) {
            alertLog.print(value);
        } else {
            alertLog.print(F("N/A"));
        }
        
        alertLog.println();
        alertLog.close();
    }
}