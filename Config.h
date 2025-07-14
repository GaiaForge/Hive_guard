/**
 * Config.h
 * Hardware configuration and constants
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <RTClib.h>
#include <SD.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_GFX.h>
#include <Adafruit_BME280.h>


// =============================================================================
// HARDWARE PINS
// =============================================================================

// OLED Display
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

// SD Card
#define SD_CS_PIN 10

// Battery monitoring
#define VBATPIN A6

// Buttons
#define BTN_UP    5
#define BTN_DOWN  6
#define BTN_SELECT 9
#define BTN_BACK  11

// =============================================================================
// DISPLAY POWER CONTROL
// =============================================================================

// Display power control
#define DISPLAY_POWER_PIN 12    // Digital pin to control display power
#define DISPLAY_POWER_ON HIGH   // Pin state for display on
#define DISPLAY_POWER_OFF LOW   // Pin state for display off


// Field mode display timeout (1-5 minutes only)
#define DEFAULT_DISPLAY_TIMEOUT_MIN 2     // Default 2 minutes
#define MIN_DISPLAY_TIMEOUT_MIN 1         // Minimum 1 minute  
#define MAX_DISPLAY_TIMEOUT_MIN 5         // Maximum 5 minutes

// =============================================================================
// TIMING CONSTANTS
// =============================================================================

const unsigned long SENSOR_INTERVAL = 5000;    // 5 seconds
const unsigned long DISPLAY_INTERVAL = 1000;   // 1 second
const unsigned long BUTTON_INTERVAL = 50;      // 50ms
const unsigned long AUDIO_INTERVAL = 100;      // 100ms
const unsigned long DEBOUNCE_DELAY = 50;       // 50ms

// PCF8523 specific settings
#define RTC_BACKUP_BATTERY_SWITCHOVER true
#define RTC_CALIBRATION_MODE PCF8523_TwoHours
#define RTC_CAPACITOR_SELECTION PCF8523_Capacitor_12_5pF

// Time sync settings  
#define TIME_SYNC_INTERVAL_HOURS 24  // Sync with external time daily
#define MAX_TIME_DRIFT_SECONDS 300   // 5 minutes max drift before warning

// =============================================================================
// AUDIO CONFIGURATION
// =============================================================================
#define AUDIO_INPUT_PIN  A4  // MAX9814 microphone input pin
#define AUDIO_SAMPLE_BUFFER_SIZE 128
#define AUDIO_SAMPLE_RATE 8000

// =============================================================================
// DEFAULT SETTINGS
// =============================================================================

// Temperature defaults (Celsius)
#define DEFAULT_TEMP_MIN 15.0
#define DEFAULT_TEMP_MAX 40.0
#define DEFAULT_TEMP_OFFSET 0.0

// Humidity defaults (%)
#define DEFAULT_HUMIDITY_MIN 40.0
#define DEFAULT_HUMIDITY_MAX 80.0
#define DEFAULT_HUMIDITY_OFFSET 0.0

// Audio defaults
#define DEFAULT_AUDIO_SENSITIVITY 5

// Bee frequency ranges (Hz)
#define DEFAULT_QUEEN_FREQ_MIN 200
#define DEFAULT_QUEEN_FREQ_MAX 350
#define DEFAULT_SWARM_FREQ_MIN 400
#define DEFAULT_SWARM_FREQ_MAX 600
#define DEFAULT_STRESS_THRESHOLD 70

// Logging defaults
#define DEFAULT_LOG_INTERVAL 10  // minutes
#define DEFAULT_LOG_ENABLED true

// System defaults
#define DEFAULT_DISPLAY_BRIGHTNESS 7
#define DEFAULT_FIELD_MODE_ENABLED false


// =============================================================================
// SYSTEM CONSTANTS
// =============================================================================

#define SETTINGS_MAGIC_NUMBER 0xBEE51234  
#define MAX_LOG_AGE_DAYS 365  // 1 year retention

// Battery voltage thresholds
#define BATTERY_USB_THRESHOLD 4.5
#define BATTERY_FULL 4.1
#define BATTERY_HIGH 3.9
#define BATTERY_MED 3.7
#define BATTERY_LOW 3.5
#define BATTERY_CRITICAL 3.2

#endif // CONFIG_H

// Add to Config.h - African bee specific settings

// =============================================================================
// AFRICAN BEE DEFAULTS
// =============================================================================

// Temperature thresholds for African bees
#define AFRICAN_TEMP_MIN 18.0      // They're more cold sensitive
#define AFRICAN_TEMP_MAX 35.0      // Critical heat threshold
#define AFRICAN_TEMP_OPTIMAL 28.0  // Ideal temperature

// African bee audio signatures
#define AFRICAN_QUEEN_FREQ_MIN 230  // Slightly higher pitched
#define AFRICAN_QUEEN_FREQ_MAX 380  
#define AFRICAN_SWARM_FREQ_MIN 420  // More aggressive swarming
#define AFRICAN_SWARM_FREQ_MAX 650
#define AFRICAN_AGGRESSION_FREQ 700 // Defensive behavior

// Activity thresholds
#define AFRICAN_NORMAL_ACTIVITY 30   // They're generally more active
#define AFRICAN_HIGH_ACTIVITY 70
#define AFRICAN_ABSCONDING_PATTERN 85 // Very high activity before leaving

// Time windows (African bees work different hours)
#define AFRICAN_MORNING_START 6      // Earlier start
#define AFRICAN_PEAK_START 9         
#define AFRICAN_PEAK_END 16         
#define AFRICAN_EVENING_END 18       // Earlier end due to predators

// =============================================================================
// TanzaniaN TOP BAR HIVE SPECIFICS
// =============================================================================

// Different thermal dynamics than Langstroth
#define KTB_THERMAL_MASS_FACTOR 0.8  // Less insulation
#define KTB_VENTILATION_FACTOR 1.2   // Better airflow

// =============================================================================
// FIELD DEPLOYMENT SETTINGS
// =============================================================================

// Power saving for extended deployment
#define FIELD_LOG_INTERVAL 15        // Log every 15 minutes
#define FIELD_DISPLAY_TIMEOUT 30000  // Display off after 30 seconds
#define FIELD_SENSOR_INTERVAL 10000  // Read sensors every 10 seconds

// Critical alert thresholds
#define CRITICAL_BATTERY_VOLTAGE 3.3  // Must last weeks in field
#define QUEEN_ABSENCE_ALERT_HOURS 3  // Alert after 3 hours no queen
#define ABSCONDING_RISK_THRESHOLD 60  // Alert at 60% risk