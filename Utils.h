/**
 * Utils.h
 * Utility functions header
 */

#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include "Config.h"
#include "DataStructures.h"

// Button handling functions
void updateButtonStates();
bool wasButtonPressed(int button);
bool isButtonHeld(int button);
bool isLongPress(int button);
bool shouldRepeat(int button);
void resetButtonStates();
bool readButton(int buttonNum);

// String conversion functions
const char* getBeeStateString(uint8_t state);
const char* getMonthName(int month);

// Date/time functions
int getDaysInMonth(int month, int year);
bool isLeapYear(int year);
String formatTimestamp(DateTime dt);

// Mathematical functions
float calculateDewPoint(float temperature, float humidity);
float celsiusToFahrenheit(float celsius);
float fahrenheitToCelsius(float fahrenheit);

// Statistical functions
float calculateAverage(float* values, int count);
float calculateStandardDeviation(float* values, int count);

// Memory functions
int getFreeMemory();
void printMemoryInfo();

// Validation functions
bool isValidTemperature(float temp);
bool isValidHumidity(float humidity);
bool isValidPressure(float pressure);

// System utilities
void performSystemReset();
void enterDeepSleep(uint32_t seconds);

// Debug utilities
void printSystemInfo();
void hexDump(uint8_t* data, size_t length);

// Error handling
void handleError(const char* errorMsg, bool fatal);

#endif // UTILS_H