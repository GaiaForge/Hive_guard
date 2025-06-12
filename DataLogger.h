/**
 * DataLogger.h
 * Data logging header
 */

#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include "Config.h"
#include "DataStructures.h"

// Use SDLib namespace to avoid ambiguity
using SDFile = SDLib::File;

// Function declarations
void createLogFile(RTC_DS3231& rtc, SystemStatus& status);
void logData(SensorData& data, RTC_DS3231& rtc, SystemSettings& settings, 
             SystemStatus& status);
void writeLogHeader(SDFile& file, DateTime& now, SystemSettings& settings);
void writeLogEntry(SDFile& file, DateTime& now, SensorData& data);
void checkAndCleanOldData(DateTime now);
int countFilesInDirectory(const char* dirPath);
void exportDataSummary(RTC_DS3231& rtc, SystemStatus& status);
void checkSDCard(SystemStatus& status);
void logDiagnostics(SystemStatus& status, SystemSettings& settings);

#endif // DATA_LOGGER_H