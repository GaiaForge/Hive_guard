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

// Emergency data buffer for SD failures
struct DataBuffer {
    SensorData readings[20];  // Buffer 20 readings (3+ hours at 10min intervals)
    uint8_t count;
    uint8_t writeIndex;
    bool full;
};



// Function declarations
void createLogFile(RTC_DS3231& rtc, SystemStatus& status);
void logData(SensorData& data, RTC_DS3231& rtc, SystemSettings& settings, 
             SystemStatus& status);
void writeLogHeader(SDFile& file, DateTime& now, SystemSettings& settings);
void writeLogEntry(SDFile& file, DateTime& now, SensorData& data);
void checkAndCleanOldData(DateTime now);
int countFilesInDirectory(const char* dirPath);
void exportDataSummary(RTC_DS3231& rtc, SystemStatus& status);
void checkSDCardAtStartup(Adafruit_SH1106G& display, SystemStatus& status);
void checkSDCard(SystemStatus& status);
void logDiagnostics(SystemStatus& status, SystemSettings& settings);

void logFieldEvent(uint8_t eventType, RTC_DS3231& rtc, SystemStatus& status);
void generateDailyReport(DateTime date, SensorData& avgData, DailyPattern& pattern,
                        AbscondingIndicators& risk, SystemStatus& status);
void generateAlertMessage(char* buffer, size_t bufferSize, 
                         uint8_t hiveNumber, uint8_t alertType,
                         SensorData& data);
// Add these to DataLogger.h after the existing declarations:
void storeInBuffer(const SensorData& data);
void flushBufferedData(RTC_DS3231& rtc, SystemSettings& settings, SystemStatus& status);
bool hasBufferedData();
#endif // DATA_LOGGER_H