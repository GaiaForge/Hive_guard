/**
 * HiveMonitor.ino - UPDATED with PowerManager Integration
 * Main file for Kenya Hive Monitor System
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

// Function declarations
void checkForFactoryReset();
void showResetProgress(Adafruit_SH1106G& display, unsigned long holdTime);
void checkForPreviousReset();
bool confirmFactoryReset(Adafruit_SH1106G& display);

// Display
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Sensors
Adafruit_BME280 bme;
RTC_DS3231 rtc;

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

// Power Manager 
PowerManager powerManager;

// Timing
unsigned long lastSensorRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastButtonCheck = 0;
unsigned long lastLogTime = 0;
unsigned long lastAudioSample = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    Serial.println(F("=== Kenya Hive Monitor v2.0 ==="));
    
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
    
    // Initialize RTC
    Serial.print(F("Initializing RTC..."));
    if (rtc.begin()) {
        systemStatus.rtcWorking = true;
        Serial.println(F(" OK"));
        updateDiagnosticLine(display, "RTC: OK");
        
        if (rtc.lostPower()) {
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            Serial.println(F("RTC time reset"));
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
    
    // INITIALIZE POWER MANAGER - NEW
    Serial.print(F("Initializing Power Manager..."));
    powerManager.initialize(&systemStatus, &settings);
    updateDiagnosticLine(display, "PowerMgr: OK");
    delay(500);
    
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
    updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc);
}


void loop() {
    unsigned long currentTime = millis();
    
    // UPDATE POWER MANAGER with current battery voltage - FIXED
    static unsigned long lastPowerUpdate = 0;
    if (currentTime - lastPowerUpdate >= 10000) { // Update every 10 seconds
        powerManager.updatePowerMode(currentData.batteryVoltage); // Pass actual voltage
        lastPowerUpdate = currentTime;
    }
    powerManager.update();
    
    // Periodic SD card check if not working
    static unsigned long lastSDCheck = 0;
    if (!systemStatus.sdWorking && (currentTime - lastSDCheck >= 10000)) {
        checkSDCard(systemStatus);
        if (systemStatus.sdWorking) {
            Serial.println(F("SD card detected and initialized!"));
            createLogFile(rtc, systemStatus);
        }
        lastSDCheck = currentTime;
    }
    
    // Update button states
    updateButtonStates();
    
    // HANDLE USER ACTIVITY FOR POWER MANAGER
    if (wasButtonPressed(0) || wasButtonPressed(1) || 
        wasButtonPressed(2) || wasButtonPressed(3)) {
        powerManager.handleUserActivity();
    }
    
    // Handle button presses for main navigation (only if display is on)
    if (!menuState.settingsMenuActive && powerManager.isDisplayOn()) {
        if (wasButtonPressed(0)) { // UP
            if (currentMode > 0) {
                currentMode = (DisplayMode)(currentMode - 1);
            } else {
                currentMode = MODE_POWER;
            }
            updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc);
        }
        
        if (wasButtonPressed(1)) { // DOWN
            if (currentMode < MODE_POWER) {
                currentMode = (DisplayMode)(currentMode + 1);
            } else {
                currentMode = MODE_DASHBOARD;
            }
            updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc);
        }
        
        if (wasButtonPressed(2)) { // SELECT
            if (currentMode == MODE_DASHBOARD) {
                menuState.settingsMenuActive = true;
                menuState.menuLevel = 0;
                menuState.selectedItem = 0;
                resetButtonStates();
            }
        }
        
        if (wasButtonPressed(3)) { // BACK
            currentMode = MODE_DASHBOARD;
            updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc);
        }
    }

    // Handle settings menu if active (always allow menu access)
    if (menuState.settingsMenuActive) {
        handleSettingsMenu(display, menuState, settings, rtc, currentData, systemStatus);
        return;
    }
    
    // Read sensors
    if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
        readAllSensors(bme, currentData, settings, systemStatus);
        checkAlerts(currentData, settings, systemStatus);
        lastSensorRead = currentTime;
    }
    
    // Audio sampling (only if not in power save mode)
    if (systemStatus.pdmWorking && 
        powerManager.getCurrentPowerMode() != POWER_CRITICAL &&
        (currentTime - lastAudioSample >= AUDIO_INTERVAL)) {
        processAudio(currentData, settings);
        lastAudioSample = currentTime;
    }
    
    // Data logging (only if SD card is working)
    if (settings.logEnabled && systemStatus.sdWorking) {
        unsigned long logIntervalMs = settings.logInterval * 60000UL;
        if (currentTime - lastLogTime >= logIntervalMs) {
            logData(currentData, rtc, settings, systemStatus);
            lastLogTime = currentTime;
        }
    }
    
    // Update display (only if display is on and working)
    if (systemStatus.displayWorking && powerManager.isDisplayOn() && 
        (currentTime - lastDisplayUpdate >= DISPLAY_INTERVAL)) {
        updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc);
        lastDisplayUpdate = currentTime;
    }
    
    // CHECK FOR SLEEP CONDITIONS 
    if (powerManager.shouldEnterSleep()) {
        unsigned long sleepTime = settings.logInterval * 60000UL;
        Serial.print(F("Entering field sleep for "));
        Serial.print(sleepTime / 60000);
        Serial.println(F(" minutes"));
        
        powerManager.prepareSleep();
        powerManager.enterDeepSleep(sleepTime);
        powerManager.wakeFromSleep();
        
        // Force sensor reading after wake
        readAllSensors(bme, currentData, settings, systemStatus);
        checkAlerts(currentData, settings, systemStatus);
        
        // Log data after wake
        if (settings.logEnabled && systemStatus.sdWorking) {
            logData(currentData, rtc, settings, systemStatus);
            lastLogTime = currentTime;
        }
    }

    // Check for factory reset combination
    if (!menuState.settingsMenuActive) {
        checkForFactoryReset();
    }
    
    delay(10);
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
        display.setTextColor(SH1106_WHITE);
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
    display.setTextColor(SH1106_WHITE);
    
    display.setCursor(25, 0);
    display.println(F("Factory Reset"));
    display.drawLine(0, 10, 127, 10, SH1106_WHITE);
    
    display.setCursor(10, 20);
    display.println(F("Hold for 5 seconds"));
    
    // Progress bar
    unsigned long progress = holdTime - 2000; // Start from 2 second mark
    int barWidth = map(progress, 0, 3000, 0, 100); // 3 seconds to fill
    
    display.drawRect(10, 35, 108, 10, SH1106_WHITE);
    if (barWidth > 0) {
        display.fillRect(11, 36, barWidth, 8, SH1106_WHITE);
    }
    
    display.setCursor(15, 50);
    display.println(F("Release to cancel"));
    
    display.display();
}

// Add these functions to the end of your HiveMonitor.ino file:

// =============================================================================
// FACTORY RESET CONFIRMATION AND EXECUTION
// =============================================================================

bool confirmFactoryReset(Adafruit_SH1106G& display) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH1106_WHITE);
    
    // Warning screen
    display.setCursor(15, 0);
    display.println(F("FACTORY RESET"));
    display.drawLine(0, 10, 127, 10, SH1106_WHITE);
    
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

