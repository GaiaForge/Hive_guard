// FieldModeBuffer.cpp
#include "FieldModeBuffer.h"
#include "DataLogger.h"
#include "Utils.h"    // For getBeeStateString
#include "Alerts.h"   // For getAlertString

FieldModeBufferManager fieldBuffer;

FieldModeBufferManager::FieldModeBufferManager() {
    clearBuffer();
}

bool FieldModeBufferManager::addReading(const SensorData& data, uint32_t timestamp) {
    if (buffer.count >= MAX_BUFFERED_READINGS) {
        return false; // Buffer full
    }
    
    BufferedReading& reading = buffer.readings[buffer.writeIndex];
    reading.timestamp = timestamp;
    reading.temperature = data.temperature;
    reading.humidity = data.humidity;
    reading.pressure = data.pressure;
    reading.frequency = data.dominantFreq;
    reading.soundLevel = data.soundLevel;
    reading.beeState = data.beeState;
    reading.batteryVoltage = data.batteryVoltage;
    reading.alertFlags = data.alertFlags;
    
    buffer.writeIndex = (buffer.writeIndex + 1) % MAX_BUFFERED_READINGS;
    buffer.count++;
    
    Serial.print(F("Added reading to buffer ("));
    Serial.print(buffer.count);
    Serial.println(F("/12)"));
    
    return true;
}

bool FieldModeBufferManager::isBufferFull() const {
    return (buffer.count >= MAX_BUFFERED_READINGS);
}

uint8_t FieldModeBufferManager::getBufferCount() const {
    return buffer.count;
}

void FieldModeBufferManager::clearBuffer() {
    buffer.count = 0;
    buffer.writeIndex = 0;
    buffer.lastFlushTime = millis();
}

bool FieldModeBufferManager::flushToSD(RTC_PCF8523& rtc, SystemStatus& status) {
    if (buffer.count == 0) {
        return true; // Nothing to flush
    }
    
    if (!status.sdWorking || !status.rtcWorking) {
        return false;
    }
    
    Serial.print(F("Flushing "));
    Serial.print(buffer.count);
    Serial.println(F(" readings to SD..."));
    
    // Get current date for filename
    DateTime now = rtc.now();
    char filename[30];
    sprintf(filename, "/HIVE_DATA/%04d/%04d-%02d.CSV", 
            now.year(), now.year(), now.month());
    
    // Check if file exists to write header
    bool fileExists = SD.exists(filename);
    SDLib::File dataFile = SD.open(filename, FILE_WRITE);
    
    if (dataFile) {
        // Write header if new file
        if (!fileExists) {
            dataFile.println(F("DateTime,UnixTime,Temp_C,Humidity_%,Pressure_hPa,Sound_Hz,Sound_Level,Bee_State,Battery_V,Alerts"));
        }
        
        // Write all buffered readings
        for (uint8_t i = 0; i < buffer.count; i++) {
            BufferedReading& reading = buffer.readings[i];
            DateTime readingTime = DateTime(reading.timestamp);
            
            // DateTime
            dataFile.print(readingTime.timestamp(DateTime::TIMESTAMP_FULL));
            dataFile.print(',');
            
            // Unix timestamp
            dataFile.print(reading.timestamp);
            dataFile.print(',');
            
            // Sensor data
            dataFile.print(reading.temperature, 1);
            dataFile.print(',');
            dataFile.print(reading.humidity, 1);
            dataFile.print(',');
            dataFile.print(reading.pressure, 1);
            dataFile.print(',');
            dataFile.print(reading.frequency);
            dataFile.print(',');
            dataFile.print(reading.soundLevel);
            dataFile.print(',');
            dataFile.print(getBeeStateString(reading.beeState));
            dataFile.print(',');
            dataFile.print(reading.batteryVoltage, 2);
            dataFile.print(',');
            dataFile.println(getAlertString(reading.alertFlags));
        }
        
        dataFile.close();
        
        Serial.println(F("Buffer flushed successfully"));
        clearBuffer();
        return true;
        
    } else {
        Serial.println(F("Failed to open SD file for buffer flush"));
        return false;
    }
}