/**
 * Audio.h
 * Audio processing header
 */

#ifndef AUDIO_H
#define AUDIO_H

#include "Config.h"
#include "DataStructures.h"

// Audio analysis structure
struct AudioAnalysis {
    uint16_t dominantFreq;
    uint8_t soundLevel;
    float peakToAvg;
    float spectralCentroid;
};

// Global audio variables
extern short sampleBuffer[PDM_BUFFER_SIZE];
extern volatile int samplesRead;

// Function declarations
void initializeAudio(uint8_t micGain, SystemStatus& status);
void onPDMdata();
void processAudio(SensorData& data, SystemSettings& settings);
AudioAnalysis analyzeAudioBuffer(short* buffer, int samples);
float estimateSpectralCentroid(short* buffer, int samples);
uint8_t classifyBeeState(AudioAnalysis& analysis, SystemSettings& settings);
void runAudioDiagnostics(SystemStatus& status);
void calibrateAudioLevels(SystemSettings& settings, int durationSeconds);
AbscondingIndicators detectAbscondingRisk(AudioAnalysis& analysis, 
                                          SystemSettings& settings,
                                          uint32_t currentTime);
void updateDailyPattern(DailyPattern& pattern, uint8_t hour, 
                       uint8_t activity, float temperature);
uint8_t detectEnvironmentalStress(SensorData& data, AudioAnalysis& audio,
                                 DailyPattern& pattern, RTC_DS3231& rtc);

// Add to DataLogger.h:
void logFieldEvent(uint8_t eventType, RTC_DS3231& rtc, SystemStatus& status);
void generateDailyReport(DateTime date, SensorData& avgData, DailyPattern& pattern,
                        AbscondingIndicators& risk, SystemStatus& status);
void generateAlertMessage(char* buffer, size_t bufferSize, 
                         uint8_t hiveNumber, uint8_t alertType,
                         SensorData& data);

// Add to Display.h:
void drawDetailedData(Adafruit_SH1106G& display, SensorData& data, 
                     SystemStatus& status, AbscondingIndicators& risk);
void drawDailySummary(Adafruit_SH1106G& display, DailyPattern& pattern,
                     AbscondingIndicators& indicators);
#endif // AUDIO_H