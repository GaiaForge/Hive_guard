/**
 * Sensors.h
 * Sensor management header
 */

#ifndef SENSORS_H
#define SENSORS_H

#include "Config.h"
#include "DataStructures.h"

// Function declarations
void initializeSensors(Adafruit_BMP280& bmp, Adafruit_SHT31& sht, SystemStatus& status);
void readAllSensors(Adafruit_BMP280& bmp, Adafruit_SHT31& sht, 
                    SensorData& data, SystemSettings& settings, 
                    SystemStatus& status);
void readBattery(SensorData& data);
int getBatteryLevel(float voltage);
void runSensorDiagnostics(Adafruit_BMP280& bmp, Adafruit_SHT31& sht, 
                         SystemStatus& status);
void calibrateSensors(SystemSettings& settings, SensorData& currentData, 
                     float knownTemp, float knownHumidity);

#endif // SENSORS_H