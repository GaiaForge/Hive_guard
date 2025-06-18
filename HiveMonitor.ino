/**
 * HiveMonitor.ino
 * Main file for Kenya Hive Monitor System
 * Adafruit Feather Sense nRF52840
 * Version 2.0
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

// Display
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Sensors
Adafruit_BME280 bme;

RTC_DS3231 rtc;

// System settings
SystemSettings settings;

// System status
SystemStatus systemStatus = {
    false, false, false, false, false, false, false
};


// Current sensor data
SensorData currentData;

// Display mode
DisplayMode currentMode = MODE_DASHBOARD;

// Menu state
MenuState menuState = {
    false, 0, 0, 0, 0.0, 0
};

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
        delay(2000); // Show splash for 2 seconds
    }
    
    // Now show sensor diagnostics screen
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
    
    // Show final status
    if (systemStatus.sdWorking) {
        Serial.println(F("System ready with SD logging!"));
        updateDiagnosticLine(display, "System: READY + SD");
    } else {
        Serial.println(F("System ready (no SD logging)"));
        updateDiagnosticLine(display, "System: READY (no SD)");
    }
    
    delay(2000); // Show final status
    systemStatus.systemReady = true;
    
    // Take initial sensor reading and show dashboard
    readAllSensors(bme, currentData, settings, systemStatus);
    updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc);
}

// Add to main loop() - periodic SD card checking
void loop() {
    unsigned long currentTime = millis();
    
    // Periodic SD card check if not working
    static unsigned long lastSDCheck = 0;
    if (!systemStatus.sdWorking && (currentTime - lastSDCheck >= 10000)) { // Check every 10 seconds
        checkSDCard(systemStatus);
        if (systemStatus.sdWorking) {
            Serial.println(F("SD card detected and initialized!"));
            createLogFile(rtc, systemStatus);
        }
        lastSDCheck = currentTime;
    }
    
   
    updateButtonStates();
    
    // Handle button presses for main navigation
    if (!menuState.settingsMenuActive) {
        if (wasButtonPressed(0)) { // UP
            if (currentMode > 0) {
                currentMode = (DisplayMode)(currentMode - 1);
            } else {
                currentMode = MODE_ALERTS;
            }
            updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc);
        }
        
        if (wasButtonPressed(1)) { // DOWN
            if (currentMode < MODE_ALERTS) {
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
    
    // Handle settings menu if active
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
    
    // Audio sampling
    if (systemStatus.pdmWorking && (currentTime - lastAudioSample >= AUDIO_INTERVAL)) {
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
    
    // Update display
    if (systemStatus.displayWorking && (currentTime - lastDisplayUpdate >= DISPLAY_INTERVAL)) {
        updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc);
        lastDisplayUpdate = currentTime;
    }
    
    delay(10);
}

