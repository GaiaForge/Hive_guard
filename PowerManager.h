/**
 * PowerManager.h
 * Power management system header
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "Config.h"
#include "DataStructures.h"

// =============================================================================
// POWER MANAGEMENT ENUMERATIONS
// =============================================================================

enum PowerMode {
    POWER_NORMAL = 0,      // Full power, display always on
    POWER_FIELD = 1,       // Field mode, display timeout enabled
    POWER_SAVE = 2,        // Low battery, reduced functionality
    POWER_CRITICAL = 3     // Very low battery, minimal operation
};

enum WakeUpSource {
    WAKE_BUTTON = 0,
    WAKE_TIMER = 1,
    WAKE_RTC = 2,
    WAKE_EXTERNAL = 3,
    WAKE_UNKNOWN = 4
};

enum ComponentPowerState {
    COMP_POWER_ON = 0,
    COMP_POWER_SLEEP = 1,
    COMP_POWER_OFF = 2
};

// =============================================================================
// POWER MANAGEMENT STRUCTURES
// =============================================================================

struct PowerStatus {
    PowerMode currentMode;
    bool fieldModeActive;
    bool displayOn;
    unsigned long displayTimeoutMs;
    unsigned long lastUserActivity;
    unsigned long nextSleepTime;
    unsigned long totalUptime;
    uint32_t sleepCycles;
    uint32_t buttonPresses;
    WakeUpSource lastWakeSource;
    float estimatedRuntimeHours;
    float dailyUsageEstimateMah;
    ComponentPowerState displayState;
    ComponentPowerState sensorState;
    ComponentPowerState audioState;
};

struct PowerSettings {
    bool fieldModeEnabled;
    uint8_t displayTimeoutMin;     // Display timeout in minutes (1-30)
    uint8_t sleepIntervalMin;      // Deep sleep interval in minutes
    bool autoFieldMode;            // Auto-enable field mode when battery low
    uint8_t criticalBatteryLevel;  // Battery level to enter critical mode
};

// =============================================================================
// POWER MANAGER CLASS
// =============================================================================

class PowerManager {
private:
    PowerStatus status;
    PowerSettings settings;
    SystemStatus* systemStatus;
    SystemSettings* systemSettings;
    
    // Internal timing
    unsigned long lastPowerCheck;
    unsigned long displayOffTime;
    unsigned long lastSleepTime;
    
    // Power consumption estimates (mA)
    static const float POWER_NORMAL_MA;
    static const float POWER_DISPLAY_MA;
    static const float POWER_SENSORS_MA;
    static const float POWER_AUDIO_MA;
    static const float POWER_SLEEP_MA;
    
    // Internal methods    
    void calculateRuntimeEstimate(float batteryVoltage);
    void handleDisplayTimeout();
    bool shouldEnterSleep();
    void configureWakeupSources();
    
public:
    PowerManager();
    
    // Initialization
    void initialize(SystemStatus* sysStatus, SystemSettings* sysSettings);
    void loadPowerSettings();
    void savePowerSettings();
    
    // Core power management
    void update();
    void handleUserActivity();
    void handleWakeUp(WakeUpSource source);
    
    // Field mode management
    void enableFieldMode();
    void disableFieldMode();
    bool isFieldModeActive() const;
    void updatePowerMode(float batteryVoltage);
    
    // Display management
    void turnOnDisplay();
    void turnOffDisplay();
    bool isDisplayOn() const;
    void resetDisplayTimeout();
    unsigned long getDisplayTimeRemaining() const;
    
    // Component power control
    void powerDownSensors();
    void powerUpSensors();
    void powerDownAudio();
    void powerUpAudio();
    void powerDownNonEssential();
    void powerUpAll();
    
    // Sleep management
    void enterDeepSleep(uint32_t sleepTimeMs);
    void prepareSleep();
    void wakeFromSleep();
    bool canEnterSleep() const;
    
    // Battery and power monitoring
    PowerMode getCurrentPowerMode() const;
    float getEstimatedRuntimeHours() const;
    float getDailyUsageEstimate() const;
    unsigned long getUptime() const;
    uint32_t getSleepCycles() const;
    uint32_t getButtonPresses() const;
    
    // Settings management
    void setDisplayTimeout(uint8_t minutes);
    void setFieldMode(bool enabled);
    void setAutoFieldMode(bool enabled);
    
    // Status reporting
    PowerStatus getPowerStatus() const;
    const char* getPowerModeString() const;
    const char* getWakeSourceString() const;
    
    // Diagnostics
    void printPowerStatus() const;
    void resetStatistics();
};

// =============================================================================
// GLOBAL POWER MANAGER FUNCTIONS
// =============================================================================

// These will be used by the main system
void initializePowerManager();
void updatePowerManager();
bool shouldSystemSleep();
void handleSystemWakeup();

// Component power control helpers
void setComponentPower(uint8_t component, bool powerOn);
bool getComponentPowerState(uint8_t component);

// Power mode helpers
const char* powerModeToString(PowerMode mode);
PowerMode batteryToPowerMode(float voltage);

#endif // POWER_MANAGER_H