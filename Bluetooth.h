/**
 * Bluetooth.h
 * Hybrid Bluetooth data transfer system for field data retrieval
 */

#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include "Config.h"
#include "DataStructures.h"

#ifdef NRF52_SERIES
#include <bluefruit.h>
#endif

// =============================================================================
// BLUETOOTH CONFIGURATION
// =============================================================================

#define BT_DEVICE_NAME_PREFIX "HiveMonitor_"
#define BT_SERVICE_UUID "12345678-1234-1234-1234-123456789ABC"
#define BT_DATA_CHAR_UUID "87654321-4321-4321-4321-CBA987654321"
#define BT_COMMAND_CHAR_UUID "11111111-2222-3333-4444-555555555555"
#define BT_STATUS_CHAR_UUID "22222222-3333-4444-5555-666666666666"

// Transfer settings
#define BT_CHUNK_SIZE 240
#define BT_TIMEOUT_MS 30000
#define BT_MANUAL_TIMEOUT_DEFAULT 30  // Default 30 minutes

// =============================================================================
// BLUETOOTH ENUMERATIONS
// =============================================================================

enum BluetoothMode {
    BT_MODE_OFF = 0,        // Bluetooth disabled
    BT_MODE_MANUAL = 1,     // Manual button activation only
    BT_MODE_SCHEDULED = 2,  // Scheduled hours (e.g., 7AM-6PM)
    BT_MODE_ALWAYS_ON = 3   // Always discoverable
};

enum BluetoothStatus {
    BT_STATUS_OFF = 0,
    BT_STATUS_ADVERTISING = 1,
    BT_STATUS_CONNECTED = 2,
    BT_STATUS_TRANSFERRING = 3,
    BT_STATUS_ERROR = 4
};

enum BluetoothCommand {
    BT_CMD_PING = 0x01, // Ping command to check connection
    BT_CMD_GET_STATUS = 0x02, // Get current device status
    BT_CMD_GET_CURRENT_DATA = 0x03, // Get current sensor data
    BT_CMD_LIST_FILES = 0x04, // List all available files
    BT_CMD_GET_FILE = 0x05, // Get specific file by name
    BT_CMD_GET_DAILY_SUMMARY = 0x06, // Get daily summary for a specific date
    BT_CMD_GET_ALERTS = 0x07, // Get alert history
    BT_CMD_GET_DEVICE_INFO = 0x08, // Get device information  
    BT_CMD_SET_TIME = 0x09, // Set system time (RTC)
    BT_CMD_START_CALIBRATION = 0x0A, // Start calibration process
    BT_CMD_GET_SETTINGS = 0x10,      // Get all current settings
    BT_CMD_SET_SETTING = 0x11,       // Set individual setting
    BT_CMD_FACTORY_RESET = 0x12,      // Reset to defaults
    BT_CMD_SET_DATE_TIME = 0x13,  // Set individual date/time components
    BT_CMD_START_AUDIO_CALIBRATION = 0x14, // Start audio calibration
    BT_CMD_GET_FILE_DATA = 0x15,     // Download file content
    BT_CMD_DELETE_FILE = 0x16,       // Delete file
    BT_CMD_GET_FILE_INFO = 0x17,     // Get file size/date
    BT_CMD_SET_BEE_PRESET = 0x18,     // Set bee type preset
    BT_CMD_GET_BEE_PRESETS = 0x19,    // Get available presets
};

enum BluetoothResponse {
    BT_RESP_OK = 0x10,
    BT_RESP_ERROR = 0x11,
    BT_RESP_NOT_FOUND = 0x12,
    BT_RESP_TOO_LARGE = 0x13,
    BT_RESP_BUSY = 0x14,
    BT_RESP_TIMEOUT = 0x15
};

// =============================================================================
// BLUETOOTH STRUCTURES
// =============================================================================

struct BluetoothSettings {
    BluetoothMode mode;
    uint8_t manualTimeoutMin;    // Manual mode timeout (15, 30, 60 minutes)
    uint8_t scheduleStartHour;   // Schedule start hour (0-23)
    uint8_t scheduleEndHour;     // Schedule end hour (0-23)
    bool lowPowerMode;           // Reduce advertising frequency
    uint8_t deviceId;            // Device identifier (0-255)
    char hiveName[16];           // Custom hive name (e.g., "TopBar_A1", "Village_03")
    char location[24];           // Location description (e.g., "Acacia_Tree_North")
    char beekeeper[16];          // Beekeeper name (e.g., "John_K")
};

struct BluetoothState {
    BluetoothStatus status;
    unsigned long manualStartTime;
    unsigned long lastConnectionTime;
    unsigned long totalConnections;
    unsigned long totalDataTransferred;
    bool clientConnected;
    char connectedDeviceName[32];
    uint16_t currentTransferProgress;
    uint16_t currentTransferTotal;
};

struct DataRequest {
    BluetoothCommand command;
    char filename[64];
    uint32_t startTime;
    uint32_t endTime;
    uint16_t maxSize;
};

// =============================================================================
// BLUETOOTH MANAGER CLASS
// =============================================================================

class BluetoothManager {
private:
    BluetoothSettings settings;
    BluetoothState state;
    SystemStatus* systemStatus;
    SystemSettings* systemSettings;
    
    // BLE objects
#ifdef NRF52_SERIES
    BLEService dataService;
    BLECharacteristic dataCharacteristic;
    BLECharacteristic commandCharacteristic;
    BLECharacteristic statusCharacteristic;
#endif
    
    void sendAllSettings();
    void updateSetting(uint8_t settingId, float value);
    
    // Internal timing
    unsigned long lastUpdate;
    unsigned long scheduleCheckTime;
    
    // Internal methods
    void setupBLEService();
    
    void sendResponse(BluetoothResponse response, uint8_t* data = nullptr, uint16_t len = 0);
    void sendCurrentData();
    void sendFileList();
    void sendFile(const char* filename);
    void sendDailySummary(uint32_t date);
    void sendAlerts();
    void sendBeePresetList();
    void sendDeviceInfo();
    void sendFileData(const char* filename);
    void deleteFile(const char* filename);
    void sendFileInfo(const char* filename);
    void setDateTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute);
    void startAudioCalibration(uint8_t durationSeconds);
    
    
    void updateAdvertising();
    
public:
    BluetoothManager();
    
    // Initialization
    void initialize(SystemStatus* sysStatus, SystemSettings* sysSettings);
    void loadBluetoothSettings();
    void saveBluetoothSettings();
    
    // Core Bluetooth management
    void update();
    void handleManualActivation();  // Called when button pressed
    
    // Mode management
    void setMode(BluetoothMode mode);
    BluetoothMode getMode() const;
    void setSchedule(uint8_t startHour, uint8_t endHour);
    void setManualTimeout(uint8_t minutes);
    
    // Status and control
    BluetoothStatus getStatus() const;
    bool isDiscoverable() const;
    bool isConnected() const;
    unsigned long getTimeRemaining() const;  // For manual mode
    void forceDisconnect();
    void startAdvertising();
    void stopAdvertising();
    
    // Settings access
    BluetoothSettings& getSettings() { return settings; }
    BluetoothState& getState() { return state; }
    
    // Statistics
    void printBluetoothStatus() const;
    void resetStatistics();
    
    // Utility
    String getDeviceName() const;
    bool shouldBeDiscoverable() const;
    void handleCommand(uint8_t* data, uint16_t len);
    bool isInScheduledHours(uint8_t currentHour) const;
};

// =============================================================================
// GLOBAL BLUETOOTH FUNCTIONS
// =============================================================================

// Callback functions for BLE events
#ifdef NRF52_SERIES
void bluetoothConnectCallback(uint16_t conn_handle);
void bluetoothDisconnectCallback(uint16_t conn_handle, uint8_t reason);
void bluetoothCommandCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);
#endif

// Utility functions
const char* bluetoothModeToString(BluetoothMode mode);
const char* bluetoothStatusToString(BluetoothStatus status);
String formatDataSize(uint32_t bytes);

#endif // BLUETOOTH_H