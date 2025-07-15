/**
 * Settings.h
 * Settings management header
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "Config.h"
#include "DataStructures.h"

// For nRF52840, we'll use flash_nrf5x library
#ifdef NRF52_SERIES
  #include <Adafruit_LittleFS.h>
  #include <InternalFileSystem.h>
  using namespace Adafruit_LittleFS_Namespace;
#endif

// Function declarations
SystemSettings getDefaultSettings();
uint16_t calculateChecksum(SystemSettings* s);
void loadSettings(SystemSettings& settings);
void saveSettings(SystemSettings& settings);
void validateSettings(SystemSettings& settings);
void exportSettingsToSD(SystemSettings& settings);

// ADDED: Missing function declaration that main.cpp calls
void printSettingsInfo(const SystemSettings& settings);

void clearUserData();
bool confirmFactoryReset(Adafruit_SH1106G& display);
void performFactoryReset(SystemSettings& settings, SystemStatus& status, 
                        Adafruit_SH1106G& display);

#endif // SETTINGS_H