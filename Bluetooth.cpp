/**
 * Bluetooth.cpp
 * Hybrid Bluetooth data transfer system implementation
 */

#include "Bluetooth.h"
#include "Utils.h"
#include "DataLogger.h"
#include "Sensors.h"
#include "Alerts.h"

#ifdef NRF52_SERIES

// Global bluetooth manager instance
BluetoothManager* bluetoothManagerInstance = nullptr;

// =============================================================================
// CONSTRUCTOR AND INITIALIZATION
// =============================================================================

BluetoothManager::BluetoothManager()
#ifdef NRF52_SERIES
    : dataService(BT_SERVICE_UUID),
      dataCharacteristic(BT_DATA_CHAR_UUID),
      commandCharacteristic(BT_COMMAND_CHAR_UUID),
      statusCharacteristic(BT_STATUS_CHAR_UUID)
#endif
{
    // Initialize settings
    settings.mode = BT_MODE_MANUAL;  // Default to manual mode
    settings.manualTimeoutMin = BT_MANUAL_TIMEOUT_DEFAULT;
    settings.scheduleStartHour = 7;   // 7 AM
    settings.scheduleEndHour = 18;    // 6 PM
    settings.lowPowerMode = true;
    settings.deviceId = 1;
    strcpy(settings.hiveName, "Hive_01");           // Default name
    strcpy(settings.location, "Tree_A");            // Default location  
    strcpy(settings.beekeeper, "Field_User");       // Default beekeeper
    
    // Initialize state
    state.status = BT_STATUS_OFF;
    state.manualStartTime = 0;
    state.lastConnectionTime = 0;
    state.totalConnections = 0;
    state.totalDataTransferred = 0;
    state.clientConnected = false;
    state.connectedDeviceName[0] = '\0';
    state.currentTransferProgress = 0;
    state.currentTransferTotal = 0;
    
    lastUpdate = 0;
    scheduleCheckTime = 0;
    systemStatus = nullptr;
    systemSettings = nullptr;
    
    bluetoothManagerInstance = this;
}

void BluetoothManager::initialize(SystemStatus* sysStatus, SystemSettings* sysSettings) {
    systemStatus = sysStatus;
    systemSettings = sysSettings;
    
    loadBluetoothSettings();
    
#ifdef NRF52_SERIES
    // Initialize Bluefruit
    Bluefruit.begin();
    Bluefruit.setTxPower(0);  // 0dBm for good range vs power balance
    
    // Set device name
    String deviceName = getDeviceName();
    Bluefruit.setName(deviceName.c_str());
    
    // Setup callbacks
    Bluefruit.Periph.setConnectCallback(bluetoothConnectCallback);
    Bluefruit.Periph.setDisconnectCallback(bluetoothDisconnectCallback);
    
    // Setup BLE service
    setupBLEService();
    
    Serial.print(F("Bluetooth initialized as: "));
    Serial.println(deviceName);
    Serial.print(F("Mode: "));
    Serial.println(bluetoothModeToString(settings.mode));
    
    // Start advertising based on current mode
    if (shouldBeDiscoverable()) {
        startAdvertising();
    }
#else
    Serial.println(F("Bluetooth not supported on this platform"));
#endif
}

void BluetoothManager::setupBLEService() {
#ifdef NRF52_SERIES
    // Setup service
    dataService.begin();
    
    // Setup data characteristic (for large data transfer)
    dataCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
    dataCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    dataCharacteristic.setFixedLen(BT_CHUNK_SIZE);
    dataCharacteristic.begin();
    
    // Setup command characteristic (for receiving commands)
    commandCharacteristic.setProperties(CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP);
    commandCharacteristic.setPermission(SECMODE_OPEN, SECMODE_OPEN);
    commandCharacteristic.setWriteCallback(bluetoothCommandCallback);
    commandCharacteristic.setFixedLen(64);
    commandCharacteristic.begin();
    
    // Setup status characteristic (for device status)
    statusCharacteristic.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
    statusCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
    statusCharacteristic.setFixedLen(32);
    statusCharacteristic.begin();
#endif
}

// =============================================================================
// CORE BLUETOOTH MANAGEMENT
// =============================================================================

void BluetoothManager::update() {
    unsigned long currentTime = millis();
    
    // Update every second
    if (currentTime - lastUpdate < 1000) return;
    lastUpdate = currentTime;
    
    // Handle different modes
    switch (settings.mode) {
        case BT_MODE_OFF:
            if (state.status != BT_STATUS_OFF) {
                stopAdvertising();
            }
            break;
            
        case BT_MODE_MANUAL:
            // Check if manual timeout has expired
            if (state.status == BT_STATUS_ADVERTISING && state.manualStartTime > 0) {
                unsigned long elapsed = (currentTime - state.manualStartTime) / 60000; // Convert to minutes
                if (elapsed >= settings.manualTimeoutMin) {
                    Serial.println(F("Manual Bluetooth timeout - stopping advertising"));
                    stopAdvertising();
                    state.manualStartTime = 0;
                }
            }
            break;
            
        case BT_MODE_SCHEDULED:
            // Check schedule every minute
            if (currentTime - scheduleCheckTime >= 60000) {
                scheduleCheckTime = currentTime;
                
                if (systemStatus && systemStatus->rtcWorking) {
                    // Get current hour from RTC
                    extern RTC_DS3231 rtc;
                    DateTime now = rtc.now();
                    
                    bool shouldBeOn = isInScheduledHours(now.hour());
                    
                    if (shouldBeOn && state.status == BT_STATUS_OFF) {
                        Serial.println(F("Scheduled Bluetooth start"));
                        startAdvertising();
                    } else if (!shouldBeOn && state.status == BT_STATUS_ADVERTISING) {
                        Serial.println(F("Scheduled Bluetooth stop"));
                        stopAdvertising();
                    }
                }
            }
            break;
            
        case BT_MODE_ALWAYS_ON:
            if (state.status == BT_STATUS_OFF) {
                startAdvertising();
            }
            break;
    }
    
    // Update advertising parameters based on power mode
    updateAdvertising();
}

void BluetoothManager::handleManualActivation() {
    if (settings.mode == BT_MODE_OFF) {
        Serial.println(F("Bluetooth is disabled"));
        return;
    }
    
    // Manual activation works in any mode
    Serial.print(F("Manual Bluetooth activation for "));
    Serial.print(settings.manualTimeoutMin);
    Serial.println(F(" minutes"));
    
    state.manualStartTime = millis();
    startAdvertising();
}

void BluetoothManager::startAdvertising() {
#ifdef NRF52_SERIES
    if (state.status == BT_STATUS_ADVERTISING) return;
    
    // Setup advertising packet
    Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addName();
    Bluefruit.Advertising.addService(dataService);
    
    // Set advertising parameters based on power mode
    if (settings.lowPowerMode) {
        Bluefruit.Advertising.setInterval(2000, 5000);  // 2-5 second intervals for power saving
    } else {
        Bluefruit.Advertising.setInterval(32, 244);     // Fast advertising for quick connection
    }
    
    Bluefruit.Advertising.setFastTimeout(30);  // Fast advertising for 30 seconds, then slow
    Bluefruit.Advertising.start(0);  // 0 = never timeout
    
    state.status = BT_STATUS_ADVERTISING;
    Serial.println(F("Bluetooth advertising started"));
#endif
}

void BluetoothManager::stopAdvertising() {
#ifdef NRF52_SERIES
    if (state.status == BT_STATUS_OFF) return;
    
    Bluefruit.Advertising.stop();
    
    // Disconnect any active connections
    if (state.clientConnected) {
        Bluefruit.disconnect(Bluefruit.connHandle());
    }
    
    state.status = BT_STATUS_OFF;
    state.clientConnected = false;
    Serial.println(F("Bluetooth advertising stopped"));
#endif
}

void BluetoothManager::updateAdvertising() {
    // Adjust advertising based on power mode and battery level
    if (state.status != BT_STATUS_ADVERTISING) return;
    
    // Get current battery level
    extern SensorData currentData;
    float batteryLevel = getBatteryLevel(currentData.batteryVoltage);
    
#ifdef NRF52_SERIES
    // Reduce advertising frequency when battery is low
    if (batteryLevel < 20) {
        Bluefruit.Advertising.setInterval(5000, 10000);  // Very slow advertising
    } else if (batteryLevel < 50) {
        Bluefruit.Advertising.setInterval(2000, 5000);   // Slow advertising
    } else if (settings.lowPowerMode) {
        Bluefruit.Advertising.setInterval(1000, 3000);   // Normal slow advertising
    }
#endif
}

// =============================================================================
// COMMAND HANDLING
// =============================================================================

void BluetoothManager::handleCommand(uint8_t* data, uint16_t len) {
    if (len < 1) return;
    
    BluetoothCommand cmd = (BluetoothCommand)data[0];
    Serial.print(F("BT Command: 0x"));
    Serial.println(cmd, HEX);
    
    switch (cmd) {
        case BT_CMD_PING:
            sendResponse(BT_RESP_OK);
            break;
            
        case BT_CMD_GET_STATUS:
            sendDeviceInfo();
            break;
            
        case BT_CMD_GET_CURRENT_DATA:
            sendCurrentData();
            break;
            
        case BT_CMD_LIST_FILES:
            sendFileList();
            break;
            
        case BT_CMD_GET_FILE:
            if (len > 1) {
                char filename[32];
                memcpy(filename, &data[1], min(len-1, 31));
                filename[min(len-1, 31)] = '\0';
                sendFile(filename);
            } else {
                sendResponse(BT_RESP_ERROR);
            }
            break;
            
        case BT_CMD_GET_DAILY_SUMMARY:
            if (len >= 5) {
                uint32_t date = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
                sendDailySummary(date);
            } else {
                sendResponse(BT_RESP_ERROR);
            }
            break;
            
        case BT_CMD_GET_ALERTS:
            sendAlerts();
            break;
            
        case BT_CMD_GET_DEVICE_INFO:
            sendDeviceInfo();
            break;
            
        case BT_CMD_SET_TIME:
            if (len >= 5 && systemStatus && systemStatus->rtcWorking) {
                uint32_t timestamp = (data[1] << 24) | (data[2] << 16) | (data[3] << 8) | data[4];
                extern RTC_DS3231 rtc;
                rtc.adjust(DateTime(timestamp));
                sendResponse(BT_RESP_OK);
                Serial.println(F("Time updated via Bluetooth"));
            } else {
                sendResponse(BT_RESP_ERROR);
            }
            break;
            
        default:
            sendResponse(BT_RESP_ERROR);
            break;
    }
}

void BluetoothManager::sendResponse(BluetoothResponse response, uint8_t* data, uint16_t len) {
#ifdef NRF52_SERIES
    uint8_t buffer[BT_CHUNK_SIZE];
    buffer[0] = (uint8_t)response;
    
    if (data && len > 0) {
        memcpy(&buffer[1], data, min(len, BT_CHUNK_SIZE - 1));
        dataCharacteristic.notify(buffer, len + 1);
    } else {
        dataCharacteristic.notify(buffer, 1);
    }
#endif
}

void BluetoothManager::sendCurrentData() {
    extern SensorData currentData;
    extern RTC_DS3231 rtc;
    
    char jsonData[BT_CHUNK_SIZE];
    
    // Create JSON response with current data
    snprintf(jsonData, sizeof(jsonData),
        "{"
        "\"timestamp\":%lu,"
        "\"temperature\":%.1f,"
        "\"humidity\":%.1f,"
        "\"pressure\":%.1f,"
        "\"frequency\":%d,"
        "\"soundLevel\":%d,"
        "\"beeState\":\"%s\","
        "\"battery\":%.2f,"
        "\"alerts\":\"0x%02X\""
        "}",
        systemStatus && systemStatus->rtcWorking ? rtc.now().unixtime() : millis()/1000,
        currentData.temperature,
        currentData.humidity,
        currentData.pressure,
        currentData.dominantFreq,
        currentData.soundLevel,
        getBeeStateString(currentData.beeState),
        currentData.batteryVoltage,
        currentData.alertFlags
    );
    
    sendResponse(BT_RESP_OK, (uint8_t*)jsonData, strlen(jsonData));
    
    state.totalDataTransferred += strlen(jsonData);
    Serial.println(F("Sent current data via Bluetooth"));
}

void BluetoothManager::sendDeviceInfo() {
    char deviceInfo[BT_CHUNK_SIZE];
    
    snprintf(deviceInfo, sizeof(deviceInfo),
        "{"
        "\"device\":\"%s\","
        "\"hiveName\":\"%s\","
        "\"location\":\"%s\","
        "\"beekeeper\":\"%s\","
        "\"deviceId\":%d,"
        "\"firmware\":\"v2.0\","
        "\"uptime\":%lu,"
        "\"btMode\":\"%s\","
        "\"btConnections\":%lu,"
        "\"freeMemory\":%d,"
        "\"sdCard\":%s"
        "}",
        getDeviceName().c_str(),
        settings.hiveName,
        settings.location,
        settings.beekeeper,
        settings.deviceId,
        millis() / 1000,
        bluetoothModeToString(settings.mode),
        state.totalConnections,
        getFreeMemory(),
        (systemStatus && systemStatus->sdWorking) ? "true" : "false"
    );
    
    sendResponse(BT_RESP_OK, (uint8_t*)deviceInfo, strlen(deviceInfo));
}

void BluetoothManager::sendFileList() {
    if (!systemStatus || !systemStatus->sdWorking) {
        sendResponse(BT_RESP_ERROR);
        return;
    }
    
    // Simple file listing - in a real implementation, you'd scan the SD card
    char fileList[BT_CHUNK_SIZE];
    snprintf(fileList, sizeof(fileList),
        "{"
        "\"files\":["
        "\"2024-12.CSV\","
        "\"alerts.log\","
        "\"diagnostics.log\""
        "]"
        "}"
    );
    
    sendResponse(BT_RESP_OK, (uint8_t*)fileList, strlen(fileList));
}

void BluetoothManager::sendFile(const char* filename) {
    // This would implement chunked file transfer
    // For now, send a placeholder response
    char response[64];
    snprintf(response, sizeof(response), "File transfer not implemented: %s", filename);
    sendResponse(BT_RESP_NOT_FOUND, (uint8_t*)response, strlen(response));
}

void BluetoothManager::sendDailySummary(uint32_t date) {
    // Create a daily summary from the specified date
    char summary[BT_CHUNK_SIZE];
    snprintf(summary, sizeof(summary),
        "{"
        "\"date\":%lu,"
        "\"avgTemp\":25.5,"
        "\"avgHumidity\":65.2,"
        "\"alerts\":3,"
        "\"beeActivity\":\"Normal\""
        "}",
        date
    );
    
    sendResponse(BT_RESP_OK, (uint8_t*)summary, strlen(summary));
}

void BluetoothManager::sendAlerts() {
    // Send recent alerts
    char alerts[BT_CHUNK_SIZE];
    snprintf(alerts, sizeof(alerts),
        "{"
        "\"recentAlerts\":["
        "{\"time\":1703001600,\"type\":\"TEMP_HIGH\",\"value\":42.5},"
        "{\"time\":1703005200,\"type\":\"QUEEN_ISSUE\",\"value\":0}"
        "]"
        "}"
    );
    
    sendResponse(BT_RESP_OK, (uint8_t*)alerts, strlen(alerts));
}

// =============================================================================
// MODE AND SETTINGS MANAGEMENT
// =============================================================================

void BluetoothManager::setMode(BluetoothMode mode) {
    if (settings.mode == mode) return;
    
    Serial.print(F("Bluetooth mode: "));
    Serial.print(bluetoothModeToString(settings.mode));
    Serial.print(F(" -> "));
    Serial.println(bluetoothModeToString(mode));
    
    // Stop current operation
    if (state.status != BT_STATUS_OFF) {
        stopAdvertising();
    }
    
    settings.mode = mode;
    saveBluetoothSettings();
    
    // Start new mode if appropriate
    if (shouldBeDiscoverable()) {
        startAdvertising();
    }
}

void BluetoothManager::setSchedule(uint8_t startHour, uint8_t endHour) {
    if (startHour > 23) startHour = 23;
    if (endHour > 23) endHour = 23;
    
    settings.scheduleStartHour = startHour;
    settings.scheduleEndHour = endHour;
    saveBluetoothSettings();
    
    Serial.print(F("Bluetooth schedule: "));
    Serial.print(startHour);
    Serial.print(F(":00 - "));
    Serial.print(endHour);
    Serial.println(F(":00"));
}

void BluetoothManager::setManualTimeout(uint8_t minutes) {
    if (minutes < 5) minutes = 5;
    if (minutes > 120) minutes = 120;
    
    settings.manualTimeoutMin = minutes;
    saveBluetoothSettings();
    
    Serial.print(F("Manual timeout set to "));
    Serial.print(minutes);
    Serial.println(F(" minutes"));
}

bool BluetoothManager::isInScheduledHours(uint8_t currentHour) {
    if (settings.scheduleStartHour <= settings.scheduleEndHour) {
        // Normal case: 7:00 - 18:00
        return (currentHour >= settings.scheduleStartHour && currentHour < settings.scheduleEndHour);
    } else {
        // Overnight case: 22:00 - 06:00
        return (currentHour >= settings.scheduleStartHour || currentHour < settings.scheduleEndHour);
    }
}

bool BluetoothManager::shouldBeDiscoverable() const {
    switch (settings.mode) {
        case BT_MODE_OFF:
            return false;
            
        case BT_MODE_MANUAL:
            return (state.manualStartTime > 0 && 
                   (millis() - state.manualStartTime) < (settings.manualTimeoutMin * 60000UL));
            
        case BT_MODE_SCHEDULED:
            if (systemStatus && systemStatus->rtcWorking) {
                extern RTC_DS3231 rtc;
                DateTime now = rtc.now();
                return isInScheduledHours(now.hour());
            }
            return false;
            
        case BT_MODE_ALWAYS_ON:
            return true;
            
        default:
            return false;
    }
}

// =============================================================================
// SETTINGS PERSISTENCE
// =============================================================================

void BluetoothManager::loadBluetoothSettings() {
    // In a full implementation, load from flash storage
    // For now, use defaults
    Serial.println(F("Bluetooth settings loaded (defaults)"));
}

void BluetoothManager::saveBluetoothSettings() {
    // In a full implementation, save to flash storage
    Serial.println(F("Bluetooth settings saved"));
}

// =============================================================================
// STATUS AND UTILITY FUNCTIONS
// =============================================================================

BluetoothStatus BluetoothManager::getStatus() const {
    return state.status;
}

bool BluetoothManager::isDiscoverable() const {
    return (state.status == BT_STATUS_ADVERTISING);
}

bool BluetoothManager::isConnected() const {
    return state.clientConnected;
}

unsigned long BluetoothManager::getTimeRemaining() const {
    if (settings.mode != BT_MODE_MANUAL || state.manualStartTime == 0) {
        return 0;
    }
    
    unsigned long elapsed = millis() - state.manualStartTime;
    unsigned long timeout = settings.manualTimeoutMin * 60000UL;
    
    if (elapsed >= timeout) return 0;
    return timeout - elapsed;
}

String BluetoothManager::getDeviceName() const {
    // Create device name from hive name and device ID
    // Format: "HiveName_ID" (e.g., "TopBar_A1_01", "Village_03_12")
    String deviceName = String(settings.hiveName) + "_" + String(settings.deviceId, DEC);
    
    // Ensure it fits in BLE name limit (usually 20-30 chars)
    if (deviceName.length() > 20) {
        deviceName = deviceName.substring(0, 20);
    }
    
    return deviceName;
}

void BluetoothManager::printBluetoothStatus() const {
    Serial.println(F("\n=== Bluetooth Status ==="));
    Serial.print(F("Device: ")); Serial.println(getDeviceName());
    Serial.print(F("Mode: ")); Serial.println(bluetoothModeToString(settings.mode));
    Serial.print(F("Status: ")); Serial.println(bluetoothStatusToString(state.status));
    Serial.print(F("Connected: ")); Serial.println(state.clientConnected ? "Yes" : "No");
    Serial.print(F("Total Connections: ")); Serial.println(state.totalConnections);
    Serial.print(F("Data Transferred: ")); Serial.print(state.totalDataTransferred); Serial.println(F(" bytes"));
    
    if (settings.mode == BT_MODE_MANUAL && state.manualStartTime > 0) {
        unsigned long remaining = getTimeRemaining();
        Serial.print(F("Time Remaining: ")); Serial.print(remaining / 60000); Serial.println(F(" minutes"));
    }
    
    if (settings.mode == BT_MODE_SCHEDULED) {
        Serial.print(F("Schedule: ")); Serial.print(settings.scheduleStartHour);
        Serial.print(F(":00 - ")); Serial.print(settings.scheduleEndHour); Serial.println(F(":00"));
    }
    
    Serial.println(F("=======================\n"));
}

// =============================================================================
// CALLBACK FUNCTIONS
// =============================================================================

#ifdef NRF52_SERIES
void bluetoothConnectCallback(uint16_t conn_handle) {
    if (bluetoothManagerInstance) {
        bluetoothManagerInstance->getState().clientConnected = true;
        bluetoothManagerInstance->getState().status = BT_STATUS_CONNECTED;
        bluetoothManagerInstance->getState().totalConnections++;
        bluetoothManagerInstance->getState().lastConnectionTime = millis();
        
        Serial.println(F("Bluetooth client connected"));
    }
}

void bluetoothDisconnectCallback(uint16_t conn_handle, uint8_t reason) {
    if (bluetoothManagerInstance) {
        bluetoothManagerInstance->getState().clientConnected = false;
        bluetoothManagerInstance->getState().status = BT_STATUS_ADVERTISING;
        
        Serial.print(F("Bluetooth client disconnected, reason: "));
        Serial.println(reason);
    }
}

void bluetoothCommandCallback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len) {
    if (bluetoothManagerInstance) {
        bluetoothManagerInstance->handleCommand(data, len);
    }
}
#endif

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

const char* bluetoothModeToString(BluetoothMode mode) {
    switch (mode) {
        case BT_MODE_OFF: return "Off";
        case BT_MODE_MANUAL: return "Manual";
        case BT_MODE_SCHEDULED: return "Scheduled";
        case BT_MODE_ALWAYS_ON: return "Always On";
        default: return "Unknown";
    }
}

const char* bluetoothStatusToString(BluetoothStatus status) {
    switch (status) {
        case BT_STATUS_OFF: return "Off";
        case BT_STATUS_ADVERTISING: return "Advertising";
        case BT_STATUS_CONNECTED: return "Connected";
        case BT_STATUS_TRANSFERRING: return "Transferring";
        case BT_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}

String formatDataSize(uint32_t bytes) {
    if (bytes < 1024) {
        return String(bytes) + " B";
    } else if (bytes < 1024 * 1024) {
        return String(bytes / 1024.0, 1) + " KB";
    } else {
        return String(bytes / (1024.0 * 1024.0), 1) + " MB";
    }
}

#else

// Stub implementation for non-nRF52 platforms
BluetoothManager::BluetoothManager() {
    Serial.println(F("Bluetooth not supported on this platform"));
}

void BluetoothManager::initialize(SystemStatus* sysStatus, SystemSettings* sysSettings) {
    Serial.println(F("Bluetooth initialization skipped - not supported"));
}

void BluetoothManager::update() {}
void BluetoothManager::handleManualActivation() {
    Serial.println(F("Bluetooth not available"));
}

#endif // NRF52_SERIES