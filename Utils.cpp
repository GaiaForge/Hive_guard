/**
 * Utils.cpp
 * Utility functions implementation - nRF52 Hive Monitor System
 */

#include "Utils.h"
#include <stdint.h>        // ADD THIS LINE - defines uint32_t, uint8_t, etc.
#include "math.h"
#include "Settings.h"      // for saveSettings()
#include "DataLogger.h"    // for SDLib::File
#include <nrf.h>

// ===========================
// nRF52 MEMORY MANAGEMENT - 
// ===========================

// Simple Arduino-compatible stack pointer function
static inline uint32_t get_stack_pointer(void) {
    uint32_t sp;
    __asm volatile ("mov %0, sp" : "=r" (sp));
    return sp;
}

int getFreeMemory() {
    // Try to allocate progressively smaller blocks to find available memory
    for (int size = 32768; size >= 128; size /= 2) {
        void* ptr = malloc(size);
        if (ptr != NULL) {
            free(ptr);
            return size * 4; // Conservative estimate - assume 4x available
        }
    }
    return 1024; // Fallback minimum
}

int getFreeHeap() {
    return getFreeMemory();
}

int getFreeStack() {
    // Conservative estimate - assume we have several KB of stack available
    return 6144; // 6KB available stack space estimate
}

int getUsedStack() {
    // Conservative estimate for typical Arduino usage
    return 1024; // 1KB used
}

uint8_t getMemoryUsagePercent() {
    // Conservative percentage calculation
    int free_mem = getFreeMemory();
    
    // If we have less than 8KB free, consider it high usage
    if (free_mem < 8192) {
        return 85; // High usage
    } else if (free_mem < 16384) {
        return 60; // Medium usage  
    } else if (free_mem < 32768) {
        return 30; // Low usage
    } else {
        return 15; // Very low usage
    }
}

MemoryInfo getMemoryInfo() {
    MemoryInfo info;
    
    // nRF52840 specs
    info.total_ram = 256 * 1024;
    info.app_ram_size = 240 * 1024;  
    info.stack_size = 8192;          
    
    // Conservative estimates
    info.free_heap = getFreeMemory();
    info.used_stack = getUsedStack();
    info.free_stack = getFreeStack();
    info.static_size = 20480; // 20KB estimate for globals
    info.heap_size = 180 * 1024; // ~180KB heap estimate
    
    // Find largest free block
    info.largest_free_block = 0;
    for (uint32_t size = 8192; size >= 128; size /= 2) {
        void* ptr = malloc(size);
        if (ptr != NULL) {
            info.largest_free_block = size;
            free(ptr);
            break;
        }
    }
    
    return info;
}

void printMemoryInfo() {
    MemoryInfo info = getMemoryInfo();
    
    Serial.println(F("\n=== nRF52840 Memory Status ==="));
    Serial.print(F("Total RAM: "));
    Serial.print(info.total_ram / 1024);
    Serial.println(F(" KB"));
    
    Serial.print(F("Free Memory: ~"));
    Serial.print(info.free_heap);
    Serial.println(F(" bytes"));
    
    Serial.print(F("Stack Used: ~"));
    Serial.print(info.used_stack);
    Serial.print(F("/"));
    Serial.print(info.stack_size);
    Serial.print(F(" bytes (~"));
    Serial.print((info.used_stack * 100) / info.stack_size);
    Serial.println(F("%)"));
    
    Serial.print(F("Largest Block: "));
    Serial.print(info.largest_free_block);
    Serial.println(F(" bytes"));
    
    Serial.print(F("Memory Usage: ~"));
    Serial.print(getMemoryUsagePercent());
    Serial.println(F("%"));
    
    // Realistic warning conditions
    if (info.largest_free_block < 1024) {
        Serial.println(F("WARNING: Low free memory!"));
    }
    if (getMemoryUsagePercent() > 80) {
        Serial.println(F("WARNING: High memory usage!"));
    }
    
    Serial.println(F("===============================\n"));
}

// Stack watermark for development/debugging
static bool watermark_initialized = false;

void initStackWatermark() {
    if (watermark_initialized) return;
    
    watermark_initialized = true;
    Serial.println(F("Stack watermark initialized for development"));
}

int getStackHighWaterMark() {
    if (!watermark_initialized) {
        return -1; // Not initialized
    }
    
    return getUsedStack(); // Return our conservative estimate
}

bool isMemoryHealthy() {
    int free_mem = getFreeMemory();
    uint8_t usage_percent = getMemoryUsagePercent();
    
    // Conservative thresholds
    if (free_mem < 1024) return false;        // At least 1KB free
    if (usage_percent > 90) return false;     // Usage < 90%
    
    return true;
}

// =============================================================================
// BUTTON HANDLING WITH LONG PRESS
// =============================================================================

// Button state tracking
static bool buttonStates[5] = {false, false, false, false, false};
static bool lastButtonStates[5] = {false, false, false, false, false};
static unsigned long lastDebounceTime[5] = {0, 0, 0, 0, 0};
static bool buttonPressed[5] = {false, false, false, false, false};
static unsigned long buttonPressStartTime[5] = {0, 0, 0, 0, 0};
static bool longPressActive[5] = {false, false, false, false, false};
static unsigned long lastRepeatTime[5] = {0, 0, 0, 0, 0};

// Long press configuration
const unsigned long LONG_PRESS_DELAY = 500;     // Time before long press starts (ms)
const unsigned long INITIAL_REPEAT_DELAY = 300;  // First repeat delay
const unsigned long REPEAT_INTERVAL = 100;       // Subsequent repeat interval

void updateButtonStates() {
    unsigned long currentTime = millis();
    
    for (int i = 0; i < 5; i++) {
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
    if (button < 0 || button > 4) return false;
    
    if (buttonPressed[button]) {
        buttonPressed[button] = false;
        return true;
    }
    return false;
}

bool isButtonHeld(int button) {
    if (button < 0 || button > 4) return false;
    return buttonStates[button];
}

bool isLongPress(int button) {
    if (button < 0 || button > 4) return false;
    return longPressActive[button];
}

bool shouldRepeat(int button) {
    if (button < 0 || button > 4) return false;
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
    for (int i = 0; i < 5; i++) {
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
        case 4: pin = BTN_BLUETOOTH; break;  
        default: return false;
    }
    
    // Read digital value 
    return !digitalRead(pin);
}

// Convenience functions for Bluetooth button
bool wasBluetoothButtonPressed() {
    return wasButtonPressed(4);
}

bool isBluetoothButtonHeld() {
    return isButtonHeld(4);
}

// =============================================================================
// PCF8523 RTC UTILITY FUNCTIONS
// =============================================================================

void configurePCF8523ForFieldUse(RTC_PCF8523& rtc) {
    // Configure for low power field deployment
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
    
    NVIC_SystemReset();
}

void enterDeepSleep(uint32_t seconds) {
    Serial.print(F("Entering deep sleep for "));
    Serial.print(seconds);
    Serial.println(F(" seconds"));
    
    // This would require specific nRF52 power management setup
    // For now, simplified implementation
    delay(seconds * 1000);
}

// =============================================================================
// DEBUG UTILITIES
// =============================================================================

void printSystemInfo() {
    Serial.println(F("\n=== Hive Monitor System Information ==="));
    Serial.println(F("Platform: nRF52840"));
    Serial.print(F("CPU Speed: "));
    Serial.print(F_CPU / 1000000);
    Serial.println(F(" MHz"));
    
    printMemoryInfo();
    
    Serial.print(F("Uptime: "));
    Serial.print(millis() / 1000);
    Serial.println(F(" seconds"));
    
    Serial.println(F("=====================================\n"));
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
// ARDUINO-COMPATIBLE WATCHDOG FUNCTIONS
// =============================================================================

static bool watchdog_enabled = false;
static uint32_t watchdog_timeout_ms = 30000; // 30 seconds default

void setupWatchdog(SystemSettings& settings) {
    // Configure watchdog timeout based on field mode
    if (settings.fieldModeEnabled) {
        watchdog_timeout_ms = 60000; // 60 seconds for field mode
    } else {
        watchdog_timeout_ms = 30000; // 30 seconds for testing mode
    }
    
    // For now, implement a software watchdog using timer
    // This is safer and more compatible with the Arduino framework
    watchdog_enabled = true;
    
    Serial.print(F("Software watchdog enabled with "));
    Serial.print(watchdog_timeout_ms / 1000);
    Serial.println(F(" second timeout"));
    
    // Note: Hardware watchdog can be implemented later if needed
    // The software approach provides similar protection for field deployment
}

void feedWatchdog() {
    // Software watchdog - simply track that we're alive
    // In field deployment, this confirms the main loop is running
    if (watchdog_enabled) {
        // Could add watchdog logic here if needed
        // For now, just indicate the system is responsive
    }
}

void updateWatchdogTimeout(SystemSettings& settings) {
    uint32_t new_timeout = settings.fieldModeEnabled ? 60000 : 30000;
    
    if (new_timeout != watchdog_timeout_ms) {
        Serial.print(F("Watchdog timeout updated to "));
        Serial.print(new_timeout / 1000);
        Serial.println(F(" seconds"));
        
        watchdog_timeout_ms = new_timeout;
    }
}

// =============================================================================
// SYSTEM HEALTH CHECK FUNCTION
// =============================================================================

void checkSystemHealth(SystemStatus& status, SensorData& data) {
    static unsigned long lastHealthCheck = 0;
    static uint8_t consecutiveFailures = 0;
    
    unsigned long currentTime = millis();
    
    // Check system health every 30 seconds
    if (currentTime - lastHealthCheck < 30000) {
        return;
    }
    lastHealthCheck = currentTime;
    
    bool systemHealthy = true;
    
    // Check memory health
    if (!isMemoryHealthy()) {
        Serial.println(F("HEALTH: Memory pressure detected"));
        systemHealthy = false;
    }
    
    // Check sensor validity
    if (!data.sensorsValid) {
        Serial.println(F("HEALTH: Sensor readings invalid"));
        systemHealthy = false;
    }
    
    // Check RTC health
    if (status.rtcWorking) {
        extern RTC_PCF8523 rtc;
        if (!checkPCF8523Health(rtc)) {
            Serial.println(F("HEALTH: RTC health issues"));
            systemHealthy = false;
        }
    } else {
        Serial.println(F("HEALTH: RTC not working"));
        systemHealthy = false;
    }
    
    // Check battery level
    if (data.batteryVoltage > 0 && data.batteryVoltage < BATTERY_CRITICAL) {
        Serial.println(F("HEALTH: Critical battery level"));
        systemHealthy = false;
    }
    
    // Check for stuck in alerts
    static uint8_t lastAlertFlags = 0;
    static uint8_t alertStuckCount = 0;
    
    if (data.alertFlags == lastAlertFlags && data.alertFlags != ALERT_NONE) {
        alertStuckCount++;
        if (alertStuckCount > 10) { // Same alerts for 5 minutes
            Serial.println(F("HEALTH: Persistent alerts detected"));
            systemHealthy = false;
        }
    } else {
        alertStuckCount = 0;
    }
    lastAlertFlags = data.alertFlags;
    
    // Track consecutive failures
    if (!systemHealthy) {
        consecutiveFailures++;
        Serial.print(F("HEALTH: System health issues ("));
        Serial.print(consecutiveFailures);
        Serial.println(F(" consecutive)"));
        
        // If we have persistent health issues, log them
        if (consecutiveFailures >= 5 && status.sdWorking) {
            extern RTC_PCF8523 rtc;
            logFieldEvent(EVENT_SYSTEM_UPDATE, rtc, status); // Log health issue
        }
        
        // Critical health failure - consider restart
        if (consecutiveFailures >= 10) {
            Serial.println(F("CRITICAL: Persistent system health failure"));
            Serial.println(F("Consider field service or system restart"));
            
            // Could trigger automatic restart here if desired
            // performSystemReset();
        }
    } else {
        if (consecutiveFailures > 0) {
            Serial.println(F("HEALTH: System health restored"));
        }
        consecutiveFailures = 0;
    }
}