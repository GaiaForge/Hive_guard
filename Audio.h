/**
 * Audio.h
 * Audio processing header - Clean AudioProcessor-only version
 */

#ifndef AUDIO_H
#define AUDIO_H

#include "Config.h"
#include "DataStructures.h"

// =============================================================================
// AUDIO CONFIGURATION
// =============================================================================

// FFT Configuration
#define FFT_SIZE 256
#define FFT_SIZE_LOG2 8

// Display update configuration
#define DISPLAY_UPDATE_SAMPLES 20  // Samples for real-time display
#define DISPLAY_SMOOTHING_FACTOR 0.7

// =============================================================================
// AUDIO STRUCTURES
// =============================================================================

// Lightweight structure for real-time display
struct AudioDisplayData {
    float soundLevel;         // 0-100%
    float dominantFreq;       // Estimated frequency
    float spectralCentroid;   // Estimated brightness
    float activityRatio;      // Current vs baseline
    uint8_t beeState;         // Current classification
    uint8_t abscondingRisk;   // Risk percentage
};

// Comprehensive structure for logging
struct AudioAnalysisResult {
    // Core metrics
    uint16_t dominantFreq;    // Peak frequency from FFT
    uint8_t soundLevel;       // Overall sound level 0-100%
    uint8_t beeState;         // Classified bee state
    
    // Frequency band analysis (0.0-1.0 ratios)
    float bandEnergy0_200Hz;      // Low frequency noise
    float bandEnergy200_400Hz;    // Queen piping range  
    float bandEnergy400_600Hz;    // Pre-swarm range
    float bandEnergy600_800Hz;    // Defensive/agitated range
    float bandEnergy800_1000Hz;   // High activity
    float bandEnergy1000PlusHz;   // Very high frequencies
    
    // Advanced metrics
    float spectralCentroid;   // Frequency center of mass
    float peakToAvgRatio;     // Signal consistency
    float harmonicity;        // Tonal vs noise ratio
    
    // Behavioral indicators
    bool queenDetected;       // Queen frequency present
    uint8_t abscondingRisk;   // 0-100% risk level
    float activityIncrease;   // Ratio vs baseline
    
    // Extended ML features
    float spectralRolloff;    // Frequency below which 85% of energy is contained
    float spectralFlux;       // Spectral change from previous analysis
    float zeroCrossingRate;   // Normalized zero crossing rate
    float spectralSpread;     // Frequency spread around centroid
    float spectralSkewness;   // Asymmetry of spectrum
    float spectralKurtosis;   // Peakedness of spectrum
    
    // Temporal features
    float shortTermEnergy;    // Energy in last minute (normalized)
    float midTermEnergy;      // Energy in last 10 minutes
    float longTermEnergy;     // Energy in last hour
    float energyEntropy;      // Randomness of energy distribution
    
    // Time-based features for ML
    float hourOfDaySin;       // sin(2π * hour/24)
    float hourOfDayCos;       // cos(2π * hour/24)
    float dayOfYearSin;       // sin(2π * day/365)
    float dayOfYearCos;       // cos(2π * day/365)
    
    // Context and metadata
    uint8_t contextFlags;     // Bit flags for context
    float ambientNoiseLevel;  // Background noise estimate
    uint8_t signalQuality;    // 0-100 quality score
    
    // Status
    bool analysisValid;       // Data quality flag
};

// Context flags for rich metadata
enum AudioContextFlags {
    CONTEXT_AFTER_INSPECTION = 0x01,
    CONTEXT_AFTER_FEEDING = 0x02,
    CONTEXT_WEATHER_CHANGE = 0x04,
    CONTEXT_SWARM_SEASON = 0x08,
    CONTEXT_HONEY_FLOW = 0x10,
    CONTEXT_QUEEN_CHANGE = 0x20,
    CONTEXT_MORNING = 0x40,
    CONTEXT_EVENING = 0x80
};

// Internal analysis structure
struct AudioAnalysis {
    uint16_t dominantFreq;
    uint8_t soundLevel;
    float peakToAvg;
    float spectralCentroid;
};

struct SpectralFeatures {
    float spectralCentroid;
    float totalEnergy;
    float bandEnergyRatios[6];
    float harmonicity;
};

struct ActivityTrend {
    float currentActivity;
    float baselineActivity;
    float activityIncrease;
    bool abnormalTiming;
};

// =============================================================================
// AUDIO PROCESSOR CLASS
// =============================================================================

class AudioProcessor {
private:
    // Audio buffers
    float audioBuffer[FFT_SIZE];
    float fftReal[FFT_SIZE];
    float fftImag[FFT_SIZE];
    float fftMagnitude[FFT_SIZE/2];
    float prevMagnitude[FFT_SIZE/2];  // For spectral flux
    int bufferIndex;
    
    // Real-time display data
    AudioDisplayData displayData;
    float realtimeBuffer[DISPLAY_UPDATE_SAMPLES];
    int realtimeIndex;
    
    // Tracking data
    ActivityTrend activityTrend;
    AbscondingIndicators abscondingIndicators;
    unsigned long lastQueenDetected;
    
    // Temporal tracking for ML features
    float energyHistory[60];  // 1-minute history at 1Hz
    int energyHistoryIndex;
    float shortTermEnergySum;
    float midTermEnergySum;
    float longTermEnergySum;
    
    // System references
    SystemSettings* settings;
    SystemStatus* status;
    
    // FFT functions
    void performFFT();
    void computeFFT(float* real, float* imag, int n);
    void computeMagnitudes();
    int reverseBits(int num, int log2n);
    float getFrequencyBin(int bin);
    float getBandEnergy(float freqMin, float freqMax);
    
    // Analysis functions
    AudioAnalysis analyzeAudioBuffer();
    SpectralFeatures analyzeSpectralFeatures();
    uint8_t classifyBeeState(AudioAnalysis& analysis, SpectralFeatures& features);
    void updateActivityTrend(SpectralFeatures& features);
    void updateAbscondingRisk(AudioAnalysis& analysis, float queenBandEnergy);
    void calculateExtendedFeatures(AudioAnalysisResult& result, DateTime& timestamp);
    void updateTemporalFeatures(AudioAnalysisResult& result);
    float calculateSpectralRolloff(float percentage);
    float calculateSpectralFlux();
    float calculateZeroCrossingRate();
    uint8_t calculateSignalQuality();
    
public:
    AudioProcessor();
    
    // Initialization
    void initialize(SystemSettings* sysSettings, SystemStatus* sysStatus);
    bool detectMicrophone();
    
    // Real-time processing (called frequently)
    void addSample(int rawSample);
    void updateDisplayData();
    AudioDisplayData getDisplayData() const { return displayData; }
    
    // Full analysis (called at log intervals)
    AudioAnalysisResult performFullAnalysis();
    
    // Utility functions
    void resetBuffers();
    void resetBaseline();
    bool isBufferReady() const { return bufferIndex >= FFT_SIZE; }
    int getBufferProgress() const { return (bufferIndex * 100) / FFT_SIZE; }
    
    // Diagnostics
    void runDiagnostics();
    void printStatus() const;
};

// =============================================================================
// GLOBAL AUDIO INSTANCE
// =============================================================================

extern AudioProcessor audioProcessor;

// =============================================================================
// SIMPLE INTERFACE FUNCTIONS (for main.cpp compatibility)
// =============================================================================

void initializeAudio(SystemStatus& status);
void processAudio(SensorData& data, SystemSettings& settings);
void runAudioDiagnostics(SystemStatus& status);
void calibrateAudioLevels(SystemSettings& settings, int durationSeconds);

#endif // AUDIO_H