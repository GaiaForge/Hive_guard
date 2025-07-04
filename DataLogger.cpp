/**
 * DataLogger.cpp
 * SD card data logging implementation
 */

#include "DataLogger.h"
#include "Utils.h"
#include "Alerts.h" 
#include "Sensors.h"
#include "Display.h"  // For updateDiagnosticLine

// Use SDLib namespace to avoid ambiguity
using SDFile = SDLib::File;

// =============================================================================
// FILE MANAGEMENT
// =============================================================================

void createLogFile(RTC_DS3231& rtc, SystemStatus& status) {
    if (!status.sdWorking || !status.rtcWorking) return;
    
    // Create directory structure for monthly files
    DateTime now = rtc.now();
    char dirPath[20];
    sprintf(dirPath, "/HIVE_DATA/%04d", now.year());
    
    // Create directories if they don't exist
    SD.mkdir("/HIVE_DATA");
    SD.mkdir(dirPath);
    
    Serial.print(F("Log directory created: "));
    Serial.println(dirPath);
}


// =============================================================================
// DATA LOGGING
// =============================================================================

void logData(SensorData& data, RTC_DS3231& rtc, SystemSettings& settings, 
             SystemStatus& status) {
    if (!status.rtcWorking) return;
    
    // Try normal SD logging first
    if (status.sdWorking) {
        DateTime now = rtc.now();
        char filename[30];
        sprintf(filename, "/HIVE_DATA/%04d/%04d-%02d.CSV", 
                now.year(), now.year(), now.month());
        
        bool fileExists = SD.exists(filename);
        SDFile dataFile = SD.open(filename, FILE_WRITE);
        
        if (dataFile) {
            if (!fileExists) {
                writeLogHeader(dataFile, now, settings);
            }
            writeLogEntry(dataFile, now, data);
            dataFile.close();
            
            // If successful and we have buffered data, flush it
            if (hasBufferedData()) {
                flushBufferedData(rtc, settings, status);
            }
            
            Serial.println(F("Data logged to SD"));
        } else {
            // SD write failed - store in buffer
            status.sdWorking = false;
            storeInBuffer(data);
        }
    } else {
        // SD not working - store in emergency buffer
        storeInBuffer(data);
    }
}
// =============================================================================
// EMERGENCY DATA BUFFERING
// =============================================================================

static DataBuffer emergencyBuffer = {{}, 0, 0, false};

void storeInBuffer(const SensorData& data) {
    emergencyBuffer.readings[emergencyBuffer.writeIndex] = data;
    emergencyBuffer.writeIndex = (emergencyBuffer.writeIndex + 1) % 20;
    
    if (emergencyBuffer.count < 20) {
        emergencyBuffer.count++;
    } else {
        emergencyBuffer.full = true; // Overwriting old data
    }
    
    Serial.print(F("Data buffered ("));
    Serial.print(emergencyBuffer.count);
    Serial.println(F("/20)"));
}

void flushBufferedData(RTC_DS3231& rtc, SystemSettings& settings, SystemStatus& status) {
    Serial.print(F("Flushing "));
    Serial.print(emergencyBuffer.count);
    Serial.println(F(" buffered readings..."));
    
    uint8_t startIndex = emergencyBuffer.full ? emergencyBuffer.writeIndex : 0;
    uint8_t itemsToFlush = emergencyBuffer.count;
    
    for (uint8_t i = 0; i < itemsToFlush; i++) {
        uint8_t index = (startIndex + i) % 20;
        
        // Log each buffered reading
        logData(emergencyBuffer.readings[index], rtc, settings, status);
        
        // Small delay to avoid overwhelming SD card
        delay(10);
    }
    
    // Clear buffer after successful flush
    emergencyBuffer.count = 0;
    emergencyBuffer.writeIndex = 0;
    emergencyBuffer.full = false;
    
    Serial.println(F("Buffer flush complete"));
}

bool hasBufferedData() {
    return emergencyBuffer.count > 0;
}
// =============================================================================
// LOG FILE WRITING
// =============================================================================

void writeLogHeader(SDFile& file, DateTime& now, SystemSettings& settings) {
    file.println(F("# HIVE MONITOR DATA LOG - MONTHLY FILE"));
    file.println(F("# Device ID: HIVE_Tanzania_001"));
    file.println(F("# Firmware: v2.0"));
    file.print(F("# Month: "));
    file.print(now.year());
    file.print(F("-"));
    if (now.month() < 10) file.print(F("0"));
    file.println(now.month());
    file.print(F("# File Created: "));
    file.println(now.timestamp(DateTime::TIMESTAMP_DATE));
    
    // Write settings summary
    file.print(F("# Settings: TempOffset="));
    file.print(settings.tempOffset);
    file.print(F(",HumOffset="));
    file.print(settings.humidityOffset);
    file.print(F(",LogInterval="));
    file.print(settings.logInterval);    
    file.print(F(",AudioSens="));
    file.println(settings.audioSensitivity);
    
    file.print(F("# Thresholds: Temp="));
    file.print(settings.tempMin);
    file.print(F("-"));
    file.print(settings.tempMax);
    file.print(F("C,Humidity="));
    file.print(settings.humidityMin);
    file.print(F("-"));
    file.print(settings.humidityMax);
    file.println(F("%"));
    
    file.print(F("# Audio: Queen="));
    file.print(settings.queenFreqMin);
    file.print(F("-"));
    file.print(settings.queenFreqMax);
    file.print(F("Hz,Swarm="));
    file.print(settings.swarmFreqMin);
    file.print(F("-"));
    file.print(settings.swarmFreqMax);
    file.println(F("Hz"));
    
    file.println();
    
    // Column headers
    file.println(F("DateTime,UnixTime,Temp_C,Humidity_%,Pressure_hPa,Sound_Hz,Sound_Level,Bee_State,Battery_V,Alerts"));
}

void writeLogEntry(SDFile& file, DateTime& now, SensorData& data) {
    // DateTime
    file.print(now.timestamp(DateTime::TIMESTAMP_FULL));
    file.print(',');
    
    // Unix timestamp
    file.print(now.unixtime());
    file.print(',');
    
    // Temperature
    file.print(data.temperature, 1);
    file.print(',');
    
    // Humidity
    file.print(data.humidity, 1);
    file.print(',');
    
    // Pressure
    file.print(data.pressure, 1);
    file.print(',');
    
    // Sound frequency
    file.print(data.dominantFreq);
    file.print(',');
    
    // Sound level
    file.print(data.soundLevel);
    file.print(',');
    
    // Bee state
    file.print(getBeeStateString(data.beeState));
    file.print(',');
    
    // Battery voltage
    file.print(data.batteryVoltage, 2);
    file.print(',');
    
    // Alert flags
    file.println(getAlertString(data.alertFlags));
}

// =============================================================================
// DATA MAINTENANCE
// =============================================================================

void checkAndCleanOldData(DateTime now) {
    // Calculate date one year ago
    int oldYear = now.year() - 1;
    char oldYearPath[20];
    sprintf(oldYearPath, "/HIVE_DATA/%04d", oldYear);
    
    if (SD.exists(oldYearPath)) {
        // Count files in old directory
        int fileCount = countFilesInDirectory(oldYearPath);
        
        Serial.print(F("Found "));
        Serial.print(fileCount);
        Serial.print(F(" old monthly files from "));
        Serial.println(oldYear);
        
        // For monthly files, this will be 12 files or fewer
    }
}


int countFilesInDirectory(const char* dirPath) {
    SDFile dir = SD.open(dirPath);
    int count = 0;
    
    if (dir) {
        while (true) {
            SDFile entry = dir.openNextFile();
            if (!entry) break;
            
            if (!entry.isDirectory()) {
                count++;
            }
            entry.close();
        }
        dir.close();
    }
    
    return count;
}

// =============================================================================
// DATA EXPORT
// =============================================================================

void exportDataSummary(RTC_DS3231& rtc, SystemStatus& status) {
    if (!status.sdWorking || !status.rtcWorking) return;
    
    DateTime now = rtc.now();
    SDFile summaryFile = SD.open("/data_summary.txt", FILE_WRITE);
    
    if (summaryFile) {
        summaryFile.println(F("# HIVE MONITOR DATA SUMMARY - MONTHLY FILES"));
        summaryFile.print(F("# Generated: "));
        summaryFile.println(now.timestamp(DateTime::TIMESTAMP_FULL));
        summaryFile.println();
        
        // Scan through years
        for (int year = 2024; year <= now.year(); year++) {
            char yearPath[20];
            sprintf(yearPath, "/HIVE_DATA/%04d", year);
            
            if (SD.exists(yearPath)) {
                summaryFile.print(F("Year "));
                summaryFile.print(year);
                summaryFile.println(F(":"));
                
                // List monthly files
                SDFile dir = SD.open(yearPath);
                if (dir) {
                    while (true) {
                        SDFile entry = dir.openNextFile();
                        if (!entry) break;
                        
                        if (!entry.isDirectory()) {
                            summaryFile.print(F("  File: "));
                            summaryFile.print(entry.name());
                            summaryFile.print(F(" ("));
                            summaryFile.print(entry.size());
                            summaryFile.println(F(" bytes)"));
                        }
                        entry.close();
                    }
                    dir.close();
                }
            }
        }
        
        summaryFile.close();
        Serial.println(F("Monthly data summary exported"));
    }
}
// =============================================================================
// ERROR RECOVERY
// =============================================================================

void checkSDCard(SystemStatus& status) {
    // Try to reinitialize SD card if it's not working
    if (!status.sdWorking) {
        Serial.println(F("Attempting SD card recovery..."));
        
        if (SD.begin(SD_CS_PIN)) {
            status.sdWorking = true;
            Serial.println(F("SD card recovered"));
        } else {
            Serial.println(F("SD card recovery failed"));
        }
    }
    
    // Check available space
    if (status.sdWorking) {
        // Note: SD library doesn't have built-in space checking
        // This would require platform-specific implementation
        
        // Test write capability
        SDFile testFile = SD.open("/test.tmp", FILE_WRITE);
        if (testFile) {
            testFile.println(F("test"));
            testFile.close();
            SD.remove("/test.tmp");
        } else {
            Serial.println(F("SD card write test failed"));
            status.sdWorking = false;
        }
    }
}

void checkSDCardAtStartup(Adafruit_SH1106G& display, SystemStatus& status) {
    if (SD.begin(SD_CS_PIN)) {
        status.sdWorking = true;
        Serial.println(F(" OK"));
        updateDiagnosticLine(display, "SD Card: OK");
        
        // Test write capability
        SDFile testFile = SD.open("/test.tmp", FILE_WRITE);
        if (testFile) {
            testFile.println(F("test"));
            testFile.close();
            SD.remove("/test.tmp");
            updateDiagnosticLine(display, "SD Write: OK");
        } else {
            updateDiagnosticLine(display, "SD Write: FAILED");
        }
    } else {
        status.sdWorking = false;
        Serial.println(F(" NOT FOUND"));
        updateDiagnosticLine(display, "SD Card: NOT FOUND");
        
        // Show message for 1 second
        delay(1000);
        updateDiagnosticLine(display, "Will continue without SD");
    }
}


// =============================================================================
// DIAGNOSTIC LOGGING
// =============================================================================

void logDiagnostics(SystemStatus& status, SystemSettings& settings) {
    if (!status.sdWorking) return;
    
    SDFile diagFile = SD.open("/diagnostics.log", FILE_WRITE);
    
    if (diagFile) {
        diagFile.println(F("\n=== SYSTEM DIAGNOSTICS ==="));
        diagFile.print(F("Timestamp: "));
        diagFile.println(millis());
        
        diagFile.println(F("\nSystem Status:"));
        diagFile.print(F("  RTC: "));
        diagFile.println(status.rtcWorking ? "OK" : "FAIL");
        diagFile.print(F("  Display: "));
        diagFile.println(status.displayWorking ? "OK" : "FAIL");
        diagFile.print(F("  bme280: "));
        diagFile.print(F("  SD Card: "));
        diagFile.println(status.sdWorking ? "OK" : "FAIL");
        diagFile.print(F("  PDM Mic: "));
        diagFile.println(status.pdmWorking ? "OK" : "FAIL");
        
        diagFile.println(F("\nSettings:"));
        diagFile.print(F("  Temp Offset: "));
        diagFile.println(settings.tempOffset);
        diagFile.print(F("  Humidity Offset: "));
        diagFile.println(settings.humidityOffset);
        diagFile.print(F("  Log Interval: "));
        diagFile.print(settings.logInterval);
        diagFile.println(F(" minutes"));
        
        diagFile.close();
        Serial.println(F("Diagnostics logged"));
    }
}

// =============================================================================
// FIELD EVENT LOGGING
// =============================================================================


void logFieldEvent(uint8_t eventType, RTC_DS3231& rtc, SystemStatus& status) {
    if (!status.sdWorking || !status.rtcWorking) return;
    
    DateTime now = rtc.now();
    SDLib::File eventLog = SD.open("/field_events.csv", FILE_WRITE);
    
    if (!eventLog) {
        // Create file with header if it doesn't exist
        eventLog = SD.open("/field_events.csv", FILE_WRITE);
        if (eventLog) {
            eventLog.println(F("Date,Time,Event,Temperature,Humidity,Activity,QueenStatus"));
        }
    }
    
    if (eventLog) {
        // Log the event
        eventLog.print(now.timestamp(DateTime::TIMESTAMP_DATE));
        eventLog.print(F(","));
        eventLog.print(now.timestamp(DateTime::TIMESTAMP_TIME));
        eventLog.print(F(","));
        
        // Event type
        switch(eventType) {
            case EVENT_INSPECTION: eventLog.print(F("Inspection")); break;
            case EVENT_FEEDING: eventLog.print(F("Feeding")); break;
            case EVENT_TREATMENT: eventLog.print(F("Treatment")); break;
            case EVENT_HARVEST: eventLog.print(F("Harvest")); break;
            case EVENT_QUEEN_SEEN: eventLog.print(F("Queen_Seen")); break;
            case EVENT_SWARM_CAUGHT: eventLog.print(F("Swarm_Caught")); break;
            case EVENT_ABSCONDED: eventLog.print(F("ABSCONDED")); break;
            case EVENT_PREDATOR: eventLog.print(F("Predator")); break;
            default: eventLog.print(F("Unknown")); break;
        }
        
        eventLog.println();
        eventLog.close();
    }
}

// =============================================================================
// SIMPLIFIED DAILY REPORT
// =============================================================================

void generateDailyReport(DateTime date, SensorData& avgData, DailyPattern& pattern,
                        AbscondingIndicators& risk, SystemStatus& status) {
    if (!status.sdWorking) return;
    
    char filename[30];
    sprintf(filename, "/reports/%04d%02d%02d.txt", date.year(), date.month(), date.day());
    
    // Create reports directory
    SD.mkdir("/reports");
    
    SDLib::File report = SD.open(filename, FILE_WRITE);
    if (report) {
        report.println(F("=== DAILY HIVE REPORT ==="));
        report.print(F("Date: "));
        report.println(date.timestamp(DateTime::TIMESTAMP_DATE));
        report.println();
        
        // Health Status
        report.println(F("HIVE HEALTH:"));
        if (risk.riskLevel > 70) {
            report.println(F("  STATUS: CRITICAL - High absconding risk!"));
        } else if (risk.riskLevel > 40) {
            report.println(F("  STATUS: WARNING - Monitor closely"));
        } else {
            report.println(F("  STATUS: GOOD"));
        }
        report.println();
        
        // Environmental Summary
        report.println(F("ENVIRONMENT:"));
        report.print(F("  Avg Temperature: "));
        report.print(avgData.temperature, 1);
        report.println(F(" C"));
        report.print(F("  Avg Humidity: "));
        report.print(avgData.humidity, 1);
        report.println(F(" %"));
        
        // Recommendations
        report.println();
        report.println(F("RECOMMENDATIONS:"));
        
        if (avgData.temperature > 35) {
            report.println(F("  - Provide shade or ventilation"));
        }
        if (avgData.humidity < 40) {
            report.println(F("  - Add water source nearby"));
        }
        if (risk.queenSilent) {
            report.println(F("  - URGENT: Check for queen"));
        }
        if (pattern.abnormalPattern) {
            report.println(F("  - Inspect for disease/pests"));
        }
        
        // Activity Pattern
        report.println();
        report.println(F("ACTIVITY PATTERN:"));
        report.print(F("  Most active: "));
        report.print(pattern.peakActivityTime);
        report.println(F(":00"));
        report.print(F("  Quietest: "));
        report.print(pattern.quietestTime);
        report.println(F(":00"));
        
        report.close();
    }
}

// =============================================================================
// SMS-READY ALERT MESSAGES
// =============================================================================

void generateAlertMessage(char* buffer, size_t bufferSize, 
                         uint8_t hiveNumber, uint8_t alertType,
                         SensorData& data) {
    // Keep messages under 160 chars for SMS
    switch(alertType) {
        case ALERT_SWARM_RISK:
            snprintf(buffer, bufferSize, 
                    "HIVE %d: Swarm likely! Freq:%dHz Sound:%d%%", 
                    hiveNumber, data.dominantFreq, data.soundLevel);
            break;
            
        case ALERT_QUEEN_ISSUE:
            snprintf(buffer, bufferSize,
                    "HIVE %d: Queen problem! Check immediately", 
                    hiveNumber);
            break;
            
        case ALERT_TEMP_HIGH:
            snprintf(buffer, bufferSize,
                    "HIVE %d: Too hot! %.1fC - Add shade/ventilation", 
                    hiveNumber, data.temperature);
            break;
            
        default:
            snprintf(buffer, bufferSize,
                    "HIVE %d: Alert - Check hive", 
                    hiveNumber);
    }
}