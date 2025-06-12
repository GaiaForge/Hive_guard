/**
 * Menu.h
 * Settings menu system header
 */

#ifndef MENU_H
#define MENU_H

#include "Config.h"
#include "DataStructures.h"
#include "Display.h"

// Function declarations
void enterSettingsMenu(MenuState& state, DisplayMode& mode);
void exitSettingsMenu(MenuState& state, DisplayMode& mode, SystemSettings& settings);
void handleSettingsMenu(Adafruit_SH1106G& display, MenuState& state, 
                       SystemSettings& settings, RTC_DS3231& rtc,
                       SensorData& currentData, SystemStatus& status);

// Menu drawing functions
void drawMainSettingsMenu(Adafruit_SH1106G& display, int selected);
void drawTimeDateMenu(Adafruit_SH1106G& display, int selected, DateTime dt);
void drawTimeDateMenuWithEdit(Adafruit_SH1106G& display, int selected, 
                             DateTime dt, bool editMode, int editValue);  // ADD THIS LINE
void drawEditValueScreen(Adafruit_SH1106G& display, int item, int value);
void drawSensorCalibMenu(Adafruit_SH1106G& display, int selected, 
                        SystemSettings& settings, SensorData& currentData);
void drawEditFloatValue(Adafruit_SH1106G& display, const char* title, 
                       float value, const char* unit);
void drawEditIntValue(Adafruit_SH1106G& display, const char* title, int value, const char* unit);
void drawEditBoolValue(Adafruit_SH1106G& display, const char* title, bool value);
void drawAudioMenu(Adafruit_SH1106G& display, int selected, SystemSettings& settings);
void drawLoggingMenu(Adafruit_SH1106G& display, int selected, SystemSettings& settings);
void drawAlertMenu(Adafruit_SH1106G& display, int selected, SystemSettings& settings);
void drawSystemMenu(Adafruit_SH1106G& display, int selected, SystemSettings& settings);

// Sub-menu handlers
void handleTimeDateMenu(Adafruit_SH1106G& display, MenuState& state, 
                       SystemSettings& settings, RTC_DS3231& rtc,
                       DateTime& editDateTime, bool& editingInitialized,
                       bool* buttonPressed, SystemStatus& status);
void handleSensorCalibMenu(Adafruit_SH1106G& display, MenuState& state, 
                          SystemSettings& settings, SensorData& currentData,
                          bool& editingInitialized, bool* buttonPressed);
void handleAudioMenu(Adafruit_SH1106G& display, MenuState& state, 
                    SystemSettings& settings, bool& editingInitialized,
                    bool* buttonPressed);
void handleLoggingMenu(Adafruit_SH1106G& display, MenuState& state, 
                      SystemSettings& settings, bool& editingInitialized,
                      bool* buttonPressed);
void handleAlertMenu(Adafruit_SH1106G& display, MenuState& state, 
                    SystemSettings& settings, bool& editingInitialized,
                    bool* buttonPressed);
void handleSystemMenu(Adafruit_SH1106G& display, MenuState& state, 
                     SystemSettings& settings, bool& editingInitialized,
                     bool* buttonPressed);

// Helper functions
const char* getAudioMenuTitle(int item);
const char* getAudioMenuUnit(int item);

#endif // MENU_H