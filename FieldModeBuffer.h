// FieldModeBuffer.h
#ifndef FIELD_MODE_BUFFER_H
#define FIELD_MODE_BUFFER_H

#include "Config.h"
#include "DataStructures.h"
#include "Audio.h"
class FieldModeBufferManager {
private:
    FieldModeBuffer buffer;
    
public:
    FieldModeBufferManager();
    
    // Buffer management
    bool addReading(const SensorData& data, uint32_t timestamp, const AudioAnalysisResult* audioResult = nullptr);
    bool isBufferFull() const;
    uint8_t getBufferCount() const;
    void clearBuffer();
    
    // Buffer flushing to SD
    bool flushToSD(RTC_PCF8523& rtc, SystemStatus& status);
    
    // Get buffer for direct access (if needed)
    FieldModeBuffer& getBuffer() { return buffer; }
};

extern FieldModeBufferManager fieldBuffer;

#endif