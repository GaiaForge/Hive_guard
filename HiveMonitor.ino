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
Adafruit_BMP280 bmp;
Adafruit_SHT31 sht;
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
    
    delay(2000);
    
    loadSettings(settings);
    
    Wire.begin();
    Wire.setClock(100000);
    
    if (display.begin(SCREEN_ADDRESS, true)) {
        systemStatus.displayWorking = true;
        showStartupScreen(display);
    }
    
    if (rtc.begin()) {
        systemStatus.rtcWorking = true;
        if (rtc.lostPower()) {
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
    }
    
    initializeSensors(bmp, sht, systemStatus);
    initializeAudio(settings.micGain, systemStatus);
    
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);
    pinMode(BTN_BACK, INPUT_PULLUP);
    
    SPI.begin();
    if (SD.begin(SD_CS_PIN)) {
        systemStatus.sdWorking = true;
        createLogFile(rtc, systemStatus);
    }
    
    Serial.println(F("System ready!"));
    systemStatus.systemReady = true;
    
    readAllSensors(bmp, sht, currentData, settings, systemStatus);
    updateDisplay(display, currentMode, currentData, settings, systemStatus, rtc);
}

void loop() {
    unsigned long currentTime = millis();
    
    // Update button states every loop
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
                resetButtonStates(); // Prevent button bleed-through
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
        readAllSensors(bmp, sht, currentData, settings, systemStatus);
        checkAlerts(currentData, settings);
        lastSensorRead = currentTime;
    }
    
    // Audio sampling
    if (systemStatus.pdmWorking && (currentTime - lastAudioSample >= AUDIO_INTERVAL)) {
        processAudio(currentData, settings);
        lastAudioSample = currentTime;
    }
    
    // Data logging
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
    
    // Optional: Debug button states
    // printButtonDebug();
    
    delay(10);
}

