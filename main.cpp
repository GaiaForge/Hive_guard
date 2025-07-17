
/**
 * main.cpp 
 * HiveGuard Hive Monitor System V2.0 - WITH TRUE DEEP SLEEP
 */

#include "Config.h"
#include "DataStructures.h"
#include "Settings.h"
#include "Sensors.h"
#include "Audio.h"
#include "Display.h"
#include "Menu.h"
#include "DataLogger.h"
#include "Alerts.h"
#include "Utils.h"
#include "PowerManager.h"
#include "FieldModeBuffer.h"
#include "Bluetooth.h"
#include <Wire.h>  // Required for I2C communication with PCF8523

#ifdef NRF52_SERIES
#include <nrf.h>
#include <nrf_power.h>
#endif

// State machine handlers
void handleAwakeState(unsigned long currentTime);
void handleSleepingState(unsigned long currentTime);
void handleScheduledWakeState(unsigned long currentTime);
void handleUserWakeState(unsigned long currentTime);

// =============================================================================
// SLEEP/WAKE STATE MACHINE
// =============================================================================

enum SystemState {
    STATE_AWAKE = 0,           // Normal operation, display on, user interaction
    STATE_SLEEPING = 1,        // Deep sleep, display off, waiting for wake events
    STATE_SCHEDULED_WAKE = 2,  // Woke from RTC, take readings, display stays off
    STATE_USER_WAKE = 3        // Woke from button, turn on display, resume interaction
};

// Global state variables
WakeUpSource wakeUpReason = WAKE_UNKNOWN;
SystemState currentSystemState = STATE_AWAKE;
unsigned long stateChangeTime = 0;
bool readingInProgress = false;


// Hardware objects
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_BME280 bme;
RTC_PCF8523 rtc;
BluetoothManager bluetoothManager;

// System data
SystemSettings settings;
SystemStatus systemStatus = {false, false, false, false, false, false};
SensorData currentData;
DisplayMode currentMode = MODE_DASHBOARD;
MenuState menuState = {false, 0, 0, 0, 0.0, 0};

// Audio data 
SpectralFeatures currentSpectralFeatures = {0};
ActivityTrend currentActivityTrend = {0};

// ADD THESE LINES - Environmental history tracking for ML
float lastTemperature = 0;
float lastHumidity = 0;
float lastPressure = 0;
unsigned long lastEnvReading = 0;
bool envHistoryValid = false;


// : Power Manager and Field Buffer
PowerManager powerManager;
extern FieldModeBufferManager fieldBuffer; // Defined in FieldModeBuffer.cpp


// Function declaration for field mode functions

void handleTestingModeOperation(unsigned long currentTime);


// Timing variables
unsigned long lastSensorRead = 0;
unsigned long lastLogTime = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastPowerUpdate = 0;

// Field mode state tracking
bool wasInFieldMode = false;
unsigned long fieldModeStartTime = 0;
unsigned long lastFieldModeCheck = 0;

WakeUpSource detectWakeupSource() {
#ifdef NRF52_SERIES
    // Check if we have access to nRF52 power registers
    #ifdef NRF_POWER
        // Check the reset reason register FIRST thing in setup()
        uint32_t reset_reason = NRF_POWER->RESETREAS;
        NRF_POWER->RESETREAS = 0xFFFFFFFF; // Clear for next boot
        
        // Use hex values instead of undefined constants
        if (reset_reason & 0x00040000) {  // NRF_POWER_RESETREAS_OFF_Msk equivalent
            // Woke from System OFF sleep via GPIO pin
            Serial.print(F("Reset reason: System OFF wake (0x"));
            Serial.print(reset_reason, HEX);
            Serial.println(F(")"));
            return WAKE_RTC;
        } else if (reset_reason & 0x00000001) {  // NRF_POWER_RESETREAS_RESETPIN_Msk equivalent
            // Reset pin pressed
            Serial.print(F("Reset reason: Reset pin (0x"));
            Serial.print(reset_reason, HEX);
            Serial.println(F(")"));
            return WAKE_POWER_ON;
        } else if (reset_reason & 0x00000002) {  // NRF_POWER_RESETREAS_DOG_Msk equivalent
            // Watchdog reset
            Serial.print(F("Reset reason: Watchdog (0x"));
            Serial.print(reset_reason, HEX);
            Serial.println(F(")"));
            return WAKE_UNKNOWN;
        } else {
            // Normal power-on or other reason
            Serial.print(F("Reset reason: Power-on (0x"));
            Serial.print(reset_reason, HEX);
            Serial.println(")");
            return WAKE_POWER_ON;
        }
    #else
        Serial.println("NRF_POWER not available - assuming power-on");
        return WAKE_POWER_ON;
    #endif
#else
    Serial.println("Non-nRF52 platform - assuming power-on");
    return WAKE_POWER_ON;  // Non-nRF52 platforms
#endif
}

void setup() {
    // *** DETECT WAKE-UP REASON FIRST ***
    wakeUpReason = detectWakeupSource();
    
    Serial.begin(115200);
    delay(1000); // Shorter delay for wake-ups
    
    // Print wake-up information
    Serial.println(F("=== HiveGuard Hive Monitor v2.0 - Deep Sleep Edition ==="));
    Serial.print(F("Wake-up reason: "));
    switch (wakeUpReason) {
        case WAKE_RTC:
            Serial.println(F("RTC ALARM (scheduled reading)"));
            break;
        case WAKE_POWER_ON:
            Serial.println(F("POWER ON (normal boot)"));
            break;
        default:
            Serial.println(F("UNKNOWN"));
            break;
    }
    
    // Initialize I2C and SPI
    Wire.begin();
    Wire.setClock(100000);  // 100kHz for reliable PCF8523 communication
    SPI.begin();

    // Initialize Power Manager early (before retained state check)
    powerManager.initialize(&systemStatus, &settings);
    
    // *** CHECK FOR RETAINED STATE RESTORATION ***
    bool restoredFromSleep = false;
    if (wakeUpReason == WAKE_RTC) {
        restoredFromSleep = powerManager.restoreRetainedState();
    }
    
    // If we restored from deep sleep, skip most of the normal initialization
    if (restoredFromSleep) {
        Serial.println(F("=== QUICK WAKE FROM DEEP SLEEP ==="));
        
        // Initialize only essential components for reading
        if (rtc.begin()) {
            systemStatus.rtcWorking = true;
            Serial.println(F("RTC: OK (quick init)"));
            
            // Ensure RTC is running
            if (!rtc.isrunning()) {
                Serial.println(F("Starting RTC oscillator"));
                rtc.start();
            }
            
            // Initialize deep sleep capability for quick wake
            if (powerManager.initializeDeepSleep()) {
                Serial.println(F("Deep sleep: ENABLED (quick wake)"));
            }
        }
        
        // Quick SD check
        if (SD.begin(SD_CS_PIN)) {
            systemStatus.sdWorking = true;
            Serial.println(F("SD: OK (quick init)"));
        } else {
            systemStatus.sdWorking = false;
            Serial.println(F("SD: FAILED (quick init)"));
        }
        
        // Initialize sensors
        initializeSensors(bme, systemStatus);
        Serial.println(F("Sensors: OK (quick init)"));
        
        // Load settings quickly
        loadSettings(settings);
        
        // Initialize field buffer
        fieldBuffer.clearBuffer();
        
        // Take reading and go back to sleep
        currentSystemState = STATE_SCHEDULED_WAKE;
        stateChangeTime = millis();
        Serial.println(F("=== QUICK WAKE COMPLETE ==="));
        return;
    }
    
    // *** NORMAL BOOT SEQUENCE ***
    Serial.println(F("=== NORMAL BOOT SEQUENCE ==="));
    
    // Clear any stale retained state
    powerManager.clearRetainedState();

    // Initialize display with different behavior based on wake reason
    if (display.begin(SCREEN_ADDRESS, true)) {
        systemStatus.displayWorking = true;
        
        if (wakeUpReason == WAKE_RTC) {
            // Quick wake screen for scheduled readings (fallback case)
            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SH110X_WHITE);
            display.setCursor(20, 20);
            display.println(F("Scheduled Wake"));
            display.setCursor(10, 35);
            display.println(F("Taking readings..."));
            display.display();
        } else {
            // Full startup screen for normal boot
            showStartupScreen(display);
            delay(2000);
            showSensorDiagnosticsScreen(display, systemStatus);
        }
        
        Serial.println(F("Display: OK"));
    } else {
        Serial.println(F("Display: FAILED"));
        systemStatus.displayWorking = false;
    }

    // Initialize SD card (quick check for wake-ups)
    Serial.print(F("SD Card: "));
    if (systemStatus.displayWorking && wakeUpReason == WAKE_POWER_ON) {
        checkSDCardAtStartup(display, systemStatus);
    } else {
        // Quick SD check for wake-ups
        if (SD.begin(SD_CS_PIN)) {
            systemStatus.sdWorking = true;
            Serial.println(F("OK"));
        } else {
            systemStatus.sdWorking = false;
            Serial.println(F("FAILED"));
        }
    }
    
    // Load settings
    Serial.println(F("Loading settings..."));
    loadSettings(settings);
    
    // Initialize RTC
    if (rtc.begin()) {
        systemStatus.rtcWorking = true;
        Serial.println(F("RTC: OK"));
        
        // Only set time on normal boot, not on wake-up
        if (rtc.lostPower() && wakeUpReason == WAKE_POWER_ON) {
            Serial.println(F("RTC lost power, setting time"));
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        
        // Ensure RTC is running
        if (!rtc.isrunning()) {
            Serial.println(F("Starting RTC oscillator"));
            rtc.start();
        }
        
        // *** NOW INITIALIZE DEEP SLEEP CAPABILITY AFTER RTC IS READY ***
        Serial.println(F("Checking deep sleep capability..."));
        if (powerManager.initializeDeepSleep()) {
            Serial.println(F("Deep sleep: ENABLED"));
        } else {
            Serial.println(F("Deep sleep: DISABLED (using polling fallback)"));
        }
    } else {
        Serial.println(F("RTC: FAILED"));
        systemStatus.rtcWorking = false;
    }
    
    // Initialize sensors
    initializeSensors(bme, systemStatus);
    
    // Initialize audio (only for normal boot to save time)
    if (wakeUpReason == WAKE_POWER_ON) {
        initializeAudio(systemStatus);
    }
    
    // Initialize buttons
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);
    pinMode(BTN_BACK, INPUT_PULLUP);
    Serial.println(F("Buttons: OK"));
    
    // *** CRITICAL: Initialize wake detection ***
    powerManager.initializeWakeDetection(wakeUpReason);
    
    // Initialize Bluetooth Manager
    bluetoothManager.initialize(&systemStatus, &settings);
    powerManager.setBluetoothManager(&bluetoothManager);
    
    // Initialize field buffer
    fieldBuffer.clearBuffer();
    
    Serial.println(F("=== System Ready ==="));
    systemStatus.systemReady = true;
    
    // Take initial reading
    readAllSensors(bme, currentData, settings, systemStatus);
    checkAlerts(currentData, settings, systemStatus);
    
    // *** SET INITIAL STATE BASED ON WAKE REASON ***
    if (wakeUpReason == WAKE_RTC) {
        Serial.println(F("Entering scheduled wake state for sensor reading"));
        currentSystemState = STATE_SCHEDULED_WAKE;
        stateChangeTime = millis();
        
        // For deep sleep wake-ups, keep display off
        if (powerManager.didWakeFromDeepSleep()) {
            powerManager.turnOffDisplay();
        }
    } else {
        Serial.println(F("Entering normal awake state"));
        currentSystemState = STATE_AWAKE;
        stateChangeTime = millis();
        
        // Update display for normal boot
        updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc,
                      currentSpectralFeatures, currentActivityTrend);
    }
    
    // Show power status (only for normal boot)
    if (wakeUpReason == WAKE_POWER_ON) {
        powerManager.printPowerStatus();
    }
}

void loop() {
    unsigned long currentTime = millis();
    
    // Update button states - ALWAYS FIRST
    updateButtonStates();

    // Check for Bluetooth button press (highest priority)
    if (wasBluetoothButtonPressed()) {
        Serial.println(F("BLUETOOTH BUTTON pressed"));
        powerManager.handleBluetoothButtonPress();
        
        // If we were sleeping, this wakes us up
        if (currentSystemState == STATE_SLEEPING) {
            currentSystemState = STATE_USER_WAKE;
            stateChangeTime = currentTime;
        }
        resetButtonStates();
    }
    
    // State Machine Logic
    switch (currentSystemState) {
        
        case STATE_AWAKE:
            handleAwakeState(currentTime);
            break;
            
        case STATE_SLEEPING:
            handleSleepingState(currentTime);
            break;
            
        case STATE_SCHEDULED_WAKE:
            handleScheduledWakeState(currentTime);
            break;
            
        case STATE_USER_WAKE:
            handleUserWakeState(currentTime);
            break;
    }
}

// =============================================================================
// STATE HANDLERS
// =============================================================================

void handleAwakeState(unsigned long currentTime) {
   // Handle settings menu (highest priority)
   if (menuState.settingsMenuActive) {
       handleSettingsMenu(display, menuState, settings, rtc, currentData, systemStatus);
       return; // Skip everything else when in menu
   }

   // Update Bluetooth manager
   bluetoothManager.update();

   // Handle main navigation buttons
   bool buttonPressed = false;
   
   if (wasButtonPressed(0)) { // UP
       Serial.println(F("UP pressed"));
       powerManager.handleUserActivity();
       
       if (currentMode > 0) {
           currentMode = (DisplayMode)(currentMode - 1);
       } else {
           currentMode = MODE_POWER;
       }
       buttonPressed = true;
   }
   
   if (wasButtonPressed(1)) { // DOWN  
       Serial.println(F("DOWN pressed"));
       powerManager.handleUserActivity();
       
       if (currentMode < MODE_POWER) {
           currentMode = (DisplayMode)(currentMode + 1);
       } else {
           currentMode = MODE_DASHBOARD;
       }
       buttonPressed = true;
   }
   
   if (wasButtonPressed(2)) { // SELECT
       Serial.println(F("SELECT pressed"));
       powerManager.handleUserActivity();
       
       if (currentMode == MODE_DASHBOARD) {
           Serial.println(F("Entering settings menu"));
           menuState.settingsMenuActive = true;
           menuState.menuLevel = 0;
           menuState.selectedItem = 0;
           resetButtonStates();
           return;
       }
       buttonPressed = true;
   }
   
   if (wasButtonPressed(3)) { // BACK
       Serial.println(F("BACK pressed"));
       powerManager.handleUserActivity();
       currentMode = MODE_DASHBOARD;
       buttonPressed = true;
   }

   // Force display update on button press
   if (buttonPressed) {
       Serial.print(F("Mode changed to: "));
       Serial.println(currentMode);
       updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc,
                     currentSpectralFeatures, currentActivityTrend);
       lastDisplayUpdate = currentTime;
   }

   // Run normal testing mode operation
   handleTestingModeOperation(currentTime);
   
   // Check if we should enter sleep (only in field mode)
   if (powerManager.isFieldModeActive()) {
       powerManager.update(); // This checks for timeout and triggers sleep
       
       // If display is now off, we're going to sleep
       if (!powerManager.isDisplayOn()) {
           Serial.println(F("STATE: AWAKE → SLEEPING"));
           
           // IMPORTANT: Recalculate next wake time based on current settings
           // This accounts for any changes made in the settings menu
           powerManager.updateNextWakeTime(settings.logInterval);
           
           currentSystemState = STATE_SLEEPING;
           stateChangeTime = currentTime;
           
           // Start new sleep cycle with updated settings
           powerManager.enterFieldSleep();
       }
   }
}


void handleSleepingState(unsigned long currentTime) {
    // Add debug output to see what's happening
    static unsigned long lastDebug = 0;
    if (currentTime - lastDebug > 2000) {  // Every 2 seconds
        Serial.print(powerManager.isWakeupFromScheduledTimer());        
        Serial.print(F(", lastWakeSource="));
        Serial.println(powerManager.getWakeSourceString());
        lastDebug = currentTime;
    }
    
    // Update power manager to handle wake-up events
    powerManager.update();
    
    // Check wake-up sources
    if (powerManager.isWakeupFromScheduledTimer()) {
        Serial.println(F("STATE: SLEEPING → SCHEDULED_WAKE (RTC alarm)"));
        currentSystemState = STATE_SCHEDULED_WAKE;
        stateChangeTime = currentTime;
        readingInProgress = false;
        return;
    }
    
    // Check for user button wake-up
    if (wasButtonPressed(0) || wasButtonPressed(1) || wasButtonPressed(2) || wasButtonPressed(3)) {
        Serial.println(F("STATE: SLEEPING → USER_WAKE (button press)"));
        powerManager.handleUserActivity(); // This turns on display and resets timeout
        currentSystemState = STATE_USER_WAKE;
        stateChangeTime = currentTime;
        resetButtonStates();
        return;
    }
    
    // Check for long press wake
    if (powerManager.checkForLongPressWake()) {
        Serial.println(F("STATE: SLEEPING → USER_WAKE (long press)"));
        currentSystemState = STATE_USER_WAKE;
        stateChangeTime = currentTime;
        resetButtonStates();
        return;
    }
    
    // In sleep state, we don't update display or run normal operations
    // Just minimal processing and wait for wake events
    
    // Small delay to prevent busy waiting
    delay(100);
}

void handleScheduledWakeState(unsigned long currentTime) {
    Serial.println(F("=== SCHEDULED WAKE: Taking sensor readings ==="));
    
    if (!readingInProgress) {
        // Power up sensors for this reading
        powerManager.powerUpSensors();
        Serial.println(F("Sensors powered up, stabilizing..."));
        readingInProgress = true;
        return; // Let sensors stabilize
    }
    
    // Take sensor readings (includes 200ms stabilization delay)
    readAllSensors(bme, currentData, settings, systemStatus);
    checkAlerts(currentData, settings, systemStatus);
    
    Serial.print(F("Sensors: T="));
    Serial.print(currentData.temperature, 1);
    Serial.print(F("C H="));
    Serial.print(currentData.humidity, 1);
    Serial.print(F("% P="));
    Serial.print(currentData.pressure, 1);
    Serial.print(F("hPa Bat="));
    Serial.print(currentData.batteryVoltage, 2);
    Serial.println(F("V"));
    
    // Process audio if available
    if (systemStatus.pdmWorking) {
        Serial.println(F("Collecting audio samples for full analysis..."));
        
        // Collect several samples for better analysis
        for (int i = 0; i < 50; i++) {
            processAudio(currentData, settings);
            delay(10);
        }
        
        // Perform full FFT analysis
        AudioAnalysisResult fullResult = audioProcessor.performFullAnalysis();
        if (fullResult.analysisValid) {
            currentData.dominantFreq = fullResult.dominantFreq;
            currentData.soundLevel = fullResult.soundLevel;
            currentData.beeState = fullResult.beeState;
            
            Serial.print(F("Audio: Freq="));
            Serial.print(fullResult.dominantFreq);
            Serial.print(F("Hz, Level="));
            Serial.print(fullResult.soundLevel);
            Serial.print(F("%, State="));
            Serial.println(getBeeStateString(fullResult.beeState));
        }
    }
    
    // Add to buffer instead of logging directly
    // In handleScheduledWakeState(), replace the buffer addition with:
    if (systemStatus.rtcWorking) {
        uint32_t timestamp = rtc.now().unixtime();
        
        // Get the full audio analysis result if available
        AudioAnalysisResult* audioResult = nullptr;
        if (systemStatus.pdmWorking) {
            static AudioAnalysisResult fullResult;
            fullResult = audioProcessor.performFullAnalysis();
            if (fullResult.analysisValid) {
                audioResult = &fullResult;
            }
        }
        
        if (fieldBuffer.addReading(currentData, timestamp, audioResult)) {
            Serial.print(F("Added FULL ML reading to buffer ("));
            Serial.print(fieldBuffer.getBufferCount());
            Serial.println(F(" readings)"));
        } else {
            Serial.println(F("Buffer full - flushing ML data to SD"));
            fieldBuffer.flushToSD(rtc, systemStatus);
            fieldBuffer.addReading(currentData, timestamp, audioResult);
        }
    }
    
    // Check if it's time to flush buffer
    if (powerManager.isTimeForBufferFlush() || fieldBuffer.isBufferFull()) {
        Serial.println(F("Flushing buffer to SD..."));
        fieldBuffer.flushToSD(rtc, systemStatus);
    }
    
    // Power down sensors again
    powerManager.powerDownSensors();
    powerManager.powerDownAudio();
    
    Serial.println(F("=== SCHEDULED WAKE COMPLETE ==="));
    
    // Clear wake source BEFORE transitioning to sleeping
    powerManager.clearWakeSource();
    
    // Configure next wake time using CURRENT settings (accounts for any changes)
    powerManager.updateNextWakeTime(settings.logInterval);
    
    Serial.println(F("STATE: SCHEDULED_WAKE → SLEEPING"));
    currentSystemState = STATE_SLEEPING;
    stateChangeTime = currentTime;
    readingInProgress = false;
    
    // Start new sleep cycle with updated settings
    powerManager.enterFieldSleep();
}

void handleUserWakeState(unsigned long currentTime) {
    Serial.println(F("=== USER WAKE: Full system access ==="));
    
    // Ensure display is on and components are powered up
    powerManager.wakeFromFieldSleep();
    
    // Transition to normal awake state
    Serial.println(F("STATE: USER_WAKE → AWAKE"));
    currentSystemState = STATE_AWAKE;
    stateChangeTime = currentTime;
    
    // Update display immediately
    updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc,
                  currentSpectralFeatures, currentActivityTrend);
}


void handleTestingModeOperation(unsigned long currentTime) {
    // Read sensors every 5 seconds - UNCHANGED
    if (currentTime - lastSensorRead >= 5000) {
        readAllSensors(bme, currentData, settings, systemStatus);
        checkAlerts(currentData, settings, systemStatus);
        lastSensorRead = currentTime;
        
        Serial.print(F("Sensors: T="));
        Serial.print(currentData.temperature, 1);
        Serial.print(F("C H="));
        Serial.print(currentData.humidity, 1);
        Serial.print(F("% P="));
        Serial.print(currentData.pressure, 1);
        Serial.print(F("hPa Bat="));
        Serial.print(currentData.batteryVoltage, 2);
        Serial.println(F("V"));
    }
    
    // Update Power Manager every 5 seconds
    if (currentTime - lastPowerUpdate >= 5000) {
        powerManager.updatePowerMode(currentData.batteryVoltage);
        lastPowerUpdate = currentTime;
        
        // Debug output every 30 seconds
        static unsigned long lastPowerDebug = 0;
        if (currentTime - lastPowerDebug >= 30000) {
            Serial.println(F("\n=== Power Manager Update ==="));
            Serial.print(F("Battery: "));
            Serial.print(currentData.batteryVoltage, 2);
            Serial.print(F("V ("));
            Serial.print(getBatteryLevel(currentData.batteryVoltage));
            Serial.println(F("%)"));
            
            Serial.print(F("Power Mode: "));
            Serial.println(powerManager.getPowerModeString());
            
            Serial.print(F("Field Mode: "));
            Serial.println(powerManager.isFieldModeActive() ? "ACTIVE" : "INACTIVE");
            
            Serial.print(F("Est. Runtime: "));
            Serial.print(powerManager.getEstimatedRuntimeHours(), 1);
            Serial.println(F(" hours"));
            
            Serial.print(F("Memory Usage: "));
            Serial.print(getMemoryUsagePercent());
            Serial.println(F("%"));

            // In handleTestingModeOperation(), with the other debug output:
            Serial.print(F("Stack High Water: "));
            Serial.print(getStackHighWaterMark());
            Serial.println(F(" bytes"));
            printMemoryInfo(); // Full breakdown
                        
            lastPowerDebug = currentTime;
        }
    }
    
    // CLEAN AUDIO PROCESSING - No more legacy code!
    if (systemStatus.pdmWorking) {
        // Always collect audio samples for real-time display (every 100ms)
        static unsigned long lastAudioSample = 0;
        if (currentTime - lastAudioSample >= 100) {
            processAudio(currentData, settings);
            lastAudioSample = currentTime;
        }
        
        // Run full FFT analysis every 5 seconds ONLY when viewing Sound Monitor
        static unsigned long lastFullAnalysis = 0;
        if (currentMode == MODE_SOUND && (currentTime - lastFullAnalysis >= 5000)) {
            Serial.println(F("Running full audio analysis for Sound Monitor..."));
            
            AudioAnalysisResult fullResult = audioProcessor.performFullAnalysis();
            
            if (fullResult.analysisValid) {
                // Update current data with full analysis
                currentData.dominantFreq = fullResult.dominantFreq;
                currentData.soundLevel = fullResult.soundLevel;
                currentData.beeState = fullResult.beeState;
                
                // Update spectral features for display
                currentSpectralFeatures.spectralCentroid = fullResult.spectralCentroid;
                currentSpectralFeatures.totalEnergy = fullResult.shortTermEnergy;
                currentSpectralFeatures.harmonicity = fullResult.harmonicity;
                
                // Copy band energies
                currentSpectralFeatures.bandEnergyRatios[0] = fullResult.bandEnergy0_200Hz;
                currentSpectralFeatures.bandEnergyRatios[1] = fullResult.bandEnergy200_400Hz;
                currentSpectralFeatures.bandEnergyRatios[2] = fullResult.bandEnergy400_600Hz;
                currentSpectralFeatures.bandEnergyRatios[3] = fullResult.bandEnergy600_800Hz;
                currentSpectralFeatures.bandEnergyRatios[4] = fullResult.bandEnergy800_1000Hz;
                currentSpectralFeatures.bandEnergyRatios[5] = fullResult.bandEnergy1000PlusHz;
                
                // Update activity trend for display
                currentActivityTrend.currentActivity = fullResult.shortTermEnergy * 10.0; // Scale for display
                currentActivityTrend.baselineActivity = fullResult.longTermEnergy * 10.0;
                currentActivityTrend.activityIncrease = fullResult.activityIncrease;
                currentActivityTrend.abnormalTiming = (fullResult.contextFlags & CONTEXT_EVENING) > 0;
                
                Serial.print(F("FFT Complete: Freq="));
                Serial.print(fullResult.dominantFreq);
                Serial.print(F("Hz, Centroid="));
                Serial.print(fullResult.spectralCentroid, 1);
                Serial.print(F("Hz, Activity="));
                Serial.print(fullResult.activityIncrease, 2);
                Serial.print(F("x, Baseline="));
                Serial.print(currentActivityTrend.baselineActivity, 1);
                Serial.println(F("%"));
            } else {
                Serial.println(F("FFT analysis failed - not enough samples"));
            }
            
            lastFullAnalysis = currentTime;
        }
    }
        
    // Log data if enabled
    if (settings.logEnabled) {
        unsigned long logIntervalMs = settings.logInterval * 60000UL;
        if (currentTime - lastLogTime >= logIntervalMs) {
            logData(currentData, rtc, settings, systemStatus);
            lastLogTime = currentTime;
        }
    }
    
    // Update display every second (unless button was just pressed)
    if (currentTime - lastDisplayUpdate >= 1000) {
        updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc,
                      currentSpectralFeatures, currentActivityTrend);
        lastDisplayUpdate = currentTime;
    }
    
    // Check for factory reset (BACK + SELECT held together)
    static unsigned long resetHoldStart = 0;
    if (isButtonHeld(2) && isButtonHeld(3)) { // SELECT + BACK
        if (resetHoldStart == 0) {
            resetHoldStart = currentTime;
        }
        
        if (currentTime - resetHoldStart > 5000) { // 5 seconds
            Serial.println(F("Factory reset triggered"));
            performFactoryReset(settings, systemStatus, display);
            resetHoldStart = 0;
        }
    } else {
        resetHoldStart = 0;
    }
}

