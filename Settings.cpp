/**
 * Settings.cpp - COMPLETE FIXED VERSION
 * Settings management implementation with proper LittleFS usage
 */

#include "Settings.h"
#include "Utils.h"  // For button functions

#ifdef NRF52_SERIES
  // Use namespace to avoid ambiguity
  using namespace Adafruit_LittleFS_Namespace;
  // Don't create global file object - we'll create it locally in functions
#endif

// =============================================================================
// DEFAULT SETTINGS
// =============================================================================

SystemSettings getDefaultSettings() {
    SystemSettings defaults;
    
    defaults.tempOffset = DEFAULT_TEMP_OFFSET;
    defaults.humidityOffset = DEFAULT_HUMIDITY_OFFSET;    
    defaults.audioSensitivity = DEFAULT_AUDIO_SENSITIVITY;
    defaults.queenFreqMin = DEFAULT_QUEEN_FREQ_MIN;
    defaults.queenFreqMax = DEFAULT_QUEEN_FREQ_MAX;
    defaults.swarmFreqMin = DEFAULT_SWARM_FREQ_MIN;
    defaults.swarmFreqMax = DEFAULT_SWARM_FREQ_MAX;
    defaults.stressThreshold = DEFAULT_STRESS_THRESHOLD;
    defaults.logInterval = DEFAULT_LOG_INTERVAL;
    defaults.logEnabled = DEFAULT_LOG_ENABLED;
    defaults.tempMin = DEFAULT_TEMP_MIN;
    defaults.tempMax = DEFAULT_TEMP_MAX;
    defaults.humidityMin = DEFAULT_HUMIDITY_MIN;
    defaults.humidityMax = DEFAULT_HUMIDITY_MAX;
    defaults.displayBrightness = DEFAULT_DISPLAY_BRIGHTNESS;
    defaults.fieldModeEnabled = DEFAULT_FIELD_MODE_ENABLED;
    defaults.displayTimeoutMin = DEFAULT_DISPLAY_TIMEOUT_MIN;
    defaults.magicNumber = SETTINGS_MAGIC_NUMBER;
    
    return defaults;
}

// =============================================================================
// CHECKSUM CALCULATION
// =============================================================================

uint16_t calculateChecksum(SystemSettings* s) {
    uint16_t sum = 0;
    uint8_t* data = (uint8_t*)s;
    
    // Calculate checksum for all data except the checksum field itself
    for (size_t i = 0; i < sizeof(SystemSettings) - sizeof(uint16_t); i++) {
        sum += data[i];
    }
    
    return sum;
}

// =============================================================================
// LOAD SETTINGS - FIXED VERSION
// =============================================================================

void loadSettings(SystemSettings& settings) {
#ifdef NRF52_SERIES
    // Initialize internal file system
    InternalFS.begin();
    
    // Create file object locally to avoid ambiguity
    Adafruit_LittleFS_Namespace::File settingsFile(InternalFS);
    
    // Check if settings file exists and can be opened
    if (settingsFile.open("/settings.dat", FILE_O_READ)) {
        Serial.println(F("Settings file found, reading..."));
        
        // Read settings from file
        size_t bytesRead = settingsFile.read((uint8_t*)&settings, sizeof(SystemSettings));
        settingsFile.close();
        
        Serial.print(F("Read ")); Serial.print(bytesRead); Serial.println(F(" bytes"));
        
        // Validate settings
        if (settings.magicNumber == SETTINGS_MAGIC_NUMBER && 
            settings.checksum == calculateChecksum(&settings)) {
            Serial.println(F("Settings loaded successfully from internal flash"));
            return;
        } else {
            Serial.println(F("Settings file corrupted - magic/checksum mismatch"));
            Serial.print(F("Expected magic: 0x")); Serial.println(SETTINGS_MAGIC_NUMBER, HEX);
            Serial.print(F("Got magic: 0x")); Serial.println(settings.magicNumber, HEX);
            Serial.print(F("Expected checksum: 0x")); Serial.println(calculateChecksum(&settings), HEX);
            Serial.print(F("Got checksum: 0x")); Serial.println(settings.checksum, HEX);
        }
    } else {
        Serial.println(F("Settings file not found"));
    }
#endif
    
    // Load defaults if file doesn't exist or is invalid
    Serial.println(F("Loading default settings"));
    settings = getDefaultSettings();
    settings.checksum = calculateChecksum(&settings);
    
    // Save defaults to create the file
    saveSettings(settings);
}

// =============================================================================
// SAVE SETTINGS - FIXED VERSION
// =============================================================================

void saveSettings(SystemSettings& settings) {
    Serial.println(F("saveSettings() called"));
    
    // Update checksum
    settings.checksum = calculateChecksum(&settings);
    Serial.print(F("Checksum calculated: 0x"));
    Serial.println(settings.checksum, HEX);
    
#ifdef NRF52_SERIES
    // Make sure file system is initialized
    InternalFS.begin();
    
    // Create file object locally to avoid ambiguity
    Adafruit_LittleFS_Namespace::File settingsFile(InternalFS);
    
    Serial.println(F("Opening /settings.dat for writing..."));
    
    // Use the proper Adafruit LittleFS API
    if (settingsFile.open("/settings.dat", FILE_O_WRITE)) {
        Serial.println(F("Settings file opened successfully"));
        
        // Write settings to file
        size_t bytesWritten = settingsFile.write((uint8_t*)&settings, sizeof(SystemSettings));
        settingsFile.close();
        
        Serial.print(F("Bytes written: ")); Serial.println(bytesWritten);
        
        if (bytesWritten == sizeof(SystemSettings)) {
            Serial.println(F("Settings saved successfully to internal flash"));
        } else {
            Serial.println(F("Warning: Incomplete write to settings file"));
        }
    } else {
        Serial.println(F("Failed to open settings file for writing"));
        
        // Try to format and retry once
        Serial.println(F("Attempting to format internal filesystem..."));
        if (InternalFS.format()) {
            Serial.println(F("Format successful, reinitializing..."));
            InternalFS.begin();
            
            // Create new file object after format
            Adafruit_LittleFS_Namespace::File retryFile(InternalFS);
            if (retryFile.open("/settings.dat", FILE_O_WRITE)) {
                Serial.println(F("Settings file opened after format"));
                size_t bytesWritten = retryFile.write((uint8_t*)&settings, sizeof(SystemSettings));
                retryFile.close();
                
                if (bytesWritten == sizeof(SystemSettings)) {
                    Serial.println(F("Settings saved successfully after format"));
                } else {
                    Serial.println(F("Failed to write settings even after format"));
                }
            } else {
                Serial.println(F("Still cannot open settings file after format"));
            }
        } else {
            Serial.println(F("Format failed - internal flash may be damaged"));
        }
    }
#else
    Serial.println(F("Settings saved (in RAM only - not nRF52 platform)"));
#endif
}

// =============================================================================
// SETTINGS VALIDATION
// =============================================================================

void validateSettings(SystemSettings& settings) {
    // Validate temperature offset
    if (settings.tempOffset < -10.0 || settings.tempOffset > 10.0) {
        settings.tempOffset = DEFAULT_TEMP_OFFSET;
    }
    
    // Validate humidity offset
    if (settings.humidityOffset < -20.0 || settings.humidityOffset > 20.0) {
        settings.humidityOffset = DEFAULT_HUMIDITY_OFFSET;
    }
            
    // Validate audio sensitivity
    if (settings.audioSensitivity > 10) {
        settings.audioSensitivity = DEFAULT_AUDIO_SENSITIVITY;
    }
    
    // Validate frequency ranges
    if (settings.queenFreqMin >= settings.queenFreqMax) {
        settings.queenFreqMin = DEFAULT_QUEEN_FREQ_MIN;
        settings.queenFreqMax = DEFAULT_QUEEN_FREQ_MAX;
    }
    
    if (settings.swarmFreqMin >= settings.swarmFreqMax) {
        settings.swarmFreqMin = DEFAULT_SWARM_FREQ_MIN;
        settings.swarmFreqMax = DEFAULT_SWARM_FREQ_MAX;
    }
    
    // Validate stress threshold
    if (settings.stressThreshold > 100) {
        settings.stressThreshold = DEFAULT_STRESS_THRESHOLD;
    }
    
    // Validate log interval
    if (settings.logInterval != 5 && settings.logInterval != 10 && 
        settings.logInterval != 30 && settings.logInterval != 60) {
        settings.logInterval = DEFAULT_LOG_INTERVAL;
    }
    
    // Validate temperature thresholds
    if (settings.tempMin >= settings.tempMax) {
        settings.tempMin = DEFAULT_TEMP_MIN;
        settings.tempMax = DEFAULT_TEMP_MAX;
    }
    
    // Validate humidity thresholds
    if (settings.humidityMin >= settings.humidityMax) {
        settings.humidityMin = DEFAULT_HUMIDITY_MIN;
        settings.humidityMax = DEFAULT_HUMIDITY_MAX;
    }
    
    // Validate display brightness
    if (settings.displayBrightness > 10) {
        settings.displayBrightness = DEFAULT_DISPLAY_BRIGHTNESS;
    }
}

// =============================================================================
// SETTINGS EXPORT/IMPORT
// =============================================================================

void exportSettingsToSD(SystemSettings& settings) {
    // Create a human-readable settings file on SD card
    SDLib::File exportFile = SD.open("/settings_export.txt", FILE_WRITE);
    
    if (exportFile) {
        exportFile.println(F("# Hive Monitor Settings Export"));
        exportFile.println(F("# Generated by device"));
        exportFile.println();
        
        exportFile.println(F("[Sensor Calibration]"));
        exportFile.print(F("TempOffset="));
        exportFile.println(settings.tempOffset);
        exportFile.print(F("HumidityOffset="));
        exportFile.println(settings.humidityOffset);
        exportFile.println();
        
        exportFile.println(F("[Audio Settings]"));        
        exportFile.print(F("AudioSensitivity="));
        exportFile.println(settings.audioSensitivity);
        exportFile.print(F("QueenFreqMin="));
        exportFile.println(settings.queenFreqMin);
        exportFile.print(F("QueenFreqMax="));
        exportFile.println(settings.queenFreqMax);
        exportFile.print(F("SwarmFreqMin="));
        exportFile.println(settings.swarmFreqMin);
        exportFile.print(F("SwarmFreqMax="));
        exportFile.println(settings.swarmFreqMax);
        exportFile.print(F("StressThreshold="));
        exportFile.println(settings.stressThreshold);
        exportFile.println();
        
        exportFile.println(F("[Logging]"));
        exportFile.print(F("LogInterval="));
        exportFile.println(settings.logInterval);
        exportFile.print(F("LogEnabled="));
        exportFile.println(settings.logEnabled ? "true" : "false");
        exportFile.println();
        
        exportFile.println(F("[Alert Thresholds]"));
        exportFile.print(F("TempMin="));
        exportFile.println(settings.tempMin);
        exportFile.print(F("TempMax="));
        exportFile.println(settings.tempMax);
        exportFile.print(F("HumidityMin="));
        exportFile.println(settings.humidityMin);
        exportFile.print(F("HumidityMax="));
        exportFile.println(settings.humidityMax);
        exportFile.println();
        
        exportFile.println(F("[System]"));
        exportFile.print(F("DisplayBrightness="));
        exportFile.println(settings.displayBrightness);
        
        exportFile.close();
        Serial.println(F("Settings exported to SD card"));
    }
}

void clearUserData() {
    // Optional: Clear log files (be very careful with this!)
    // For safety, we'll just create a reset marker file instead
    
    SDLib::File resetMarker = SD.open("/factory_reset_performed.txt", FILE_WRITE);
    if (resetMarker) {
        resetMarker.print(F("Factory reset performed at: "));
        resetMarker.println(millis());
        resetMarker.close();
        Serial.println(F("Reset marker created"));
    }
    
    // Optionally clear alert history
    if (SD.exists("/alerts.log")) {
        SD.remove("/alerts.log");
        Serial.println(F("Alert history cleared"));
    }
}

// =============================================================================
// SETTINGS INFORMATION DISPLAY
// =============================================================================

void printSettingsInfo(const SystemSettings& settings) {
    Serial.println(F("\n=== Current Settings ==="));
    
    // Calibration
    Serial.print(F("Temperature Offset: "));
    Serial.print(settings.tempOffset, 1);
    Serial.println(F("°C"));
    
    Serial.print(F("Humidity Offset: "));
    Serial.print(settings.humidityOffset, 1);
    Serial.println(F("%"));
    
    // Audio settings
    Serial.print(F("Audio Sensitivity: "));
    Serial.print(settings.audioSensitivity);
    Serial.println(F("/10"));
    
    Serial.print(F("Queen Frequency: "));
    Serial.print(settings.queenFreqMin);
    Serial.print(F("-"));
    Serial.print(settings.queenFreqMax);
    Serial.println(F(" Hz"));
    
    Serial.print(F("Swarm Frequency: "));
    Serial.print(settings.swarmFreqMin);
    Serial.print(F("-"));
    Serial.print(settings.swarmFreqMax);
    Serial.println(F(" Hz"));
    
    Serial.print(F("Stress Threshold: "));
    Serial.print(settings.stressThreshold);
    Serial.println(F("%"));
    
    // Logging
    Serial.print(F("Log Interval: "));
    Serial.print(settings.logInterval);
    Serial.println(F(" minutes"));
    
    Serial.print(F("Logging Enabled: "));
    Serial.println(settings.logEnabled ? "YES" : "NO");
    
    // Alert thresholds
    Serial.print(F("Temperature Range: "));
    Serial.print(settings.tempMin, 1);
    Serial.print(F(" - "));
    Serial.print(settings.tempMax, 1);
    Serial.println(F("°C"));
    
    Serial.print(F("Humidity Range: "));
    Serial.print(settings.humidityMin, 1);
    Serial.print(F(" - "));
    Serial.print(settings.humidityMax, 1);
    Serial.println(F("%"));
    
    // System settings
    Serial.print(F("Display Brightness: "));
    Serial.print(settings.displayBrightness);
    Serial.println(F("/10"));
    
    Serial.print(F("Field Mode: "));
    Serial.println(settings.fieldModeEnabled ? "ENABLED" : "DISABLED");
    
    Serial.print(F("Display Timeout: "));
    Serial.print(settings.displayTimeoutMin);
    Serial.println(F(" minutes"));
    
    // Bee type detection
    BeeType currentType = detectCurrentBeeType(settings);
    Serial.print(F("Detected Bee Type: "));
    Serial.println(getBeeTypeName(currentType));
    
    // Validation status
    Serial.print(F("Settings Valid: "));
    Serial.println(isValidSystemSettings(settings) ? "YES" : "NO");
    
    Serial.print(F("Magic Number: 0x"));
    Serial.println(settings.magicNumber, HEX);
    
    Serial.print(F("Checksum: 0x"));
    Serial.println(settings.checksum, HEX);
    
    Serial.println(F("========================\n"));
}