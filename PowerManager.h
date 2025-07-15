/**
 * PowerManager.h
 * Power management system header - WITH BLUETOOTH INTEGRATION
 */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "Config.h"
#include "DataStructures.h"

// Forward declaration to avoid circular dependency
class BluetoothManager;

// =============================================================================
// POWER MANAGEMENT ENUMERATIONS
// =============================================================================

enum PowerMode {
    POWER_TESTING = 0,      // Full power, display always on, Bluetooth always on
    POWER_FIELD = 1,       // Field mode, display timeout enabled, Bluetooth manual only
    POWER_SAVE = 2,        // Low battery, reduced functionality
    POWER_CRITICAL = 3     // Very low battery, minimal operation
};

enum WakeUpSource {
    WAKE_BUTTON = 0,
    WAKE_TIMER = 1,
    WAKE_RTC = 2,
    WAKE_EXTERNAL = 3,
    WAKE_BLUETOOTH_BUTTON = 4,  // NEW: Bluetooth button pressed
    WAKE_UNKNOWN = 5
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
    bool wokenByTimer;          // true if woken by RTC, false if by button
    uint32_t lastLogTime;       // Last time we took a reading
    uint32_t nextWakeTime;      // When to wake for next reading
    uint32_t lastFlushTime;     // Last time buffer was flushed
    bool fieldModeActive;
    bool displayOn;
    unsigned long displayTimeoutMs;
    unsigned long lastUserActivity;
    
    // NEW: Separate Bluetooth timing
    bool bluetoothOn;           // Current Bluetooth state
    unsigned long bluetoothTimeoutMs;  // Same duration as display timeout
    unsigned long lastBluetoothActivity; // Last Bluetooth connection/disconnection
    bool bluetoothManuallyActivated;    // Was Bluetooth turned on by button?
    
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
    ComponentPowerState bluetoothState;  // NEW: Bluetooth power state
};

struct PowerSettings {
    bool fieldModeEnabled;
    uint8_t displayTimeoutMin;     // Display timeout in minutes (1-5)
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
    BluetoothManager* bluetoothManager;  // NEW: Reference to Bluetooth manager
    
    // Internal timing
    unsigned long lastPowerCheck;
    unsigned long displayOffTime;
    unsigned long lastSleepTime;

    // Power consumption estimates (mA)
    static const float POWER_TESTING_MA;
    static const float POWER_DISPLAY_MA;
    static const float POWER_SENSORS_MA;
    static const float POWER_AUDIO_MA;
    static const float POWER_BLUETOOTH_MA;  // NEW: Bluetooth power consumption
    static const float POWER_SLEEP_MA;
    
    // Internal methods    
    void calculateRuntimeEstimate(float batteryVoltage);
    void handleDisplayTimeout();
    void handleBluetoothTimeout();  // NEW: Handle Bluetooth timeout
    void configureButtonWakeup();
    void checkFieldModeTimeout(unsigned long currentTime);  
    void checkBluetoothTimeout(unsigned long currentTime);  // NEW: Check Bluetooth timeout
    
    // Long press wake detection
    unsigned long longPressStartTime;
    bool longPressDetected;
    static const unsigned long LONG_PRESS_WAKE_TIME = 1000; // 1 second for wake
        
    
    
public:
    PowerManager();
    
    // Initialization
    void initialize(SystemStatus* sysStatus, SystemSettings* sysSettings);
    void setBluetoothManager(BluetoothManager* btManager);  // NEW: Set Bluetooth manager reference
    void loadPowerSettings();
    void savePowerSettings();

    // Hardware display power control 
    void initializeDisplayPower();
    void turnOnDisplay();
    void turnOffDisplay();
    bool isDisplayOn() const;
    void resetDisplayTimeout();
    unsigned long getDisplayTimeRemaining() const;
    
    // NEW: Bluetooth power control
    void activateBluetooth();       // Turn on Bluetooth (manual button press)
    void deactivateBluetooth();     // Turn off Bluetooth (timeout or manual)
    bool isBluetoothOn() const;
    void handleBluetoothConnection();    // Reset Bluetooth timer on connection
    void handleBluetoothDisconnection(); // Start Bluetooth countdown on disconnect
    unsigned long getBluetoothTimeRemaining() const;
    
    // Core power management
    void update();
    void handleUserActivity();              // Resets display timer only
    void handleBluetoothActivity();         // NEW: Resets Bluetooth timer only
    void handleBluetoothButtonPress();      // NEW: Handle external Bluetooth button
    void handleWakeUp(WakeUpSource source);
    bool shouldEnterSleep();
    void configureWakeupSources();
    
    // Field mode management
    void enableFieldMode();
    void disableFieldMode();
    bool isFieldModeActive() const;
    void updatePowerMode(float batteryVoltage);

    // Field mode sleep management
    bool shouldTakeReading() const;
    void updateNextWakeTime(uint8_t logIntervalMinutes);
    bool isTimeForBufferFlush() const;
    void setWakeSource(bool fromTimer);
    bool wasWokenByTimer() const;
    bool checkForLongPressWake();
        
    // Component power control
    void powerDownSensors();
    void powerUpSensors();
    void powerDownAudio();
    void powerUpAudio();
    void powerDownBluetooth();      // NEW: Power down Bluetooth
    void powerUpBluetooth();        // NEW: Power up Bluetooth
    void powerDownNonEssential();
    void powerUpAll();
    
    // Sleep management
    void enterDeepSleep(uint32_t sleepTimeMs);
    void prepareSleep();
    void wakeFromSleep();
    bool canEnterSleep() const;
    void enterFieldSleep();
    void wakeFromFieldSleep(); 
    
    // Battery and power monitoring
    PowerMode getCurrentPowerMode() const;
    float getEstimatedRuntimeHours() const;
    float getDailyUsageEstimate() const;
    unsigned long getUptime() const;
    uint32_t getSleepCycles() const;
    uint32_t getButtonPresses() const;

    // Memory management
    void printMemoryStatus() const;
    
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