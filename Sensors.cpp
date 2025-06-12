/**
 * Sensors.cpp
 * Sensor reading and management
 */

#include "Sensors.h"

// =============================================================================
// SENSOR INITIALIZATION
// =============================================================================

void initializeSensors(Adafruit_BMP280& bmp, Adafruit_SHT31& sht, SystemStatus& status) {
    // Initialize BMP280
    if (bmp.begin(0x77) || bmp.begin(0x76)) {
        status.bmpWorking = true;
        
        // Configure BMP280
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                        Adafruit_BMP280::SAMPLING_X2,
                        Adafruit_BMP280::SAMPLING_X16,
                        Adafruit_BMP280::FILTER_X16,
                        Adafruit_BMP280::STANDBY_MS_500);
        
        Serial.println(F("BMP280 initialized"));
    } else {
        Serial.println(F("BMP280 not found"));
    }
    
    // Initialize SHT31
    if (sht.begin(0x44)) {
        status.shtWorking = true;
        sht.heater(false);  // Disable heater
        Serial.println(F("SHT31 initialized"));
    } else {
        Serial.println(F("SHT31 not found"));
    }
}

// =============================================================================
// SENSOR READING
// =============================================================================

void readAllSensors(Adafruit_BMP280& bmp, Adafruit_SHT31& sht, 
                    SensorData& data, SystemSettings& settings, 
                    SystemStatus& status) {
    // Read battery first
    readBattery(data);
    
    // Read environmental sensors
    bool tempValid = false;
    bool humidValid = false;
    bool pressureValid = false;
    
    // Read BMP280
    if (status.bmpWorking) {
        float bmpTemp = bmp.readTemperature();
        float bmpPressure = bmp.readPressure() / 100.0;  // Convert to hPa
        
        if (!isnan(bmpTemp) && !isnan(bmpPressure)) {
            data.temperature = bmpTemp + settings.tempOffset;
            data.pressure = bmpPressure;
            tempValid = true;
            pressureValid = true;
        }
    }
    
    // Read SHT31
    if (status.shtWorking) {
        float shtTemp = sht.readTemperature();
        float shtHumidity = sht.readHumidity();
        
        if (!isnan(shtTemp) && !isnan(shtHumidity)) {
            // Average temperature if we have both readings
            if (!tempValid) {
                data.temperature = shtTemp + settings.tempOffset;
            } else {
                // Average the two temperature readings
                data.temperature = ((data.temperature + shtTemp + settings.tempOffset) / 2.0);
            }
            
            data.humidity = shtHumidity + settings.humidityOffset;
            
            // Constrain humidity to valid range
            data.humidity = constrain(data.humidity, 0.0, 100.0);
            
            tempValid = true;
            humidValid = true;
        }
    }
    
    // Update sensor validity flag
    data.sensorsValid = (tempValid && humidValid && pressureValid);
}

// =============================================================================
// BATTERY MONITORING
// =============================================================================

void readBattery(SensorData& data) {
    // Read analog value
    float measuredvbat = analogRead(VBATPIN);
    
    // Convert to voltage
    // Voltage divider reduces battery voltage by half
    // Reference voltage is 3.6V
    // ADC is 10-bit (0-1023)
    measuredvbat *= 2;      // Account for voltage divider
    measuredvbat *= 3.6;    // Reference voltage
    measuredvbat /= 1024;   // Convert from ADC value
    
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

void runSensorDiagnostics(Adafruit_BMP280& bmp, Adafruit_SHT31& sht, 
                         SystemStatus& status) {
    Serial.println(F("\n=== Sensor Diagnostics ==="));
    
    // Check BMP280
    if (status.bmpWorking) {
        Serial.print(F("BMP280: OK - "));
        Serial.print(F("Temp: "));
        Serial.print(bmp.readTemperature());
        Serial.print(F("°C, Pressure: "));
        Serial.print(bmp.readPressure() / 100.0);
        Serial.println(F(" hPa"));
    } else {
        Serial.println(F("BMP280: NOT FOUND"));
    }
    
    // Check SHT31
    if (status.shtWorking) {
        Serial.print(F("SHT31: OK - "));
        Serial.print(F("Temp: "));
        Serial.print(sht.readTemperature());
        Serial.print(F("°C, Humidity: "));
        Serial.print(sht.readHumidity());
        Serial.println(F("%"));
    } else {
        Serial.println(F("SHT31: NOT FOUND"));
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