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

#endif // SETTINGS_H