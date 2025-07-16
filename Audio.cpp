/**
 * Audio.cpp
 * Modern Audio processing implementation - Clean AudioProcessor-only version
 */

#include "Audio.h"
#include "Utils.h"
#include <math.h>

// =============================================================================
// GLOBAL AUDIO PROCESSOR INSTANCE
// =============================================================================

AudioProcessor audioProcessor;

// =============================================================================
// AUDIO PROCESSOR IMPLEMENTATION
// =============================================================================

AudioProcessor::AudioProcessor() {
    bufferIndex = 0;
    realtimeIndex = 0;
    lastQueenDetected = 0;
    settings = nullptr;
    status = nullptr;
    
    // Initialize display data
    displayData.soundLevel = 0;
    displayData.dominantFreq = 0;
    displayData.spectralCentroid = 0;
    displayData.activityRatio = 1.0;
    displayData.beeState = BEE_UNKNOWN;
    displayData.abscondingRisk = 0;
    
    // Initialize activity trend
    activityTrend.currentActivity = 0;
    activityTrend.baselineActivity = 30.0; // Default baseline
    activityTrend.activityIncrease = 1.0;
    activityTrend.abnormalTiming = false;
    
    // Initialize absconding indicators
    abscondingIndicators.queenSilent = false;
    abscondingIndicators.increasedActivity = false;
    abscondingIndicators.erraticPattern = false;
    abscondingIndicators.riskLevel = 0;
    abscondingIndicators.lastQueenDetected = 0;
    
    // Initialize temporal tracking
    energyHistoryIndex = 0;
    shortTermEnergySum = 0;
    midTermEnergySum = 0;
    longTermEnergySum = 0;
    for (int i = 0; i < 60; i++) {
        energyHistory[i] = 0;
    }
    
    // Initialize previous magnitude buffer
    for (int i = 0; i < FFT_SIZE/2; i++) {
        prevMagnitude[i] = 0;
    }
}

void AudioProcessor::initialize(SystemSettings* sysSettings, SystemStatus* sysStatus) {
    settings = sysSettings;
    status = sysStatus;
    
    // Set ADC resolution
    analogReadResolution(12);
    
    // Detect microphone
    if (detectMicrophone()) {
        status->pdmWorking = true;
        Serial.println(F("AudioProcessor: Microphone detected"));
    } else {
        status->pdmWorking = false;
        Serial.println(F("AudioProcessor: No microphone detected"));
    }
    
    resetBuffers();
}

bool AudioProcessor::detectMicrophone() {
    int minVal = 4095, maxVal = 0;
    
    // Take multiple samples to check for variation
    for (int i = 0; i < 50; i++) {
        int reading = analogRead(AUDIO_INPUT_PIN);
        if (reading < minVal) minVal = reading;
        if (reading > maxVal) maxVal = reading;
        delay(10);
    }
    
    int variation = maxVal - minVal;
    Serial.print(F("Microphone variation: "));
    Serial.println(variation);
    
    return (variation > 200); // Threshold for microphone detection
}

// =============================================================================
// REAL-TIME PROCESSING
// =============================================================================

void AudioProcessor::addSample(int rawSample) {
    // Convert to centered float sample
    float sample = (float)(rawSample - 2048);
    
    // Add to FFT buffer
    if (bufferIndex < FFT_SIZE) {
        audioBuffer[bufferIndex++] = sample;
    }
    
    // Add to real-time buffer (circular)
    realtimeBuffer[realtimeIndex] = sample;
    realtimeIndex = (realtimeIndex + 1) % DISPLAY_UPDATE_SAMPLES;
}

void AudioProcessor::updateDisplayData() {
    // Quick analysis of recent samples for display
    float minVal = 4095, maxVal = -4095;
    float sum = 0;
    int zeroCrossings = 0;
    float lastSample = 0;
    
    // Analyze recent samples
    for (int i = 0; i < DISPLAY_UPDATE_SAMPLES; i++) {
        float sample = realtimeBuffer[i];
        if (sample < minVal) minVal = sample;
        if (sample > maxVal) maxVal = sample;
        sum += fabs(sample);
        
        // Zero crossing detection
        if (i > 0 && ((lastSample < 0 && sample >= 0) || 
            (lastSample >= 0 && sample < 0))) {
            zeroCrossings++;
        }
        lastSample = sample;
    }
    
    // Calculate metrics with smoothing
    float variation = maxVal - minVal;
    float rawLevel = map(variation, 0, 2000, 0, 100);
    rawLevel = constrain(rawLevel, 0, 100);
    
    // Apply smoothing
    displayData.soundLevel = (displayData.soundLevel * DISPLAY_SMOOTHING_FACTOR) + 
                            (rawLevel * (1.0 - DISPLAY_SMOOTHING_FACTOR));
    
    // Frequency estimation
    if (zeroCrossings > 2) {
        float duration = DISPLAY_UPDATE_SAMPLES * 0.001; // Approximate
        float rawFreq = (zeroCrossings / 2.0) / duration;
        displayData.dominantFreq = (displayData.dominantFreq * 0.8) + 
                                  (rawFreq * 0.2);
    }
    
    // Simple centroid estimation
    float rawCentroid = displayData.dominantFreq;
    if (variation > 1000) rawCentroid += 200;
    displayData.spectralCentroid = (displayData.spectralCentroid * 0.85) + 
                                  (rawCentroid * 0.15);
    
    // Activity ratio
    if (activityTrend.baselineActivity > 5) {
        displayData.activityRatio = displayData.soundLevel / activityTrend.baselineActivity;
    } else {
        displayData.activityRatio = 1.0;
    }
    
    // Copy latest classification data
    displayData.abscondingRisk = abscondingIndicators.riskLevel;
}

// =============================================================================
// FULL FFT ANALYSIS
// =============================================================================

AudioAnalysisResult AudioProcessor::performFullAnalysis() {
    AudioAnalysisResult result = {0};
    
    // Check if we have enough data
    if (bufferIndex < FFT_SIZE / 2) {
        result.analysisValid = false;
        Serial.print(F("Not enough samples for FFT: "));
        Serial.print(bufferIndex);
        Serial.print(F("/"));
        Serial.println(FFT_SIZE);
        return result;
    }
    
    // Get current timestamp for time features
    DateTime timestamp = DateTime(millis()/1000);
    
    // Perform FFT
    performFFT();
    
    // Analyze results
    AudioAnalysis analysis = analyzeAudioBuffer();
    SpectralFeatures features = analyzeSpectralFeatures();
    
    // Update trends
    updateActivityTrend(features);
    updateAbscondingRisk(analysis, features.bandEnergyRatios[1]);
    
    // Classify bee state
    uint8_t beeState = classifyBeeState(analysis, features);
    
    // Fill core result structure
    result.dominantFreq = analysis.dominantFreq;
    result.soundLevel = analysis.soundLevel;
    result.beeState = beeState;
    
    // Band energies
    result.bandEnergy0_200Hz = features.bandEnergyRatios[0];
    result.bandEnergy200_400Hz = features.bandEnergyRatios[1];
    result.bandEnergy400_600Hz = features.bandEnergyRatios[2];
    result.bandEnergy600_800Hz = features.bandEnergyRatios[3];
    result.bandEnergy800_1000Hz = features.bandEnergyRatios[4];
    result.bandEnergy1000PlusHz = features.bandEnergyRatios[5];
    
    // Advanced metrics
    result.spectralCentroid = analysis.spectralCentroid;
    result.peakToAvgRatio = analysis.peakToAvg;
    result.harmonicity = features.harmonicity;
    
    // Behavioral indicators
    result.queenDetected = !abscondingIndicators.queenSilent;
    result.abscondingRisk = abscondingIndicators.riskLevel;
    result.activityIncrease = activityTrend.activityIncrease;
    
    // Calculate extended ML features
    calculateExtendedFeatures(result, timestamp);
    
    // Update temporal features
    updateTemporalFeatures(result);
    
    // Signal quality assessment
    result.signalQuality = calculateSignalQuality();
    
    result.analysisValid = true;
    
    // Update display data with full analysis results
    displayData.beeState = beeState;
    displayData.abscondingRisk = abscondingIndicators.riskLevel;
    displayData.spectralCentroid = analysis.spectralCentroid;
    displayData.dominantFreq = analysis.dominantFreq;
    displayData.soundLevel = analysis.soundLevel;
    
    // Save current magnitude for next spectral flux calculation
    for (int i = 0; i < FFT_SIZE/2; i++) {
        prevMagnitude[i] = fftMagnitude[i];
    }
    
    // Reset buffer for next analysis
    bufferIndex = 0;
    
    Serial.print(F("FFT Analysis complete: Freq="));
    Serial.print(result.dominantFreq);
    Serial.print(F("Hz, Centroid="));
    Serial.print(result.spectralCentroid, 1);
    Serial.print(F("Hz, Level="));
    Serial.print(result.soundLevel);
    Serial.println(F("%"));
    
    return result;
}

// =============================================================================
// FFT IMPLEMENTATION
// =============================================================================

void AudioProcessor::performFFT() {
    // Copy audio buffer to FFT arrays
    for (int i = 0; i < FFT_SIZE; i++) {
        if (i < bufferIndex) {
            fftReal[i] = audioBuffer[i];
        } else {
            fftReal[i] = 0; // Zero-pad if needed
        }
        fftImag[i] = 0;
    }
    
    // Apply Hamming window
    for (int i = 0; i < FFT_SIZE; i++) {
        float window = 0.54 - 0.46 * cos(2 * PI * i / (FFT_SIZE - 1));
        fftReal[i] *= window;
    }
    
    // Compute FFT
    computeFFT(fftReal, fftImag, FFT_SIZE);
    
    // Compute magnitudes
    computeMagnitudes();
}

void AudioProcessor::computeFFT(float* real, float* imag, int n) {
    int log2n = FFT_SIZE_LOG2;
    
    // Bit reversal
    for (int i = 0; i < n; i++) {
        int j = reverseBits(i, log2n);
        if (j > i) {
            float tempReal = real[i];
            float tempImag = imag[i];
            real[i] = real[j];
            imag[i] = imag[j];
            real[j] = tempReal;
            imag[j] = tempImag;
        }
    }
    
    // FFT computation
    for (int s = 1; s <= log2n; s++) {
        int m = 1 << s;
        float theta = -2 * PI / m;
        float wmReal = cos(theta);
        float wmImag = sin(theta);
        
        for (int k = 0; k < n; k += m) {
            float wReal = 1;
            float wImag = 0;
            
            for (int j = 0; j < m / 2; j++) {
                int t = k + j;
                int u = t + m / 2;
                
                float tReal = wReal * real[u] - wImag * imag[u];
                float tImag = wReal * imag[u] + wImag * real[u];
                
                real[u] = real[t] - tReal;
                imag[u] = imag[t] - tImag;
                real[t] = real[t] + tReal;
                imag[t] = imag[t] + tImag;
                
                float tempWReal = wReal * wmReal - wImag * wmImag;
                wImag = wReal * wmImag + wImag * wmReal;
                wReal = tempWReal;
            }
        }
    }
}

int AudioProcessor::reverseBits(int num, int log2n) {
    int reversed = 0;
    for (int i = 0; i < log2n; i++) {
        if (num & (1 << i)) {
            reversed |= 1 << (log2n - 1 - i);
        }
    }
    return reversed;
}

void AudioProcessor::computeMagnitudes() {
    for (int i = 0; i < FFT_SIZE / 2; i++) {
        fftMagnitude[i] = sqrt(fftReal[i] * fftReal[i] + fftImag[i] * fftImag[i]);
    }
}

float AudioProcessor::getFrequencyBin(int bin) {
    return (float)bin * AUDIO_SAMPLE_RATE / FFT_SIZE;
}

float AudioProcessor::getBandEnergy(float freqMin, float freqMax) {
    int binMin = (int)(freqMin * FFT_SIZE / AUDIO_SAMPLE_RATE);
    int binMax = (int)(freqMax * FFT_SIZE / AUDIO_SAMPLE_RATE);
    
    binMin = constrain(binMin, 0, FFT_SIZE / 2 - 1);
    binMax = constrain(binMax, 0, FFT_SIZE / 2 - 1);
    
    float energy = 0;
    for (int i = binMin; i <= binMax; i++) {
        energy += fftMagnitude[i] * fftMagnitude[i];
    }
    
    return energy;
}

// =============================================================================
// ANALYSIS FUNCTIONS
// =============================================================================

AudioAnalysis AudioProcessor::analyzeAudioBuffer() {
    AudioAnalysis result = {0};
    
    // Find dominant frequency from FFT
    float maxMagnitude = 0;
    int maxBin = 0;
    
    for (int i = 2; i < FFT_SIZE / 2; i++) {
        if (fftMagnitude[i] > maxMagnitude) {
            maxMagnitude = fftMagnitude[i];
            maxBin = i;
        }
    }
    
    result.dominantFreq = getFrequencyBin(maxBin);
    
    // Calculate total energy for sound level
    float totalEnergy = 0;
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        totalEnergy += fftMagnitude[i] * fftMagnitude[i];
    }
    
    // Sound level (0-100 scale)
    result.soundLevel = constrain(log10(totalEnergy + 1) * 10, 0, 100);
    
    // Peak to average ratio
    float avgMagnitude = 0;
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        avgMagnitude += fftMagnitude[i];
    }
    avgMagnitude /= (FFT_SIZE / 2 - 1);
    result.peakToAvg = (avgMagnitude > 0) ? maxMagnitude / avgMagnitude : 0;
    
    // Spectral centroid
    float weightedSum = 0;
    float magnitudeSum = 0;
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        float freq = getFrequencyBin(i);
        weightedSum += freq * fftMagnitude[i];
        magnitudeSum += fftMagnitude[i];
    }
    result.spectralCentroid = (magnitudeSum > 0) ? weightedSum / magnitudeSum : 0;
    
    return result;
}

SpectralFeatures AudioProcessor::analyzeSpectralFeatures() {
    SpectralFeatures features = {0};
    
    // Calculate total energy
    float totalEnergy = getBandEnergy(0, AUDIO_SAMPLE_RATE / 2);
    features.totalEnergy = log10(totalEnergy + 1);
    
    // Calculate energy in each band
    float band0_200 = getBandEnergy(0, 200);
    float band200_400 = getBandEnergy(200, 400);
    float band400_600 = getBandEnergy(400, 600);
    float band600_800 = getBandEnergy(600, 800);
    float band800_1000 = getBandEnergy(800, 1000);
    float band1000_plus = getBandEnergy(1000, AUDIO_SAMPLE_RATE / 2);
    
    // Calculate ratios
    float sumBands = band0_200 + band200_400 + band400_600 + 
                     band600_800 + band800_1000 + band1000_plus;
    
    if (sumBands > 0) {
        features.bandEnergyRatios[0] = band0_200 / sumBands;
        features.bandEnergyRatios[1] = band200_400 / sumBands;
        features.bandEnergyRatios[2] = band400_600 / sumBands;
        features.bandEnergyRatios[3] = band600_800 / sumBands;
        features.bandEnergyRatios[4] = band800_1000 / sumBands;
        features.bandEnergyRatios[5] = band1000_plus / sumBands;
    }
    
    // Spectral centroid
    float weightedSum = 0;
    float magnitudeSum = 0;
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        float freq = getFrequencyBin(i);
        weightedSum += freq * fftMagnitude[i];
        magnitudeSum += fftMagnitude[i];
    }
    features.spectralCentroid = (magnitudeSum > 0) ? weightedSum / magnitudeSum : 0;
    
    // Harmonicity
    int fundBin = 0;
    float maxMag = 0;
    for (int i = 200 * FFT_SIZE / AUDIO_SAMPLE_RATE; 
         i < 800 * FFT_SIZE / AUDIO_SAMPLE_RATE && i < FFT_SIZE / 2; i++) {
        if (fftMagnitude[i] > maxMag) {
            maxMag = fftMagnitude[i];
            fundBin = i;
        }
    }
    
    if (fundBin > 0) {
        float fundamental = fftMagnitude[fundBin];
        float harmonic = 0;
        int harmonicBin = fundBin * 2;
        if (harmonicBin < FFT_SIZE / 2) {
            harmonic = fftMagnitude[harmonicBin];
        }
        features.harmonicity = (fundamental > 0) ? harmonic / fundamental : 0;
    }
    
    return features;
}

uint8_t AudioProcessor::classifyBeeState(AudioAnalysis& analysis, SpectralFeatures& features) {
    if (!settings) return BEE_UNKNOWN;
    
    uint16_t freq = analysis.dominantFreq;
    uint8_t level = analysis.soundLevel;
    float peakRatio = analysis.peakToAvg;
    
    // Quiet hive
    if (level < 10) {
        return BEE_QUIET;
    }
    
    // Queen detection
    if (freq >= settings->queenFreqMin && freq <= settings->queenFreqMax) {
        if (features.bandEnergyRatios[1] > 0.3 && level > 30) {
            return BEE_QUEEN_PRESENT;
        }
    }
    
    // Pre-swarm detection
    if (freq >= settings->swarmFreqMin && freq <= settings->swarmFreqMax) {
        if (features.bandEnergyRatios[2] > 0.4 && level > 60) {
            return BEE_PRE_SWARM;
        }
    }
    
    // Defensive behavior
    if (features.bandEnergyRatios[3] > 0.3 || features.bandEnergyRatios[4] > 0.2) {
        if (level > 70) {
            return BEE_DEFENSIVE;
        }
    }
    
    // Stress detection
    if (level > settings->stressThreshold) {
        if (peakRatio > 4.0 || features.spectralCentroid > 500) {
            return BEE_STRESSED;
        }
    }
    
    // Missing queen
    if (level > 50 && features.bandEnergyRatios[1] < 0.1) {
        return BEE_QUEEN_MISSING;
    }
    
    // Active vs Normal
    if (level > 50) {
        return BEE_ACTIVE;
    } else {
        return BEE_NORMAL;
    }
}

void AudioProcessor::updateActivityTrend(SpectralFeatures& features) {
    // Update current activity
    activityTrend.currentActivity = features.totalEnergy;
    
    // Update baseline (slow adaptation)
    if (activityTrend.baselineActivity == 0) {
        activityTrend.baselineActivity = features.totalEnergy;
    } else {
        activityTrend.baselineActivity = 0.99 * activityTrend.baselineActivity + 
                                        0.01 * features.totalEnergy;
    }
    
    // Calculate increase ratio
    if (activityTrend.baselineActivity > 0) {
        activityTrend.activityIncrease = activityTrend.currentActivity / 
                                         activityTrend.baselineActivity;
    } else {
        activityTrend.activityIncrease = 1.0;
    }
    
    // Check for abnormal timing
    static unsigned long lastHighActivity = 0;
    if (activityTrend.activityIncrease > 1.5) {
        unsigned long timeSinceHigh = millis() - lastHighActivity;
        activityTrend.abnormalTiming = (timeSinceHigh < 60000); // Within 1 minute
        lastHighActivity = millis();
    }
}

void AudioProcessor::updateAbscondingRisk(AudioAnalysis& analysis, float queenBandEnergy) {
    if (!settings) return;
    
    // Queen detection
    bool queenPresent = (analysis.dominantFreq >= settings->queenFreqMin && 
                        analysis.dominantFreq <= settings->queenFreqMax &&
                        queenBandEnergy > 0.25);
    
    if (queenPresent) {
        abscondingIndicators.lastQueenDetected = millis();
        abscondingIndicators.queenSilent = false;
    }
    
    // Check if queen silent too long
    uint32_t timeSinceQueen = millis() - abscondingIndicators.lastQueenDetected;
    abscondingIndicators.queenSilent = (timeSinceQueen > 3600000); // 1 hour
    
    // Activity indicators
    abscondingIndicators.increasedActivity = (activityTrend.activityIncrease > 1.5);
    abscondingIndicators.erraticPattern = activityTrend.abnormalTiming;
    
    // Calculate risk level
    abscondingIndicators.riskLevel = 0;
    if (abscondingIndicators.queenSilent) abscondingIndicators.riskLevel += 40;
    if (abscondingIndicators.increasedActivity) abscondingIndicators.riskLevel += 30;
    if (abscondingIndicators.erraticPattern) abscondingIndicators.riskLevel += 30;
    
    if (abscondingIndicators.riskLevel > 100) abscondingIndicators.riskLevel = 100;
}

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

void AudioProcessor::resetBuffers() {
    bufferIndex = 0;
    realtimeIndex = 0;
    
    for (int i = 0; i < FFT_SIZE; i++) {
        audioBuffer[i] = 0;
    }
    
    for (int i = 0; i < DISPLAY_UPDATE_SAMPLES; i++) {
        realtimeBuffer[i] = 0;
    }
}

void AudioProcessor::resetBaseline() {
    activityTrend.baselineActivity = 30.0;
    activityTrend.currentActivity = 30.0;
    activityTrend.activityIncrease = 1.0;
}

void AudioProcessor::runDiagnostics() {
    Serial.println(F("\n=== Audio Diagnostics ==="));
    
    if (status && status->pdmWorking) {
        Serial.println(F("Microphone: OK"));
        
        // Sample for 1 second
        unsigned long startTime = millis();
        int samples = 0;
        float maxLevel = 0;
        
        while (millis() - startTime < 1000) {
            int sample = analogRead(AUDIO_INPUT_PIN);
            addSample(sample);
            samples++;
            
            if (displayData.soundLevel > maxLevel) {
                maxLevel = displayData.soundLevel;
            }
            
            delay(1);
        }
        
        Serial.print(F("Samples in 1 sec: "));
        Serial.println(samples);
        Serial.print(F("Max sound level: "));
        Serial.print(maxLevel);
        Serial.println(F("%"));
        
    } else {
        Serial.println(F("Microphone: NOT INITIALIZED"));
    }
    
    Serial.print(F("Buffer progress: "));
    Serial.print(getBufferProgress());
    Serial.println(F("%"));
    
    Serial.println(F("========================\n"));
}

void AudioProcessor::printStatus() const {
    Serial.println(F("\n=== Audio Status ==="));
    Serial.print(F("Sound Level: ")); Serial.print(displayData.soundLevel); Serial.println(F("%"));
    Serial.print(F("Frequency: ")); Serial.print(displayData.dominantFreq); Serial.println(F(" Hz"));
    Serial.print(F("Centroid: ")); Serial.print(displayData.spectralCentroid); Serial.println(F(" Hz"));
    Serial.print(F("Activity: x")); Serial.println(displayData.activityRatio);
    Serial.print(F("Bee State: ")); Serial.println(displayData.beeState);
    Serial.print(F("Absconding Risk: ")); Serial.print(displayData.abscondingRisk); Serial.println(F("%"));
    Serial.println(F("==================="));
}

// =============================================================================
// EXTENDED ML FEATURE CALCULATIONS
// =============================================================================

void AudioProcessor::calculateExtendedFeatures(AudioAnalysisResult& result, DateTime& timestamp) {
    // Time-based features for ML (cyclical encoding)
    float hour = timestamp.hour() + timestamp.minute() / 60.0;
    result.hourOfDaySin = sin(2 * PI * hour / 24.0);
    result.hourOfDayCos = cos(2 * PI * hour / 24.0);
    
    // Day of year (approximate - doesn't account for leap years precisely)
    int dayOfYear = timestamp.unixtime() / 86400 % 365;
    result.dayOfYearSin = sin(2 * PI * dayOfYear / 365.0);
    result.dayOfYearCos = cos(2 * PI * dayOfYear / 365.0);
    
    // Spectral rolloff - frequency below which 85% of energy is contained
    result.spectralRolloff = calculateSpectralRolloff(0.85);
    
    // Spectral flux - measure of spectral change
    result.spectralFlux = calculateSpectralFlux();
    
    // Zero crossing rate
    result.zeroCrossingRate = calculateZeroCrossingRate();
    
    // Spectral spread - measure of bandwidth
    float variance = 0;
    float totalMag = 0;
    for (int i = 1; i < FFT_SIZE/2; i++) {
        float freq = getFrequencyBin(i);
        float diff = freq - result.spectralCentroid;
        variance += diff * diff * fftMagnitude[i];
        totalMag += fftMagnitude[i];
    }
    result.spectralSpread = (totalMag > 0) ? sqrt(variance / totalMag) : 0;
    
    // Spectral skewness - asymmetry of spectrum
    float skewness = 0;
    if (result.spectralSpread > 0) {
        for (int i = 1; i < FFT_SIZE/2; i++) {
            float freq = getFrequencyBin(i);
            float diff = freq - result.spectralCentroid;
            skewness += pow(diff / result.spectralSpread, 3) * fftMagnitude[i];
        }
        result.spectralSkewness = (totalMag > 0) ? skewness / totalMag : 0;
    }
    
    // Spectral kurtosis - peakedness of spectrum
    float kurtosis = 0;
    if (result.spectralSpread > 0) {
        for (int i = 1; i < FFT_SIZE/2; i++) {
            float freq = getFrequencyBin(i);
            float diff = freq - result.spectralCentroid;
            kurtosis += pow(diff / result.spectralSpread, 4) * fftMagnitude[i];
        }
        result.spectralKurtosis = (totalMag > 0) ? (kurtosis / totalMag) - 3.0 : 0;
    }
    
    // Context flags based on time and conditions
    result.contextFlags = 0;
    if (hour >= 6 && hour < 10) {
        result.contextFlags |= CONTEXT_MORNING;
    } else if (hour >= 17 && hour < 21) {
        result.contextFlags |= CONTEXT_EVENING;
    }
    
    // Ambient noise level estimation (energy in very low and very high frequencies)
    float noiseEnergy = getBandEnergy(0, 100) + getBandEnergy(2000, 4000);
    float totalEnergy = getBandEnergy(0, 4000);
    result.ambientNoiseLevel = (totalEnergy > 0) ? (noiseEnergy / totalEnergy) * 100 : 0;
}

float AudioProcessor::calculateSpectralRolloff(float percentage) {
    float totalEnergy = 0;
    for (int i = 0; i < FFT_SIZE/2; i++) {
        totalEnergy += fftMagnitude[i] * fftMagnitude[i];
    }
    
    float threshold = totalEnergy * percentage;
    float cumulativeEnergy = 0;
    
    for (int i = 0; i < FFT_SIZE/2; i++) {
        cumulativeEnergy += fftMagnitude[i] * fftMagnitude[i];
        if (cumulativeEnergy >= threshold) {
            return getFrequencyBin(i);
        }
    }
    
    return getFrequencyBin(FFT_SIZE/2 - 1);
}

float AudioProcessor::calculateSpectralFlux() {
    float flux = 0;
    for (int i = 0; i < FFT_SIZE/2; i++) {
        float diff = fftMagnitude[i] - prevMagnitude[i];
        // Only count positive differences (onset detection)
        if (diff > 0) {
            flux += diff * diff;
        }
    }
    return sqrt(flux);
}

float AudioProcessor::calculateZeroCrossingRate() {
    int crossings = 0;
    for (int i = 1; i < FFT_SIZE; i++) {
        if ((audioBuffer[i-1] >= 0 && audioBuffer[i] < 0) ||
            (audioBuffer[i-1] < 0 && audioBuffer[i] >= 0)) {
            crossings++;
        }
    }
    return (float)crossings / FFT_SIZE;
}

void AudioProcessor::updateTemporalFeatures(AudioAnalysisResult& result) {
    // Update energy history
    float currentEnergy = 0;
    for (int i = 0; i < FFT_SIZE/2; i++) {
        currentEnergy += fftMagnitude[i] * fftMagnitude[i];
    }
    currentEnergy = log10(currentEnergy + 1); // Log scale
    
    // Remove oldest value from running sums BEFORE overwriting
    float oldestEnergy = energyHistory[energyHistoryIndex];
    
    // Update running sums efficiently
    shortTermEnergySum -= oldestEnergy;  // Remove old value
    shortTermEnergySum += currentEnergy; // Add new value
    
    // Update mid-term sum (last 10 readings)
    // Note: This needs a more sophisticated approach for proper sliding window
    // For now, we'll recalculate it to ensure accuracy
    
    // Add new value to buffer
    energyHistory[energyHistoryIndex] = currentEnergy;
    energyHistoryIndex = (energyHistoryIndex + 1) % 60;
    
    // Calculate short term energy (last 60 readings)
    result.shortTermEnergy = shortTermEnergySum / 60.0;
    
    // Mid term (last 10 readings) - recalculate for accuracy
    midTermEnergySum = 0;
    for (int i = 0; i < 10; i++) {
        int idx = (energyHistoryIndex - i - 1 + 60) % 60;
        midTermEnergySum += energyHistory[idx];
    }
    result.midTermEnergy = midTermEnergySum / 10.0;
    
    // Long term is baseline from activity trend
    result.longTermEnergy = activityTrend.baselineActivity / 10.0;
    
    // Energy entropy - measure of energy distribution randomness
    float energyMean = shortTermEnergySum / 60.0;
    float variance = 0;
    for (int i = 0; i < 60; i++) {
        float diff = energyHistory[i] - energyMean;
        variance += diff * diff;
    }
    float stdDev = sqrt(variance / 60.0);
    
    // Normalized entropy (0-1 scale)
    result.energyEntropy = (energyMean > 0) ? stdDev / energyMean : 0;
    result.energyEntropy = constrain(result.energyEntropy, 0, 1);
}

uint8_t AudioProcessor::calculateSignalQuality() {
    // Assess signal quality based on multiple factors
    uint8_t quality = 100;
    
    // Check for clipping
    int clippedSamples = 0;
    for (int i = 0; i < FFT_SIZE; i++) {
        if (abs(audioBuffer[i]) > 2000) { // Near max for 12-bit ADC
            clippedSamples++;
        }
    }
    if (clippedSamples > FFT_SIZE / 10) {
        quality -= 20; // Significant clipping
    }
    
    // Check for DC offset issues
    float dcOffset = 0;
    for (int i = 0; i < FFT_SIZE; i++) {
        dcOffset += audioBuffer[i];
    }
    dcOffset /= FFT_SIZE;
    if (abs(dcOffset) > 500) {
        quality -= 10; // Large DC offset
    }
    
    // Check for very low signal
    float maxAmplitude = 0;
    for (int i = 0; i < FFT_SIZE; i++) {
        if (abs(audioBuffer[i]) > maxAmplitude) {
            maxAmplitude = abs(audioBuffer[i]);
        }
    }
    if (maxAmplitude < 100) {
        quality -= 30; // Very weak signal
    }
    
    // Check for excessive noise (high frequency energy)
    float noiseRatio = getBandEnergy(2000, 4000) / getBandEnergy(0, 2000);
    if (noiseRatio > 0.5) {
        quality -= 10; // High noise
    }
    
    return quality;
}

// =============================================================================
// SIMPLE INTERFACE FUNCTIONS FOR COMPATIBILITY
// =============================================================================

void initializeAudio(SystemStatus& status) {
    audioProcessor.initialize(nullptr, &status);
}

void processAudio(SensorData& data, SystemSettings& settings) {
    // Simple interface: just collect one sample and update display data
    int audioSample = analogRead(AUDIO_INPUT_PIN);
    audioProcessor.addSample(audioSample);
    audioProcessor.updateDisplayData();
    
    // Update sensor data with current display values
    AudioDisplayData displayData = audioProcessor.getDisplayData();
    data.dominantFreq = (uint16_t)displayData.dominantFreq;
    data.soundLevel = (uint8_t)displayData.soundLevel;
    data.beeState = displayData.beeState;
}

void runAudioDiagnostics(SystemStatus& status) {
    audioProcessor.runDiagnostics();
}

void calibrateAudioLevels(SystemSettings& settings, int durationSeconds) {
    Serial.println(F("Starting audio calibration..."));
    Serial.println(F("Ensure hive is in normal state"));
    
    unsigned long startTime = millis();
    unsigned long duration = durationSeconds * 1000;
    
    float avgFreq = 0;
    float avgLevel = 0;
    int sampleCount = 0;
    
    while (millis() - startTime < duration) {
        audioProcessor.updateDisplayData();
        AudioDisplayData data = audioProcessor.getDisplayData();
        
        avgFreq += data.dominantFreq;
        avgLevel += data.soundLevel;
        sampleCount++;
        
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
        
        Serial.println(F("\nSuggested settings:"));
        Serial.print(F("Queen frequency range: "));
        Serial.print(avgFreq - 50);
        Serial.print(F(" - "));
        Serial.println(avgFreq + 50);
        Serial.print(F("Stress threshold: "));
        Serial.println(avgLevel + 30);
    }
}