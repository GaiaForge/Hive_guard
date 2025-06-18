/**
 * Alerts.h
 * Alert system header
 */

#ifndef ALERTS_H
#define ALERTS_H

#include "Config.h"
#include "DataStructures.h"

// Function declarations
void checkAlerts(SensorData& data, SystemSettings& settings, SystemStatus& status);
uint8_t getAlertPriority(uint8_t alertFlag);
const char* getAlertDescription(uint8_t alertFlag);
const char* getAlertString(uint8_t alertFlags);
void handleAlertActions(SensorData& data, SystemSettings& settings);

// Specific alert handlers
void handleSwarmAlert();
void handleQueenAlert();
void handleTemperatureAlert(float temp, SystemSettings& settings);
void handleHumidityAlert(float humidity, SystemSettings& settings);
void handleLowBatteryAlert(float voltage);

// Alert statistics and logging
void handleSDErrorAlert();
void getAlertStatistics(uint32_t& totalAlerts, uint32_t alertCounts[8]);
void logAlert(uint8_t alertType, float value, RTC_DS3231& rtc, SystemStatus& status);

#endif // ALERTS_H