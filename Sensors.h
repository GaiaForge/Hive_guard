#ifndef SENSORS_H
#define SENSORS_H

#include "Config.h"
#include "DataStructures.h"
#include <Adafruit_BME280.h>  

// Function declarations
void initializeSensors(Adafruit_BME280& bme, SystemStatus& status);
void readAllSensors(Adafruit_BME280& bme, SensorData& data, 
                    SystemSettings& settings, SystemStatus& status);
void runSensorDiagnostics(Adafruit_BME280& bme, SystemStatus& status);

void readBattery(SensorData& data);
int getBatteryLevel(float voltage);

void calibrateSensors(SystemSettings& settings, SensorData& currentData, 
                     float knownTemp, float knownHumidity);

#endif // SENSORS_H