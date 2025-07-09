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

struct SpectralFeatures {
    float spectralCentroid;      // "Brightness" of sound
    float totalEnergy;           // Overall sound energy
    float bandEnergyRatios[6];   // Energy in different frequency bands
    float harmonicity;           // How harmonic the sound is
};

struct ActivityTrend {
    float currentActivity;
    float baselineActivity;      // 24-hour rolling average  
    float activityIncrease;      // Current vs baseline ratio
    bool abnormalTiming;         // Active at wrong times
};

// Global audio variables
extern int audioBuffer[AUDIO_SAMPLE_BUFFER_SIZE];
extern volatile int audioSampleIndex;


// Function declarations
void initializeAudio(SystemStatus& status);
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
                                 DailyPattern& pattern, RTC_PCF8523& rtc);

                                 SpectralFeatures analyzeAudioFFT();
void updateActivityTrend(ActivityTrend& trend, SpectralFeatures& current, uint8_t hour);

#endif // AUDIO_H