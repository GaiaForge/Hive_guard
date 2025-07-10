/**
 * HiveMonitor.ino - UPDATED with PowerManager Integration and DFU Support
 * Main file for Tanzania Hive Monitor System
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


#ifdef NRF52_SERIES
#include <nrf.h>
#include <nrf_power.h>
#endif

using namespace Adafruit_LittleFS_Namespace;

// Function declarations
void checkForFactoryReset();
void showResetProgress(Adafruit_SH1106G& display, unsigned long holdTime);
void checkForPreviousReset();
bool confirmFactoryReset(Adafruit_SH1106G& display);
void enterDFUMode();
bool checkForRemoteDFURequest();
void handleDFUCommand(uint8_t* data, uint16_t len);
void checkDFURequests();

// Display
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Sensors
Adafruit_BME280 bme;
RTC_PCF8523 rtc;

// System settings and status
SystemSettings settings;
SystemStatus systemStatus = {
    false, false, false, false, false, false
};

// Current sensor data
SensorData currentData;

// Display mode and menu
DisplayMode currentMode = MODE_DASHBOARD;
MenuState menuState = {
    false, 0, 0, 0, 0.0, 0
};

// Power Manager & Bluetooth manager
PowerManager powerManager;
BluetoothManager bluetoothManager;

// Audio processing
SpectralFeatures currentSpectralFeatures;
ActivityTrend currentActivityTrend;

// Timing
unsigned long lastSensorRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastButtonCheck = 0;
unsigned long lastLogTime = 0;
unsigned long lastAudioSample = 0;

void setup() {
    Serial.begin(115200);

    while (!Serial && millis() < 3000);
    Serial.println(F("=== HiveGuard System Starting ==="));
    Serial.println(F("About to initialize Bluetooth..."));
    
    delay(1000);
    
    // Load settings first
    loadSettings(settings);
    
    // Initialize I2C
    Wire.begin();
    Wire.setClock(100000);
    
    // Initialize display and show splash screen
    if (display.begin(SCREEN_ADDRESS, true)) {
        systemStatus.displayWorking = true;
        showStartupScreen(display);
        delay(2000);
    }
    
    // Show sensor diagnostics screen
    showSensorDiagnosticsScreen(display, systemStatus);
    
    // Initialize RTC - UPDATED for PCF8523
    Serial.print(F("Initializing RTC..."));
    if (rtc.begin()) {
        systemStatus.rtcWorking = true;
        Serial.println(F(" OK"));
        updateDiagnosticLine(display, "RTC: OK");
        
        // PCF8523 specific initialization
        if (rtc.lostPower()) {
            Serial.println(F("RTC lost power, setting time!"));
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        
        // PCF8523 specific: Start the RTC if not running
        if (!rtc.initialized() || rtc.lostPower()) {
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            // Enable the oscillator
            rtc.start();
        }
        
    } else {
        Serial.println(F(" FAILED"));
        updateDiagnosticLine(display, "RTC: FAILED");
    }
    delay(500);
    
    // Initialize sensors
    Serial.print(F("Initializing BME280..."));
    initializeSensors(bme, systemStatus);
    if (systemStatus.bmeWorking) {
        updateDiagnosticLine(display, "BME280: OK");
    } else {
        updateDiagnosticLine(display, "BME280: FAILED");
    }
    delay(500);
    
    // Initialize audio
    Serial.print(F("Initializing Audio..."));
    initializeAudio(systemStatus);
    if (systemStatus.pdmWorking) {
        updateDiagnosticLine(display, "Audio: OK");
    } else {
        updateDiagnosticLine(display, "Audio: FAILED");
    }
    delay(500);
    currentSpectralFeatures = {0};
    currentActivityTrend = {0};
    
    // Initialize buttons
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);
    pinMode(BTN_BACK, INPUT_PULLUP);
    updateDiagnosticLine(display, "Buttons: OK");
    delay(500);
    
    // Initialize SPI for SD card
    SPI.begin();
    
    // Check SD card
    Serial.print(F("Checking SD card..."));
    checkSDCardAtStartup(display, systemStatus);

    // Check for previous factory reset
    if (systemStatus.sdWorking) {
        checkForPreviousReset();
    }
    
    // INITIALIZE POWER MANAGER - 
    Serial.print(F("Initializing Power Manager..."));
    powerManager.initialize(&systemStatus, &settings);
    updateDiagnosticLine(display, "PowerMgr: OK");
    delay(500);

    // INITIALIZE BLUETOOTH MANAGER -
    Serial.println(F("=== Initializing Bluetooth Manager ==="));
    bluetoothManager.initialize(&systemStatus, &settings);
    updateDiagnosticLine(display, "Bluetooth: Initializing...");
    delay(1000);

    // CHECK BLUETOOTH STATUS 
    Serial.print(F("Bluetooth Mode: "));
    Serial.println(bluetoothModeToString(bluetoothManager.getSettings().mode));
    Serial.print(F("Bluetooth Status: "));
    Serial.println(bluetoothStatusToString(bluetoothManager.getStatus()));
    Serial.print(F("Should be discoverable: "));
    Serial.println(bluetoothManager.shouldBeDiscoverable() ? "YES" : "NO");

    updateDiagnosticLine(display, "Bluetooth: Checking...");
    delay(2000);

    Serial.println(F("Bluetooth initialization complete"));
    bluetoothManager.printBluetoothStatus(); // This should show current status

    // Watchdog setup
    setupWatchdog(settings);
    
    Serial.println(F("=== System Ready - Watchdog Active ==="));
    
    // Show final status
    if (systemStatus.sdWorking) {
        Serial.println(F("System ready with SD logging!"));
        updateDiagnosticLine(display, "System: READY + SD");
    } else {
        Serial.println(F("System ready (no SD logging)"));
        updateDiagnosticLine(display, "System: READY (no SD)");
    }
    
    delay(2000);
    systemStatus.systemReady = true;
    
    // Take initial sensor reading and show dashboard
    readAllSensors(bme, currentData, settings, systemStatus);
    updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc, 
              currentSpectralFeatures, currentActivityTrend);
}

void loop() {
    unsigned long currentTime = millis();
    
    // FEED WATCHDOG AT START OF EACH LOOP ITERATION
    feedWatchdog();

    bluetoothManager.update();
    
    // Check for DFU requests periodically
    checkDFURequests();
    
    // UPDATE POWER MANAGER with current battery voltage
    static unsigned long lastPowerUpdate = 0;
    if (currentTime - lastPowerUpdate >= 10000) { // Update every 10 seconds
        powerManager.updatePowerMode(currentData.batteryVoltage);
        lastPowerUpdate = currentTime;
        feedWatchdog(); // Feed after power manager update
    }
    powerManager.update();
    
    // Periodic SD card check if not working
    static unsigned long lastSDCheck = 0;
    if (!systemStatus.sdWorking && (currentTime - lastSDCheck >= 10000)) {
        feedWatchdog(); // Feed before potentially slow SD operations
        checkSDCard(systemStatus);
        if (systemStatus.sdWorking) {
            Serial.println(F("SD card detected and initialized!"));
            createLogFile(rtc, systemStatus);
            feedWatchdog(); // Feed after SD card recovery
        }
        lastSDCheck = currentTime;
    }
    
    // Update button states
    updateButtonStates();
    
    // HANDLE USER ACTIVITY FOR POWER MANAGER
    if (wasButtonPressed(0) || wasButtonPressed(1) || 
        wasButtonPressed(2) || wasButtonPressed(3)) {
        powerManager.handleUserActivity();
        powerManager.setWakeSource(false); // Woken by button
        feedWatchdog(); // Feed after user interaction
    }
    
    // Handle button presses for main navigation (only if display is on)
    if (!menuState.settingsMenuActive && powerManager.isDisplayOn()) {
        if (wasButtonPressed(0)) { // UP
            if (currentMode > 0) {
                currentMode = (DisplayMode)(currentMode - 1);
            } else {
                currentMode = MODE_POWER;
            }
            updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc, 
              currentSpectralFeatures, currentActivityTrend);
            feedWatchdog(); // Feed after display update
        }
        
        if (wasButtonPressed(1)) { // DOWN
            if (currentMode < MODE_POWER) {
                currentMode = (DisplayMode)(currentMode + 1);
            } else {
                currentMode = MODE_DASHBOARD;
            }
            updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc, 
              currentSpectralFeatures, currentActivityTrend);
            feedWatchdog(); // Feed after display update
        }
        
        if (wasButtonPressed(2)) { // SELECT
            if (currentMode == MODE_DASHBOARD) {
                menuState.settingsMenuActive = true;
                menuState.menuLevel = 0;
                menuState.selectedItem = 0;
                resetButtonStates();
                feedWatchdog(); // Feed after menu activation
            }
        }
        
        if (wasButtonPressed(3)) { // BACK
            currentMode = MODE_DASHBOARD;
            updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc, 
              currentSpectralFeatures, currentActivityTrend);
            feedWatchdog(); // Feed after display update
        }
    }

    // Handle settings menu if active (always allow menu access)
    if (menuState.settingsMenuActive) {
        feedWatchdog(); // Feed before menu operations
        handleSettingsMenu(display, menuState, settings, rtc, currentData, systemStatus);
        feedWatchdog(); // Feed after menu operations
        return;
    }
    
    // FIELD MODE SLEEP/WAKE CYCLE
    if (powerManager.isFieldModeActive()) {
        // Check if it's time to take a reading
        if (powerManager.shouldTakeReading()) {
            Serial.println(F("Field mode: Taking scheduled reading"));
            feedWatchdog(); // Feed before long field operation
            
            // Take readings
            readAllSensors(bme, currentData, settings, systemStatus);
            feedWatchdog(); // Feed after sensor reading
            
            // Process audio for 1 second
            if (systemStatus.pdmWorking) {
                unsigned long audioStart = millis();
                unsigned long lastWatchdogFeed = millis();
                
                while (millis() - audioStart < 1000) {
                    processAudio(currentData, settings);
                    
                    // Feed watchdog every 200ms during audio processing
                    if (millis() - lastWatchdogFeed >= 200) {
                        feedWatchdog();
                        lastWatchdogFeed = millis();
                    }
                    
                    delay(10);
                }
                
                // Analyze the collected audio
                if (audioSampleIndex > 0) {
                    currentSpectralFeatures = analyzeAudioFFT();
                    AudioAnalysis analysis = analyzeAudioBuffer();
                    currentData.dominantFreq = analysis.dominantFreq;
                    currentData.soundLevel = analysis.soundLevel;
                    currentData.beeState = classifyBeeState(analysis, settings);
                    audioSampleIndex = 0; // Reset buffer
                }
                feedWatchdog(); // Feed after audio analysis
            }
            
            // Check alerts
            checkAlerts(currentData, settings, systemStatus);
            
            // Add to buffer instead of writing directly
            uint32_t timestamp = systemStatus.rtcWorking ? rtc.now().unixtime() : millis()/1000;
            fieldBuffer.addReading(currentData, timestamp);
            
            // Update next wake time
            powerManager.updateNextWakeTime(settings.logInterval);
            
            // Check if we should flush buffer to SD (hourly or when full)
            if (fieldBuffer.getBufferCount() >= 12 || // Buffer full
                (millis() - fieldBuffer.getBuffer().lastFlushTime) >= 3600000UL) { // 1 hour
                
                Serial.println(F("Field mode: Flushing buffer to SD"));
                feedWatchdog(); // Feed before SD operations
                fieldBuffer.flushToSD(rtc, systemStatus);
                feedWatchdog(); // Feed after SD operations
            }
            
            // Mark that we were woken by timer for next sleep
            powerManager.setWakeSource(true);
            feedWatchdog(); // Feed after field mode reading complete
        }
        
        // Check if we should go back to sleep
        if (powerManager.shouldEnterSleep()) {
            unsigned long timeUntilNextReading = settings.logInterval * 60000UL;
            
            Serial.print(F("Field mode: Sleeping for "));
            Serial.print(timeUntilNextReading / 60000);
            Serial.println(F(" minutes"));
            
            // Prepare for sleep
            powerManager.prepareSleep();
            
            // DON'T feed watchdog before sleep - let it wake us if sleep fails
            Serial.println(F("Entering sleep - watchdog will wake us if needed"));
            powerManager.enterDeepSleep(timeUntilNextReading);
            
            // When we wake up, we'll be at the start of loop() again
            powerManager.setWakeSource(true);
            powerManager.wakeFromSleep();
            
            // Feed watchdog immediately after wake
            feedWatchdog();
            Serial.println(F("Woke from field mode sleep"));
        }
    } else {
        // TESTING MODE - Normal operation
        
        // Read sensors at regular intervals
        if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
            readAllSensors(bme, currentData, settings, systemStatus);
            checkAlerts(currentData, settings, systemStatus);
            lastSensorRead = currentTime;
            feedWatchdog(); // Feed after sensor reading
        }
        
        // Audio sampling (only if not in power save mode)
        if (systemStatus.pdmWorking && 
            powerManager.getCurrentPowerMode() != POWER_CRITICAL &&
            (currentTime - lastAudioSample >= AUDIO_INTERVAL)) {
            
            processAudio(currentData, settings);
            
            // Add new spectral analysis (with safety check)
            if (audioSampleIndex > 0) {
                currentSpectralFeatures = analyzeAudioFFT();
                
                // Update activity trend (need current hour)
                if (systemStatus.rtcWorking) {
                    DateTime now = rtc.now();
                    updateActivityTrend(currentActivityTrend, currentSpectralFeatures, now.hour());
                }
                
                feedWatchdog(); // Feed after audio processing
            }
            
            lastAudioSample = currentTime;
        }
        
        // Data logging (only if SD card is working) - Testing mode logs directly
        if (settings.logEnabled && systemStatus.sdWorking) {
            unsigned long logIntervalMs = settings.logInterval * 60000UL;
            if (currentTime - lastLogTime >= logIntervalMs) {
                feedWatchdog(); // Feed before SD write
                logData(currentData, rtc, settings, systemStatus);
                lastLogTime = currentTime;
                feedWatchdog(); // Feed after SD write
            }
        }
    }
    
    // Update display (only if display is on and working)
    if (systemStatus.displayWorking && powerManager.isDisplayOn() && 
        (currentTime - lastDisplayUpdate >= DISPLAY_INTERVAL)) {
        updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc,
                    currentSpectralFeatures, currentActivityTrend);
        lastDisplayUpdate = currentTime;
        feedWatchdog(); // Feed after display update
    }
    
    // System health monitoring (includes watchdog safety checks)
    checkSystemHealth(systemStatus, currentData);

    // Check for factory reset combination
    if (!menuState.settingsMenuActive) {
        checkForFactoryReset();
        feedWatchdog(); // Feed after factory reset check
    }
    
    // FINAL WATCHDOG FEED AT END OF LOOP
    feedWatchdog();
    
    delay(10);
}
// =============================================================================
// DFU (DEVICE FIRMWARE UPDATE) FUNCTIONS
// =============================================================================

void enterDFUMode() {
    Serial.println(F("Entering DFU mode for firmware update..."));
    
    // Show DFU message on display
    if (systemStatus.displayWorking) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(15, 20);
        display.println(F("FIRMWARE UPDATE"));
        display.setCursor(10, 30);
        display.println(F("Connect via Bluetooth"));
        display.setCursor(20, 40);
        display.println(F("Please wait..."));
        display.display();
        delay(2000);
    }
    
    // Save current state before DFU
    if (systemStatus.sdWorking) {
        // Flush any pending data
        if (hasBufferedData()) {
            flushBufferedData(rtc, settings, systemStatus);
        }
        
        // Log DFU entry (need to add this event type to DataStructures.h)
        // logFieldEvent(EVENT_SYSTEM_UPDATE, rtc, systemStatus);
        
        // Create DFU marker file
        SDLib::File dfuMarker = SD.open("/dfu_started.txt", FILE_WRITE);
        if (dfuMarker) {
            dfuMarker.print(F("DFU started at: "));
            dfuMarker.println(millis());
            dfuMarker.close();
        }
    }
    
#ifdef NRF52_SERIES
    // Set the magic number to enter DFU mode on reset
    NRF_POWER->GPREGRET = 0xA8; // DFU mode magic number
    
    // Reset the system to enter DFU bootloader
    NVIC_SystemReset();
#else
    Serial.println(F("DFU not supported on this platform"));
#endif
}

bool checkForRemoteDFURequest() {
    // Check for DFU request file on SD card
    if (systemStatus.sdWorking && SD.exists("/dfu_request.txt")) {
        Serial.println(F("DFU request file found"));
        
        // Read the file to get details
        SDLib::File dfuFile = SD.open("/dfu_request.txt", FILE_READ);
        if (dfuFile) {
            String request = dfuFile.readString();
            dfuFile.close();
            
            // Remove the request file
            SD.remove("/dfu_request.txt");
            
            // Check if request is valid (you can add authentication here)
            if (request.indexOf("CONFIRMED_DFU") >= 0) {
                return true;
            }
        }
    }
    
    // Check for special button combination for DFU
    static unsigned long dfuHoldStart = 0;
    static bool dfuInProgress = false;
    
    // UP + DOWN + SELECT held together for 3 seconds = DFU mode
    if (isButtonHeld(0) && isButtonHeld(1) && isButtonHeld(2)) {
        if (dfuHoldStart == 0) {
            dfuHoldStart = millis();
            dfuInProgress = false;
        }
        
        unsigned long holdTime = millis() - dfuHoldStart;
        
        // Show DFU progress after 1 second
        if (holdTime > 1000 && !dfuInProgress) {
            dfuInProgress = true;
            
            if (systemStatus.displayWorking) {
                display.clearDisplay();
                display.setTextSize(1);
                display.setTextColor(SH110X_WHITE);
                display.setCursor(20, 20);
                display.println(F("DFU MODE"));
                display.setCursor(5, 30);
                display.println(F("Hold 2 more seconds"));
                display.display();
            }
        }
        
        // Enter DFU after 3 seconds
        if (holdTime > 3000) {
            dfuHoldStart = 0;
            dfuInProgress = false;
            return true;
        }
    } else {
        dfuHoldStart = 0;
        dfuInProgress = false;
    }
    
    return false;
}

void handleDFUCommand(uint8_t* data, uint16_t len) {
    if (len >= 4) {
        // Check for DFU command signature: 0xDF, 0xU0, 0x20, 0x25
        // Note: 0xU0 is not valid hex, so using 0xF0 instead
        if (data[0] == 0xDF && data[1] == 0xF0 && data[2] == 0x20 && data[3] == 0x25) {
            Serial.println(F("Remote DFU command received"));
            
            // Optional: Add authentication here
            // For now, we'll allow it immediately
            
            // Enter DFU mode
            enterDFUMode();
        }
    }
}

void checkDFURequests() {
    static unsigned long lastDFUCheck = 0;
    
    // Check every 30 seconds
    if (millis() - lastDFUCheck >= 30000) {
        lastDFUCheck = millis();
        
        if (checkForRemoteDFURequest()) {
            Serial.println(F("Remote DFU request confirmed"));
            enterDFUMode();
        }
    }
}

// =============================================================================
// STARTUP RESET CHECK
// =============================================================================

void checkForPreviousReset() {
    if (SD.exists("/factory_reset_performed.txt")) {
        Serial.println(F("Previous factory reset detected"));
        
        // Optionally show message on display
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SH110X_WHITE);
        display.setCursor(10, 20);
        display.println(F("Factory reset"));
        display.setCursor(15, 30);
        display.println(F("completed"));
        display.setCursor(5, 40);
        display.println(F("Settings restored"));
        display.display();
        delay(2000);
        
        // Remove the marker file
        SD.remove("/factory_reset_performed.txt");
    }
}

// =============================================================================
// FACTORY RESET DETECTION 
// =============================================================================

void checkForFactoryReset() {
    static unsigned long resetHoldStart = 0;
    static bool resetInProgress = false;
    
    // Check for button combination: BACK + SELECT held together
    if (isButtonHeld(2) && isButtonHeld(3)) { // SELECT + BACK
        if (resetHoldStart == 0) {
            resetHoldStart = millis();
            resetInProgress = false;
        }
        
        unsigned long holdTime = millis() - resetHoldStart;
        
        // Show progress on display after 2 seconds
        if (holdTime > 2000 && !resetInProgress) {
            resetInProgress = true;
            showResetProgress(display, holdTime);
        }
        
        // Trigger reset after 5 seconds
        if (holdTime > 5000) {
            if (confirmFactoryReset(display)) {
                performFactoryReset(settings, systemStatus, display);
            }
            resetHoldStart = 0;
            resetInProgress = false;
        }
        
    } else {
        resetHoldStart = 0;
        resetInProgress = false;
    }
}

void showResetProgress(Adafruit_SH1106G& display, unsigned long holdTime) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    display.setCursor(25, 0);
    display.println(F("Factory Reset"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    display.setCursor(10, 20);
    display.println(F("Hold for 5 seconds"));
    
    // Progress bar
    unsigned long progress = holdTime - 2000; // Start from 2 second mark
    int barWidth = map(progress, 0, 3000, 0, 100); // 3 seconds to fill
    
    display.drawRect(10, 35, 108, 10, SH110X_WHITE);
    if (barWidth > 0) {
        display.fillRect(11, 36, barWidth, 8, SH110X_WHITE);
    }
    
    display.setCursor(15, 50);
    display.println(F("Release to cancel"));
    
    display.display();
}

// =============================================================================
// FACTORY RESET CONFIRMATION AND EXECUTION
// =============================================================================

bool confirmFactoryReset(Adafruit_SH1106G& display) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    // Warning screen
    display.setCursor(15, 0);
    display.println(F("FACTORY RESET"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    display.setCursor(0, 16);
    display.println(F("This will erase ALL"));
    display.setCursor(0, 26);
    display.println(F("custom settings and"));
    display.setCursor(0, 36);
    display.println(F("restore defaults."));
    
    display.setCursor(0, 50);
    display.println(F("UP:Cancel DOWN:Reset"));
    
    display.display();
    
    // Wait for user choice
    unsigned long startTime = millis();
    while (millis() - startTime < 10000) { // 10 second timeout
        updateButtonStates();
        
        if (wasButtonPressed(0)) { // UP - Cancel
            return false;
        }
        
        if (wasButtonPressed(1)) { // DOWN - Confirm
            return true;
        }
        
        delay(50);
    }
    
    return false; // Timeout = cancel
}