// FieldModeBuffer.cpp
#include "FieldModeBuffer.h"
#include "DataLogger.h"
#include "Utils.h"    // For getBeeStateString
#include "Alerts.h"   // For getAlertString
#include "Audio.h"

FieldModeBufferManager fieldBuffer;

FieldModeBufferManager::FieldModeBufferManager() {
    clearBuffer();
}

bool FieldModeBufferManager::addReading(const SensorData& data, uint32_t timestamp, const AudioAnalysisResult* audioResult) {
    if (buffer.count >= MAX_BUFFERED_READINGS) {
        return false; // Buffer full
    }
    
    BufferedReading& reading = buffer.readings[buffer.writeIndex];
    reading.timestamp = timestamp;
    
    // Basic sensor data
    reading.temperature = data.temperature;
    reading.humidity = data.humidity;
    reading.pressure = data.pressure;
    reading.batteryVoltage = data.batteryVoltage;
    reading.alertFlags = data.alertFlags;
    
    // Basic audio data
    reading.dominantFreq = data.dominantFreq;
    reading.soundLevel = data.soundLevel;
    reading.beeState = data.beeState;
    
     // NEW: Calculate environmental ML features
    reading.dewPoint = calculateDewPoint(data.temperature, data.humidity);
    reading.vapourPressureDeficit = calculateVPD(data.temperature, data.humidity);
    reading.heatIndex = calculateHeatIndex(data.temperature, data.humidity);
    
    // Calculate rates (need access to previous readings)
    extern float lastTemperature, lastHumidity, lastPressure;
    extern unsigned long lastEnvReading;
    extern bool envHistoryValid;
    
    if (envHistoryValid && lastEnvReading > 0) {
        float timeDiffHours = (millis() - lastEnvReading) / 3600000.0; // Convert to hours
        if (timeDiffHours > 0 && timeDiffHours < 2.0) { // Only if reasonable time diff
            reading.temperatureRate = (data.temperature - lastTemperature) / timeDiffHours;
            reading.humidityRate = (data.humidity - lastHumidity) / timeDiffHours;
            reading.pressureRate = (data.pressure - lastPressure) / timeDiffHours;
        } else {
            reading.temperatureRate = 0;
            reading.humidityRate = 0;
            reading.pressureRate = 0;
        }
    } else {
        reading.temperatureRate = 0;
        reading.humidityRate = 0;
        reading.pressureRate = 0;
    }
    
    // Update history for next calculation
    lastTemperature = data.temperature;
    lastHumidity = data.humidity;
    lastPressure = data.pressure;
    lastEnvReading = millis();
    envHistoryValid = true;
    
    // Calculate comfort and stress indices (need access to settings)
    reading.foragingComfortIndex = calculateForagingComfortIndex(
        data.temperature, data.humidity, data.pressure);
    
    // For stress calculation, we need the settings thresholds
    extern SystemSettings settings;
    reading.environmentalStress = calculateEnvironmentalStress(
        data.temperature, data.humidity, data.pressure,
        settings.tempMin, settings.tempMax, settings.humidityMin, settings.humidityMax);
    


    // FULL ML AUDIO DATA 
    if (audioResult && audioResult->analysisValid) {
        // Frequency band analysis
        reading.bandEnergy0_200Hz = audioResult->bandEnergy0_200Hz;
        reading.bandEnergy200_400Hz = audioResult->bandEnergy200_400Hz;
        reading.bandEnergy400_600Hz = audioResult->bandEnergy400_600Hz;
        reading.bandEnergy600_800Hz = audioResult->bandEnergy600_800Hz;
        reading.bandEnergy800_1000Hz = audioResult->bandEnergy800_1000Hz;
        reading.bandEnergy1000PlusHz = audioResult->bandEnergy1000PlusHz;
        
        // Advanced spectral features
        reading.spectralCentroid = audioResult->spectralCentroid;
        reading.spectralRolloff = audioResult->spectralRolloff;
        reading.spectralFlux = audioResult->spectralFlux;
        reading.spectralSpread = audioResult->spectralSpread;
        reading.spectralSkewness = audioResult->spectralSkewness;
        reading.spectralKurtosis = audioResult->spectralKurtosis;
        reading.zeroCrossingRate = audioResult->zeroCrossingRate;
        reading.peakToAvgRatio = audioResult->peakToAvgRatio;
        reading.harmonicity = audioResult->harmonicity;
        
        // Temporal features
        reading.shortTermEnergy = audioResult->shortTermEnergy;
        reading.midTermEnergy = audioResult->midTermEnergy;
        reading.longTermEnergy = audioResult->longTermEnergy;
        reading.energyEntropy = audioResult->energyEntropy;
        
        // Time-based cyclical features
        reading.hourOfDaySin = audioResult->hourOfDaySin;
        reading.hourOfDayCos = audioResult->hourOfDayCos;
        reading.dayOfYearSin = audioResult->dayOfYearSin;
        reading.dayOfYearCos = audioResult->dayOfYearCos;
        
        // Context and quality
        reading.contextFlags = audioResult->contextFlags;
        reading.ambientNoiseLevel = audioResult->ambientNoiseLevel;
        reading.signalQuality = audioResult->signalQuality;
        
        // Behavioral indicators
        reading.queenDetected = audioResult->queenDetected;
        reading.abscondingRisk = audioResult->abscondingRisk;
        reading.activityIncrease = audioResult->activityIncrease;
        
        reading.analysisValid = true;
    } else {
        // No audio data - zero out ML features
        reading.bandEnergy0_200Hz = 0;
        reading.bandEnergy200_400Hz = 0;
        reading.bandEnergy400_600Hz = 0;
        reading.bandEnergy600_800Hz = 0;
        reading.bandEnergy800_1000Hz = 0;
        reading.bandEnergy1000PlusHz = 0;
        reading.spectralCentroid = 0;
        reading.spectralRolloff = 0;
        reading.spectralFlux = 0;
        reading.spectralSpread = 0;
        reading.spectralSkewness = 0;
        reading.spectralKurtosis = 0;
        reading.zeroCrossingRate = 0;
        reading.peakToAvgRatio = 0;
        reading.harmonicity = 0;
        reading.shortTermEnergy = 0;
        reading.midTermEnergy = 0;
        reading.longTermEnergy = 0;
        reading.energyEntropy = 0;
        reading.hourOfDaySin = 0;
        reading.hourOfDayCos = 0;
        reading.dayOfYearSin = 0;
        reading.dayOfYearCos = 0;
        reading.contextFlags = 0;
        reading.ambientNoiseLevel = 0;
        reading.signalQuality = 0;
        reading.queenDetected = false;
        reading.abscondingRisk = 0;
        reading.activityIncrease = 0;
        reading.analysisValid = false;
    }
    
    buffer.writeIndex = (buffer.writeIndex + 1) % MAX_BUFFERED_READINGS;
    buffer.count++;
    
    Serial.print(F("Added FULL ML reading to buffer ("));
    Serial.print(buffer.count);
    Serial.println(F(" readings)"));
    
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
    Serial.println(F(" ML readings to SD..."));
    
    // Get current date for filename
    DateTime now = rtc.now();
    char filename[12];
    sprintf(filename, "/H%02d%02d.CSV", now.year() % 100, now.month());
    
    // Check if file exists to write header
    bool fileExists = SD.exists(filename);
    SDLib::File dataFile = SD.open(filename, FILE_WRITE);
    
    if (dataFile) {
        // Write ML header if new file
        if (!fileExists) {
            dataFile.println(F("DateTime,UnixTime,Temp_C,Humidity_%,Pressure_hPa,Battery_V,Alerts,"
                            "Sound_Hz,Sound_Level,Bee_State,"
                            "Band0_200Hz,Band200_400Hz,Band400_600Hz,Band600_800Hz,Band800_1000Hz,Band1000PlusHz,"
                            "SpectralCentroid,SpectralRolloff,SpectralFlux,SpectralSpread,SpectralSkewness,SpectralKurtosis,"
                            "ZeroCrossingRate,PeakToAvgRatio,Harmonicity,"
                            "ShortTermEnergy,MidTermEnergy,LongTermEnergy,EnergyEntropy,"
                            "HourSin,HourCos,DayYearSin,DayYearCos,"
                            "ContextFlags,AmbientNoise,SignalQuality,"
                            "QueenDetected,AbscondingRisk,ActivityIncrease,AnalysisValid,"
                            "DewPoint,VPD,HeatIndex,TempRate,HumidityRate,PressureRate,ForagingIndex,EnvStress"));
        }
        
        // Write all buffered readings with FULL ML DATA
        for (uint8_t i = 0; i < buffer.count; i++) {
            BufferedReading& reading = buffer.readings[i];
            DateTime readingTime = DateTime(reading.timestamp);
            
            // Basic data
            dataFile.print(readingTime.timestamp(DateTime::TIMESTAMP_FULL)); dataFile.print(',');
            dataFile.print(reading.timestamp); dataFile.print(',');
            dataFile.print(reading.temperature, 2); dataFile.print(',');
            dataFile.print(reading.humidity, 2); dataFile.print(',');
            dataFile.print(reading.pressure, 2); dataFile.print(',');
            dataFile.print(reading.batteryVoltage, 3); dataFile.print(',');
            dataFile.print(getAlertString(reading.alertFlags)); dataFile.print(',');
            
            // Basic audio
            dataFile.print(reading.dominantFreq); dataFile.print(',');
            dataFile.print(reading.soundLevel); dataFile.print(',');
            dataFile.print(getBeeStateString(reading.beeState)); dataFile.print(',');
            
            // ALL THE ML GOODNESS!
            dataFile.print(reading.bandEnergy0_200Hz, 4); dataFile.print(',');
            dataFile.print(reading.bandEnergy200_400Hz, 4); dataFile.print(',');
            dataFile.print(reading.bandEnergy400_600Hz, 4); dataFile.print(',');
            dataFile.print(reading.bandEnergy600_800Hz, 4); dataFile.print(',');
            dataFile.print(reading.bandEnergy800_1000Hz, 4); dataFile.print(',');
            dataFile.print(reading.bandEnergy1000PlusHz, 4); dataFile.print(',');
            dataFile.print(reading.spectralCentroid, 2); dataFile.print(',');
            dataFile.print(reading.spectralRolloff, 2); dataFile.print(',');
            dataFile.print(reading.spectralFlux, 4); dataFile.print(',');
            dataFile.print(reading.spectralSpread, 2); dataFile.print(',');
            dataFile.print(reading.spectralSkewness, 4); dataFile.print(',');
            dataFile.print(reading.spectralKurtosis, 4); dataFile.print(',');
            dataFile.print(reading.zeroCrossingRate, 4); dataFile.print(',');
            dataFile.print(reading.peakToAvgRatio, 3); dataFile.print(',');
            dataFile.print(reading.harmonicity, 4); dataFile.print(',');
            dataFile.print(reading.shortTermEnergy, 3); dataFile.print(',');
            dataFile.print(reading.midTermEnergy, 3); dataFile.print(',');
            dataFile.print(reading.longTermEnergy, 3); dataFile.print(',');
            dataFile.print(reading.energyEntropy, 4); dataFile.print(',');
            dataFile.print(reading.hourOfDaySin, 4); dataFile.print(',');
            dataFile.print(reading.hourOfDayCos, 4); dataFile.print(',');
            dataFile.print(reading.dayOfYearSin, 4); dataFile.print(',');
            dataFile.print(reading.dayOfYearCos, 4); dataFile.print(',');
            dataFile.print(reading.contextFlags); dataFile.print(',');
            dataFile.print(reading.ambientNoiseLevel, 2); dataFile.print(',');
            dataFile.print(reading.signalQuality); dataFile.print(',');
            dataFile.print(reading.queenDetected ? "TRUE" : "FALSE"); dataFile.print(',');
            dataFile.print(reading.abscondingRisk); dataFile.print(',');
            dataFile.print(reading.activityIncrease, 3); dataFile.print(',');
            dataFile.println(reading.analysisValid ? "TRUE" : "FALSE");

            // Environmental ML data
            dataFile.print(reading.dewPoint, 2); dataFile.print(',');
            dataFile.print(reading.vapourPressureDeficit, 3); dataFile.print(',');
            dataFile.print(reading.heatIndex, 2); dataFile.print(',');
            dataFile.print(reading.temperatureRate, 3); dataFile.print(',');
            dataFile.print(reading.humidityRate, 3); dataFile.print(',');
            dataFile.print(reading.pressureRate, 3); dataFile.print(',');
            dataFile.print(reading.foragingComfortIndex, 1); dataFile.print(',');
            dataFile.println(reading.environmentalStress, 1);
        }
        
        dataFile.close();
        
        Serial.println(F("FULL ML buffer flushed successfully - PURE DATA GOLD!"));
        clearBuffer();
        return true;
        
    } else {
        Serial.println(F("Failed to open SD file for ML buffer flush"));
        return false;
    }
}