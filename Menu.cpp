/**
 * Menu.cpp
 * Settings menu system implementation with long press support
 */

#include "Menu.h"
#include "Settings.h"
#include "Utils.h"

// Menu item counts
const int MAIN_MENU_ITEMS = 6;
const int SENSOR_CALIB_ITEMS = 2;
const int AUDIO_SETTINGS_ITEMS = 7;
const int LOGGING_ITEMS = 2;
const int ALERT_ITEMS = 4;
const int SYSTEM_ITEMS = 1;

// =============================================================================
// MENU NAVIGATION
// =============================================================================

void enterSettingsMenu(MenuState& state, DisplayMode& mode) {
    state.settingsMenuActive = true;
    mode = MODE_SETTINGS;
    state.menuLevel = 0;
    state.selectedItem = 0;
    Serial.println(F("Entered settings"));
}

void exitSettingsMenu(MenuState& state, DisplayMode& mode, SystemSettings& settings) {
    state.settingsMenuActive = false;
    mode = MODE_DASHBOARD;
    saveSettings(settings);
    Serial.println(F("Exited settings"));
}

// =============================================================================
// MAIN MENU HANDLER
// =============================================================================

void handleSettingsMenu(Adafruit_SH1106G& display, MenuState& state,
                       SystemSettings& settings, RTC_DS3231& rtc,
                       SensorData& currentData, SystemStatus& status) {
    static bool editingInitialized = false;
    static DateTime editDateTime;
    static int mainMenuSelection = 0;
    
    // Handle menu levels
    if (state.menuLevel == 0) {
        // Main settings menu
        if (wasButtonPressed(0)) { // UP
            state.selectedItem--;
            if (state.selectedItem < 0) state.selectedItem = MAIN_MENU_ITEMS - 1;
        }
        if (wasButtonPressed(1)) { // DOWN
            state.selectedItem++;
            if (state.selectedItem >= MAIN_MENU_ITEMS) state.selectedItem = 0;
        }
        if (wasButtonPressed(2)) { // SELECT
            mainMenuSelection = state.selectedItem;
            state.menuLevel = 1;
            state.selectedItem = 0;
            editingInitialized = false;
            
            // Initialize for Time & Date menu
            if (mainMenuSelection == 0 && status.rtcWorking) {
                editDateTime = rtc.now();
            }
            
            resetButtonStates(); // Prevent button bleed-through to submenu
        }
        if (wasButtonPressed(3)) { // BACK
            DisplayMode mode = MODE_DASHBOARD;
            exitSettingsMenu(state, mode, settings);
            return;
        }
        
        drawMainSettingsMenu(display, state.selectedItem);
        
    } else if (state.menuLevel == 1) {
        // Sub-menus - create buttonPressed array for compatibility
        bool buttonPressed[4] = {false, false, false, false};
        
        switch (mainMenuSelection) {
            case 0: // Time & Date
                handleTimeDateMenu(display, state, settings, rtc, editDateTime,
                                 editingInitialized, buttonPressed, status);
                break;
            case 1: // Sensor Calibration
                handleSensorCalibMenu(display, state, settings, currentData,
                                    editingInitialized, buttonPressed);
                break;
            case 2: // Audio Settings
                handleAudioMenu(display, state, settings, editingInitialized, buttonPressed);
                break;
            case 3: // Logging Settings
                handleLoggingMenu(display, state, settings, editingInitialized, buttonPressed);
                break;
            case 4: // Alert Thresholds
                handleAlertMenu(display, state, settings, editingInitialized, buttonPressed);
                break;
            case 5: // System Settings
                handleSystemMenu(display, state, settings, editingInitialized, buttonPressed);
                break;
        }
    }
}

// =============================================================================
// MAIN MENU DRAWING
// =============================================================================

void drawMainSettingsMenu(Adafruit_SH1106G& display, int selected) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH1106_WHITE);
    
    // Title
    display.setCursor(35, 0);
    display.println(F("Settings"));
    display.drawLine(0, 10, 127, 10, SH1106_WHITE);
    
    const char* items[] = {
        "Time & Date",
        "Sensor Calib",
        "Audio Settings",
        "Logging",
        "Alert Thresholds",
        "System"
    };
    
    // Calculate visible items
    const int visibleItems = 4;
    int startItem = 0;
    if (selected >= visibleItems) {
        startItem = selected - visibleItems + 1;
    }
    
    for (int i = 0; i < visibleItems && (startItem + i) < MAIN_MENU_ITEMS; i++) {
        int itemIndex = startItem + i;
        int y = 16 + (i * 12);
        
        if (itemIndex == selected) {
            display.setCursor(0, y);
            display.print(F(">"));
        }
        
        display.setCursor(12, y);
        display.print(items[itemIndex]);
    }
    
    display.display();
}

// =============================================================================
// TIME & DATE MENU
// =============================================================================

void handleTimeDateMenu(Adafruit_SH1106G& display, MenuState& state, 
                       SystemSettings& settings, RTC_DS3231& rtc,
                       DateTime& editDateTime, bool& editingInitialized,
                       bool* buttonPressed, SystemStatus& status) {
    static int editValue = 0;
    static bool inEditMode = false;
    
    // Time & Date selection menu
    if (!inEditMode) {
        // Navigation mode - move cursor up/down
        if (wasButtonPressed(0)) { // UP
            state.selectedItem--;
            if (state.selectedItem < 0) state.selectedItem = 4;
        }
        if (wasButtonPressed(1)) { // DOWN
            state.selectedItem++;
            if (state.selectedItem > 4) state.selectedItem = 0;
        }
        if (wasButtonPressed(2)) { // SELECT - Enter edit mode
            inEditMode = true;
            switch (state.selectedItem) {
                case 0: editValue = editDateTime.year(); break;
                case 1: editValue = editDateTime.month(); break;
                case 2: editValue = editDateTime.day(); break;
                case 3: editValue = editDateTime.hour(); break;
                case 4: editValue = editDateTime.minute(); break;
            }
        }
        if (wasButtonPressed(3)) { // BACK - Return to settings menu
            state.menuLevel = 0;
            state.selectedItem = 0;
            return;
        }
        
    } else {
        // Edit mode - modify values with long press support
        if (wasButtonPressed(0) || shouldRepeat(0)) { // UP - Increment value
            editValue++;
            switch (state.selectedItem) {
                case 0: if (editValue > 2050) editValue = 2020; break;
                case 1: if (editValue > 12) editValue = 1; break;
                case 2: {
                    int maxDay = getDaysInMonth(editDateTime.month(), editDateTime.year());
                    if (editValue > maxDay) editValue = 1;
                    break;
                }
                case 3: if (editValue > 23) editValue = 0; break;
                case 4: if (editValue > 59) editValue = 0; break;
            }
        }
        
        if (wasButtonPressed(1) || shouldRepeat(1)) { // DOWN - Decrement value
            editValue--;
            switch (state.selectedItem) {
                case 0: if (editValue < 2020) editValue = 2050; break;
                case 1: if (editValue < 1) editValue = 12; break;
                case 2: {
                    int maxDay = getDaysInMonth(editDateTime.month(), editDateTime.year());
                    if (editValue < 1) editValue = maxDay;
                    break;
                }
                case 3: if (editValue < 0) editValue = 23; break;
                case 4: if (editValue < 0) editValue = 59; break;
            }
        }
        
        if (wasButtonPressed(2)) { // SELECT - Save value
            // Update the DateTime object with new value
            switch (state.selectedItem) {
                case 0:
                    editDateTime = DateTime(editValue, editDateTime.month(), 
                                          editDateTime.day(), editDateTime.hour(), 
                                          editDateTime.minute(), editDateTime.second());
                    break;
                case 1:
                    editDateTime = DateTime(editDateTime.year(), editValue, 
                                          editDateTime.day(), editDateTime.hour(), 
                                          editDateTime.minute(), editDateTime.second());
                    break;
                case 2:
                    editDateTime = DateTime(editDateTime.year(), editDateTime.month(), 
                                          editValue, editDateTime.hour(), 
                                          editDateTime.minute(), editDateTime.second());
                    break;
                case 3:
                    editDateTime = DateTime(editDateTime.year(), editDateTime.month(), 
                                          editDateTime.day(), editValue, 
                                          editDateTime.minute(), editDateTime.second());
                    break;
                case 4:
                    editDateTime = DateTime(editDateTime.year(), editDateTime.month(), 
                                          editDateTime.day(), editDateTime.hour(), 
                                          editValue, editDateTime.second());
                    break;
            }
            
            // Update RTC if working
            if (status.rtcWorking) {
                rtc.adjust(editDateTime);
                Serial.println(F("RTC updated"));
            }
            
            // Exit edit mode
            inEditMode = false;
        }
        
        if (wasButtonPressed(3)) { // BACK - Exit edit mode without saving
            inEditMode = false;
        }
    }
    
    // Draw the menu with edit mode indication
    drawTimeDateMenuWithEdit(display, state.selectedItem, editDateTime, inEditMode, editValue);
}

// Add this new drawing function after handleTimeDateMenu:

void drawTimeDateMenuWithEdit(Adafruit_SH1106G& display, int selected, 
                             DateTime dt, bool editMode, int editValue) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH1106_WHITE);
    
    display.setCursor(25, 0);
    display.println(F("Time & Date"));
    display.drawLine(0, 10, 127, 10, SH1106_WHITE);
    
    const char* labels[] = {"Year:", "Month:", "Day:", "Hour:", "Minute:"};
    int values[] = {dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute()};
    
    // Use blinking effect for edit mode
    bool showValue = true;
    if (editMode) {
        // Blink the value being edited
        showValue = (millis() / 300) % 2;  // Blink every 300ms
        values[selected] = editValue;  // Show the edit value instead
    }
    
    for (int i = 0; i < 5; i++) {
        int y = 16 + (i * 10);
        
        if (i == selected) {
            display.setCursor(0, y);
            display.print(F(">"));
            
            if (editMode) {
                // Show edit indicator
                display.setCursor(120, y);
                display.print(F("*"));
            }
        }
        
        display.setCursor(12, y);
        display.print(labels[i]);
        display.setCursor(60, y);
        
        if (i == selected && editMode && !showValue) {
            // Don't show value during blink off phase
        } else {
            if (i == 1) {
                display.print(getMonthName(values[i]));
            } else {
                display.print(values[i]);
            }
        }
    }
    
    // Show edit mode hint at bottom
    if (editMode) {
        display.setCursor(0, 56);
        display.print(F("EDIT: UP/DN SEL:Save"));
    }
    
    display.display();
}

void drawEditValueScreen(Adafruit_SH1106G& display, int item, int value) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH1106_WHITE);
    
    const char* titles[] = {"Set Year", "Set Month", "Set Day", "Set Hour", "Set Minute"};
    
    display.setCursor(30, 0);
    display.println(titles[item]);
    display.drawLine(0, 10, 127, 10, SH1106_WHITE);
    
    display.setTextSize(3);
    
    if (item == 1) {
        display.setTextSize(2);
        const char* monthName = getMonthName(value);
        display.setCursor(35, 25);
        display.print(monthName);
    } else {
        display.setCursor(45, 25);
        display.print(value);
    }
    
    display.setTextSize(1);
    display.setCursor(5, 55);
    display.print(F("UP/DN:Change SEL:Save"));
    
    display.display();
}

// =============================================================================
// SENSOR CALIBRATION MENU
// =============================================================================

void handleSensorCalibMenu(Adafruit_SH1106G& display, MenuState& state, 
                          SystemSettings& settings, SensorData& currentData,
                          bool& editingInitialized, bool* buttonPressed) {
    static bool editing = false;
    
    if (!editing) {
        if (wasButtonPressed(0)) { // UP
            state.selectedItem--;
            if (state.selectedItem < 0) state.selectedItem = SENSOR_CALIB_ITEMS - 1;
        }
        if (wasButtonPressed(1)) { // DOWN
            state.selectedItem++;
            if (state.selectedItem >= SENSOR_CALIB_ITEMS) state.selectedItem = 0;
        }
        if (wasButtonPressed(2)) { // SELECT
            editing = true;
            if (state.selectedItem == 0) {
                state.editFloatValue = settings.tempOffset;
            } else {
                state.editFloatValue = settings.humidityOffset;
            }
        }
        if (wasButtonPressed(3)) { // BACK
            state.menuLevel = 0;
            state.selectedItem = 1;
        }
        
        drawSensorCalibMenu(display, state.selectedItem, settings, currentData);
    } else {
        // Editing mode with long press support
        if (wasButtonPressed(0) || shouldRepeat(0)) { // UP
            if (state.selectedItem == 0) {
                state.editFloatValue += 0.1;
                if (state.editFloatValue > 10.0) state.editFloatValue = 10.0;
            } else {
                state.editFloatValue += 0.5;
                if (state.editFloatValue > 20.0) state.editFloatValue = 20.0;
            }
        }
        if (wasButtonPressed(1) || shouldRepeat(1)) { // DOWN
            if (state.selectedItem == 0) {
                state.editFloatValue -= 0.1;
                if (state.editFloatValue < -10.0) state.editFloatValue = -10.0;
            } else {
                state.editFloatValue -= 0.5;
                if (state.editFloatValue < -20.0) state.editFloatValue = -20.0;
            }
        }
        if (wasButtonPressed(2)) { // SELECT - Save
            if (state.selectedItem == 0) {
                settings.tempOffset = state.editFloatValue;
            } else {
                settings.humidityOffset = state.editFloatValue;
            }
            editing = false;
            saveSettings(settings);
        }
        if (wasButtonPressed(3)) { // BACK - Cancel
            editing = false;
        }
        
        drawEditFloatValue(display, 
                          state.selectedItem == 0 ? "Temp Offset" : "Humid Offset", 
                          state.editFloatValue, 
                          state.selectedItem == 0 ? "c" : "%");
    }
}


void drawSensorCalibMenu(Adafruit_SH1106G& display, int selected, 
                        SystemSettings& settings, SensorData& currentData) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH1106_WHITE);
    
    display.setCursor(20, 0);
    display.println(F("Sensor Calibration"));
    display.drawLine(0, 10, 127, 10, SH1106_WHITE);
    
    // Temperature offset
    if (selected == 0) {
        display.setCursor(0, 20);
        display.print(F(">"));
    }
    display.setCursor(12, 20);
    display.print(F("Temp Offset: "));
    display.print(settings.tempOffset, 1);
    display.print(F("c"));
    
    // Humidity offset
    if (selected == 1) {
        display.setCursor(0, 32);
        display.print(F(">"));
    }
    display.setCursor(12, 32);
    display.print(F("Humid Offset: "));
    display.print(settings.humidityOffset, 1);
    display.print(F("%"));
    
    // Current readings
    display.setCursor(0, 48);
    display.print(F("Raw T:"));
    display.print(currentData.temperature - settings.tempOffset, 1);
    display.print(F(" H:"));
    display.print(currentData.humidity - settings.humidityOffset, 1);
    
    display.display();
}

void drawEditFloatValue(Adafruit_SH1106G& display, const char* title, 
                       float value, const char* unit) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH1106_WHITE);
    
    display.setCursor(30, 0);
    display.println(title);
    display.drawLine(0, 10, 127, 10, SH1106_WHITE);
    
    display.setTextSize(2);
    display.setCursor(30, 25);
    if (value >= 0) display.print(F("+"));
    display.print(value, 1);
    display.print(unit);
    
    display.setTextSize(1);
    display.setCursor(5, 55);
    display.print(F("UP/DN:Change SEL:Save"));
    
    display.display();
}

// =============================================================================
// AUDIO SETTINGS MENU
// =============================================================================

void handleAudioMenu(Adafruit_SH1106G& display, MenuState& state, 
                    SystemSettings& settings, bool& editingInitialized,
                    bool* buttonPressed) {
    static bool editing = false;
    static int audioMenuItem = 0;
    const int AUDIO_ITEMS = 6;
    
    if (!editing) {
        if (wasButtonPressed(0)) { // UP
            audioMenuItem--;
            if (audioMenuItem < 0) audioMenuItem = AUDIO_ITEMS - 1;
        }
        if (wasButtonPressed(1)) { // DOWN
            audioMenuItem++;
            if (audioMenuItem >= AUDIO_ITEMS) audioMenuItem = 0;
        }
        if (wasButtonPressed(2)) { // SELECT
            editing = true;
            switch (audioMenuItem) {                
                case 0: state.editIntValue = settings.audioSensitivity; break;
                case 1: state.editIntValue = settings.queenFreqMin; break;
                case 2: state.editIntValue = settings.queenFreqMax; break;
                case 3: state.editIntValue = settings.swarmFreqMin; break;
                case 4: state.editIntValue = settings.swarmFreqMax; break;
                case 5: state.editIntValue = settings.stressThreshold; break;
            }
        }
        if (wasButtonPressed(3)) { // BACK
            state.menuLevel = 0;
            state.selectedItem = 2;
        }
        
        drawAudioMenu(display, audioMenuItem, settings);
    } else {
        // Editing mode with long press support
        if (wasButtonPressed(0) || shouldRepeat(0)) { // UP
            switch (audioMenuItem) {                
                case 0: // Audio Sensitivity
                    state.editIntValue++;
                    if (state.editIntValue > 10) state.editIntValue = 10;
                    break;
                case 1: // Queen Freq Min
                case 2: // Queen Freq Max
                case 3: // Swarm Freq Min
                case 4: // Swarm Freq Max
                    state.editIntValue += 10;
                    if (state.editIntValue > 1000) state.editIntValue = 1000;
                    break;
                case 5: // Stress Threshold
                    state.editIntValue += 5;
                    if (state.editIntValue > 100) state.editIntValue = 100;
                    break;
            }
        }
        if (wasButtonPressed(1) || shouldRepeat(1)) { // DOWN
            switch (audioMenuItem) {                
                case 0: // Audio Sensitivity
                    state.editIntValue--;
                    if (state.editIntValue < 0) state.editIntValue = 0;
                    break;
                case 1: // Queen Freq Min
                case 2: // Queen Freq Max
                case 3: // Swarm Freq Min
                case 4: // Swarm Freq Max
                    state.editIntValue -= 10;
                    if (state.editIntValue < 50) state.editIntValue = 50;
                    break;
                case 5: // Stress Threshold
                    state.editIntValue -= 5;
                    if (state.editIntValue < 0) state.editIntValue = 0;
                    break;
            }
        }
        if (wasButtonPressed(2)) { // SELECT - Save
            switch (audioMenuItem) {                
                case 0: settings.audioSensitivity = state.editIntValue; break;
                case 1: settings.queenFreqMin = state.editIntValue; break;
                case 2: settings.queenFreqMax = state.editIntValue; break;
                case 3: settings.swarmFreqMin = state.editIntValue; break;
                case 4: settings.swarmFreqMax = state.editIntValue; break;
                case 5: settings.stressThreshold = state.editIntValue; break;
            }
            editing = false;
            saveSettings(settings);
        }
        if (wasButtonPressed(3)) { // BACK - Cancel
            editing = false;
        }
        
        drawEditIntValue(display, getAudioMenuTitle(audioMenuItem), 
                        state.editIntValue, getAudioMenuUnit(audioMenuItem));
    }
}

void drawAudioMenu(Adafruit_SH1106G& display, int selected, SystemSettings& settings) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH1106_WHITE);
    
    display.setCursor(25, 0);
    display.println(F("Audio Settings"));
    display.drawLine(0, 10, 127, 10, SH1106_WHITE);
    
    const char* labels[] = {
        "Sensitivity:", "Queen Min:", 
        "Queen Max:", "Swarm Min:", "Swarm Max:", "Stress Lvl:"
    };
    
    int values[] = {
        settings.audioSensitivity,
        settings.queenFreqMin, settings.queenFreqMax,
        settings.swarmFreqMin, settings.swarmFreqMax,
        settings.stressThreshold
    };
    
    // Show 4 items at a time
    int startItem = selected > 3 ? selected - 3 : 0;
    for (int i = 0; i < 4 && (startItem + i) < 6; i++) {
        int itemIdx = startItem + i;
        int y = 16 + (i * 10);
        
        if (itemIdx == selected) {
            display.setCursor(0, y);
            display.print(F(">"));
        }
        
        display.setCursor(12, y);
        display.print(labels[itemIdx]);
        display.setCursor(75, y);
        display.print(values[itemIdx]);
    }
    
    display.display();
}

// =============================================================================
// LOGGING SETTINGS MENU
// =============================================================================

void handleLoggingMenu(Adafruit_SH1106G& display, MenuState& state, 
                      SystemSettings& settings, bool& editingInitialized,
                      bool* buttonPressed) {
    static bool editing = false;
    static int logMenuItem = 0;
    
    if (!editing) {
        if (wasButtonPressed(0)) { // UP
            logMenuItem = 1 - logMenuItem;
        }
        if (wasButtonPressed(1)) { // DOWN
            logMenuItem = 1 - logMenuItem;
        }
        if (wasButtonPressed(2)) { // SELECT
            editing = true;
            if (logMenuItem == 0) {
                state.editIntValue = settings.logInterval;
            }
        }
        if (wasButtonPressed(3)) { // BACK
            state.menuLevel = 0;
            state.selectedItem = 3;
        }
        
        drawLoggingMenu(display, logMenuItem, settings);
    } else {
        if (logMenuItem == 0) { // Log interval
            if ((wasButtonPressed(0) || shouldRepeat(0)) || 
                (wasButtonPressed(1) || shouldRepeat(1))) { // UP or DOWN
                // Cycle through valid intervals: 5, 10, 30, 60
                if (state.editIntValue == 5) state.editIntValue = 10;
                else if (state.editIntValue == 10) state.editIntValue = 30;
                else if (state.editIntValue == 30) state.editIntValue = 60;
                else state.editIntValue = 5;
            }
            if (wasButtonPressed(2)) { // SELECT - Save
                settings.logInterval = state.editIntValue;
                editing = false;
                saveSettings(settings);
            }
        } else { // Log enabled
            if (wasButtonPressed(0) || wasButtonPressed(1)) { // Toggle
                settings.logEnabled = !settings.logEnabled;
            }
            if (wasButtonPressed(2)) { // SELECT - Save
                editing = false;
                saveSettings(settings);
            }
        }
        if (wasButtonPressed(3)) { // BACK
            editing = false;
        }
        
        if (logMenuItem == 0) {
            drawEditIntValue(display, "Log Interval", state.editIntValue, "min");
        } else {
            drawEditBoolValue(display, "Logging", settings.logEnabled);
        }
    }
}

void drawLoggingMenu(Adafruit_SH1106G& display, int selected, SystemSettings& settings) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH1106_WHITE);
    
    display.setCursor(30, 0);
    display.println(F("Logging Setup"));
    display.drawLine(0, 10, 127, 10, SH1106_WHITE);
    
    // Log interval
    if (selected == 0) {
        display.setCursor(0, 20);
        display.print(F(">"));
    }
    display.setCursor(12, 20);
    display.print(F("Interval: "));
    display.print(settings.logInterval);
    display.print(F(" min"));
    
    // Log enabled
    if (selected == 1) {
        display.setCursor(0, 32);
        display.print(F(">"));
    }
    display.setCursor(12, 32);
    display.print(F("Logging: "));
    display.print(settings.logEnabled ? "ON" : "OFF");
    
    display.display();
}

// =============================================================================
// ALERT THRESHOLDS MENU
// =============================================================================

void handleAlertMenu(Adafruit_SH1106G& display, MenuState& state, 
                    SystemSettings& settings, bool& editingInitialized,
                    bool* buttonPressed) {
    static bool editing = false;
    static int alertMenuItem = 0;
    const int ALERT_ITEMS = 4;
    
    if (!editing) {
        if (wasButtonPressed(0)) { // UP
            alertMenuItem--;
            if (alertMenuItem < 0) alertMenuItem = ALERT_ITEMS - 1;
        }
        if (wasButtonPressed(1)) { // DOWN
            alertMenuItem++;
            if (alertMenuItem >= ALERT_ITEMS) alertMenuItem = 0;
        }
        if (wasButtonPressed(2)) { // SELECT
            editing = true;
            switch (alertMenuItem) {
                case 0: state.editFloatValue = settings.tempMin; break;
                case 1: state.editFloatValue = settings.tempMax; break;
                case 2: state.editFloatValue = settings.humidityMin; break;
                case 3: state.editFloatValue = settings.humidityMax; break;
            }
        }
        if (wasButtonPressed(3)) { // BACK
            state.menuLevel = 0;
            state.selectedItem = 4;
        }
        
        drawAlertMenu(display, alertMenuItem, settings);
    } else {
        // Editing mode with long press support
        float increment = (alertMenuItem < 2) ? 0.5 : 1.0; // Temp: 0.5°C, Humidity: 1%
        
        if (wasButtonPressed(0) || shouldRepeat(0)) { // UP
            state.editFloatValue += increment;
            // Validate ranges
            switch (alertMenuItem) {
                case 0: if (state.editFloatValue > 40) state.editFloatValue = 40; break;
                case 1: if (state.editFloatValue > 60) state.editFloatValue = 60; break;
                case 2: if (state.editFloatValue > 90) state.editFloatValue = 90; break;
                case 3: if (state.editFloatValue > 100) state.editFloatValue = 100; break;
            }
        }
        if (wasButtonPressed(1) || shouldRepeat(1)) { // DOWN
            state.editFloatValue -= increment;
            // Validate ranges
            switch (alertMenuItem) {
                case 0: if (state.editFloatValue < -10) state.editFloatValue = -10; break;
                case 1: if (state.editFloatValue < 0) state.editFloatValue = 0; break;
                case 2: if (state.editFloatValue < 0) state.editFloatValue = 0; break;
                case 3: if (state.editFloatValue < 20) state.editFloatValue = 20; break;
            }
        }
        if (wasButtonPressed(2)) { // SELECT - Save
            switch (alertMenuItem) {
                case 0: settings.tempMin = state.editFloatValue; break;
                case 1: settings.tempMax = state.editFloatValue; break;
                case 2: settings.humidityMin = state.editFloatValue; break;
                case 3: settings.humidityMax = state.editFloatValue; break;
            }
            editing = false;
            saveSettings(settings);
        }
        if (wasButtonPressed(3)) { // BACK - Cancel
            editing = false;
        }
        
        const char* titles[] = {"Temp Min", "Temp Max", "Humid Min", "Humid Max"};
        const char* units[] = {"c", "c", "%", "%"};
        drawEditFloatValue(display, titles[alertMenuItem], state.editFloatValue, units[alertMenuItem]);
    }
}

void drawAlertMenu(Adafruit_SH1106G& display, int selected, SystemSettings& settings) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH1106_WHITE);
    
    display.setCursor(20, 0);
    display.println(F("Alert Thresholds"));
    display.drawLine(0, 10, 127, 10, SH1106_WHITE);
    
    const char* labels[] = {"Temp Min:", "Temp Max:", "Humid Min:", "Humid Max:"};
    float values[] = {settings.tempMin, settings.tempMax, settings.humidityMin, settings.humidityMax};
    const char* units[] = {"c", "c", "%", "%"};
    
    for (int i = 0; i < 4; i++) {
        int y = 16 + (i * 12);
        
        if (i == selected) {
            display.setCursor(0, y);
            display.print(F(">"));
        }
        
        display.setCursor(12, y);
        display.print(labels[i]);
        display.setCursor(70, y);
        display.print(values[i], 1);
        display.print(units[i]);
    }
    
    display.display();
}

// =============================================================================
// SYSTEM SETTINGS MENU
// =============================================================================

void handleSystemMenu(Adafruit_SH1106G& display, MenuState& state, 
                     SystemSettings& settings, bool& editingInitialized,
                     bool* buttonPressed) {
    static bool editing = false;
    
    if (!editing) {
        if (wasButtonPressed(2)) { // SELECT
            editing = true;
            state.editIntValue = settings.displayBrightness;
        }
        if (wasButtonPressed(3)) { // BACK
            state.menuLevel = 0;
            state.selectedItem = 5;
        }
        
        drawSystemMenu(display, 0, settings);
    } else {
        // Editing mode with long press support
        if (wasButtonPressed(0) || shouldRepeat(0)) { // UP
            state.editIntValue++;
            if (state.editIntValue > 10) state.editIntValue = 10;
        }
        if (wasButtonPressed(1) || shouldRepeat(1)) { // DOWN
            state.editIntValue--;
            if (state.editIntValue < 1) state.editIntValue = 1;
        }
        if (wasButtonPressed(2)) { // SELECT - Save
            settings.displayBrightness = state.editIntValue;
            editing = false;
            saveSettings(settings);
            // Apply brightness setting
            // display.setBrightness(map(settings.displayBrightness, 1, 10, 0, 255));
        }
        if (wasButtonPressed(3)) { // BACK
            editing = false;
        }
        
        drawEditIntValue(display, "Brightness", state.editIntValue, "/10");
    }
}

void drawSystemMenu(Adafruit_SH1106G& display, int selected, SystemSettings& settings) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH1106_WHITE);
    
    display.setCursor(25, 0);
    display.println(F("System Settings"));
    display.drawLine(0, 10, 127, 10, SH1106_WHITE);
    
    display.setCursor(0, 20);
    display.print(F(">"));
    display.setCursor(12, 20);
    display.print(F("Brightness: "));
    display.print(settings.displayBrightness);
    display.print(F("/10"));
    
    display.display();
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

void drawEditIntValue(Adafruit_SH1106G& display, const char* title, int value, const char* unit) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH1106_WHITE);
    
    display.setCursor(30, 0);
    display.println(title);
    display.drawLine(0, 10, 127, 10, SH1106_WHITE);
    
    display.setTextSize(2);
    display.setCursor(40, 25);
    display.print(value);
    display.print(unit);
    
    display.setTextSize(1);
    display.setCursor(5, 55);
    display.print(F("UP/DN:Change SEL:Save"));
    
    display.display();
}

void drawEditBoolValue(Adafruit_SH1106G& display, const char* title, bool value) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH1106_WHITE);
    
    display.setCursor(40, 0);
    display.println(title);
    display.drawLine(0, 10, 127, 10, SH1106_WHITE);
    
    display.setTextSize(2);
    display.setCursor(45, 25);
    display.print(value ? "ON" : "OFF");
    
    display.setTextSize(1);
    display.setCursor(5, 55);
    display.print(F("UP/DN:Toggle SEL:Save"));
    
    display.display();
}

const char* getAudioMenuTitle(int item) {
    const char* titles[] = {
        "Sensitivity", "Queen Min Freq",
        "Queen Max Freq", "Swarm Min Freq", "Swarm Max Freq",
        "Stress Level"
    };
    return titles[item];
}

const char* getAudioMenuUnit(int item) {
    if (item == 0) return "/10";  // Sensitivity
    if (item < 5) return " Hz";   // Frequencies
    return "%";                   // Stress level
}