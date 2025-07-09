/**
 * Utils.cpp
 * Utility functions implementation
 */

#include "Utils.h"
#include "math.h"
#include "Settings.h"    // for saveSettings()
#include "DataLogger.h"  // for SDLib::File
#ifdef NRF52_SERIES
#include <nrf.h>
#include <nrf_wdt.h>
#endif

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
    char buffer[25];
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

/**
 * PCF8523 Utility Functions
 * Additional functions for PCF8523 specific features
 */

void configurePCF8523ForFieldUse(RTC_PCF8523& rtc) {
    // Configure for low power field deployment
        
    // Enable battery switchover mode
    rtc.enableSecondTimer(); // For accurate timekeeping
    
    Serial.println(F("PCF8523 configured for field deployment"));
}

bool checkPCF8523Health(RTC_PCF8523& rtc) {
    // Check if RTC is still running and accurate
    
    // Check if oscillator is running
    if (!rtc.isrunning()) {
        Serial.println(F("Warning: PCF8523 oscillator stopped"));
        return false;
    }
    
    // Check for power issues
    if (rtc.lostPower()) {
        Serial.println(F("Warning: PCF8523 lost power"));
        return false;
    }
    
    return true;
}

void printPCF8523Status(RTC_PCF8523& rtc) {
    Serial.println(F("\n=== PCF8523 RTC Status ==="));
    
    DateTime now = rtc.now();
    Serial.print(F("Current time: "));
    Serial.println(now.timestamp(DateTime::TIMESTAMP_FULL));
    
    Serial.print(F("Running: "));
    Serial.println(rtc.isrunning() ? "YES" : "NO");
    
    Serial.print(F("Lost power: "));
    Serial.println(rtc.lostPower() ? "YES" : "NO");
    
    Serial.print(F("Initialized: "));
    Serial.println(rtc.initialized() ? "YES" : "NO");
    
    Serial.println(F("========================\n"));
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

// 2. Watchdog Timer Functions
void setupWatchdog(SystemSettings& settings) {
    #ifdef NRF52_SERIES
    // Calculate watchdog timeout based on logging interval
    uint32_t logIntervalSeconds = settings.logInterval * 60;  // Convert minutes to seconds
    
    // Watchdog timeout = 2x logging interval + 60 seconds safety margin
    // This ensures watchdog doesn't trigger during normal sleep/wake cycles
    uint32_t watchdogTimeoutSeconds = (logIntervalSeconds * 2) + 60;
    
    // Constrain to reasonable limits
    if (watchdogTimeoutSeconds < 120) {
        watchdogTimeoutSeconds = 120;  // Minimum 2 minutes
    }
    if (watchdogTimeoutSeconds > 7200) {
        watchdogTimeoutSeconds = 7200; // Maximum 2 hours
    }
    
    Serial.print(F("Setting up adaptive watchdog timer: "));
    Serial.print(watchdogTimeoutSeconds);
    Serial.println(F(" seconds"));
    Serial.print(F("Based on log interval: "));
    Serial.print(settings.logInterval);
    Serial.println(F(" minutes"));
    
    // Configure watchdog with calculated timeout
    NRF_WDT->CONFIG = (WDT_CONFIG_HALT_Pause << WDT_CONFIG_HALT_Pos) | 
                      (WDT_CONFIG_SLEEP_Run << WDT_CONFIG_SLEEP_Pos);
    NRF_WDT->CRV = watchdogTimeoutSeconds * 32768; // Convert to RTC ticks
    NRF_WDT->RREN |= WDT_RREN_RR0_Msk;
    NRF_WDT->TASKS_START = 1;
    
    Serial.println(F("Adaptive watchdog enabled"));
    
    #else
    Serial.println(F("Watchdog not available on this platform"));
    #endif
}

void feedWatchdog() {
    #ifdef NRF52_SERIES
    NRF_WDT->RR[0] = WDT_RR_RR_Reload;
    #endif
}

void updateWatchdogTimeout(SystemSettings& settings) {
    #ifdef NRF52_SERIES
    Serial.println(F("Reconfiguring watchdog for new logging interval..."));
    
    // Stop current watchdog (if possible - may need full reset)
    // Note: On nRF52, once started, watchdog cannot be stopped
    // So we'll just reconfigure the timeout value
    
    // Calculate new timeout
    uint32_t logIntervalSeconds = settings.logInterval * 60;
    uint32_t watchdogTimeoutSeconds = (logIntervalSeconds * 2) + 60;
    
    // Constrain to reasonable limits
    if (watchdogTimeoutSeconds < 120) {
        watchdogTimeoutSeconds = 120;
    }
    if (watchdogTimeoutSeconds > 7200) {
        watchdogTimeoutSeconds = 7200;
    }
    
    // For nRF52, we need to restart the system to change watchdog timeout
    // This is a limitation of the nRF52 watchdog - once started, it can't be reconfigured
    Serial.print(F("New watchdog timeout would be: "));
    Serial.print(watchdogTimeoutSeconds);
    Serial.println(F(" seconds"));
    Serial.println(F("Note: Watchdog timeout will apply after next system restart"));
    
    // Alternative: Just inform user that change will take effect on restart
    // The new timeout will be applied when setupWatchdog() is called on next boot
    
    #else
    Serial.println(F("Watchdog reconfiguration not available on this platform"));
    #endif
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

void performFactoryReset(SystemSettings& settings, SystemStatus& status, 
                        Adafruit_SH1106G& display) {
    Serial.println(F("=== FACTORY RESET INITIATED ==="));
    
    // Show progress on display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(20, 20);
    display.println(F("Resetting..."));
    display.setCursor(10, 30);
    display.println(F("Please wait"));
    display.display();
    
    // Load default settings
    initializeSystemSettings(settings);
    
    // Save default settings to flash
    saveSettings(settings);
    
    // Clear alert history if SD is working
    if (status.sdWorking) {
        if (SD.exists("/alerts.log")) {
            SD.remove("/alerts.log");
            Serial.println(F("Alert history cleared"));
        }
        
        // Create reset marker file
        SDLib::File resetMarker = SD.open("/factory_reset_performed.txt", FILE_WRITE);
        if (resetMarker) {
            resetMarker.print(F("Factory reset performed at: "));
            resetMarker.println(millis());
            resetMarker.close();
            Serial.println(F("Reset marker created"));
        }
    }
    
    Serial.println(F("Factory reset complete - all settings restored to defaults"));
    Serial.println(F("System will restart in 3 seconds..."));
    
    // Show completion message
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(15, 20);
    display.println(F("Reset complete"));
    display.setCursor(10, 30);
    display.println(F("Restarting..."));
    display.display();
    
    delay(3000);
    
    // Reset the system
    performSystemReset();
}


// =============================================================================
// SYSTEM HEALTH MONITORING
// =============================================================================

void checkSystemHealth(SystemStatus& status, SensorData& data) {
    static unsigned long lastHealthCheck = 0;
    if (millis() - lastHealthCheck < 30000) return; // Check every 30 seconds
    lastHealthCheck = millis();
    
    // Always feed watchdog during health check
    feedWatchdog();
    
    // Check for stuck sensors
    static float lastTemp = -999; // Use -999 as "uninitialized" marker
    static int stuckSensorCount = 0;
    
    if (lastTemp != -999 && abs(data.temperature - lastTemp) < 0.1) {
        stuckSensorCount++;
        if (stuckSensorCount > 10) { // Same reading for 5 minutes
            Serial.println(F("WARNING: Sensors may be stuck"));
            // Don't disable sensors - might just be stable environment
            // Log to SD if available
            if (status.sdWorking) {
                SDLib::File healthLog = SD.open("/health.log", FILE_WRITE);
                if (healthLog) {
                    healthLog.print(millis());
                    healthLog.print(F(",SENSOR_STUCK,"));
                    healthLog.println(data.temperature);
                    healthLog.close();
                }
            }
        }
    } else {
        stuckSensorCount = 0;
        lastTemp = data.temperature;
    }
    
    // Check audio buffer overflow
    extern volatile int audioSampleIndex;
    if (audioSampleIndex >= AUDIO_SAMPLE_BUFFER_SIZE) {
        Serial.println(F("WARNING: Audio buffer overflow - resetting"));
        audioSampleIndex = 0; // Emergency reset
        
        // Log the overflow
        if (status.sdWorking) {
            SDLib::File healthLog = SD.open("/health.log", FILE_WRITE);
            if (healthLog) {
                healthLog.print(millis());
                healthLog.println(F(",AUDIO_OVERFLOW"));
                healthLog.close();
            }
        }
    }
    
    // Check memory usage
    int freeMemory = getFreeMemory();
    if (freeMemory < 1000) { // Less than 1KB free
        Serial.print(F("WARNING: Low memory: "));
        Serial.print(freeMemory);
        Serial.println(F(" bytes"));
        
        // Log memory warning
        if (status.sdWorking) {
            SDLib::File healthLog = SD.open("/health.log", FILE_WRITE);
            if (healthLog) {
                healthLog.print(millis());
                healthLog.print(F(",LOW_MEMORY,"));
                healthLog.println(freeMemory);
                healthLog.close();
            }
        }
    }
    
    // Check system uptime and log periodic health status
    static unsigned long lastHealthLog = 0;
    if (millis() - lastHealthLog > 3600000UL) { // Log every hour
        lastHealthLog = millis();
        
        Serial.println(F("=== Hourly Health Check ==="));
        Serial.print(F("Uptime: ")); Serial.print(millis() / 3600000); Serial.println(F(" hours"));
        Serial.print(F("Free Memory: ")); Serial.print(freeMemory); Serial.println(F(" bytes"));
        Serial.print(F("RTC: ")); Serial.println(status.rtcWorking ? "OK" : "FAIL");
        Serial.print(F("BME280: ")); Serial.println(status.bmeWorking ? "OK" : "FAIL");
        Serial.print(F("SD Card: ")); Serial.println(status.sdWorking ? "OK" : "FAIL");
        Serial.print(F("Audio: ")); Serial.println(status.pdmWorking ? "OK" : "FAIL");
        Serial.print(F("Display: ")); Serial.println(status.displayWorking ? "OK" : "FAIL");
        Serial.println(F("=========================="));
        
        // Log to SD card for field monitoring
        if (status.sdWorking) {
            SDLib::File healthLog = SD.open("/health.log", FILE_WRITE);
            if (healthLog) {
                healthLog.print(millis());
                healthLog.print(F(",HOURLY_STATUS,"));
                healthLog.print(F("RTC:")); healthLog.print(status.rtcWorking ? 1 : 0);
                healthLog.print(F(",BME:")); healthLog.print(status.bmeWorking ? 1 : 0);
                healthLog.print(F(",SD:")); healthLog.print(status.sdWorking ? 1 : 0);
                healthLog.print(F(",AUDIO:")); healthLog.print(status.pdmWorking ? 1 : 0);
                healthLog.print(F(",DISP:")); healthLog.print(status.displayWorking ? 1 : 0);
                healthLog.print(F(",MEM:")); healthLog.print(freeMemory);
                healthLog.print(F(",TEMP:")); healthLog.println(data.temperature);
                healthLog.close();
            }
        }
    }
    
    // Feed watchdog at end of health check
    feedWatchdog();
}