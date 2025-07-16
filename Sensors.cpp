/**
 * Sensors.cpp
 * Sensor reading and management
 */

#include "Sensors.h"

// =============================================================================
// SENSOR INITIALIZATION
// =============================================================================
void initializeSensors(Adafruit_BME280& bme, SystemStatus& status) {
    // Initialize BME280 (handles temp, humidity, AND pressure)
    if (bme.begin(0x77) || bme.begin(0x76)) {
        status.bmeWorking = true;
        
        // Configure BME280 for forced mode (power efficient)
        bme.setSampling(Adafruit_BME280::MODE_FORCED,     // Take reading then sleep
                        Adafruit_BME280::SAMPLING_X2,     // temperature
                        Adafruit_BME280::SAMPLING_X2,     // pressure  
                        Adafruit_BME280::SAMPLING_X2,     // humidity
                        Adafruit_BME280::FILTER_X16,
                        Adafruit_BME280::STANDBY_MS_500);
        
        Serial.println(F("BME280 initialized (forced mode)"));
    } else {
        Serial.println(F("BME280 not found"));
        status.bmeWorking = false;
    }
}

void readAllSensors(Adafruit_BME280& bme, SensorData& data, 
                    SystemSettings& settings, SystemStatus& status) {


    // Add 200ms stabilization delay for sensors after wake-up
    static unsigned long lastReading = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastReading < 200) {
        delay(200 - (currentTime - lastReading));
    }
    lastReading = millis();

    // Read battery first
    readBattery(data);
    
    // Read BME280 - trigger forced measurement
    if (status.bmeWorking) {
        // In forced mode, we need to trigger a measurement
        bme.takeForcedMeasurement();
        
        // Now read the results
        float temp = bme.readTemperature();
        float humidity = bme.readHumidity();
        float pressure = bme.readPressure() / 100.0;  // Convert to hPa
        
        if (!isnan(temp) && !isnan(humidity) && !isnan(pressure)) {
            data.temperature = temp + settings.tempOffset;
            data.humidity = humidity + settings.humidityOffset;
            data.pressure = pressure;
            
            // Constrain humidity to valid range
            data.humidity = constrain(data.humidity, 0.0, 100.0);
            data.sensorsValid = true;
        } else {
            data.sensorsValid = false;
        }
    } else {
        data.sensorsValid = false;
    }
}


// =============================================================================
// BATTERY MONITORING
// =============================================================================

void readBattery(SensorData& data) {
    // Explicitly set 12-bit resolution (nRF52840 default)
    analogReadResolution(12);
    
    // Read analog value
    float measuredvbat = analogRead(VBATPIN);
    
    // Convert to voltage (corrected for 12-bit ADC)
    measuredvbat *= 2;      // Account for voltage divider
    measuredvbat *= 3.6;    // Reference voltage
    measuredvbat /= 4095;   // Convert from 12-bit ADC value (not 1024)
    
    data.batteryVoltage = measuredvbat;
}

// =============================================================================
// BATTERY LEVEL CALCULATION
// =============================================================================

int getBatteryLevel(float voltage) {
    // USB power
    if (voltage >= BATTERY_USB_THRESHOLD) {
        return 100;
    }
    // Battery levels
    else if (voltage >= BATTERY_FULL) {
        return 100;
    }
    else if (voltage >= BATTERY_HIGH) {
        return 80;
    }
    else if (voltage >= 3.8) {
        return 60;
    }
    else if (voltage >= BATTERY_MED) {
        return 40;
    }
    else if (voltage >= BATTERY_LOW) {
        return 20;
    }
    else if (voltage >= BATTERY_CRITICAL) {
        return 10;
    }
    else {
        return 0;
    }
}

// =============================================================================
// SENSOR DIAGNOSTICS
// =============================================================================

void runSensorDiagnostics(Adafruit_BME280& bme, SystemStatus& status) {
    Serial.println(F("\n=== Sensor Diagnostics ==="));
    
    // Check BME280
    if (status.bmeWorking) {
        Serial.print(F("BME280: OK - "));
        Serial.print(F("Temp: "));
        Serial.print(bme.readTemperature());
        Serial.print(F("°C, Humidity: "));
        Serial.print(bme.readHumidity());
        Serial.print(F("%, Pressure: "));
        Serial.print(bme.readPressure() / 100.0);
        Serial.println(F(" hPa"));
    } else {
        Serial.println(F("BME280: NOT FOUND"));
    }
    
    // Battery status
    float voltage = 0;
    for (int i = 0; i < 10; i++) {
        voltage += analogRead(VBATPIN);
        delay(10);
    }
    voltage /= 10;  // Average
    voltage *= 2 * 3.6 / 1024;
    
    Serial.print(F("Battery: "));
    Serial.print(voltage);
    Serial.print(F("V ("));
    Serial.print(getBatteryLevel(voltage));
    Serial.println(F("%)"));
    
    Serial.println(F("========================\n"));
}

// =============================================================================
// SENSOR CALIBRATION
// =============================================================================

void calibrateSensors(SystemSettings& settings, SensorData& currentData, 
                     float knownTemp, float knownHumidity) {
    // Calculate offsets based on known values
    float measuredTemp = currentData.temperature - settings.tempOffset;
    float measuredHumidity = currentData.humidity - settings.humidityOffset;
    
    settings.tempOffset = knownTemp - measuredTemp;
    settings.humidityOffset = knownHumidity - measuredHumidity;
    
    // Constrain to valid ranges
    settings.tempOffset = constrain(settings.tempOffset, -10.0, 10.0);
    settings.humidityOffset = constrain(settings.humidityOffset, -20.0, 20.0);
    
    Serial.print(F("Calibration complete. Temp offset: "));
    Serial.print(settings.tempOffset);
    Serial.print(F("°C, Humidity offset: "));
    Serial.print(settings.humidityOffset);
    Serial.println(F("%"));
}