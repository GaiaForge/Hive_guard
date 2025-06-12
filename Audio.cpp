/**
 * Audio.cpp
 * Audio processing and bee state classification
 */

#include "Audio.h"
#include "Utils.h"

// Global audio buffers
short sampleBuffer[PDM_BUFFER_SIZE];
volatile int samplesRead = 0;

// =============================================================================
// AUDIO INITIALIZATION
// =============================================================================

void initializeAudio(uint8_t micGain, SystemStatus& status) {
    // Set PDM data handler
    PDM.onReceive(onPDMdata);
    
    // Initialize PDM with mono channel at 16kHz
    if (PDM.begin(1, AUDIO_SAMPLE_RATE)) {
        status.pdmWorking = true;
        
        // Set microphone gain (0-100)
        PDM.setGain(micGain * 10);
        
        Serial.println(F("PDM microphone initialized"));
    } else {
        Serial.println(F("PDM initialization failed"));
    }
}

// =============================================================================
// PDM DATA HANDLER
// =============================================================================

void onPDMdata() {
    // Query the number of available bytes
    int bytesAvailable = PDM.available();
    
    // Read into the sample buffer
    PDM.read(sampleBuffer, bytesAvailable);
    
    // Update samples read count
    samplesRead = bytesAvailable / 2;  // 16-bit samples
}

// =============================================================================
// AUDIO PROCESSING
// =============================================================================

void processAudio(SensorData& data, SystemSettings& settings) {
    if (samplesRead == 0) {
        return;
    }
    
    // Process the audio buffer
    AudioAnalysis analysis = analyzeAudioBuffer(sampleBuffer, samplesRead);
    
    // Update sensor data
    data.dominantFreq = analysis.dominantFreq;
    data.soundLevel = analysis.soundLevel;
    
    // Classify bee state
    data.beeState = classifyBeeState(analysis, settings);
    
    // Reset samples read
    samplesRead = 0;
}

// =============================================================================
// AUDIO ANALYSIS
// =============================================================================

AudioAnalysis analyzeAudioBuffer(short* buffer, int samples) {
    AudioAnalysis result;
    
    // Zero crossing analysis for frequency detection
    int zeroCrossings = 0;
    int lastSign = 0;
    long sumAmplitude = 0;
    int maxAmplitude = 0;
    
    for (int i = 1; i < samples; i++) {
        // Calculate amplitude sum and max
        int amplitude = abs(buffer[i]);
        sumAmplitude += amplitude;
        if (amplitude > maxAmplitude) {
            maxAmplitude = amplitude;
        }
        
        // Detect zero crossings
        int currentSign = (buffer[i] >= 0) ? 1 : -1;
        if (lastSign != 0 && currentSign != lastSign) {
            zeroCrossings++;
        }
        lastSign = currentSign;
    }
    
    // Calculate dominant frequency from zero crossings
    float duration = (float)samples / AUDIO_SAMPLE_RATE;
    result.dominantFreq = (zeroCrossings / 2.0) / duration;
    
    // Calculate sound level (0-100)
    float avgAmplitude = sumAmplitude / samples;
    result.soundLevel = constrain(map(avgAmplitude, 0, 5000, 0, 100), 0, 100);
    
    // Calculate peak-to-average ratio (for detecting specific patterns)
    result.peakToAvg = (avgAmplitude > 0) ? (float)maxAmplitude / avgAmplitude : 0;
    
    // Simple spectral centroid estimation
    result.spectralCentroid = estimateSpectralCentroid(buffer, samples);
    
    return result;
}

// =============================================================================
// SPECTRAL ANALYSIS
// =============================================================================

float estimateSpectralCentroid(short* buffer, int samples) {
    // Simple spectral centroid estimation using zero-crossing rate variations
    // This gives us a rough idea of where the frequency content is centered
    
    const int windowSize = 64;
    const int numWindows = samples / windowSize;
    
    if (numWindows < 2) {
        return 0;
    }
    
    float totalWeightedFreq = 0;
    float totalEnergy = 0;
    
    for (int w = 0; w < numWindows; w++) {
        int windowStart = w * windowSize;
        int zeroCrossings = 0;
        float windowEnergy = 0;
        
        // Count zero crossings in this window
        for (int i = 1; i < windowSize; i++) {
            int idx = windowStart + i;
            if (idx < samples) {
                if ((buffer[idx-1] >= 0 && buffer[idx] < 0) ||
                    (buffer[idx-1] < 0 && buffer[idx] >= 0)) {
                    zeroCrossings++;
                }
                windowEnergy += abs(buffer[idx]);
            }
        }
        
        // Estimate frequency for this window
        float windowFreq = (zeroCrossings * AUDIO_SAMPLE_RATE) / (2.0 * windowSize);
        
        // Weight by energy
        totalWeightedFreq += windowFreq * windowEnergy;
        totalEnergy += windowEnergy;
    }
    
    return (totalEnergy > 0) ? totalWeightedFreq / totalEnergy : 0;
}

// =============================================================================
// BEE STATE CLASSIFICATION
// =============================================================================

uint8_t classifyBeeState(AudioAnalysis& analysis, SystemSettings& settings) {
    uint16_t freq = analysis.dominantFreq;
    uint8_t level = analysis.soundLevel;
    float peakRatio = analysis.peakToAvg;
    
    // Quiet hive
    if (level < 10) {
        return BEE_QUIET;
    }
    
    // Queen detection (specific frequency range with stable pattern)
    if (freq >= settings.queenFreqMin && freq <= settings.queenFreqMax) {
        // Queen piping has a distinctive pattern
        if (peakRatio > 2.5 && level > 30) {
            return BEE_QUEEN_PRESENT;
        }
    }
    
    // Pre-swarm detection (higher frequencies, increased activity)
    if (freq >= settings.swarmFreqMin && freq <= settings.swarmFreqMax) {
        if (level > 60 && peakRatio > 3.0) {
            return BEE_PRE_SWARM;
        }
    }
    
    // Stress detection (high activity, irregular patterns)
    if (level > settings.stressThreshold) {
        if (peakRatio > 4.0 || analysis.spectralCentroid > 500) {
            return BEE_STRESSED;
        }
    }
    
    // Defensive behavior (very high frequencies)
    if (freq > 600 && level > 70) {
        return BEE_DEFENSIVE;
    }
    
    // Missing queen (specific frequency absence with high activity)
    if (level > 50 && (freq < settings.queenFreqMin || freq > settings.queenFreqMax)) {
        // Look for absence of queen frequencies during high activity
        if (analysis.spectralCentroid < 250) {
            return BEE_QUEEN_MISSING;
        }
    }
    
    // Active vs Normal
    if (level > 50) {
        return BEE_ACTIVE;
    } else {
        return BEE_NORMAL;
    }
}

// =============================================================================
// ABSCONDING DETECTION
// =============================================================================


AbscondingIndicators detectAbscondingRisk(AudioAnalysis& analysis, 
                                          SystemSettings& settings,
                                          uint32_t currentTime) {
    static uint32_t lastQueenTime = 0;
    static float activityBaseline = 0;
    static bool baselineSet = false;
    static float activityHistory[10] = {0};
    static int historyIndex = 0;
    
    AbscondingIndicators indicators;
    
    // Update activity history
    activityHistory[historyIndex] = analysis.soundLevel;
    historyIndex = (historyIndex + 1) % 10;
    
    // Calculate baseline after collecting enough data
    if (!baselineSet && historyIndex == 0) {
        activityBaseline = calculateAverage(activityHistory, 10);
        baselineSet = true;
    }
    
    // Check for queen presence
    if (analysis.dominantFreq >= settings.queenFreqMin && 
        analysis.dominantFreq <= settings.queenFreqMax &&
        analysis.soundLevel > 20) {
        lastQueenTime = currentTime;
        indicators.lastQueenDetected = currentTime;
    }
    
    // Queen silent for too long?
    uint32_t timeSinceQueen = currentTime - lastQueenTime;
    indicators.queenSilent = (timeSinceQueen > 3600000); // 1 hour
    
    // Increased activity?
    if (baselineSet) {
        float currentActivity = calculateAverage(activityHistory, 10);
        indicators.increasedActivity = (currentActivity > activityBaseline * 1.5);
    }
    
    // Erratic patterns?
    float variance = calculateStandardDeviation(activityHistory, 10);
    indicators.erraticPattern = (variance > 20);
    
    // Calculate risk level
    indicators.riskLevel = 0;
    if (indicators.queenSilent) indicators.riskLevel += 40;
    if (indicators.increasedActivity) indicators.riskLevel += 30;
    if (indicators.erraticPattern) indicators.riskLevel += 30;
    
    // African bees specific patterns
    if (analysis.dominantFreq > 450 && analysis.soundLevel > 70) {
        // High frequency + high volume = distress
        indicators.riskLevel += 20;
    }
    
    if (indicators.riskLevel > 100) indicators.riskLevel = 100;
    
    return indicators;
}

// =============================================================================
// DAILY PATTERN TRACKING
// =============================================================================



void updateDailyPattern(DailyPattern& pattern, uint8_t hour, 
                       uint8_t activity, float temperature) {
    // Update hourly averages
    pattern.hourlyActivity[hour] = (pattern.hourlyActivity[hour] + activity) / 2;
    pattern.hourlyTemperature[hour] = (uint8_t)temperature;
    
    // Find peak and quiet times
    uint8_t maxActivity = 0;
    uint8_t minActivity = 255;
    for (int i = 0; i < 24; i++) {
        if (pattern.hourlyActivity[i] > maxActivity) {
            maxActivity = pattern.hourlyActivity[i];
            pattern.peakActivityTime = i;
        }
        if (pattern.hourlyActivity[i] < minActivity && pattern.hourlyActivity[i] > 0) {
            minActivity = pattern.hourlyActivity[i];
            pattern.quietestTime = i;
        }
    }
    
    // Check for abnormal patterns
    // Normal: High activity 10am-4pm, quiet at night
    pattern.abnormalPattern = false;
    if (pattern.peakActivityTime < 9 || pattern.peakActivityTime > 17) {
        pattern.abnormalPattern = true; // Peak activity outside normal hours
    }
    if (hour >= 22 || hour <= 5) {
        if (activity > 50) {
            pattern.abnormalPattern = true; // Too active at night
        }
    }
}

// =============================================================================
// ENVIRONMENTAL STRESS DETECTION
// =============================================================================


uint8_t detectEnvironmentalStress(SensorData& data, AudioAnalysis& audio,
                                 DailyPattern& pattern, RTC_DS3231& rtc) {
    uint8_t stressFactors = STRESS_NONE;
    
    // Temperature stress (African bees sensitive to heat)
    if (data.temperature > 35.0) {  // 35°C is stressful
        stressFactors |= STRESS_HEAT;
    }
    if (data.temperature < 15.0) {  // Cold stress
        stressFactors |= STRESS_COLD;
    }
    
    // Humidity stress
    if (data.humidity > 85.0 || data.humidity < 30.0) {
        stressFactors |= STRESS_HUMIDITY;
    }
    
    // Predator detection (sudden loud noises)
    if (audio.soundLevel > 90 && audio.peakToAvg > 5.0) {
        stressFactors |= STRESS_PREDATOR;
    }
    
    // Disease indicators (abnormal quiet during day)
    DateTime now = rtc.now();  // Now rtc is passed as parameter
    if (now.hour() >= 10 && now.hour() <= 16 && audio.soundLevel < 20) {
        stressFactors |= STRESS_DISEASE;
    }
    
    // Hunger (very low activity overall)
    float avgDayActivity = 0;
    for (int i = 9; i <= 17; i++) {
        avgDayActivity += pattern.hourlyActivity[i];
    }
    avgDayActivity /= 9;
    if (avgDayActivity < 25) {
        stressFactors |= STRESS_HUNGER;
    }
    
    return stressFactors;
}

// =============================================================================
// AUDIO DIAGNOSTICS
// =============================================================================

void runAudioDiagnostics(SystemStatus& status) {
    Serial.println(F("\n=== Audio Diagnostics ==="));
    
    if (status.pdmWorking) {
        Serial.println(F("PDM Microphone: OK"));
        
        // Sample for 1 second
        unsigned long startTime = millis();
        int totalSamples = 0;
        float maxLevel = 0;
        
        while (millis() - startTime < 1000) {
            if (samplesRead > 0) {
                AudioAnalysis analysis = analyzeAudioBuffer(sampleBuffer, samplesRead);
                totalSamples += samplesRead;
                
                if (analysis.soundLevel > maxLevel) {
                    maxLevel = analysis.soundLevel;
                }
                
                samplesRead = 0;
            }
            delay(10);
        }
        
        Serial.print(F("Samples in 1 sec: "));
        Serial.println(totalSamples);
        Serial.print(F("Max sound level: "));
        Serial.print(maxLevel);
        Serial.println(F("%"));
    } else {
        Serial.println(F("PDM Microphone: NOT INITIALIZED"));
    }
    
    Serial.println(F("========================\n"));
}

// =============================================================================
// AUDIO CALIBRATION
// =============================================================================

void calibrateAudioLevels(SystemSettings& settings, int durationSeconds) {
    Serial.println(F("Starting audio calibration..."));
    Serial.println(F("Ensure hive is in normal state"));
    
    unsigned long startTime = millis();
    unsigned long duration = durationSeconds * 1000;
    
    float avgFreq = 0;
    float avgLevel = 0;
    int sampleCount = 0;
    
    while (millis() - startTime < duration) {
        if (samplesRead > 0) {
            AudioAnalysis analysis = analyzeAudioBuffer(sampleBuffer, samplesRead);
            
            avgFreq += analysis.dominantFreq;
            avgLevel += analysis.soundLevel;
            sampleCount++;
            
            samplesRead = 0;
        }
        delay(100);
    }
    
    if (sampleCount > 0) {
        avgFreq /= sampleCount;
        avgLevel /= sampleCount;
        
        Serial.print(F("Average frequency: "));
        Serial.print(avgFreq);
        Serial.println(F(" Hz"));
        Serial.print(F("Average level: "));
        Serial.print(avgLevel);
        Serial.println(F("%"));
        
        // Suggest threshold adjustments
        Serial.println(F("\nSuggested settings:"));
        Serial.print(F("Queen frequency range: "));
        Serial.print(avgFreq - 50);
        Serial.print(F(" - "));
        Serial.println(avgFreq + 50);
        Serial.print(F("Stress threshold: "));
        Serial.println(avgLevel + 30);
    }
}