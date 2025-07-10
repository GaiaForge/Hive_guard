/**
 * Menu.cpp
 * Settings menu system implementation with long press support
 */

#include "Menu.h"
#include "Settings.h"
#include "Utils.h"
#include "PowerManager.h"  
#include "Bluetooth.h"

// Menu item counts
const int MAIN_MENU_ITEMS = 8;
const int SENSOR_CALIB_ITEMS = 2;
const int AUDIO_SETTINGS_ITEMS = 7;
const int LOGGING_ITEMS = 2;
const int ALERT_ITEMS = 4;
const int SYSTEM_ITEMS = 3;  // Brightness + Field Mode + Display Timeout

// Forward declaration
extern PowerManager powerManager;

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
                       SystemSettings& settings, RTC_PCF8523& rtc,
                       SensorData& currentData, SystemStatus& status) {
    static bool editingInitialized = false;
    static DateTime editDateTime;
    static int mainMenuSelection = 0;
    
    // Handle menu levels
    if (state.menuLevel == 0) {
        // Main settings menu navigation (unchanged)
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
            
            resetButtonStates();
        }
        if (wasButtonPressed(3)) { // BACK
            DisplayMode mode = MODE_DASHBOARD;
            exitSettingsMenu(state, mode, settings);
            return;
        }
        
        drawMainSettingsMenu(display, state.selectedItem);
        
    } else if (state.menuLevel == 1) {
        // Sub-menus - UPDATE THE SWITCH STATEMENT:
        bool buttonPressed[4] = {false, false, false, false};
        
        switch (mainMenuSelection) {
            case 0: // Time & Date
                handleTimeDateMenu(display, state, settings, rtc, editDateTime,
                                 editingInitialized, buttonPressed, status);
                break;
                
            case 1: // Bee Type Presets - NEW CASE
                handleBeePresetMenu(display, state, settings, editingInitialized, buttonPressed);
                break;
                
            case 2: // Sensor Calibration (was case 1)
                handleSensorCalibMenu(display, state, settings, currentData,
                                    editingInitialized, buttonPressed);
                break;
                
            case 3: // Audio Settings (was case 2)
                handleAudioMenu(display, state, settings, editingInitialized, buttonPressed);
                break;
                
            case 4: // Logging Settings (was case 3)
                handleLoggingMenu(display, state, settings, editingInitialized, buttonPressed);
                break;
                
            case 5: // Alert Thresholds (was case 4)
                handleAlertMenu(display, state, settings, editingInitialized, buttonPressed);
                break;
                
            case 6: // System Settings (was case 5)
                handleSystemMenu(display, state, settings, editingInitialized, buttonPressed);
                break;
                
            case 7: // Bluetooth settings (was case 6)
                handleBluetoothMenu(display, state, settings, editingInitialized, buttonPressed);
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
    display.setTextColor(SH110X_WHITE);
    
    // Title
    display.setCursor(35, 0);
    display.println(F("Settings"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    const char* items[] = {
        "Time & Date",
        "Bee Type Presets",    
        "Sensor Calib",
        "Audio Settings",
        "Logging",
        "Alert Thresholds",
        "System",
        "Bluetooth",
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
// BLUETOOTH MENU DRAWING FUNCTIONS
// =============================================================================

void drawBluetoothMenu(Adafruit_SH1106G& display, int selected, BluetoothSettings& btSettings) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    display.setCursor(25, 0);
    display.println(F("Bluetooth Setup"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    const char* labels[] = {
        "Mode:", "Manual Time:", "Start Hour:", "End Hour:", "Device ID:"
    };
    
    // Show 4 items at a time
    int startItem = selected > 3 ? selected - 3 : 0;
    for (int i = 0; i < 4 && (startItem + i) < 5; i++) {
        int itemIdx = startItem + i;
        int y = 16 + (i * 10);
        
        if (itemIdx == selected) {
            display.setCursor(0, y);
            display.print(F(">"));
        }
        
        display.setCursor(12, y);
        display.print(labels[itemIdx]);
        display.setCursor(75, y);
        
        switch (itemIdx) {
            case 0: // Mode
                display.print(bluetoothModeToString(btSettings.mode));
                break;
            case 1: // Manual Timeout
                display.print(btSettings.manualTimeoutMin);
                display.print(F("min"));
                break;
            case 2: // Start Hour
                display.print(btSettings.scheduleStartHour);
                display.print(F(":00"));
                break;
            case 3: // End Hour
                display.print(btSettings.scheduleEndHour);
                display.print(F(":00"));
                break;
            case 4: // Device ID
                display.print(btSettings.deviceId);
                break;
        }
    }
    
    // Show current status at bottom
    extern BluetoothManager bluetoothManager;
    display.setCursor(0, 56);
    display.print(F("Status: "));
    display.print(bluetoothStatusToString(bluetoothManager.getStatus()));
    
    display.display();
}

void drawEditBluetoothMode(Adafruit_SH1106G& display, BluetoothMode mode) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    display.setCursor(25, 0);
    display.println(F("Bluetooth Mode"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    display.setTextSize(2);
    display.setCursor(15, 25);
    display.print(bluetoothModeToString(mode));
    
    display.setTextSize(1);
    display.setCursor(0, 45);
    switch (mode) {
        case BT_MODE_OFF:
            display.print(F("Bluetooth disabled"));
            break;
        case BT_MODE_MANUAL:
            display.print(F("Button activated only"));
            break;
        case BT_MODE_SCHEDULED:
            display.print(F("Active during set hours"));
            break;
        case BT_MODE_ALWAYS_ON:
            display.print(F("Always discoverable"));
            break;
    }
    
    display.setCursor(5, 55);
    display.print(F("UP/DN:Change SEL:Save"));
    
    display.display();
}


// =============================================================================
// TIME & DATE MENU
// =============================================================================

// Suggested improvement to handleTimeDateMenu() in Menu.cpp

void handleTimeDateMenu(Adafruit_SH1106G& display, MenuState& state, 
                       SystemSettings& settings, RTC_PCF8523& rtc,
                       DateTime& editDateTime, bool& editingInitialized,
                       bool* buttonPressed, SystemStatus& status) {
    
    static int editValue = 0;
    static bool inEditMode = false;
    static bool timeChanged = false;  // Track if we need to update RTC
    
    if (!inEditMode) {
        // Navigation mode
        if (wasButtonPressed(0)) { // UP
            state.selectedItem--;
            if (state.selectedItem < 0) state.selectedItem = 4;
        }
        if (wasButtonPressed(1)) { // DOWN
            state.selectedItem++;
            if (state.selectedItem > 4) state.selectedItem = 0;
        }
        if (wasButtonPressed(2)) { // SELECT - enter edit mode
            inEditMode = true;
            switch (state.selectedItem) {
                case 0: editValue = editDateTime.year(); break;
                case 1: editValue = editDateTime.month(); break;
                case 2: editValue = editDateTime.day(); break;
                case 3: editValue = editDateTime.hour(); break;
                case 4: editValue = editDateTime.minute(); break;
            }
        }
        if (wasButtonPressed(3)) { // BACK - exit menu
            // Update RTC only if time was changed
            if (timeChanged && status.rtcWorking) {
                rtc.adjust(editDateTime);
                Serial.println(F("RTC time updated"));
                timeChanged = false;
            }
            state.menuLevel = 0;
            state.selectedItem = 0;
            return;
        }
        
    } else {
        // Edit mode - modify values
        if (wasButtonPressed(0) || shouldRepeat(0)) { // UP
            editValue++;
            // Handle wraparound for each field...
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
        
        if (wasButtonPressed(1) || shouldRepeat(1)) { // DOWN
            editValue--;
            // Handle wraparound...
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
        
        if (wasButtonPressed(2)) { // SELECT - save this field
            // Update the DateTime object
            switch (state.selectedItem) {
                case 0: // Year
                    editDateTime = DateTime(editValue, editDateTime.month(), 
                                          editDateTime.day(), editDateTime.hour(), 
                                          editDateTime.minute(), editDateTime.second());
                    break;
                case 1: // Month
                    editDateTime = DateTime(editDateTime.year(), editValue, 
                                          editDateTime.day(), editDateTime.hour(), 
                                          editDateTime.minute(), editDateTime.second());
                    break;
                case 2: // Day
                    editDateTime = DateTime(editDateTime.year(), editDateTime.month(), 
                                          editValue, editDateTime.hour(), 
                                          editDateTime.minute(), editDateTime.second());
                    break;
                case 3: // Hour
                    editDateTime = DateTime(editDateTime.year(), editDateTime.month(), 
                                          editDateTime.day(), editValue, 
                                          editDateTime.minute(), editDateTime.second());
                    break;
                case 4: // Minute
                    editDateTime = DateTime(editDateTime.year(), editDateTime.month(), 
                                          editDateTime.day(), editDateTime.hour(), 
                                          editValue, editDateTime.second());
                    break;
            }
            
            timeChanged = true;  // Mark that time needs updating
            inEditMode = false;  // Exit edit mode
        }
        
        if (wasButtonPressed(3)) { // BACK - cancel edit
            inEditMode = false;
        }
    }
    
    // Draw the menu
    drawTimeDateMenuWithEdit(display, state.selectedItem, editDateTime, inEditMode, editValue);
}
 
void drawTimeDateMenuWithEdit(Adafruit_SH1106G& display, int selected, 
                             DateTime dt, bool editMode, int editValue) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    display.setCursor(25, 0);
    display.println(F("Time & Date"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
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
    display.setTextColor(SH110X_WHITE);
    
    const char* titles[] = {"Set Year", "Set Month", "Set Day", "Set Hour", "Set Minute"};
    
    display.setCursor(30, 0);
    display.println(titles[item]);
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
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
    display.setTextColor(SH110X_WHITE);
    
    display.setCursor(20, 0);
    display.println(F("Sensor Calibration"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
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
    display.setTextColor(SH110X_WHITE);
    
    display.setCursor(30, 0);
    display.println(title);
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
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
    display.setTextColor(SH110X_WHITE);
    
    display.setCursor(25, 0);
    display.println(F("Audio Settings"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    // Show current bee type at top
    extern BeeType detectCurrentBeeType(const SystemSettings& settings);
    extern const char* getBeeTypeName(BeeType beeType);
    
    BeeType currentType = detectCurrentBeeType(settings);
    display.setCursor(0, 14);
    display.print(F("Bee Type: "));
    display.println(getBeeTypeName(currentType));
    
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
    
    // Show 4 items at a time, starting from y=26 (below bee type)
    int startItem = selected > 3 ? selected - 3 : 0;
    for (int i = 0; i < 4 && (startItem + i) < 6; i++) {
        int itemIdx = startItem + i;
        int y = 26 + (i * 10);
        
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
                // Store old interval for comparison
                uint8_t oldInterval = settings.logInterval;
                
                // Update to new interval
                settings.logInterval = state.editIntValue;
                
                // Update watchdog timeout if interval changed
                if (oldInterval != settings.logInterval) {
                    Serial.print(F("Log interval changed: "));
                    Serial.print(oldInterval);
                    Serial.print(F(" -> "));
                    Serial.print(settings.logInterval);
                    Serial.println(F(" minutes"));
                    
                    // Reconfigure watchdog for new interval
                    updateWatchdogTimeout(settings);
                    
                    // Show confirmation on display
                    display.clearDisplay();
                    display.setTextSize(1);
                    display.setTextColor(SH110X_WHITE);
                    display.setCursor(10, 20);
                    display.print(F("Log interval: "));
                    display.print(settings.logInterval);
                    display.print(F("min"));
                    display.setCursor(5, 35);
                    display.println(F("Watchdog updated"));
                    display.display();
                    delay(1500); // Show for 1.5 seconds
                }
                
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
    display.setTextColor(SH110X_WHITE);
    
    display.setCursor(30, 0);
    display.println(F("Logging Setup"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
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
// BEE PRESET FUNCTIONS (add to DataStructures.cpp)
// =============================================================================


void handleBeePresetMenu(Adafruit_SH1106G& display, MenuState& state, 
                        SystemSettings& settings, bool& editingInitialized,
                        bool* buttonPressed) {
    static bool editing = false;
    static int presetMenuItem = 0;
    const int PRESET_ITEMS = NUM_BEE_PRESETS - 1; // Exclude custom from selection
    
    if (!editing) {
        if (wasButtonPressed(0)) { // UP
            presetMenuItem--;
            if (presetMenuItem < 0) presetMenuItem = PRESET_ITEMS - 1;
        }
        if (wasButtonPressed(1)) { // DOWN
            presetMenuItem++;
            if (presetMenuItem >= PRESET_ITEMS) presetMenuItem = 0;
        }
        if (wasButtonPressed(2)) { // SELECT - Apply preset
            BeeType selectedType = (BeeType)(presetMenuItem + 1); // +1 to skip custom
            
            // Apply the preset
            applyBeePreset(settings, selectedType);
            saveSettings(settings);
            
            // Show success message
            display.clearDisplay();
            display.setCursor(15, 20);
            display.print(F("Preset Applied!"));
            display.setCursor(10, 30);
            display.print(getBeeTypeName(selectedType));
            display.display();
            delay(2000);
            return;
        }
        if (wasButtonPressed(3)) { // BACK
            state.menuLevel = 0;
            state.selectedItem = 1; // Return to bee presets in main menu
        }
        
        drawBeePresetMenu(display, presetMenuItem, settings);
    }
}

void drawBeePresetMenu(Adafruit_SH1106G& display, int selected, SystemSettings& settings) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    display.setCursor(15, 0);
    display.println(F("Bee Type Presets"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    // Show current type
    BeeType currentType = detectCurrentBeeType(settings);
    display.setCursor(0, 14);
    display.print(F("Current: "));
    display.print(getBeeTypeName(currentType));
    
    // List available presets (skip custom)
    for (int i = 0; i < 3 && i < (NUM_BEE_PRESETS - 1); i++) {
        int presetIdx = i + 1; // +1 to skip custom
        int y = 26 + (i * 12);
        
        if (i == selected) {
            display.setCursor(0, y);
            display.print(F(">"));
        }
        
        display.setCursor(12, y);
        display.print(BEE_PRESETS[presetIdx].name);
    }
    
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
    display.setTextColor(SH110X_WHITE);
    
    display.setCursor(20, 0);
    display.println(F("Alert Thresholds"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
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
    static int systemMenuItem = 0;
    const int SYSTEM_ITEMS = 4; // Brightness + Field Mode + Display Timeout + Factory Reset
    
    extern PowerManager powerManager;
    
    if (!editing) {
        if (wasButtonPressed(0)) { // UP
            systemMenuItem--;
            if (systemMenuItem < 0) systemMenuItem = SYSTEM_ITEMS - 1;
        }
        if (wasButtonPressed(1)) { // DOWN
            systemMenuItem++;
            if (systemMenuItem >= SYSTEM_ITEMS) systemMenuItem = 0;
        }
        if (wasButtonPressed(2)) { // SELECT
            if (systemMenuItem == 3) { // Factory Reset
                // Show confirmation dialog
                display.clearDisplay();
                display.setTextSize(1);
                display.setTextColor(SH110X_WHITE);
                display.setCursor(15, 0);
                display.println(F("FACTORY RESET"));
                display.drawLine(0, 10, 127, 10, SH110X_WHITE);
                display.setCursor(0, 20);
                display.println(F("This will erase ALL"));
                display.setCursor(0, 30);
                display.println(F("settings!"));
                display.setCursor(0, 45);
                display.println(F("UP:Cancel DOWN:Reset"));
                display.display();
                
                // Wait for confirmation
                unsigned long startTime = millis();
                while (millis() - startTime < 5000) {
                    updateButtonStates();
                    if (wasButtonPressed(0)) return; // Cancel
                    if (wasButtonPressed(1)) {
                        // Perform reset
                        extern SystemStatus systemStatus;
                        performFactoryReset(settings, systemStatus, display);
                        return;
                    }
                    delay(50);
                }
                return;
            } else {
                editing = true;
                switch (systemMenuItem) {
                    case 0: state.editIntValue = settings.displayBrightness; break;
                    case 1: state.editIntValue = settings.fieldModeEnabled ? 1 : 0; break;
                    case 2: state.editIntValue = settings.displayTimeoutMin; break;
                }
            }
        }
        if (wasButtonPressed(3)) { // BACK
            state.menuLevel = 0;
            state.selectedItem = 5;
        }
        
        drawSystemMenu(display, systemMenuItem, settings);
    } else {
        // Handle editing for brightness, field mode, timeout
        if (systemMenuItem == 0) { // Display Brightness
            if (wasButtonPressed(0) || shouldRepeat(0)) {
                state.editIntValue++;
                if (state.editIntValue > 10) state.editIntValue = 10;
            }
            if (wasButtonPressed(1) || shouldRepeat(1)) {
                state.editIntValue--;
                if (state.editIntValue < 1) state.editIntValue = 1;
            }
            if (wasButtonPressed(2)) { // Save
                settings.displayBrightness = state.editIntValue;
                editing = false;
                saveSettings(settings);
            }
            drawEditIntValue(display, "Brightness", state.editIntValue, "/10");
            
        } else if (systemMenuItem == 1) { // Field Mode
            if (wasButtonPressed(0) || wasButtonPressed(1)) {
                state.editIntValue = 1 - state.editIntValue; // Toggle
            }
            if (wasButtonPressed(2)) { // Save
                settings.fieldModeEnabled = (state.editIntValue == 1);
                powerManager.setFieldMode(settings.fieldModeEnabled);
                editing = false;
                saveSettings(settings);
            }
            drawEditBoolValue(display, "Field Mode", state.editIntValue == 1);
            
        } else if (systemMenuItem == 2) { // Display Timeout
            if (wasButtonPressed(0) || shouldRepeat(0)) {
                state.editIntValue++;
                if (state.editIntValue > 30) state.editIntValue = 30;
            }
            if (wasButtonPressed(1) || shouldRepeat(1)) {
                state.editIntValue--;
                if (state.editIntValue < 1) state.editIntValue = 1;
            }
            if (wasButtonPressed(2)) { // Save
                settings.displayTimeoutMin = state.editIntValue;
                powerManager.setDisplayTimeout(settings.displayTimeoutMin);
                editing = false;
                saveSettings(settings);
            }
            drawEditIntValue(display, "Timeout", state.editIntValue, "min");
        }
        
        if (wasButtonPressed(3)) { // BACK - Cancel
            editing = false;
        }
    }
}

void drawSystemMenu(Adafruit_SH1106G& display, int selected, SystemSettings& settings) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    display.setCursor(25, 0);
    display.println(F("System Settings"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    // Display Brightness
    if (selected == 0) {
        display.setCursor(0, 16);
        display.print(F(">"));
    }
    display.setCursor(12, 16);
    display.print(F("Brightness: "));
    display.print(settings.displayBrightness);
    display.print(F("/10"));
    
    // Field Mode
    if (selected == 1) {
        display.setCursor(0, 26);
        display.print(F(">"));
    }
    display.setCursor(12, 26);
    display.print(F("Field Mode: "));
    display.print(settings.fieldModeEnabled ? "ON" : "OFF");
    
    // Display Timeout
    if (selected == 2) {
        display.setCursor(0, 36);
        display.print(F(">"));
    }
    display.setCursor(12, 36);
    display.print(F("Timeout: "));
    display.print(settings.displayTimeoutMin);
    display.print(F("min"));
    
    // Factory Reset
    if (selected == 3) {
        display.setCursor(0, 46);
        display.print(F(">"));
    }
    display.setCursor(12, 46);
    display.print(F("Factory Reset"));
    
    // Show current power mode at bottom
    extern PowerManager powerManager;
    display.setCursor(0, 56);
    display.print(F("Mode: "));
    display.print(powerManager.getPowerModeString());
    
    display.display();
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

void drawEditIntValue(Adafruit_SH1106G& display, const char* title, int value, const char* unit) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    display.setCursor(30, 0);
    display.println(title);
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
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
    display.setTextColor(SH110X_WHITE);
    
    display.setCursor(40, 0);
    display.println(title);
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
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

// =============================================================================
// TEXT EDITING FOR BLUETOOTH NAMES
// =============================================================================

// Simple character set for naming (no spaces, field-friendly)
const char CHAR_SET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
const int CHAR_SET_SIZE = sizeof(CHAR_SET) - 1;

void handleTextEdit(char* textBuffer, int maxLength, const char* title, 
                   Adafruit_SH1106G& display, bool& editing) {
    static int cursorPos = 0;
    static int charIndex = 0;
    
    if (!editing) {
        cursorPos = strlen(textBuffer);
        if (cursorPos >= maxLength) cursorPos = maxLength - 1;
        charIndex = 0;
        editing = true;
    }
    
    if (wasButtonPressed(0)) { // UP - next character
        charIndex++;
        if (charIndex >= CHAR_SET_SIZE) charIndex = 0;
    }
    
    if (wasButtonPressed(1)) { // DOWN - previous character  
        charIndex--;
        if (charIndex < 0) charIndex = CHAR_SET_SIZE - 1;
    }
    
    if (wasButtonPressed(2)) { // SELECT - confirm character and move cursor
        if (cursorPos < maxLength - 1) {
            textBuffer[cursorPos] = CHAR_SET[charIndex];
            textBuffer[cursorPos + 1] = '\0';
            cursorPos++;
            charIndex = 0;
        }
    }
    
    if (wasButtonPressed(3)) { // BACK - save and exit OR delete character
        if (strlen(textBuffer) > 0 && cursorPos > 0) {
            // Delete last character
            cursorPos--;
            textBuffer[cursorPos] = '\0';
            charIndex = 0;
        } else {
            // Save and exit if string is not empty
            if (strlen(textBuffer) > 0) {
                editing = false;
            }
        }
    }
    
    drawTextEditor(display, title, textBuffer, cursorPos, CHAR_SET[charIndex]);
}

void drawTextEditor(Adafruit_SH1106G& display, const char* title, 
                   const char* text, int cursorPos, char currentChar) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    // Title
    display.setCursor(5, 0);
    display.print(F("Edit: "));
    display.print(title);
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    // Current text
    display.setCursor(5, 20);
    display.print(F("Text: "));
    display.print(text);
    
    // Show cursor and current character
    display.setCursor(5, 32);
    display.print(F("Char: "));
    display.setTextSize(2);
    display.print(currentChar);
    
    // Instructions
    display.setTextSize(1);
    display.setCursor(0, 48);
    display.print(F("UP/DN:Char SEL:Add"));
    display.setCursor(0, 56);
    display.print(F("BACK:Del/Save"));
    
    display.display();
}

// =============================================================================
// ENHANCED BLUETOOTH MENU WITH TEXT EDITING
// =============================================================================

void handleBluetoothMenu(Adafruit_SH1106G& display, MenuState& state, 
                        SystemSettings& settings, bool& editingInitialized,
                        bool* buttonPressed) {
    static bool editing = false;
    static int bluetoothMenuItem = 0;
    const int BLUETOOTH_ITEMS = 8;  // Mode, Manual Timeout, Start Hour, End Hour, Device ID, Hive Name, Location, Beekeeper
    
    extern BluetoothManager bluetoothManager;
    BluetoothSettings& btSettings = bluetoothManager.getSettings();
    
    if (!editing) {
        if (wasButtonPressed(0)) { // UP
            bluetoothMenuItem--;
            if (bluetoothMenuItem < 0) bluetoothMenuItem = BLUETOOTH_ITEMS - 1;
        }
        if (wasButtonPressed(1)) { // DOWN
            bluetoothMenuItem++;
            if (bluetoothMenuItem >= BLUETOOTH_ITEMS) bluetoothMenuItem = 0;
        }
        if (wasButtonPressed(2)) { // SELECT
            editing = true;
            switch (bluetoothMenuItem) {
                case 0: state.editIntValue = (int)btSettings.mode; break;
                case 1: state.editIntValue = btSettings.manualTimeoutMin; break;
                case 2: state.editIntValue = btSettings.scheduleStartHour; break;
                case 3: state.editIntValue = btSettings.scheduleEndHour; break;
                case 4: state.editIntValue = btSettings.deviceId; break;
                case 5: // Hive Name - will be handled separately
                case 6: // Location - will be handled separately  
                case 7: // Beekeeper - will be handled separately
                    break;
            }
        }
        if (wasButtonPressed(3)) { // BACK
            state.menuLevel = 0;
            state.selectedItem = 6;  // Return to Bluetooth in main menu
        }
        
        drawBluetoothMenu(display, bluetoothMenuItem, btSettings);
        
    } else {
        // Editing mode
        if (bluetoothMenuItem == 0) { // Bluetooth Mode
            if (wasButtonPressed(0) || wasButtonPressed(1)) { // UP or DOWN - cycle through modes
                state.editIntValue++;
                if (state.editIntValue > 3) state.editIntValue = 0;  // 0=Off, 1=Manual, 2=Scheduled, 3=Always On
            }
            if (wasButtonPressed(2)) { // SELECT - Save
                bluetoothManager.setMode((BluetoothMode)state.editIntValue);
                editing = false;
            }
            drawEditBluetoothMode(display, (BluetoothMode)state.editIntValue);
            
        } else if (bluetoothMenuItem == 1) { // Manual Timeout
            if (wasButtonPressed(0) || shouldRepeat(0)) { // UP
                state.editIntValue += 5;
                if (state.editIntValue > 120) state.editIntValue = 120;
            }
            if (wasButtonPressed(1) || shouldRepeat(1)) { // DOWN
                state.editIntValue -= 5;
                if (state.editIntValue < 5) state.editIntValue = 5;
            }
            if (wasButtonPressed(2)) { // SELECT - Save
                bluetoothManager.setManualTimeout(state.editIntValue);
                editing = false;
            }
            drawEditIntValue(display, "Manual Timeout", state.editIntValue, "min");
            
        } else if (bluetoothMenuItem == 2 || bluetoothMenuItem == 3) { // Schedule Hours
            if (wasButtonPressed(0) || shouldRepeat(0)) { // UP
                state.editIntValue++;
                if (state.editIntValue > 23) state.editIntValue = 0;
            }
            if (wasButtonPressed(1) || shouldRepeat(1)) { // DOWN
                state.editIntValue--;
                if (state.editIntValue < 0) state.editIntValue = 23;
            }
            if (wasButtonPressed(2)) { // SELECT - Save
                if (bluetoothMenuItem == 2) {
                    bluetoothManager.setSchedule(state.editIntValue, btSettings.scheduleEndHour);
                } else {
                    bluetoothManager.setSchedule(btSettings.scheduleStartHour, state.editIntValue);
                }
                editing = false;
            }
            const char* title = (bluetoothMenuItem == 2) ? "Schedule Start" : "Schedule End";
            drawEditIntValue(display, title, state.editIntValue, ":00");
            
        } else if (bluetoothMenuItem == 4) { // Device ID
            if (wasButtonPressed(0) || shouldRepeat(0)) { // UP
                state.editIntValue++;
                if (state.editIntValue > 255) state.editIntValue = 1;
            }
            if (wasButtonPressed(1) || shouldRepeat(1)) { // DOWN
                state.editIntValue--;
                if (state.editIntValue < 1) state.editIntValue = 255;
            }
            if (wasButtonPressed(2)) { // SELECT - Save
                btSettings.deviceId = state.editIntValue;
                bluetoothManager.saveBluetoothSettings();
                editing = false;
            }
            drawEditIntValue(display, "Device ID", state.editIntValue, "");
            
        } else if (bluetoothMenuItem == 5) { // Hive Name
            static bool textEditing = false;
            static char tempName[16];
            
            if (!textEditing) {
                strcpy(tempName, btSettings.hiveName);
                textEditing = true;
            }
            
            handleTextEdit(tempName, sizeof(tempName), "Hive Name", display, textEditing);
            
            if (!textEditing) {
                strcpy(btSettings.hiveName, tempName);
                bluetoothManager.saveBluetoothSettings();
                editing = false;
            }
            
        } else if (bluetoothMenuItem == 6) { // Location
            static bool textEditing = false;
            static char tempLocation[24];
            
            if (!textEditing) {
                strcpy(tempLocation, btSettings.location);
                textEditing = true;
            }
            
            handleTextEdit(tempLocation, sizeof(tempLocation), "Location", display, textEditing);
            
            if (!textEditing) {
                strcpy(btSettings.location, tempLocation);
                bluetoothManager.saveBluetoothSettings();
                editing = false;
            }
            
        } else if (bluetoothMenuItem == 7) { // Beekeeper
            static bool textEditing = false;
            static char tempBeekeeper[16];
            
            if (!textEditing) {
                strcpy(tempBeekeeper, btSettings.beekeeper);
                textEditing = true;
            }
            
            handleTextEdit(tempBeekeeper, sizeof(tempBeekeeper), "Beekeeper", display, textEditing);
            
            if (!textEditing) {
                strcpy(btSettings.beekeeper, tempBeekeeper);
                bluetoothManager.saveBluetoothSettings();
                editing = false;
            }
        }
        
        if (wasButtonPressed(3)) { // BACK - Cancel
            editing = false;
        }
    }
}
