/**
 * Display.cpp
 * Display management and UI rendering
 */

#include "Display.h"
#include "Utils.h"

// =============================================================================
// DISPLAY INITIALIZATION
// =============================================================================

void showStartupScreen(Adafruit_SH1106G& display) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    // Draw bee icon (simple)
    drawBeeIcon(display, 54, 10);
    
    display.setCursor(25, 30);
    display.println(F("Hive Monitor"));
    display.setCursor(35, 42);
    display.println(F("v2.0"));
    display.setCursor(15, 54);
    display.println(F("Initializing..."));
    
    display.display();
}

// Add these functions to Display.cpp

static int currentDiagLine = 0;

void showSensorDiagnosticsScreen(Adafruit_SH1106G& display, SystemStatus& status) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    // Title
    display.setCursor(15, 0);
    display.println(F("System Diagnostics"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    // Reset line counter
    currentDiagLine = 0;
    
    display.display();
}

void updateDiagnosticLine(Adafruit_SH1106G& display, const char* message) {
    int y = 16 + (currentDiagLine * 10);
    
    if (y > 54) return; // Don't overflow screen
    
    // Clear the line area
    display.fillRect(0, y, 128, 8, SH110X_BLACK);
    
    // Draw the message
    display.setCursor(2, y);
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.print(message);
    
    display.display();
    currentDiagLine++;
    }



// =============================================================================
// MAIN DISPLAY UPDATE
// =============================================================================


void updateDisplay(Adafruit_SH1106G& display, DisplayMode mode, 
                  SensorData& data, SystemSettings& settings,
                  SystemStatus& status, RTC_DS3231& rtc,
                  SpectralFeatures& features, ActivityTrend& trend) {
    if (!status.displayWorking) return;
    
    switch (mode) {
        case MODE_DASHBOARD:
            drawDashboard(display, data, status, rtc);
            break;
            
        case MODE_SOUND:
            drawSoundScreen(display, data, settings, features, trend);
            break;
            
        case MODE_ALERTS:
            drawAlertsScreen(display, data);
            break;
            
        case MODE_POWER:
            drawPowerScreen(display, data, settings, status);
            break;
            
        case MODE_SETTINGS:
            // Handled by menu system
            break;
    }
}
// =============================================================================
// DASHBOARD SCREEN
// =============================================================================


void drawDashboard(Adafruit_SH1106G& display, SensorData& data, 
                  SystemStatus& status, RTC_DS3231& rtc) {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);
    
    // Top line: Date/Time on left, Battery icon on right
    if (status.rtcWorking) {
        DateTime now = rtc.now();
        display.setCursor(0, 0);
        // Full date format
        char dateStr[18];
        sprintf(dateStr, "%02d/%02d/%04d %02d:%02d", now.month(), now.day(), now.year(), now.hour(), now.minute());
        display.print(dateStr);
    } else {
        display.setCursor(0, 0);
        display.print(F("--/--/---- --:--"));
    }
    
    // Battery icon on top right
    drawBatteryIcon(display, 112, 0, data.batteryVoltage);
    
    // Separator line
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    // Environmental data
    int y = 16;
    
    // Temperature
    display.setCursor(0, y);
    display.print(F("Temp: "));
    if (data.sensorsValid) {
        display.print(data.temperature, 1);
        display.print(F("C"));
    } else {
        display.print(F("--.-C"));
    }
    
    // Humidity
    y += 10;
    display.setCursor(0, y);
    display.print(F("Humidity: "));
    if (data.sensorsValid) {
        display.print(data.humidity, 1);
        display.print(F("%"));
    } else {
        display.print(F("--.-%%"));
    }
    
    // Pressure
    y += 10;
    display.setCursor(0, y);
    display.print(F("Pressure: "));
    if (data.sensorsValid) {
        display.print(data.pressure, 1);
        display.print(F(" hPa"));
    } else {
        display.print(F("----.- hPa"));
    }
    
    // Bottom separator
    display.drawLine(0, 52, 127, 52, SH110X_WHITE);
    
    // Status at bottom
    display.setCursor(0, 56);
    display.print(F("STATUS: "));
    
    if (data.beeState == BEE_PRE_SWARM) {
        display.print(F("PRE-SWARM"));
    } else if (data.beeState == BEE_QUEEN_MISSING) {
        display.print(F("NO QUEEN"));
    } else if (data.alertFlags & ALERT_TEMP_HIGH) {
        display.print(F("TOO HOT"));
    } else if (data.alertFlags & ALERT_TEMP_LOW) {
        display.print(F("TOO COLD"));
    } else if (data.alertFlags != ALERT_NONE) {
        display.print(F("ALERT"));
    } else {
        display.print(F("NORMAL"));
    }
    
    display.display();
}

// Alternative data-rich display mode
void drawDetailedData(Adafruit_SH1106G& display, SensorData& data, 
                     SystemStatus& status, AbscondingIndicators& risk) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    // Title
    display.setCursor(30, 0);
    display.println(F("Detailed Data"));
    display.drawLine(0, 9, 127, 9, SH110X_WHITE);
    
    // Two column layout for maximum data
    int y = 13;
    
    // Left column - Environmental
    display.setCursor(0, y);
    display.print(F("Temp:"));
    display.setCursor(35, y);
    display.print(data.temperature, 1);
    display.print(F("C"));
    
    y += 9;
    display.setCursor(0, y);
    display.print(F("Humid:"));
    display.setCursor(35, y);
    display.print(data.humidity, 1);
    display.print(F("%"));
    
    y += 9;
    display.setCursor(0, y);
    display.print(F("Press:"));
    display.setCursor(35, y);
    display.print(data.pressure, 0);
    
    y += 9;
    display.setCursor(0, y);
    display.print(F("Batt:"));
    display.setCursor(35, y);
    display.print(data.batteryVoltage, 2);
    display.print(F("V"));
    
    // Right column - Bee data
    y = 13;
    display.setCursor(70, y);
    display.print(F("Freq:"));
    display.setCursor(95, y);
    display.print(data.dominantFreq);
    
    y += 9;
    display.setCursor(70, y);
    display.print(F("Vol:"));
    display.setCursor(95, y);
    display.print(data.soundLevel);
    display.print(F("%"));
    
    y += 9;
    display.setCursor(70, y);
    display.print(F("Risk:"));
    display.setCursor(95, y);
    display.print(risk.riskLevel);
    display.print(F("%"));
    
    y += 9;
    display.setCursor(70, y);
    display.print(F("State:"));
    display.setCursor(95, y);
    display.print(data.beeState);
    
    // Bottom status line
    display.setCursor(0, 55);
    if (risk.queenSilent) {
        display.print(F("Queen silent >"));
        display.print((millis() - risk.lastQueenDetected) / 3600000);
        display.print(F("h"));
    } else {
        display.print(F("Systems: "));
        int working = 0;
        if (status.rtcWorking) working++;
        if (status.bmeWorking) working++;        
        if (status.sdWorking) working++;
        if (status.pdmWorking) working++;
        display.print(working);
        display.print(F("/4 OK"));
    }
    
    display.display();
}
// =============================================================================
// SOUND MONITOR SCREEN
// =============================================================================

void drawSoundScreen(Adafruit_SH1106G& display, SensorData& data, 
                    SystemSettings& settings, SpectralFeatures& features,
                    ActivityTrend& trend) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    // Title
    display.setCursor(30, 0);
    display.println(F("Sound Monitor"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    // Centroid frequency (more meaningful than zero-crossing)
    display.setCursor(0, 16);
    display.print(F("Centroid: "));
    display.print((int)features.spectralCentroid);
    display.print(F(" Hz"));
    
    // Sound level with visual bar (keep existing function)
    display.setCursor(0, 28);
    display.print(F("Level:"));
    drawSoundLevelBar(display, 45, 26, 80, 10, data.soundLevel);
    
    // Baseline comparison (very useful for beekeepers)
    display.setCursor(0, 40);
    display.print(F("Baseline: "));
    display.print((int)trend.baselineActivity);
    display.print(F("% ("));
    
    int change = (int)((trend.activityIncrease - 1.0) * 100);
    if(change >= 0) display.print(F("+"));
    display.print(change);
    display.print(F("%)"));
    
    // Pattern status (key insight for beekeepers)
    display.setCursor(0, 52);
    display.print(F("Pattern: "));
    if(trend.abnormalTiming) {
        display.print(F("ABNORMAL"));
    } else if(trend.activityIncrease > 1.5) {
        display.print(F("HIGH"));
    } else if(trend.activityIncrease < 0.7) {
        display.print(F("LOW"));
    } else {
        display.print(F("NORMAL"));
    }
    
    display.display();
}
// =============================================================================
// ALERTS SCREEN
// =============================================================================

void drawAlertsScreen(Adafruit_SH1106G& display, SensorData& data) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    // Title
    display.setCursor(40, 0);
    display.println(F("Alerts"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    int yPos = 16;
    
    if (data.alertFlags == ALERT_NONE) {
        display.setCursor(30, 30);
        display.print(F("No active alerts"));
    } else {
        // Display each active alert
        if (data.alertFlags & ALERT_TEMP_HIGH) {
            drawAlertLine(display, yPos, "Temp HIGH", data.temperature, "c");
            yPos += 10;
        }
        if (data.alertFlags & ALERT_TEMP_LOW) {
            drawAlertLine(display, yPos, "Temp LOW", data.temperature, "c");
            yPos += 10;
        }
        if (data.alertFlags & ALERT_HUMIDITY_HIGH) {
            drawAlertLine(display, yPos, "Humidity HIGH", data.humidity, "%");
            yPos += 10;
        }
        if (data.alertFlags & ALERT_HUMIDITY_LOW) {
            drawAlertLine(display, yPos, "Humidity LOW", data.humidity, "%");
            yPos += 10;
        }
        if (data.alertFlags & ALERT_QUEEN_ISSUE) {
            drawAlertLine(display, yPos, "Queen issue", 0, "");
            yPos += 10;
        }
        if (data.alertFlags & ALERT_SWARM_RISK) {
            drawAlertLine(display, yPos, "Swarm risk!", 0, "");
            yPos += 10;
        }
        if (data.alertFlags & ALERT_LOW_BATTERY) {
            drawAlertLine(display, yPos, "Low battery", data.batteryVoltage, "V");
            yPos += 10;
        }
        if (data.alertFlags & ALERT_SD_ERROR) {
            drawAlertLine(display, yPos, "SD card error", 0, "");
            yPos += 10;
        }
    }
    
    display.display();
}

void drawPowerScreen(Adafruit_SH1106G& display, SensorData& data, 
                    SystemSettings& settings, SystemStatus& status) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    display.setCursor(30, 0);
    display.println(F("Power Status"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    // Battery voltage and percentage
    display.setCursor(0, 16);
    display.print(F("Battery: "));
    display.print(data.batteryVoltage, 2);
    display.print(F("V"));
    
    display.setCursor(0, 28);
    display.print(F("Level: "));
    display.print(getBatteryLevel(data.batteryVoltage));
    display.print(F("%"));
    
    // Power source indication
    display.setCursor(0, 40);
    if (data.batteryVoltage >= BATTERY_USB_THRESHOLD) {
        display.print(F("Source: USB Power"));
    } else {
        display.print(F("Source: Battery"));
    }
    
    // System uptime
    display.setCursor(0, 52);
    display.print(F("Uptime: "));
    display.print(millis() / 3600000);
    display.print(F("h"));
    
    display.display();
}
// =============================================================================
// UI COMPONENTS
// =============================================================================

void drawHeader(Adafruit_SH1106G& display, SensorData& data, SystemStatus& status) {
    // Title
    display.setCursor(0, 0);
    display.print(F("Hive Monitor"));
    
    // Battery icon
    drawBatteryIcon(display, 100, 0, data.batteryVoltage);
    
    // Alert indicator
    if (data.alertFlags != ALERT_NONE) {
        display.setCursor(85, 0);
        display.print(F("!"));
    }
    
    // SD card indicator
    if (status.sdWorking) {
        display.setCursor(70, 0);
        display.print(F("SD"));
    }
    
    // Separator line
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
}

void drawTimeDate(Adafruit_SH1106G& display, int y, RTC_DS3231& rtc, 
                 SystemStatus& status) {
    display.setCursor(0, y);
    display.write(0x07); // Clock symbol
    display.setCursor(12, y);
    
    if (status.rtcWorking) {
        DateTime now = rtc.now();
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", now.hour(), now.minute());
        display.print(timeStr);
        
        display.setCursor(48, y);
        char dateStr[9];
        sprintf(dateStr, "%02d/%02d/%02d", now.day(), now.month(), now.year() % 100);
        display.print(dateStr);
    } else {
        display.print(F("--:-- --/--/--"));
    }
}

void drawEnvironmentalData(Adafruit_SH1106G& display, int y, SensorData& data) {
    // Temperature
    display.setCursor(0, y);
    display.write(0x0F); // Thermometer symbol
    display.setCursor(12, y);
    if (data.sensorsValid) {
        display.print(data.temperature, 1);
        display.print(F("c"));
    } else {
        display.print(F("--.-c"));
    }
    
    // Humidity
    display.setCursor(0, y + 12);
    display.write(0x04); // Drop symbol
    display.setCursor(12, y + 12);
    if (data.sensorsValid) {
        display.print(data.humidity, 1);
        display.print(F("%"));
    } else {
        display.print(F("--.-%%"));
    }
    
    // Pressure
    display.setCursor(0, y + 24);
    display.write(0x19); // Down arrow
    display.setCursor(12, y + 24);
    if (data.sensorsValid) {
        display.print(data.pressure, 1);
        display.print(F(" hPa"));
    } else {
        display.print(F("---.- hPa"));
    }
}

void drawBeeState(Adafruit_SH1106G& display, int x, int y, uint8_t state) {
    display.setCursor(x, y);
    display.print(F("Bee:"));
    display.setCursor(x, y + 12);
    
    switch (state) {
        case BEE_QUIET: 
            display.print(F("Quiet")); 
            break;
        case BEE_NORMAL: 
            display.print(F("Normal")); 
            break;
        case BEE_ACTIVE: 
            display.print(F("Active")); 
            break;
        case BEE_QUEEN_PRESENT: 
            display.print(F("Queen+")); 
            break;
        case BEE_QUEEN_MISSING: 
            display.print(F("Queen-")); 
            break;
        case BEE_PRE_SWARM: 
            display.print(F("Swarm!")); 
            break;
        case BEE_DEFENSIVE: 
            display.print(F("Defend")); 
            break;
        case BEE_STRESSED: 
            display.print(F("Stress")); 
            break;
        default: 
            display.print(F("...")); 
            break;
    }
}

void drawBatteryIcon(Adafruit_SH1106G& display, int16_t x, int16_t y, float voltage) {
    if (voltage >= BATTERY_USB_THRESHOLD) {
        // Lightning bolt for USB power
        display.drawLine(x + 2, y + 1, x + 5, y + 4, SH110X_WHITE);
        display.drawLine(x + 4, y + 3, x + 7, y + 3, SH110X_WHITE);
        display.drawLine(x + 6, y + 4, x + 9, y + 7, SH110X_WHITE);
        display.drawLine(x + 1, y + 2, x + 4, y + 2, SH110X_WHITE);
        display.drawLine(x + 7, y + 5, x + 10, y + 5, SH110X_WHITE);
        return;
    }
    
    // Battery outline
    display.drawRect(x, y + 2, 12, 6, SH110X_WHITE);
    display.drawRect(x + 12, y + 3, 2, 4, SH110X_WHITE);
    
    // Fill based on level
    int level = getBatteryLevel(voltage);
    int fillWidth = map(level, 0, 100, 0, 10);
    if (fillWidth > 0) {
        display.fillRect(x + 1, y + 3, fillWidth, 4, SH110X_WHITE);
    }
}

void drawSoundLevelBar(Adafruit_SH1106G& display, int x, int y, 
                      int width, int height, uint8_t level) {
    // Draw outline
    display.drawRect(x, y, width + 2, height, SH110X_WHITE);
    
    // Fill based on level
    int fillWidth = map(level, 0, 100, 0, width);
    if (fillWidth > 0) {
        display.fillRect(x + 1, y + 1, fillWidth, height - 2, SH110X_WHITE);
    }
    
    // Show percentage
    display.setCursor(x + width + 5, y + 2);
    display.print(level);
    display.print(F("%"));
}

void drawAlertLine(Adafruit_SH1106G& display, int y, const char* text, 
                  float value, const char* unit) {
    display.setCursor(0, y);
    display.print(F("> "));
    display.print(text);
    
    if (strlen(unit) > 0) {
        display.print(F(": "));
        display.print(value, 1);
        display.print(unit);
    }
}

void drawBeeIcon(Adafruit_SH1106G& display, int x, int y) {
    // Simple bee icon
    // Body
    display.fillCircle(x + 10, y + 5, 4, SH110X_WHITE);
    // Head
    display.fillCircle(x + 4, y + 5, 3, SH110X_WHITE);
    // Wings
    display.drawCircle(x + 10, y, 3, SH110X_WHITE);
    display.drawCircle(x + 10, y + 10, 3, SH110X_WHITE);
    // Stripes
    display.drawLine(x + 8, y + 2, x + 8, y + 8, SH110X_BLACK);
    display.drawLine(x + 11, y + 2, x + 11, y + 8, SH110X_BLACK);
}

// daily summary screen

void drawDailySummary(Adafruit_SH1106G& display, DailyPattern& pattern,
                     AbscondingIndicators& indicators) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    
    // Title
    display.setCursor(25, 0);
    display.println(F("Daily Summary"));
    display.drawLine(0, 10, 127, 10, SH110X_WHITE);
    
    // Absconding risk with visual indicator
    display.setCursor(0, 16);
    display.print(F("Absconding Risk: "));
    display.print(indicators.riskLevel);
    display.print(F("%"));
    
    // Risk level bar
    display.drawRect(0, 26, 102, 8, SH110X_WHITE);
    int riskBar = map(indicators.riskLevel, 0, 100, 0, 100);
    display.fillRect(1, 27, riskBar, 6, SH110X_WHITE);
    
    // Key indicators
    display.setCursor(0, 38);
    if (indicators.queenSilent) {
        display.println(F("! Queen not heard"));
    } else {
        display.print(F("Queen OK - "));
        display.print((millis() - indicators.lastQueenDetected) / 60000);
        display.println(F(" min ago"));
    }
    
    // Activity pattern
    display.setCursor(0, 48);
    display.print(F("Peak activity: "));
    display.print(pattern.peakActivityTime);
    display.print(F(":00"));
    
    if (pattern.abnormalPattern) {
        display.setCursor(0, 56);
        display.print(F("! Unusual pattern"));
    }
    
    display.display();
}