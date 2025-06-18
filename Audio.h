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
extern int audioBuffer[AUDIO_SAMPLE_BUFFER_SIZE];
extern volatile int audioSampleIndex;

// Missing variables that were referenced
extern short* sampleBuffer;
extern volatile int samplesRead;

// Function declarations - CORRECTED SIGNATURES
void initializeAudio(uint8_t micGain, SystemStatus& status);
void processAudio(SensorData& data, SystemSettings& settings);

// Fixed: analyzeAudioBuffer() takes no parameters, uses global audioBuffer
AudioAnalysis analyzeAudioBuffer();

// Fixed: spectral centroid function name and signature
float estimateSpectralCentroidOptimized();

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


#endif // AUDIO_H