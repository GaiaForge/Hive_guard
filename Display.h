/**
 * Display.h
 * Display management header
 */



#ifndef DISPLAY_H
#define DISPLAY_H

#include "Audio.h"  // For SpectralFeatures and ActivityTrend
#include "Config.h"
#include "DataStructures.h"
#include "Sensors.h"

// Function declarations
void showStartupScreen(Adafruit_SH1106G& display);


void updateDisplay(Adafruit_SH1106G& display, DisplayMode mode, 
                  SensorData& data, SystemSettings& settings,
                  SystemStatus& status, RTC_PCF8523& rtc,
                  SpectralFeatures& features, ActivityTrend& trend);

void drawTimeDateMenuWithEdit(Adafruit_SH1106G& display, int selected, 
                             DateTime dt, bool editMode, int editValue);

void showSensorDiagnosticsScreen(Adafruit_SH1106G& display, SystemStatus& status);
void updateDiagnosticLine(Adafruit_SH1106G& display, const char* message);
void checkSDCardAtStartup(Adafruit_SH1106G& display, SystemStatus& status);

// Screen drawing functions
void drawDashboard(Adafruit_SH1106G& display, SensorData& data, 
                  SystemStatus& status, RTC_PCF8523& rtc);
void drawSoundScreen(Adafruit_SH1106G& display, SensorData& data, 
                    SystemSettings& settings, SpectralFeatures& features,
                    ActivityTrend& trend);
void drawAlertsScreen(Adafruit_SH1106G& display, SensorData& data);
void drawPowerScreen(Adafruit_SH1106G& display, SensorData& data, 
                    SystemSettings& settings, SystemStatus& status);

                    
// UI component functions
void drawHeader(Adafruit_SH1106G& display, SensorData& data, SystemStatus& status);
void drawTimeDate(Adafruit_SH1106G& display, int y, RTC_PCF8523& rtc, 
                 SystemStatus& status);
void drawEnvironmentalData(Adafruit_SH1106G& display, int y, SensorData& data);
void drawBeeState(Adafruit_SH1106G& display, int x, int y, uint8_t state);
void drawBatteryIcon(Adafruit_SH1106G& display, int16_t x, int16_t y, float voltage);
void drawSoundLevelBar(Adafruit_SH1106G& display, int x, int y, 
                      int width, int height, uint8_t level);
void drawAlertLine(Adafruit_SH1106G& display, int y, const char* text, 
                  float value, const char* unit);
void drawBeeIcon(Adafruit_SH1106G& display, int x, int y);

void drawDetailedData(Adafruit_SH1106G& display, SensorData& data, 
                     SystemStatus& status, AbscondingIndicators& risk);
void drawDailySummary(Adafruit_SH1106G& display, DailyPattern& pattern,
                     AbscondingIndicators& indicators);


#endif // DISPLAY_H