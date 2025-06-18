/**
 * DataStructures.h
 * Data structures and enumerations
 */

#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <Arduino.h>

// =============================================================================
// ENUMERATIONS
// =============================================================================

// Bee state classification
enum BeeState {
    BEE_QUIET = 0,
    BEE_NORMAL = 1,
    BEE_ACTIVE = 2,
    BEE_QUEEN_PRESENT = 3,
    BEE_QUEEN_MISSING = 4,
    BEE_PRE_SWARM = 5,
    BEE_DEFENSIVE = 6,
    BEE_STRESSED = 7,
    BEE_UNKNOWN = 8
};

// Alert flags (bit flags)
enum AlertFlags {
    ALERT_NONE = 0x00,
    ALERT_TEMP_HIGH = 0x01,
    ALERT_TEMP_LOW = 0x02,
    ALERT_HUMIDITY_HIGH = 0x04,
    ALERT_HUMIDITY_LOW = 0x08,
    ALERT_QUEEN_ISSUE = 0x10,
    ALERT_SWARM_RISK = 0x20,
    ALERT_LOW_BATTERY = 0x40,
    ALERT_SD_ERROR = 0x80
};

// Display modes
enum DisplayMode {
    MODE_DASHBOARD,
    MODE_SOUND,
    MODE_ALERTS,
    MODE_SETTINGS
};

// =============================================================================
// DATA STRUCTURES
// =============================================================================

// System settings structure
struct SystemSettings {
    // Sensor calibration
    float tempOffset;           // -10.0 to +10.0°C
    float humidityOffset;       // -20.0 to +20.0%
    
    // Audio settings    
    uint8_t audioSensitivity;  // 0-10
    
    // Classification thresholds
    uint16_t queenFreqMin;     // Hz
    uint16_t queenFreqMax;     // Hz
    uint16_t swarmFreqMin;     // Hz
    uint16_t swarmFreqMax;     // Hz
    uint8_t stressThreshold;   // 0-100
    
    // Logging
    uint8_t logInterval;       // 5, 10, 30, 60 minutes
    bool logEnabled;
    
    // Alert thresholds
    float tempMin;
    float tempMax;
    float humidityMin;
    float humidityMax;
    
    // System
    uint8_t displayBrightness; // 0-10
    uint32_t magicNumber;      // Validation
    uint16_t checksum;         // Simple checksum
};

// Sensor data structure
struct SensorData {
    // Environmental
    float temperature;
    float humidity;
    float pressure;
    float batteryVoltage;
    
    // Audio analysis
    uint16_t dominantFreq;
    uint8_t soundLevel;
    uint8_t beeState;
    
    // Status
    uint8_t alertFlags;
    bool sensorsValid;
};

// System status
struct SystemStatus {
    bool systemReady;
    bool rtcWorking;
    bool displayWorking;
    bool bmeWorking;   
    bool sdWorking;
    bool pdmWorking;
};

// Menu state
struct MenuState {
    bool settingsMenuActive;
    int menuLevel;
    int selectedItem;
    int editingItem;
    float editFloatValue;
    int editIntValue;
};

// Log entry structure (for reference)
struct LogEntry {
    uint32_t unixTime;
    float temperature;
    float humidity;
    float pressure;
    uint16_t dominantFreq;
    uint8_t soundLevel;
    uint8_t beeState;
    float batteryVoltage;
    uint8_t alertFlags;
};

// =============================================================================
// ABSCONDING DETECTION STRUCTURES
// =============================================================================

// Indicators for absconding risk
struct AbscondingIndicators {
    bool queenSilent;           // No queen sounds detected
    bool increasedActivity;     // Higher than normal frequency
    bool erraticPattern;        // Irregular sound patterns
    uint8_t riskLevel;         // 0-100%
    uint32_t lastQueenDetected; // Timestamp of last queen sound
};

// Daily activity pattern tracking
struct DailyPattern {
    uint8_t hourlyActivity[24];    // Average activity per hour
    uint8_t hourlyTemperature[24]; // Average temp per hour
    uint16_t peakActivityTime;     // Hour with most activity
    uint16_t quietestTime;         // Hour with least activity
    bool abnormalPattern;          // Deviation from normal
};

// Field event types
enum FieldEvents {
    EVENT_INSPECTION = 1,
    EVENT_FEEDING = 2,
    EVENT_TREATMENT = 3,
    EVENT_HARVEST = 4,
    EVENT_QUEEN_SEEN = 5,
    EVENT_SWARM_CAUGHT = 6,
    EVENT_ABSCONDED = 7,
    EVENT_PREDATOR = 8
};

// Environmental stress factors
enum StressFactors {
    STRESS_NONE = 0,
    STRESS_HEAT = 1,
    STRESS_COLD = 2,
    STRESS_HUMIDITY = 4,
    STRESS_PREDATOR = 8,
    STRESS_DISEASE = 16,
    STRESS_HUNGER = 32
};

#endif // DATA_STRUCTURES_H