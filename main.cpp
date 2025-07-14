/**
 * main.cpp 
 * HiveGuard Hive Monitor System V1.0 - Fixed Settings Storage + Reverted Display Timeout
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

// Audio data (simplified)
SpectralFeatures currentSpectralFeatures = {0};
ActivityTrend currentActivityTrend = {0};

// : Power Manager and Field Buffer
PowerManager powerManager;
extern FieldModeBufferManager fieldBuffer; // Defined in FieldModeBuffer.cpp


// Function declarations for field mode functions
void handleFieldModeTransitions();
void handleFieldModeOperation(unsigned long currentTime);
void handleTestingModeOperation(unsigned long currentTime);
void enterFieldSleep();

// Timing variables
unsigned long lastSensorRead = 0;
unsigned long lastLogTime = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastPowerUpdate = 0;

// Field mode state tracking
bool wasInFieldMode = false;
unsigned long fieldModeStartTime = 0;
unsigned long lastFieldModeCheck = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println(F("=== HiveGuard Hive Monitor v1.0 - : Display Power Control Integrated==="));
    
    // Initialize I2C and SPI first
    Wire.begin();
    Wire.setClock(100000);
    SPI.begin();

    
    // Initialize display
    if (display.begin(SCREEN_ADDRESS, true)) {
        systemStatus.displayWorking = true;
        showStartupScreen(display);
        Serial.println(F("Display: OK (hardware power control enabled)"));
    } else {
        Serial.println(F("Display: FAILED"));
    }
    delay(1000);
    
    // Initialize SD card BEFORE loading settings
    Serial.print(F("SD Card: "));
    if (SD.begin(SD_CS_PIN)) {
        systemStatus.sdWorking = true;
        Serial.println(F("OK"));
    } else {
        systemStatus.sdWorking = false;
        Serial.println(F("FAILED - settings will not persist!"));
    }
    
    // : Load settings from SD card
    Serial.println(F("Loading settings..."));
    loadSettings(settings);
    printSettingsInfo(settings); // : Show settings info at startup
    
    // Initialize display
    if (display.begin(SCREEN_ADDRESS, true)) {
        systemStatus.displayWorking = true;
        showStartupScreen(display);
        Serial.println(F("Display: OK"));
    } else {
        Serial.println(F("Display: FAILED"));
    }
    delay(1000);
    
    // Create log file if SD is working
    if (systemStatus.sdWorking) {
        createLogFile(rtc, systemStatus);
    }
    
    // Initialize RTC
    Serial.print(F("RTC: "));
    if (rtc.begin()) {
        systemStatus.rtcWorking = true;
        Serial.println(F("OK"));
        
        if (rtc.lostPower()) {
            Serial.println(F("RTC lost power, setting time"));
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        
        if (!rtc.initialized() || rtc.lostPower()) {
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            rtc.start();
        }
    } else {
        Serial.println(F("FAILED"));
        systemStatus.rtcWorking = false;
    }
    
    // Initialize sensors
    Serial.print(F("BME280: "));
    initializeSensors(bme, systemStatus);
    if (systemStatus.bmeWorking) {
        Serial.println(F("OK"));
    } else {
        Serial.println(F("FAILED"));
    }
    
    // Initialize audio (with proper detection)
    Serial.print(F("Audio: "));
    initializeAudio(systemStatus);
    if (systemStatus.pdmWorking) {
        Serial.println(F("Microphone detected"));
    } else {
        Serial.println(F("No microphone - audio disabled"));
    }
    
    // Initialize buttons
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);
    pinMode(BTN_BACK, INPUT_PULLUP);
    Serial.println(F("Buttons: OK"));
    
    // : Initialize Power Manager with fixed settings
    Serial.print(F("Power Manager: "));
    powerManager.initialize(&systemStatus, &settings);
    Serial.println(F("OK - Settings Persistence Fixed"));

    //  Initialize Bluetooth Manager
    Serial.print(F("Bluetooth Manager: "));
    bluetoothManager.initialize(&systemStatus, &settings);
    Serial.println(F("OK"));
    
    // Link PowerManager with BluetoothManager
    powerManager.setBluetoothManager(&bluetoothManager);
    
    // : Initialize field buffer
    Serial.println(F("Field Buffer: Initialized"));
    fieldBuffer.clearBuffer();
    
    Serial.println(F("=== System Ready ==="));
    systemStatus.systemReady = true;
    
    // Take initial reading
    readAllSensors(bme, currentData, settings, systemStatus);
    checkAlerts(currentData, settings, systemStatus);
    
    // : Set initial wake time for field mode
    if (systemStatus.rtcWorking) {
        powerManager.updateNextWakeTime(settings.logInterval);
    }
    
    // Update display
    updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc,
                  currentSpectralFeatures, currentActivityTrend);
    
    // : Show initial power status with settings info
    Serial.println(F("\n=== Initial Power Status ==="));
    powerManager.printPowerStatus();
    Serial.print(F("Field Mode from Settings: "));
    Serial.println(settings.fieldModeEnabled ? "YES" : "NO");
    Serial.print(F("Display Timeout from Settings: "));
    Serial.print(settings.displayTimeoutMin);
    Serial.println(F(" minutes"));
    
    // : Export settings for backup/verification
    if (systemStatus.sdWorking) {
        exportSettingsToSD(settings);
        Serial.println(F("Settings exported for verification"));
    }
}

void loop() {
    unsigned long currentTime = millis();
    
    // Update button states - ALWAYS FIRST (unchanged)
    updateButtonStates();

    // Check for Bluetooth button press (highest priority in field mode)
    if (wasBluetoothButtonPressed()) {
        Serial.println(F("BLUETOOTH BUTTON pressed"));
        powerManager.handleBluetoothButtonPress();
        resetButtonStates(); // Clear other button states to prevent conflicts
    }
    
    // : Check for field mode state changes
    handleFieldModeTransitions();
    
    // Handle settings menu (highest priority) - UNCHANGED
    if (menuState.settingsMenuActive) {
        handleSettingsMenu(display, menuState, settings, rtc, currentData, systemStatus);
        return; // Skip everything else when in menu
    }

    // Update Bluetooth manager
    bluetoothManager.update();
    
    // : Field mode operation logic (NO DISPLAY TIMEOUT)
    if (powerManager.isFieldModeActive()) {
        handleFieldModeOperation(currentTime);
        return; // Field mode has its own loop logic
    }
    
    // Handle main navigation buttons - UNCHANGED (with activity tracking)
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
    
    // Force display update on button press - UNCHANGED
    if (buttonPressed) {
        Serial.print(F("Mode changed to: "));
        Serial.println(currentMode);
        updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc,
                      currentSpectralFeatures, currentActivityTrend);
        lastDisplayUpdate = currentTime;
    }
    
    // : Normal (Testing) mode operation
    handleTestingModeOperation(currentTime);
}

// : Enhanced function to handle field mode transitions
void handleFieldModeTransitions() {
    bool fieldModeNow = powerManager.isFieldModeActive();
    
    // Detect field mode activation
    if (!wasInFieldMode && fieldModeNow) {
        Serial.println(F("\n=== FIELD MODE ACTIVATED ==="));
        Serial.print(F("Log interval: "));
        Serial.print(settings.logInterval);
        Serial.println(F(" minutes"));
        Serial.println(F("Display stays on - timeout disabled in "));
        Serial.println(F("Press any button to see current data"));
        Serial.println(F("Use Settings menu to exit field mode"));
        Serial.println(F("================================\n"));
        
        fieldModeStartTime = millis();
        fieldBuffer.clearBuffer();
        wasInFieldMode = true;
    }
    
    // Detect field mode deactivation
    if (wasInFieldMode && !fieldModeNow) {
        Serial.println(F("\n=== FIELD MODE DEACTIVATED ==="));
        Serial.print(F("Field mode duration: "));
        Serial.print((millis() - fieldModeStartTime) / 60000);
        Serial.println(F(" minutes"));
        
        // Flush any remaining buffer data
        if (fieldBuffer.getBufferCount() > 0) {
            Serial.println(F("Flushing remaining buffer data..."));
            fieldBuffer.flushToSD(rtc, systemStatus);
        }
        
        Serial.println(F("Returning to Testing Mode"));
        Serial.println(F("==============================\n"));
        
        wasInFieldMode = false;
    }
}

// : Field mode operation logic 
void handleFieldModeOperation(unsigned long currentTime) {
    // Check for long press wake when display is off
    if (!powerManager.isDisplayOn()) {
        if (powerManager.checkForLongPressWake()) {
            resetButtonStates();
            return; // System is now awake, let next loop handle normal interaction
        }
        // When display is off, ignore all other button presses
        resetButtonStates(); // Clear any normal presses
    } else {
        // Display is on - handle normal button presses for navigation/interaction
        if (wasButtonPressed(0) || wasButtonPressed(1) || wasButtonPressed(2) || wasButtonPressed(3)) {
            Serial.println(F("Button pressed in field mode - user interaction"));
            powerManager.handleUserActivity(); // Resets timeout
            
            updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc,
                          currentSpectralFeatures, currentActivityTrend);
            
            resetButtonStates();
            return; // Let main loop handle menu navigation normally
        }
    }
    
    // Skip sensor readings if display is still on (during countdown period)
    if (powerManager.isDisplayOn()) {
        // During countdown: just update display with current (stale) data, no sensor reads
        if (currentTime - lastDisplayUpdate >= 5000) { // Every 5 seconds
            updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc,
                          currentSpectralFeatures, currentActivityTrend);
            lastDisplayUpdate = currentTime;
        }
        return; // Skip all sensor reading logic below
    }
    
    // Check if it's time to take a reading
    if (powerManager.shouldTakeReading()) {
        Serial.println(F("\n--- Field Mode: Taking Scheduled Reading ---"));
        
        // Power up sensors for this reading
        powerManager.powerUpSensors();
        delay(100); // Small delay for sensors to stabilize
        
        // Read all sensors
        readAllSensors(bme, currentData, settings, systemStatus);
        checkAlerts(currentData, settings, systemStatus);
        
        // Log sensor data
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
            processAudio(currentData, settings);
            if (audioSampleIndex > 0) {
                AudioAnalysis analysis = analyzeAudioBuffer();
                currentData.dominantFreq = analysis.dominantFreq;
                currentData.soundLevel = analysis.soundLevel;
                currentData.beeState = classifyBeeState(analysis, settings);
                audioSampleIndex = 0;
            }
        }
        
        // Add to buffer instead of logging directly
        if (systemStatus.rtcWorking) {
            uint32_t timestamp = rtc.now().unixtime();
            if (fieldBuffer.addReading(currentData, timestamp)) {
                Serial.print(F("Added to buffer ("));
                Serial.print(fieldBuffer.getBufferCount());
                Serial.println(F(" readings)"));
            } else {
                Serial.println(F("Buffer full - flushing to SD"));
                fieldBuffer.flushToSD(rtc, systemStatus);
                fieldBuffer.addReading(currentData, timestamp);
            }
        }
        
        // Check if it's time to flush buffer (every hour or when full)
        if (powerManager.isTimeForBufferFlush() || fieldBuffer.isBufferFull()) {
            Serial.println(F("Flushing buffer to SD..."));
            fieldBuffer.flushToSD(rtc, systemStatus);
        }
        
        // Update display with current data
        updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc,
                      currentSpectralFeatures, currentActivityTrend);
        
        // Set next wake time
        powerManager.updateNextWakeTime(settings.logInterval);
        
        Serial.print(F("Next reading in "));
        Serial.print(settings.logInterval);
        Serial.println(F(" minutes"));
        Serial.println(F("--------------------------------\n"));

        // Power down sensors again until next reading
        powerManager.powerDownSensors();
        powerManager.powerDownAudio();
        Serial.println(F("Sensors powered down until next reading"));
    }
  
    // Update display regularly in field mode (display always on)
    if (currentTime - lastDisplayUpdate >= 5000) { // Every 5 seconds
        updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc,
                      currentSpectralFeatures, currentActivityTrend);
        lastDisplayUpdate = currentTime;
    }
}

// : Testing mode operation (normal operation) - Enhanced
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
    
    // : Update Power Manager every 5 seconds
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
            
            lastPowerDebug = currentTime;
        }
    }
    
    // Process audio if microphone is present - UNCHANGED
    if (systemStatus.pdmWorking && (currentTime % 200 == 0)) { // Every 200ms
        processAudio(currentData, settings);
        
        if (audioSampleIndex > 0) {
            currentSpectralFeatures = analyzeAudioFFT();
            AudioAnalysis analysis = analyzeAudioBuffer();
            currentData.dominantFreq = analysis.dominantFreq;
            currentData.soundLevel = analysis.soundLevel;
            currentData.beeState = classifyBeeState(analysis, settings);
            audioSampleIndex = 0; // Reset buffer
        }
    }
    
    // Log data if enabled and SD is working - UNCHANGED
    if (settings.logEnabled && systemStatus.sdWorking && systemStatus.rtcWorking) {
        unsigned long logIntervalMs = settings.logInterval * 60000UL;
        if (currentTime - lastLogTime >= logIntervalMs) {
            logData(currentData, rtc, settings, systemStatus);
            lastLogTime = currentTime;
            Serial.println(F("Data logged to SD"));
        }
    }
    
    // Update display every second (unless button was just pressed) - UNCHANGED
    if (currentTime - lastDisplayUpdate >= 1000) {
        updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc,
                      currentSpectralFeatures, currentActivityTrend);
        lastDisplayUpdate = currentTime;
    }
    
    // Check for factory reset (BACK + SELECT held together) - UNCHANGED
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


// : Field sleep implementation 
void enterFieldSleep() {
    Serial.print(F("Field sleep for "));
    Serial.print(settings.logInterval);
    Serial.println(F(" minutes..."));
    
    // Simple delay-based sleep
    powerManager.powerDownNonEssential();
    
    unsigned long sleepTimeMs = settings.logInterval * 60000UL;
    unsigned long sleepStart = millis();
    unsigned long lastSleepUpdate = sleepStart;
    
    Serial.println(F("Entering field sleep mode..."));
    Serial.println(F("Press any button to wake and see data"));
    
    while (millis() - sleepStart < sleepTimeMs) {
        // Check for button press to wake early
        updateButtonStates();
        if (wasButtonPressed(0) || wasButtonPressed(1) || wasButtonPressed(2) || wasButtonPressed(3)) {
            Serial.println(F("Sleep interrupted by button press"));
            powerManager.setWakeSource(false); // Woken by button
            powerManager.handleUserActivity(); 
            break;
        }
        
        // Show sleep progress every 30 seconds
        if (millis() - lastSleepUpdate >= 30000) {
            unsigned long elapsed = millis() - sleepStart;
            unsigned long remaining = sleepTimeMs - elapsed;
            Serial.print(F("Sleep: "));
            Serial.print(remaining / 60000);
            Serial.print(F("m "));
            Serial.print((remaining % 60000) / 1000);
            Serial.println(F("s remaining"));
            lastSleepUpdate = millis();
        }
        
        delay(100); // Small delay to prevent busy-waiting
    }
    
    // Wake up
    Serial.println(F("Waking from field sleep"));
    powerManager.powerUpAll();
    powerManager.setWakeSource(true); // Woken by timer (if not button)
}