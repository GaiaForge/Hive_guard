/**
 * Audio.cpp
 * Audio processing and bee state classification
 */

#include "Audio.h"
#include "Utils.h"

// Global audio buffers
int audioBuffer[AUDIO_SAMPLE_BUFFER_SIZE];
volatile int audioSampleIndex = 0;


// =============================================================================
// AUDIO INITIALIZATION
// =============================================================================

void initializeAudio(SystemStatus& status) {
    analogReadResolution(12);
    
    // Test A4 for actual microphone signal
    int minVal = 4095, maxVal = 0;
    
    for (int i = 0; i < 20; i++) {
        int reading = analogRead(A4);
        if (reading < minVal) minVal = reading;
        if (reading > maxVal) maxVal = reading;
        delay(10);
    }
    
    int variation = maxVal - minVal;
    
        
    if (variation > 200) {
        status.pdmWorking = true;
        Serial.println(F("MAX9814 microphone detected on A4"));
        Serial.print(F("Audio variation: "));
        Serial.println(variation);
    } else {
        status.pdmWorking = false;
        Serial.println(F("No microphone on A4 (floating pin)"));
        Serial.print(F("A4 variation: "));
        Serial.print(variation);
        Serial.println(F(" (need >200 for mic)"));
    }
}

// =============================================================================
// AUDIO PROCESSING
// =============================================================================
void processAudio(SensorData& data, SystemSettings& settings) {
    static unsigned long lastSampleTime = 0;
    unsigned long currentTime = millis();
    
    // Collect multiple samples to fill the buffer faster
    int samplesTaken = 0;
    while (samplesTaken < 10 && audioSampleIndex < AUDIO_SAMPLE_BUFFER_SIZE) {
        if (currentTime - lastSampleTime >= 1) { // Sample every 1ms instead of 0.125ms
            int audioSample = analogRead(AUDIO_INPUT_PIN);
            
            audioBuffer[audioSampleIndex] = audioSample;
            audioSampleIndex++;
            samplesTaken++;
            
            lastSampleTime = currentTime;
            currentTime = millis(); // Update for next sample
        } else {
            break; // Don't wait, just take what we can
        }
    }
    
    // Analyze when we have enough samples
    if (audioSampleIndex >= AUDIO_SAMPLE_BUFFER_SIZE) {
        AudioAnalysis analysis = analyzeAudioBuffer();
        
        data.dominantFreq = analysis.dominantFreq;
        data.soundLevel = analysis.soundLevel;
        data.beeState = classifyBeeState(analysis, settings);
        
        audioSampleIndex = 0; // Reset safely
    }
}

// =============================================================================
// AUDIO ANALYSIS - FIXED FUNCTION SIGNATURE
// =============================================================================

AudioAnalysis analyzeAudioBuffer() {  // Fixed: no parameters, uses global audioBuffer
    AudioAnalysis result;
    
    if (audioSampleIndex < 32) {
        // Not enough samples for reliable analysis
        result.dominantFreq = 0;
        result.soundLevel = 0;
        result.peakToAvg = 0;
        result.spectralCentroid = 0;
        return result;
    }
    
    // Variables for analysis
    int zeroCrossings = 0;
    int lastSign = 0;
    long sumAmplitude = 0;
    int maxAmplitude = 0;
    int minAmplitude = 4095;  // 12-bit ADC max
    
    // DC offset removal - calculate average first
    long dcOffset = 0;
    for (int i = 0; i < audioSampleIndex; i++) {
        dcOffset += audioBuffer[i];
    }
    dcOffset /= audioSampleIndex;
    
    // Main analysis loop with DC offset removed
    for (int i = 1; i < audioSampleIndex; i++) {
        // Remove DC offset (MAX9814 centers around 2048 for 12-bit ADC)
        int centeredSample = audioBuffer[i] - dcOffset;
        
        // Calculate amplitude statistics
        int amplitude = abs(centeredSample);
        sumAmplitude += amplitude;
        
        if (amplitude > maxAmplitude) {
            maxAmplitude = amplitude;
        }
        if (amplitude < minAmplitude) {
            minAmplitude = amplitude;
        }
        
        // Zero crossing detection for frequency estimation
        int currentSign = (centeredSample >= 0) ? 1 : -1;
        if (lastSign != 0 && currentSign != lastSign) {
            zeroCrossings++;
        }
        lastSign = currentSign;
    }
    
    // Calculate dominant frequency from zero crossings
    float duration = (float)audioSampleIndex / AUDIO_SAMPLE_RATE;
    if (duration > 0 && zeroCrossings > 0) {
        result.dominantFreq = (zeroCrossings / 2.0) / duration;
        
        // Constrain to reasonable bee frequency range (50-1000 Hz)
        if (result.dominantFreq < 50) result.dominantFreq = 0;
        if (result.dominantFreq > 1000) result.dominantFreq = 1000;
    } else {
        result.dominantFreq = 0;
    }
    
    // Calculate sound level optimized for MAX9814
    // MAX9814 has AGC, so we need to look at signal dynamics rather than absolute level
    float avgAmplitude = (float)sumAmplitude / audioSampleIndex;
    
    // Scale to 0-100 range, accounting for 12-bit ADC range
    // Use dynamic range (max-min) rather than just average for AGC compensation
    int dynamicRange = maxAmplitude - minAmplitude;
    
    // Combine average amplitude and dynamic range for better AGC handling
    float normalizedLevel = (avgAmplitude * 0.7) + (dynamicRange * 0.3);
    result.soundLevel = constrain(map(normalizedLevel, 0, 2048, 0, 100), 0, 100);
    
    // Calculate peak-to-average ratio (important for bee sound classification)
    result.peakToAvg = (avgAmplitude > 0) ? (float)maxAmplitude / avgAmplitude : 0;
    
    // Enhanced spectral centroid estimation optimized for bee frequencies
    result.spectralCentroid = estimateSpectralCentroidOptimized();
    
    return result;
}

// =============================================================================
// SPECTRAL ANALYSIS
// =============================================================================

float estimateSpectralCentroidOptimized() {
    // Optimized for bee frequency analysis (100-800 Hz range)
    const int windowSize = 32;  // Smaller windows for better frequency resolution
    const int numWindows = audioSampleIndex / windowSize;
    
    if (numWindows < 3) {
        return 0;
    }
    
    float totalWeightedFreq = 0;
    float totalEnergy = 0;
    
    // Calculate DC offset for this buffer
    long dcOffset = 0;
    for (int i = 0; i < audioSampleIndex; i++) {
        dcOffset += audioBuffer[i];
    }
    dcOffset /= audioSampleIndex;
    
    for (int w = 0; w < numWindows; w++) {
        int windowStart = w * windowSize;
        int zeroCrossings = 0;
        float windowEnergy = 0;
                
        // Analyze this window
        for (int i = 1; i < windowSize; i++) {
            int idx = windowStart + i;
            if (idx >= audioSampleIndex) break;
            
            // Remove DC offset
            int current = audioBuffer[idx] - dcOffset;
            int previous = audioBuffer[idx-1] - dcOffset;
            
            // Zero crossing detection
            int currentSign = (current >= 0) ? 1 : -1;
            int previousSign = (previous >= 0) ? 1 : -1;
            
            if (previousSign != currentSign) {
                zeroCrossings++;
            }
            
            // Energy calculation
            windowEnergy += abs(current);
        }
        
        // Estimate frequency for this window
        float windowDuration = (float)windowSize / AUDIO_SAMPLE_RATE;
        float windowFreq = 0;
        if (windowDuration > 0 && zeroCrossings > 0) {
            windowFreq = (zeroCrossings / 2.0) / windowDuration;
            
            // Weight frequencies in bee range more heavily
            float freqWeight = 1.0;
            if (windowFreq >= 200 && windowFreq <= 600) {
                freqWeight = 2.0;  // Boost bee frequency range
            } else if (windowFreq >= 100 && windowFreq <= 800) {
                freqWeight = 1.5;  // Moderate boost for extended bee range
            }
            
            windowEnergy *= freqWeight;
        }
        
        // Weight by energy (only if frequency is reasonable)
        if (windowFreq >= 50 && windowFreq <= 1000) {
            totalWeightedFreq += windowFreq * windowEnergy;
            totalEnergy += windowEnergy;
        }
    }
    
    return (totalEnergy > 0) ? totalWeightedFreq / totalEnergy : 0;
}

// Add to Audio.cpp after existing functions

SpectralFeatures analyzeAudioFFT() {
    SpectralFeatures result = {0};
    
    if (audioSampleIndex < 64) {
        return result;  // Not enough data
    }
    
    // Simple spectral centroid calculation using existing zero-crossing method
    // We'll enhance this with real FFT later
    float weightedFreqSum = 0;
    float totalEnergy = 0;
    
    // Analyze in small windows to estimate spectral content
    for (int w = 0; w < audioSampleIndex - 32; w += 16) {
        int zeroCrossings = 0;
        float windowEnergy = 0;
        
        for (int i = w + 1; i < w + 32 && i < audioSampleIndex; i++) {
            int current = audioBuffer[i];
            int previous = audioBuffer[i-1];
            
            // Zero crossing detection
            if ((current >= 2048 && previous < 2048) || 
                (current < 2048 && previous >= 2048)) {
                zeroCrossings++;
            }
            
            // Energy calculation
            windowEnergy += abs(current - 2048);
        }
        
        // Estimate frequency for this window
        float windowFreq = (zeroCrossings / 2.0) * (AUDIO_SAMPLE_RATE / 32.0);
        
        weightedFreqSum += windowFreq * windowEnergy;
        totalEnergy += windowEnergy;
    }
    
    // Calculate spectral centroid
    if (totalEnergy > 0) {
        result.spectralCentroid = weightedFreqSum / totalEnergy;
        result.totalEnergy = totalEnergy / 1000.0; // Scale down
    }
    
    // Constrain to reasonable range
    result.spectralCentroid = constrain(result.spectralCentroid, 50, 1000);
    
    return result;
}

void updateActivityTrend(ActivityTrend& trend, SpectralFeatures& current, uint8_t hour) {
    // Initialize baseline if this is first reading
    if (trend.baselineActivity == 0) {
        trend.baselineActivity = current.totalEnergy;
        trend.currentActivity = current.totalEnergy;
        trend.activityIncrease = 1.0;
        trend.abnormalTiming = false;
        return;
    }
    
    // Update current activity
    trend.currentActivity = current.totalEnergy;
    
    // Update baseline with slow exponential smoothing (99% old, 1% new)
    trend.baselineActivity = 0.99 * trend.baselineActivity + 0.01 * current.totalEnergy;
    
    // Calculate activity increase ratio
    if (trend.baselineActivity > 0) {
        trend.activityIncrease = trend.currentActivity / trend.baselineActivity;
    } else {
        trend.activityIncrease = 1.0;
    }
    
    // Check for abnormal timing patterns
    // High activity at night (22:00-06:00) or very low activity during day (10:00-16:00)
    if ((hour >= 22 || hour <= 6) && trend.activityIncrease > 1.5) {
        trend.abnormalTiming = true;
    } else if (hour >= 10 && hour <= 16 && trend.activityIncrease < 0.3) {
        trend.abnormalTiming = true;
    } else {
        trend.abnormalTiming = false;
    }
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
                                 DailyPattern& pattern, RTC_PCF8523& rtc) {
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
        Serial.println(F("Microphone: OK"));
        
        // Sample for 1 second
        unsigned long startTime = millis();
        int totalSamples = 0;
        float maxLevel = 0;
        
        while (millis() - startTime < 1000) {
            if (audioSampleIndex > 0) {
                AudioAnalysis analysis = analyzeAudioBuffer();
                totalSamples += audioSampleIndex;
                
                if (analysis.soundLevel > maxLevel) {
                    maxLevel = analysis.soundLevel;
                }
                
                audioSampleIndex = 0;  // Reset for next analysis
            }
            delay(10);
        }
        
        Serial.print(F("Samples in 1 sec: "));
        Serial.println(totalSamples);
        Serial.print(F("Max sound level: "));
        Serial.print(maxLevel);
        Serial.println(F("%"));
    } else {
        Serial.println(F("Microphone: NOT INITIALIZED"));
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
        if (audioSampleIndex > 0) {
            AudioAnalysis analysis = analyzeAudioBuffer();
            
            avgFreq += analysis.dominantFreq;
            avgLevel += analysis.soundLevel;
            sampleCount++;
            
            audioSampleIndex = 0;  // Reset for next analysis
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