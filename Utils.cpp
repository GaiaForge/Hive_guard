/**
 * Utils.cpp
 * Utility functions implementation
 */

#include "Utils.h"

// =============================================================================
// BUTTON HANDLING
// =============================================================================

// Replace the button handling section in Utils.cpp with this enhanced version:

// =============================================================================
// BUTTON HANDLING WITH LONG PRESS
// =============================================================================

// Button state tracking
static bool buttonStates[4] = {false, false, false, false};
static bool lastButtonStates[4] = {false, false, false, false};
static unsigned long lastDebounceTime[4] = {0, 0, 0, 0};
static bool buttonPressed[4] = {false, false, false, false};
static unsigned long buttonPressStartTime[4] = {0, 0, 0, 0};
static bool longPressActive[4] = {false, false, false, false};
static unsigned long lastRepeatTime[4] = {0, 0, 0, 0};

// Long press configuration
const unsigned long LONG_PRESS_DELAY = 500;     // Time before long press starts (ms)
const unsigned long INITIAL_REPEAT_DELAY = 300;  // First repeat delay
const unsigned long REPEAT_INTERVAL = 100;       // Subsequent repeat interval

void updateButtonStates() {
    unsigned long currentTime = millis();
    
    for (int i = 0; i < 4; i++) {
        bool reading = readButton(i);
        
        if (reading != lastButtonStates[i]) {
            lastDebounceTime[i] = currentTime;
        }
        
        if ((currentTime - lastDebounceTime[i]) > DEBOUNCE_DELAY) {
            if (reading != buttonStates[i]) {
                buttonStates[i] = reading;
                
                if (buttonStates[i]) {
                    // Button just pressed
                    buttonPressed[i] = true;
                    buttonPressStartTime[i] = currentTime;
                    longPressActive[i] = false;
                    lastRepeatTime[i] = currentTime;
                } else {
                    // Button released
                    longPressActive[i] = false;
                }
            }
        }
        
        // Handle long press
        if (buttonStates[i] && !longPressActive[i]) {
            if (currentTime - buttonPressStartTime[i] >= LONG_PRESS_DELAY) {
                // Long press threshold reached
                longPressActive[i] = true;
                lastRepeatTime[i] = currentTime;
            }
        }
        
        lastButtonStates[i] = reading;
    }
}

bool wasButtonPressed(int button) {
    if (button < 0 || button > 3) return false;
    
    if (buttonPressed[button]) {
        buttonPressed[button] = false;
        return true;
    }
    return false;
}

bool isButtonHeld(int button) {
    if (button < 0 || button > 3) return false;
    return buttonStates[button];
}

bool isLongPress(int button) {
    if (button < 0 || button > 3) return false;
    return longPressActive[button];
}

bool shouldRepeat(int button) {
    if (button < 0 || button > 3) return false;
    if (!longPressActive[button]) return false;
    
    unsigned long currentTime = millis();
    unsigned long timeSinceStart = currentTime - buttonPressStartTime[button];
    unsigned long timeSinceRepeat = currentTime - lastRepeatTime[button];
    
    // Use different repeat rates based on how long button has been held
    unsigned long repeatDelay = REPEAT_INTERVAL;
    if (timeSinceStart < LONG_PRESS_DELAY + INITIAL_REPEAT_DELAY) {
        repeatDelay = INITIAL_REPEAT_DELAY;
    }
    
    if (timeSinceRepeat >= repeatDelay) {
        lastRepeatTime[button] = currentTime;
        return true;
    }
    
    return false;
}

void resetButtonStates() {
    for (int i = 0; i < 4; i++) {
        buttonPressed[i] = false;
        buttonStates[i] = false;
        lastButtonStates[i] = false;
        longPressActive[i] = false;
    }
}

bool readButton(int buttonNum) {
    int pin;
    switch (buttonNum) {
        case 0: pin = BTN_UP; break;
        case 1: pin = BTN_DOWN; break;
        case 2: pin = BTN_SELECT; break;
        case 3: pin = BTN_BACK; break;
        default: return false;
    }
    
    // Read analog value and consider pressed if below threshold
    return (analogRead(pin) < 100);
}


// =============================================================================
// STRING CONVERSIONS
// =============================================================================

const char* getBeeStateString(uint8_t state) {
    switch (state) {
        case BEE_QUIET: return "QUIET";
        case BEE_NORMAL: return "NORMAL";
        case BEE_ACTIVE: return "ACTIVE";
        case BEE_QUEEN_PRESENT: return "QUEEN_OK";
        case BEE_QUEEN_MISSING: return "NO_QUEEN";
        case BEE_PRE_SWARM: return "PRE_SWARM";
        case BEE_DEFENSIVE: return "DEFENSIVE";
        case BEE_STRESSED: return "STRESSED";
        default: return "UNKNOWN";
    }
}

const char* getMonthName(int month) {
    const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    
    if (month >= 1 && month <= 12) {
        return months[month - 1];
    }
    return "???";
}

// =============================================================================
// DATE/TIME FUNCTIONS
// =============================================================================

int getDaysInMonth(int month, int year) {
    const int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    if (month < 1 || month > 12) return 30;
    
    int days = daysInMonth[month - 1];
    
    // Check for leap year (February)
    if (month == 2 && isLeapYear(year)) {
        days = 29;
    }
    
    return days;
}

bool isLeapYear(int year) {
    return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

String formatTimestamp(DateTime dt) {
    char buffer[20];
    sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
            dt.year(), dt.month(), dt.day(),
            dt.hour(), dt.minute(), dt.second());
    return String(buffer);
}

// =============================================================================
// MATHEMATICAL FUNCTIONS
// =============================================================================

float calculateDewPoint(float temperature, float humidity) {
    // Magnus formula approximation
    float a = 17.27;
    float b = 237.7;
    float alpha = ((a * temperature) / (b + temperature)) + log(humidity / 100.0);
    float dewPoint = (b * alpha) / (a - alpha);
    return dewPoint;
}

float celsiusToFahrenheit(float celsius) {
    return (celsius * 9.0 / 5.0) + 32.0;
}

float fahrenheitToCelsius(float fahrenheit) {
    return (fahrenheit - 32.0) * 5.0 / 9.0;
}

// =============================================================================
// STATISTICAL FUNCTIONS
// =============================================================================

float calculateAverage(float* values, int count) {
    if (count == 0) return 0;
    
    float sum = 0;
    for (int i = 0; i < count; i++) {
        sum += values[i];
    }
    return sum / count;
}

float calculateStandardDeviation(float* values, int count) {
    if (count < 2) return 0;
    
    float avg = calculateAverage(values, count);
    float sumSquares = 0;
    
    for (int i = 0; i < count; i++) {
        float diff = values[i] - avg;
        sumSquares += diff * diff;
    }
    
    return sqrt(sumSquares / (count - 1));
}

// =============================================================================
// MEMORY FUNCTIONS
// =============================================================================

int getFreeMemory() {
    // Simple implementation that works for most Arduino boards
    // For nRF52, this gives an approximation
    char top;
    return &top - reinterpret_cast<char*>(malloc(1));
}

void printMemoryInfo() {
    Serial.print(F("Free memory: "));
    Serial.print(getFreeMemory());
    Serial.println(F(" bytes"));
}

// =============================================================================
// VALIDATION FUNCTIONS
// =============================================================================

bool isValidTemperature(float temp) {
    return (temp >= -50.0 && temp <= 100.0);
}

bool isValidHumidity(float humidity) {
    return (humidity >= 0.0 && humidity <= 100.0);
}

bool isValidPressure(float pressure) {
    return (pressure >= 300.0 && pressure <= 1100.0);
}

// =============================================================================
// SYSTEM UTILITIES
// =============================================================================

void performSystemReset() {
    Serial.println(F("System reset requested..."));
    delay(100);
    
#ifdef NRF52_SERIES
    NVIC_SystemReset();
#else
    // For other boards, use watchdog timer
    // This is platform-specific
    while (1); // Hang to trigger watchdog
#endif
}

void enterDeepSleep(uint32_t seconds) {
#ifdef NRF52_SERIES
    // nRF52 deep sleep implementation
    // This would require specific power management setup
    Serial.print(F("Entering deep sleep for "));
    Serial.print(seconds);
    Serial.println(F(" seconds"));
    
    // Simplified - actual implementation would use nRF52 power management
    delay(seconds * 1000);
#else
    // For other platforms
    Serial.println(F("Deep sleep not implemented for this platform"));
#endif
}

// =============================================================================
// DEBUG UTILITIES
// =============================================================================

void printSystemInfo() {
    Serial.println(F("\n=== System Information ==="));
    
#ifdef NRF52_SERIES
    Serial.println(F("Platform: nRF52"));
#else
    Serial.println(F("Platform: Unknown"));
#endif
    
    Serial.print(F("CPU Speed: "));
    Serial.print(F_CPU / 1000000);
    Serial.println(F(" MHz"));
    
    printMemoryInfo();
    
    Serial.print(F("Uptime: "));
    Serial.print(millis() / 1000);
    Serial.println(F(" seconds"));
    
    Serial.println(F("=======================\n"));
}

void hexDump(uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (i % 16 == 0) {
            Serial.print(F("\n"));
            if (i < 0x10) Serial.print(F("0"));
            Serial.print(i, HEX);
            Serial.print(F(": "));
        }
        
        if (data[i] < 0x10) Serial.print(F("0"));
        Serial.print(data[i], HEX);
        Serial.print(F(" "));
    }
    Serial.println();
}

// =============================================================================
// ERROR HANDLING
// =============================================================================

void handleError(const char* errorMsg, bool fatal) {
    Serial.print(F("ERROR: "));
    Serial.println(errorMsg);
    
    // Log to SD if available
    // This would require access to SD card object
    
    if (fatal) {
        Serial.println(F("FATAL ERROR - System halted"));
        while (1) {
            // Flash LED or other indicator
            delay(500);
        }
    }
}