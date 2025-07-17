/**
 * Utils.h
 * Utility functions header - nRF52 Hive Monitor System
 */

#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include "Config.h"
#include "DataStructures.h"

// =============================================================================
// MEMORY MANAGEMENT STRUCTURES
// =============================================================================

// Memory information structure for nRF52 system
struct MemoryInfo {
    uint32_t total_ram;          // Total system RAM (256KB)
    uint32_t app_ram_size;       // Available to application
    uint32_t stack_size;         // Total stack size
    uint32_t heap_size;          // Total heap size
    uint32_t static_size;        // BSS + DATA sections
    uint32_t free_heap;          // Current free heap
    uint32_t free_stack;         // Current free stack
    uint32_t used_stack;         // Current used stack
    uint32_t largest_free_block; // Largest contiguous free block
};

// =============================================================================
// MEMORY FUNCTIONS
// =============================================================================

int getFreeMemory();
int getFreeHeap();
int getFreeStack();
int getUsedStack();
MemoryInfo getMemoryInfo();
void printMemoryInfo();

// Development/debugging functions
void initStackWatermark();
int getStackHighWaterMark();

// Field deployment functions
bool isMemoryHealthy();
uint8_t getMemoryUsagePercent();

// =============================================================================
// PCF8523 RTC FUNCTIONS
// =============================================================================

void configurePCF8523ForFieldUse(RTC_PCF8523& rtc);
bool checkPCF8523Health(RTC_PCF8523& rtc);
void printPCF8523Status(RTC_PCF8523& rtc);

// =============================================================================
// BUTTON HANDLING FUNCTIONS
// =============================================================================

void updateButtonStates();
bool wasButtonPressed(int button);
bool isButtonHeld(int button);
bool isLongPress(int button);
bool shouldRepeat(int button);
void resetButtonStates();
bool readButton(int buttonNum);
bool wasBluetoothButtonPressed();
bool isBluetoothButtonHeld();

// =============================================================================
// STRING CONVERSION FUNCTIONS
// =============================================================================

const char* getBeeStateString(uint8_t state);
const char* getMonthName(int month);

// =============================================================================
// DATE/TIME FUNCTIONS
// =============================================================================

int getDaysInMonth(int month, int year);
bool isLeapYear(int year);
String formatTimestamp(DateTime dt);

// =============================================================================
// MATHEMATICAL FUNCTIONS
// =============================================================================

float calculateDewPoint(float temperature, float humidity);
float celsiusToFahrenheit(float celsius);
float fahrenheitToCelsius(float fahrenheit);

// =============================================================================
// STATISTICAL FUNCTIONS
// =============================================================================

float calculateAverage(float* values, int count);
float calculateStandardDeviation(float* values, int count);

// =============================================================================
// VALIDATION FUNCTIONS
// =============================================================================

bool isValidTemperature(float temp);
bool isValidHumidity(float humidity);
bool isValidPressure(float pressure);

// Environmental ML calculations
float calculateDewPoint(float temperature, float humidity);
float calculateVPD(float temperature, float humidity);
float calculateHeatIndex(float temperature, float humidity);
float calculateForagingComfortIndex(float temp, float humidity, float pressure);
float calculateEnvironmentalStress(float temp, float humidity, float pressure, 
                                 float tempMin, float tempMax, float humMin, float humMax);

// =============================================================================
// SYSTEM UTILITIES
// =============================================================================

void performSystemReset();
void enterDeepSleep(uint32_t seconds);
void performFactoryReset(SystemSettings& settings, SystemStatus& status, 
                        Adafruit_SH1106G& display);

// Watchdog functions
void setupWatchdog(SystemSettings& settings);
void feedWatchdog();
void updateWatchdogTimeout(SystemSettings& settings);

// System health monitoring
void checkSystemHealth(SystemStatus& status, SensorData& data);

// =============================================================================
// DEBUG UTILITIES
// =============================================================================

void printSystemInfo();
void hexDump(uint8_t* data, size_t length);

// =============================================================================
// ERROR HANDLING
// =============================================================================

void handleError(const char* errorMsg, bool fatal);

#endif // UTILS_H