/**
 * Display.h
 * Display management header
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include "Config.h"
#include "DataStructures.h"
#include "Sensors.h"

// Function declarations
void showStartupScreen(Adafruit_SH1106G& display);
void updateDisplay(Adafruit_SH1106G& display, DisplayMode mode, 
                  SensorData& data, SystemSettings& settings,
                  SystemStatus& status, RTC_DS3231& rtc);

// Screen drawing functions
void drawDashboard(Adafruit_SH1106G& display, SensorData& data, 
                  SystemStatus& status, RTC_DS3231& rtc);
void drawSoundScreen(Adafruit_SH1106G& display, SensorData& data, 
                    SystemSettings& settings);
void drawAlertsScreen(Adafruit_SH1106G& display, SensorData& data);

// UI component functions
void drawHeader(Adafruit_SH1106G& display, SensorData& data, SystemStatus& status);
void drawTimeDate(Adafruit_SH1106G& display, int y, RTC_DS3231& rtc, 
                 SystemStatus& status);
void drawEnvironmentalData(Adafruit_SH1106G& display, int y, SensorData& data);
void drawBeeState(Adafruit_SH1106G& display, int x, int y, uint8_t state);
void drawBatteryIcon(Adafruit_SH1106G& display, int16_t x, int16_t y, float voltage);
void drawSoundLevelBar(Adafruit_SH1106G& display, int x, int y, 
                      int width, int height, uint8_t level);
void drawAlertLine(Adafruit_SH1106G& display, int y, const char* text, 
                  float value, const char* unit);
void drawBeeIcon(Adafruit_SH1106G& display, int x, int y);

#endif // DISPLAY_H