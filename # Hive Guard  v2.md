# Tanzania Hive Monitor - Complete User Manual
## Version 2.1 - Professional Beekeeping Monitoring System

---

# Table of Contents

1. [System Overview](#system-overview)
2. [Hardware Setup](#hardware-setup)
3. [First Time Setup](#first-time-setup)
4. [Daily Operation](#daily-operation)
5. [Menu System & Settings](#menu-system--settings)
6. [Field Mode Operation](#field-mode-operation)
7. [Bluetooth Data Retrieval](#bluetooth-data-retrieval)
8. [Alert System](#alert-system)
9. [Firmware Updates](#firmware-updates)
10. [Troubleshooting](#troubleshooting)
11. [Maintenance](#maintenance)
12. [Data Analysis](#data-analysis)
13. [Advanced Features](#advanced-features)

---

# System Overview

## What the Hive Monitor Does

The Tanzania Hive Monitor is a solar-powered electronic system that continuously monitors your beehive's health by tracking:

- **Temperature & Humidity** - Critical for brood development
- **Sound Analysis** - Detects queen presence, swarming behavior, stress
- **Activity Patterns** - Monitors daily bee activity cycles
- **Environmental Conditions** - Tracks weather impacts on hive health
- **Alert Generation** - Warns of critical situations requiring intervention

## Key Features

### 🔋 **Power Management**
- **Testing Mode**: Full power, always-on display for setup and testing
- **Field Mode**: Battery-optimized operation for weeks of autonomous monitoring
- **Automatic Power Saving**: Adjusts operation based on battery level

### 📊 **Data Collection**
- **Real-time Monitoring**: Continuous sensor readings every 5 seconds
- **Data Logging**: Automatic CSV file creation with timestamps
- **Emergency Buffering**: Stores data during SD card failures
- **Monthly File Organization**: Automatic file management by date

### 📱 **Connectivity**
- **Bluetooth Data Transfer**: Wireless data retrieval with smartphone
- **Multiple Transfer Modes**: Manual, scheduled, or always-on Bluetooth
- **Remote Configuration**: Update settings via Bluetooth connection

### 🎯 **African Bee Optimized**
- **Specialized Audio Analysis**: Tuned for African bee frequency patterns
- **Climate Adapted**: Temperature and humidity ranges for tropical conditions
- **Absconding Detection**: Early warning system for colony abandonment
- **Defensive Behavior Recognition**: Identifies aggressive colony states

---

# Hardware Setup

## Required Components

### Core System
- **Adafruit Feather nRF52840 Express** (main controller)
- **BME280 Sensor** (temperature, humidity, pressure)
- **MAX9814 Microphone** (audio monitoring)
- **128x64 OLED Display** (user interface)
- **PCF8523 RTC Module** (real-time clock)
- **MicroSD Card Module** (data storage)
- **4 Tactile Buttons** (navigation)

### Power System
- **3.7V Li-Po Battery** (1200mAh minimum recommended)
- **Solar Panel** (5V, 1W minimum for field deployment)
- **Charging Circuit** (built into Feather nRF52840)

### Enclosure
- **Weatherproof Housing** (IP65 minimum for outdoor use)
- **Ventilation Mesh** (for microphone access)
- **Mounting Hardware** (for hive attachment)

## Physical Installation

### 1. Hive Placement
```
IDEAL POSITION:
- Mount on side of hive body (not super)
- 15-20cm from entrance
- Protected from direct rain
- South-facing for solar panel optimization
- Accessible for button operation
```

### 2. Sensor Positioning
- **Microphone**: Face toward hive entrance, protected by mesh
- **BME280**: Inside hive airspace or protected external mounting
- **Solar Panel**: Clear sky view, 30° angle toward equator

### 3. Wiring Connections
```
I2C Devices (BME280, OLED, RTC):
- VCC → 3.3V
- GND → Ground
- SDA → Pin SDA
- SCL → Pin SCL

Audio (MAX9814):
- VCC → 3.3V
- GND → Ground
- OUT → Pin A4

SD Card:
- VCC → 3.3V
- GND → Ground
- MOSI → Pin MOSI
- MISO → Pin MISO
- SCK → Pin SCK
- CS → Pin 10

Buttons:
- UP → Pin A0
- DOWN → Pin A1
- SELECT → Pin A2
- BACK → Pin A3
```

---

# First Time Setup

## Initial Power-On

1. **Connect Battery**: Plug in charged Li-Po battery
2. **First Boot**: System will show startup screen for 3 seconds
3. **Diagnostics**: Automatic hardware check (RTC, sensors, SD card)
4. **Default Dashboard**: System starts in Testing Mode

## Initial Configuration

### Step 1: Set Date & Time
```
Navigation: SELECT → Settings → Time & Date
1. Press UP/DOWN to select year/month/day/hour/minute
2. Press SELECT to edit selected field
3. Use UP/DOWN to change value
4. Press SELECT to save
5. Press BACK to return to main menu
```

### Step 2: Calibrate Sensors
```
Navigation: SELECT → Settings → Sensor Calib
1. Place known thermometer near BME280
2. Select "Temp Offset"
3. Adjust to match known temperature
4. Repeat for humidity if hygrometer available
```

### Step 3: Configure Audio Settings
```
Navigation: SELECT → Settings → Audio Settings
- Sensitivity: Start with 5, adjust based on background noise
- Queen Frequencies: 200-350 Hz (default for African bees)
- Swarm Frequencies: 400-600 Hz
- Stress Threshold: 70% (default)
```

### Step 4: Set Alert Thresholds
```
Navigation: SELECT → Settings → Alert Thresholds
African Bee Recommended Ranges:
- Temperature: 18°C - 35°C
- Humidity: 40% - 80%
```

### Step 5: Configure Bluetooth
```
Navigation: SELECT → Settings → Bluetooth
1. Set Device ID (1-255)
2. Enter Hive Name (e.g., "TREE_A1")
3. Enter Location (e.g., "NORTH_FIELD")
4. Enter Beekeeper Name
5. Choose Mode:
   - Manual: Button-activated only
   - Scheduled: Specific hours (e.g., 7AM-6PM)
   - Always On: Continuous (high battery usage)
```

---

# Daily Operation

## Display Modes

### Dashboard (Main Screen)
```
┌─────────────────────────────┐
│ 15/03/2024 14:25      [🔋] │
├─────────────────────────────┤
│ Temp: 28.5°C                │
│ Humidity: 65.2%             │
│ Pressure: 1013.2 hPa        │
├─────────────────────────────┤
│ STATUS: NORMAL              │
└─────────────────────────────┘
```

**Navigation:**
- UP/DOWN: Switch between display modes
- SELECT: Enter settings menu
- BACK: Return to dashboard

### Sound Monitor
```
┌─────────────────────────────┐
│        Sound Monitor        │
├─────────────────────────────┤
│ Centroid: 285 Hz            │
│ Level: [████░░░░░░] 45%     │
│ Baseline: 42% (+7%)         │
│ Pattern: NORMAL             │
└─────────────────────────────┘
```

**Key Indicators:**
- **Centroid**: Average frequency (queen = 200-350 Hz)
- **Level**: Current sound intensity
- **Baseline**: Comparison to normal activity
- **Pattern**: NORMAL/HIGH/LOW/ABNORMAL

### Alerts Screen
```
┌─────────────────────────────┐
│           Alerts            │
├─────────────────────────────┤
│ > Temp HIGH: 36.2°C         │
│ > Queen issue               │
│                             │
│                             │
└─────────────────────────────┘
```

### Power Status
```
┌─────────────────────────────┐
│        Power Status         │
├─────────────────────────────┤
│ Battery: 3.85V              │
│ Level: 75%                  │
│ Source: Battery             │
│ Uptime: 12h                 │
└─────────────────────────────┘
```

## Button Functions

### Single Press Actions
- **UP**: Previous screen/menu item
- **DOWN**: Next screen/menu item  
- **SELECT**: Enter menu/confirm selection
- **BACK**: Exit menu/return to dashboard

### Long Press Actions (Hold > 0.5 seconds)
- **UP/DOWN**: Fast scroll in menus
- **SELECT + BACK** (5 seconds): Factory reset
- **UP + DOWN + SELECT** (3 seconds): DFU mode (firmware update)

---

# Menu System & Settings

## Main Settings Menu

### Time & Date
- **Function**: Set system clock
- **Important**: Required for accurate data logging
- **Tip**: Use 24-hour format

### Sensor Calibration
- **Temperature Offset**: ±10°C adjustment
- **Humidity Offset**: ±20% adjustment
- **When to use**: If readings don't match known values

### Audio Settings
- **Sensitivity**: 0-10 (higher = more sensitive)
- **Queen Frequencies**: Typical range 200-350 Hz
- **Swarm Frequencies**: Typical range 400-600 Hz
- **Stress Threshold**: Sound level indicating stress (0-100%)

### Logging Settings
- **Log Interval**: 5, 10, 30, or 60 minutes
- **Log Enabled**: On/Off toggle
- **Recommendation**: 10 minutes for most applications

### Alert Thresholds
Configure when system generates alerts:

#### Temperature Alerts
- **Min**: Below this triggers "TEMP_LOW" alert
- **Max**: Above this triggers "TEMP_HIGH" alert
- **African Bee Ranges**: 18-35°C optimal

#### Humidity Alerts
- **Min**: Below this triggers "HUMIDITY_LOW" alert  
- **Max**: Above this triggers "HUMIDITY_HIGH" alert
- **African Bee Ranges**: 40-80% optimal

### System Settings
- **Display Brightness**: 1-10 (higher uses more battery)
- **Field Mode**: Enable battery optimization
- **Display Timeout**: 1-30 minutes (field mode only)
- **Factory Reset**: Restore all defaults

### Bluetooth Settings
- **Mode**: Off/Manual/Scheduled/Always On
- **Manual Timeout**: 15-120 minutes when button-activated
- **Schedule**: Start and end hours for scheduled mode
- **Device Info**: Set name, location, beekeeper ID

---

# Field Mode Operation

## What is Field Mode?

Field Mode is a special operating mode designed for extended battery life during unattended monitoring. It automatically manages power consumption while maintaining data collection.

## Enabling Field Mode

### Method 1: Menu Setting
```
Navigation: SELECT → Settings → System → Field Mode → ON
```

### Method 2: Automatic Activation
- System automatically enters Field Mode when battery < 50%
- Can be disabled in System Settings

## Field Mode Behavior

### Power Management
- **Display**: Turns off after 30 seconds of inactivity
- **Sensors**: Read every 15 minutes (configurable)
- **Audio**: Samples for 5 seconds every reading cycle
- **Sleep**: Deep sleep between readings (< 1mA consumption)

### Data Collection
- **Buffered Logging**: Stores up to 12 readings in memory
- **Batch Writing**: Writes to SD card hourly to reduce power
- **Emergency Storage**: Keeps data if SD card fails

### User Interaction
- **Wake Display**: Press any button
- **Full Functionality**: All menus available when display is on
- **Battery Indicator**: Shows remaining runtime estimate

## Field Mode Schedule

```
Example 15-minute cycle:
00:00 - Wake up, read sensors
00:05 - Audio analysis (5 seconds)
00:10 - Store data in buffer
15:00 - Repeat cycle

Every hour:
- Flush buffer to SD card
- Check alerts
- Update Bluetooth if scheduled
```

## Battery Life Estimates

### Testing Mode (Always On)
- **1200mAh Battery**: ~8-12 hours
- **Solar Panel**: Can sustain if > 4 hours daily sun

### Field Mode  
- **1200mAh Battery**: ~3-4 weeks
- **Solar Panel**: Can sustain indefinitely with minimal sun

---

# Bluetooth Data Retrieval

## Bluetooth Modes

### 1. Manual Mode (Recommended for field use)
- **Activation**: Press any button on device
- **Duration**: 30 minutes default (configurable 15-120 min)
- **Power Usage**: Minimal impact on battery
- **Use Case**: Periodic data collection visits

### 2. Scheduled Mode
- **Operation**: Automatically discoverable during set hours
- **Example**: 7:00 AM - 6:00 PM daily
- **Power Usage**: Moderate battery impact
- **Use Case**: Regular monitoring with predictable access times

### 3. Always On Mode
- **Operation**: Continuously discoverable
- **Power Usage**: High battery drain
- **Use Case**: Testing, permanent power available

## Device Discovery

### Device Naming Convention
```
Format: [HiveName]_[DeviceID]
Examples:
- TREE_A1_01 (Tree A1, Device 1)
- FIELD3_12 (Field 3, Device 12)
- VILLAGE_05 (Village hive, Device 5)
```

### Connection Process
1. **Enable Bluetooth** on smartphone/tablet
2. **Activate Device** (if in Manual mode)
3. **Scan for Devices** named "HIVE*" or your custom name
4. **Connect** (no pairing required)
5. **Transfer Data** using compatible app

## Data Transfer Commands

### Available Data Types
- **Current Readings**: Live sensor data
- **Daily Summaries**: Aggregated daily statistics  
- **Alert History**: Log of all triggered alerts
- **File List**: Available CSV files for download
- **Device Status**: Battery, uptime, system health

### Data Formats

#### Current Data (JSON)
```json
{
  "timestamp": 1679155200,
  "temperature": 28.5,
  "humidity": 65.2,
  "pressure": 1013.2,
  "frequency": 285,
  "soundLevel": 45,
  "beeState": "NORMAL",
  "battery": 3.85,
  "alerts": "0x00"
}
```

#### Daily Summary (JSON)
```json
{
  "date": "2024-03-15",
  "avgTemp": 27.3,
  "avgHumidity": 63.8,
  "alerts": 2,
  "beeActivity": "Normal",
  "queenDetected": true,
  "riskLevel": 15
}
```

## Smartphone App Development

### Basic Connection (JavaScript/React Native)
```javascript
// Connect to hive monitor
const device = await BluetoothSerial.connect(deviceId);

// Request current data
await device.write('GET_CURRENT_DATA');
const response = await device.read();
const data = JSON.parse(response);

// Display temperature
console.log(`Hive temperature: ${data.temperature}°C`);
```

---

# Alert System

## Alert Types & Meanings

### 🌡️ **Temperature Alerts**

#### TEMP_HIGH
- **Trigger**: Temperature above configured maximum
- **Typical**: > 35°C for African bees
- **Meaning**: Risk of overheating, brood damage
- **Action**: Provide shade, improve ventilation

#### TEMP_LOW  
- **Trigger**: Temperature below configured minimum
- **Typical**: < 18°C for African bees
- **Meaning**: Risk of chilled brood, reduced activity
- **Action**: Check insulation, windbreak, colony strength

### 💧 **Humidity Alerts**

#### HUMIDITY_HIGH
- **Trigger**: Humidity above configured maximum  
- **Typical**: > 80%
- **Meaning**: Risk of moisture buildup, mold, fermentation
- **Action**: Improve ventilation, check for leaks

#### HUMIDITY_LOW
- **Trigger**: Humidity below configured minimum
- **Typical**: < 40%  
- **Meaning**: Risk of dried nectar, stressed bees
- **Action**: Provide water source, check ventilation

### 👑 **Queen Alerts**

#### QUEEN_ISSUE
- **Trigger**: No queen frequencies detected for > 3 hours
- **Meaning**: Queen may be missing, dead, or distressed
- **Action**: URGENT - Inspect hive immediately, look for queen

#### QUEEN_PRESENT
- **Trigger**: Strong queen frequencies detected
- **Meaning**: Queen actively laying, colony healthy
- **Action**: Normal monitoring

### 🐝 **Behavioral Alerts**

#### SWARM_RISK
- **Trigger**: High frequency activity (400-600 Hz) + high volume
- **Meaning**: Colony preparing to swarm
- **Action**: URGENT - Check for queen cells, add space, split hive

#### DEFENSIVE
- **Trigger**: Very high frequencies (> 600 Hz) + high volume
- **Meaning**: Bees are agitated or defensive
- **Action**: Approach with caution, check for disturbances

#### STRESSED  
- **Trigger**: Irregular patterns + high activity
- **Meaning**: Colony under stress (disease, pests, hunger)
- **Action**: Inspect for mites, disease signs, food stores

### ⚡ **System Alerts**

#### LOW_BATTERY
- **Trigger**: Battery voltage < 3.5V
- **Meaning**: System will shut down soon
- **Action**: Charge battery or check solar panel

#### SD_ERROR
- **Trigger**: SD card write failure
- **Meaning**: Data logging stopped
- **Action**: Check SD card connection, replace if needed

## Alert Responses

### Immediate Actions (Critical Alerts)
1. **SWARM_RISK**: Check within 2 hours
2. **QUEEN_ISSUE**: Check within 4 hours  
3. **TEMP_HIGH**: Provide cooling immediately

### Daily Monitoring (Warning Alerts)
1. **Temperature/Humidity**: Adjust on next visit
2. **STRESSED**: Inspect thoroughly on next visit
3. **DEFENSIVE**: Plan careful approach

### System Maintenance (Info Alerts)
1. **LOW_BATTERY**: Service on next visit
2. **SD_ERROR**: Replace/check SD card

## Alert History

### Viewing Alert Log
```
Navigation: Main Screen → DOWN → Alerts Screen
```

### Alert Log Format (SD Card)
```
DateTime,AlertType,Value
2024-03-15 14:30:00,TEMP_HIGH,36.2
2024-03-15 15:45:00,SWARM_RISK,N/A
2024-03-15 16:20:00,QUEEN_ISSUE,N/A
```

---

# Firmware Updates

## When to Update Firmware

### Mandatory Updates
- **Security fixes** for Bluetooth vulnerabilities
- **Critical bug fixes** affecting data accuracy
- **Hardware compatibility** updates

### Recommended Updates  
- **New features** for bee monitoring
- **Performance improvements** for battery life
- **Enhanced analysis** algorithms

### Optional Updates
- **User interface** improvements
- **Additional data export** formats
- **New alert types**

## Update Methods

### Method 1: DFU Mode (Device Firmware Update)

#### Entering DFU Mode
1. **Button Combination**: Hold UP + DOWN + SELECT for 3 seconds
2. **Display Shows**: "FIRMWARE UPDATE - Connect via Bluetooth"
3. **LED Behavior**: Rapid blue flashing (nRF52840 built-in LED)

#### Using nRF Connect App (Recommended)
1. **Download**: "nRF Connect for Mobile" (Nordic Semiconductor)
2. **Enable DFU Mode** on device (above steps)
3. **Open nRF Connect** and scan for devices
4. **Look for**: "DfuTarg" or "Hive_DFU"
5. **Connect** and select "DFU"
6. **Choose Firmware File**: Select .zip file from download
7. **Upload**: Wait for completion (2-3 minutes)
8. **Automatic Restart**: Device will reboot with new firmware

#### Using Computer (Advanced)
```bash
# Install nrfutil
pip install nrfutil

# Flash firmware
nrfutil dfu usb-serial -pkg firmware.zip -p COM3
```

### Method 2: Bluetooth OTA (Over-The-Air)

#### Requirements
- Custom smartphone app with OTA capability
- Device in normal operation (not DFU mode)
- Firmware file in .bin format

#### Process
1. **Enable Bluetooth** on device (Manual/Scheduled/Always On)
2. **Connect** with OTA-capable app
3. **Select Firmware** file from phone storage
4. **Initiate Transfer** (may take 10-15 minutes)
5. **Auto-restart** when complete

### Method 3: Programming Header (Development)

#### Hardware Requirements
- **J-Link Programmer** or compatible
- **SWD Connection**: SWCLK, SWDIO, VCC, GND pins
- **PlatformIO** or **Arduino IDE** with nRF52 support

#### Development Upload
```bash
# Using PlatformIO
pio run -t upload

# Using nrfjprog
nrfjprog --program firmware.hex --chiperase --verify --reset
```

## Firmware Files

### File Types
- **.zip**: DFU package (recommended for nRF Connect)
- **.hex**: Intel HEX format (for programmers)
- **.bin**: Binary format (for OTA updates)

### Version Information
```
Current Version: 2.1.0
Release Date: March 2024
Git Hash: abc123def456
Build: Release
Target: nRF52840
```

### Checking Current Version
```
Navigation: SELECT → Settings → System → About
or
Boot Screen: Shows version for 3 seconds
```

## Backup Before Update

### Settings Backup
1. **Bluetooth Export**: Connect and download current settings
2. **SD Card**: Copy settings_export.txt file
3. **Photo**: Take pictures of current settings screens

### Data Backup
1. **Copy All CSV Files** from SD card
2. **Export Bluetooth Data** if needed
3. **Note Custom Configurations** (thresholds, names, etc.)

## Recovery Procedures

### Soft Recovery (Settings Lost)
1. **Check Version**: Verify update completed
2. **Reconfigure**: Re-enter settings from backup
3. **Test Functions**: Verify all sensors working
4. **Restore Data**: Copy CSV files back if needed

### Hard Recovery (Boot Failure)
1. **Force DFU Mode**: Hold all buttons while powering on
2. **Re-flash Firmware**: Use nRF Connect with known good firmware
3. **Factory Reset**: Use SELECT + BACK combination
4. **Complete Reconfiguration**: Start from initial setup

### Emergency Recovery (Brick Recovery)
1. **Hardware Programmer Required**: J-Link or compatible
2. **SWD Connection**: Direct programming pins
3. **Erase and Reflash**: Complete firmware restoration
4. **Contact Support**: If still not working

---

# Troubleshooting

## Common Issues

### Display Problems

#### Blank Screen
**Symptoms**: No display output, backlight may be on
**Causes**: 
- I2C connection failure
- Power supply issue
- Display module failure

**Solutions**:
1. Check I2C wiring (SDA, SCL, VCC, GND)
2. Verify 3.3V power supply
3. Try different I2C address (0x3C or 0x3D)
4. Replace display module

#### Garbled Display
**Symptoms**: Corrupted text, missing pixels, artifacts
**Causes**:
- Loose I2C connections
- Electromagnetic interference
- Power supply noise

**Solutions**:
1. Secure all connections
2. Add I2C pull-up resistors (4.7kΩ)
3. Check power supply filtering
4. Move away from interference sources

### Sensor Issues

#### Temperature Reading Errors
**Symptoms**: Readings of -999°C, NaN, or clearly wrong values
**Causes**:
- BME280 connection failure
- Sensor damage
- I2C address conflict

**Solutions**:
1. Check BME280 wiring and connections
2. Verify I2C address (0x77 or 0x76)
3. Test with known good sensor
4. Check for I2C bus conflicts

#### Audio Not Working
**Symptoms**: Sound level always 0%, no frequency detection
**Causes**:
- MAX9814 connection issue
- Analog input configuration
- Microphone placement

**Solutions**:
1. Verify MAX9814 wiring (VCC, GND, OUT to A4)
2. Check analog input configuration
3. Test with known sound source
4. Adjust sensitivity settings

### SD Card Problems

#### Card Not Detected
**Symptoms**: "SD: NOT FOUND" message at startup
**Causes**:
- SPI wiring incorrect
- Card not inserted properly
- Incompatible card format
- Power supply insufficient

**Solutions**:
1. Check SPI connections (MOSI, MISO, SCK, CS)
2. Reseat SD card firmly
3. Format card as FAT32
4. Try different SD card (≤32GB recommended)
5. Check 3.3V power supply capacity

#### Data Logging Stopped
**Symptoms**: No new data in CSV files, "SD_ERROR" alert
**Causes**:
- Card full
- Card corrupted
- File system errors
- Card ejected while writing

**Solutions**:
1. Check available space on card
2. Run disk check/repair on computer
3. Backup data and reformat card
4. Replace with new SD card

### Bluetooth Issues

#### Device Not Discoverable
**Symptoms**: Cannot find device when scanning
**Causes**:
- Bluetooth disabled or in wrong mode
- Device in sleep mode
- Bluetooth stack failure
- Range/interference issues

**Solutions**:
1. Check Bluetooth mode in settings
2. Activate manual mode with button press
3. Move closer to device (< 10 meters)
4. Restart device (power cycle)
5. Clear Bluetooth cache on phone

#### Connection Drops
**Symptoms**: Connects then immediately disconnects
**Causes**:
- Low battery
- Bluetooth interference
- App compatibility issues
- Firmware bugs

**Solutions**:
1. Charge battery (>3.7V recommended)
2. Move away from WiFi/interference sources
3. Try different connection app
4. Update device firmware

### Power Issues

#### Short Battery Life
**Symptoms**: Battery drains faster than expected
**Causes**:
- Display always on
- Not in field mode
- Bluetooth always active
- Hardware fault

**Solutions**:
1. Enable field mode for extended operation
2. Set Bluetooth to manual mode
3. Reduce display brightness
4. Check for short circuits
5. Replace old/damaged battery

#### Won't Charge
**Symptoms**: Battery percentage not increasing
**Causes**:
- Charging circuit failure
- Bad USB cable
- Battery at end of life
- Temperature too extreme

**Solutions**:
1. Try different USB cable
2. Check charging LED indicator
3. Allow battery to warm/cool to room temperature
4. Replace battery if >2 years old

### Real-Time Clock Issues

#### Wrong Time After Power Loss
**Symptoms**: Time resets when battery removed
**Causes**:
- RTC backup battery dead
- RTC module failure
- Power supply issue

**Solutions**:
1. Replace CR2032 battery in RTC module
2. Check RTC module connections
3. Set time after every power cycle
4. Replace RTC module

## Diagnostic Procedures

### System Health Check
```
Navigation: Power on → Observe startup diagnostics
Expected output:
- RTC: OK
- BME280: OK  
- Audio: OK
- Buttons: OK
- SD Card: OK or NOT FOUND
- System: READY
```

### Sensor Test
```
Navigation: SELECT → Settings → Run Diagnostics (if available)
or
Observe Dashboard for reasonable values:
- Temperature: Environmental range (0-50°C)
- Humidity: 20-90%
- Pressure: 950-1050 hPa
- Battery: >3.0V
```

### Audio Test
```
Method 1: Speak near microphone, observe sound level changes
Method 2: Tap hive gently, watch for activity spikes
Method 3: Navigation → Sound Monitor screen
Expected: Sound level responds to noise
```

### Bluetooth Test
```
Method 1: Enable manual mode, scan with phone
Method 2: Check device name in settings matches scan results
Method 3: Attempt connection with nRF Connect app
```

## Factory Reset

### When to Factory Reset
- Settings corrupted
- Unknown configuration problems
- Preparing device for new installation
- Troubleshooting persistent issues

### Reset Procedure
1. **Button Combination**: Hold SELECT + BACK for 5 seconds
2. **Confirmation Screen**: Use UP to cancel, DOWN to confirm
3. **Reset Progress**: Wait for completion (30 seconds)
4. **Automatic Restart**: Device reboots with defaults
5. **Reconfiguration**: Go through initial setup again

### What Gets Reset
- All user settings to factory defaults
- Bluetooth device name and configuration
- Alert thresholds
- Display preferences
- Calibration offsets

### What Remains
- Stored data files on SD card
- Hardware configuration
- Firmware version
- Serial number/hardware ID

---

# Maintenance

## Regular Maintenance Schedule

### Weekly (Active Season)
- **Visual Inspection**: Check for physical damage
- **Battery Check**: Verify charge level >50%
- **Data Review**: Check for unusual patterns or alerts
- **Solar Panel**: Clean surface if dusty/dirty

### Monthly
- **Deep Clean**: Remove housing, clean all surfaces
- **Connection Check**: Verify all connectors secure
- **Sensor Calibration**: Compare readings to known values
- **SD Card**: Check available space, backup data

### Seasonal
- **Battery Replacement**: Replace Li-Po if >2 years old
- **Firmware Update**: Check for and install updates
- **Full Calibration**: Complete sensor recalibration
- **Documentation**: Update hive records and configurations

### Annual
- **Component Replacement**: Replace wear items (SD card, RTC battery)
- **Enclosure Service**: Check seals, replace if needed
- **System Upgrade**: Evaluate new features/hardware
- **Data Archive**: Long-term storage of historical data

## Cleaning Procedures

### External Cleaning
1. **Power Off**: Remove battery before cleaning
2. **Dry Brush**: Remove debris with soft brush
3. **Damp Cloth**: Clean housing with slightly damp cloth
4. **Dry Completely**: Ensure no moisture before reassembly
5. **Solar Panel**: Use glass cleaner for optimal efficiency

### Internal Cleaning
1. **Compressed Air**: Blow out dust from PCB and connectors
2. **Alcohol Wipes**: Clean circuit board with isopropyl alcohol
3. **Contact Cleaner**: Spray connectors if oxidation present
4. **Dry Time**: Allow 30 minutes drying before reassembly

### Sensor Maintenance
- **BME280**: No user maintenance required
- **Microphone**: Keep mesh clean, replace if damaged
- **Display**: Clean with microfiber cloth only
- **Buttons**: Clean contacts if response poor

## Replacement Parts

### Consumable Items
| Part | Typical Life | Replacement Interval |
|------|-------------|---------------------|
| Li-Po Battery | 2-3 years | When capacity <70% |
| SD Card | 3-5 years | When errors occur |
| RTC Battery | 5-10 years | When time resets |
| Enclosure Seals | 2-3 years | When cracked/hard |

### Electronic Components
| Part | Failure Rate | Replacement Cost |
|------|-------------|------------------|
| BME280 Sensor | Low | $10-15 |
| MAX9814 Mic | Low | $5-10 |
| OLED Display | Medium | $15-25 |
| nRF52840 Board | Very Low | $25-35 |

### Upgrade Opportunities
- **Larger Battery**: 2500mAh for longer field operation
- **Bigger Solar Panel**: 2W for faster charging
- **Weather Station**: Add wind/rain sensors
- **Cellular Module**: Remote data transmission
- **GPS Module**: Location tracking for mobile hives

## Data Management

### File Organization
```
SD Card Structure:
/HIVE_DATA/
  /2024/
    2024-01.CSV    (January data)
    2024-02.CSV    (February data)
    ...
/alerts.log        (Alert history)
/settings_export.txt (Settings backup)
/field_events.csv   (Manual event log)
```

### Backup Strategy
1. **Monthly Download**: Retrieve all new data files
2. **Cloud Storage**: Upload to Google Drive/Dropbox
3. **Local Backup**: Keep copies on computer
4. **Version Control**: Track firmware and setting changes

### Data Retention
- **Device Storage**: 1-2 years typical SD card capacity
- **Cloud Backup**: Unlimited (recommend permanent)
- **Local Analysis**: Keep 5+ years for trend analysis
- **Research Data**: Archive indefinitely for scientific value

---

# Data Analysis

## Understanding Your Data

### CSV File Format
```csv
DateTime,UnixTime,Temp_C,Humidity_%,Pressure_hPa,Sound_Hz,Sound_Level,Bee_State,Battery_V,Alerts
2024-03-15 10:00:00,1710504000,28.5,65.2,1013.2,285,45,NORMAL,3.85,0x00
2024-03-15 10:10:00,1710504600,28.7,64.8,1013.1,292,48,NORMAL,3.85,0x00
```

### Key Metrics

#### Temperature Analysis
- **Daily Range**: Should be 5-10°C difference day/night
- **Seasonal Trends**: Track adaptation to weather changes
- **Brood Area**: Stable 32-35°C indicates healthy brood rearing
- **Alerts**: Frequent temperature alerts suggest ventilation issues

#### Humidity Patterns
- **Morning Peak**: Often highest just after dawn
- **Afternoon Low**: Typically lowest mid-afternoon
- **Seasonal Variation**: Higher during rainy season
- **Colony Strength**: Strong colonies maintain stable humidity

#### Sound Analysis
- **Baseline Activity**: Establish normal patterns for your hive
- **Daily Cycle**: Typically highest 10AM-4PM
- **Frequency Trends**: Queen frequencies (200-350Hz) indicate health
- **Volume Changes**: Sudden increases may indicate disturbance

### Analysis Tools

#### Spreadsheet Analysis (Excel/Google Sheets)
```excel
=AVERAGE(C2:C100)          // Average temperature
=MAX(C2:C100)-MIN(C2:C100) // Daily temperature range
=COUNTIF(I2:I100,"!=0x00") // Count of alert events
```

#### Python Analysis
```python
import pandas as pd
import matplotlib.pyplot as plt

# Load data
df = pd.read_csv('2024-03.CSV')
df['DateTime'] = pd.to_datetime(df['DateTime'])

# Plot temperature trends
plt.figure(figsize=(12,6))
plt.plot(df['DateTime'], df['Temp_C'])
plt.title('Hive Temperature Over Time')
plt.ylabel('Temperature (°C)')
plt.xlabel('Date')
plt.show()

# Calculate daily statistics
daily_stats = df.groupby(df['DateTime'].dt.date).agg({
    'Temp_C': ['mean', 'min', 'max'],
    'Humidity_%': ['mean', 'min', 'max'],
    'Sound_Level': 'mean'
})
```

#### R Analysis
```r
library(ggplot2)
library(dplyr)

# Load and prepare data
hive_data <- read.csv("2024-03.CSV")
hive_data$DateTime <- as.POSIXct(hive_data$DateTime)

# Create activity heatmap
ggplot(hive_data, aes(x=hour(DateTime), y=day(DateTime), fill=Sound_Level)) +
  geom_tile() +
  scale_fill_gradient(low="blue", high="red") +
  labs(title="Hive Activity Heatmap", x="Hour of Day", y="Day of Month")
```

## Pattern Recognition

### Normal Patterns
- **Daily Temperature**: Gradual rise/fall following ambient
- **Activity Cycle**: Peak activity 10AM-4PM, quiet at night
- **Weather Response**: Activity drops before storms
- **Seasonal Changes**: Gradual adaptation to climate shifts

### Warning Patterns
- **Erratic Temperature**: Wild swings indicate ventilation issues
- **Night Activity**: Unusual activity at night suggests problems
- **Frequency Shifts**: Changes in dominant frequency patterns
- **Silent Periods**: Extended periods of low activity

### Critical Patterns
- **Temperature Spikes**: Sudden increases >40°C
- **Activity Explosion**: Sudden high activity + high frequency
- **Complete Silence**: No activity during normal hours
- **Frequency Absence**: Missing queen frequencies >6 hours

## Actionable Insights

### Immediate Actions (Same Day)
- **Swarm Indicators**: High frequency + high volume + temperature spike
- **Queen Loss**: Absence of 200-350Hz frequencies for >3 hours
- **Overheating**: Temperature >38°C with high humidity
- **Predator Attack**: Sudden activity spike with defensive frequencies

### Short-term Actions (Within Week)
- **Gradual Temperature Rise**: Trending toward dangerous levels
- **Decreasing Activity**: Steady decline over several days
- **Humidity Issues**: Persistent high/low humidity readings
- **Power Problems**: Declining battery levels

### Long-term Actions (Seasonal)
- **Climate Adaptation**: Preparing for seasonal changes
- **Equipment Upgrade**: Based on performance patterns
- **Hive Management**: Optimizing location/configuration
- **Research Insights**: Contributing to bee behavior studies

---

# Advanced Features

## Custom Threshold Configuration

### Adaptive Thresholds
Instead of fixed thresholds, configure adaptive ranges based on:
- **Seasonal Variations**: Different limits for wet/dry seasons
- **Colony Strength**: Adjust based on hive population
- **Local Climate**: Customize for your specific microclimate
- **Bee Race**: Optimize for African vs European bee characteristics

### Time-Based Thresholds
```
Example Configuration:
Night (22:00-06:00): Temperature 15-30°C, Low activity expected
Day (06:00-18:00): Temperature 18-35°C, Normal activity
Evening (18:00-22:00): Temperature 16-33°C, Reduced activity
```

## Multi-Hive Management

### Device Naming Strategy
```
Naming Convention: [Location]_[HiveType]_[Number]
Examples:
- NORTH_FIELD_01, NORTH_FIELD_02 (Multiple hives in north field)
- TREE_A_01 (Single hive at Tree A)
- VILLAGE_COOP_01 (Cooperative hive in village)
```

### Data Correlation
- **Comparative Analysis**: Compare performance across hives
- **Environmental Impact**: How location affects hive health
- **Management Effectiveness**: Which interventions work best
- **Early Warning**: Use healthy hives to predict problems

### Centralized Monitoring
- **Bluetooth Scanning**: Walk route to collect from multiple devices
- **Data Aggregation**: Combine data from all hives
- **Alert Prioritization**: Which hives need attention first
- **Resource Planning**: Optimize intervention timing

## Research Applications

### Citizen Science
Your data can contribute to:
- **Climate Change Studies**: How warming affects bee behavior
- **Colony Collapse Research**: Early warning indicators
- **Pollination Studies**: Activity patterns vs crop flowering
- **Conservation Efforts**: Wild vs managed hive comparisons

### Data Sharing Protocols
- **Anonymization**: Remove location data if needed
- **Standard Formats**: Export in research-compatible formats
- **Metadata**: Include hive management practices
- **Consent**: Understand data usage before sharing

## Integration Possibilities

### Weather Station Integration
Add external sensors for:
- **Wind Speed/Direction**: Affects foraging patterns
- **Rainfall Measurement**: Correlates with hive activity
- **Light Levels**: Track day length effects
- **Barometric Pressure**: Predict weather changes

### Smart Hive Features
- **Automated Ventilation**: Servo-controlled vents based on temperature
- **Feeding Alerts**: Weight sensors to detect food shortage
- **Security Monitoring**: Motion sensors for theft prevention
- **Remote Cameras**: Visual monitoring capability

### Agricultural Integration
- **Crop Monitoring**: Coordinate with farming activities
- **Pesticide Alerts**: Warn before harmful applications
- **Pollination Optimization**: Track hive service efficiency
- **Yield Correlation**: Link bee activity to crop success

## Customization Options

### Hardware Modifications
- **Extended Battery**: 5000mAh for month-long operation
- **Solar Upgrade**: 5W panel for faster charging
- **Weatherproofing**: IP67 rating for extreme conditions
- **Remote Antenna**: Extend Bluetooth range

### Software Customization
- **Alert Thresholds**: Fine-tune for your specific needs
- **Data Logging**: Adjust intervals and parameters
- **Display Themes**: Customize information display
- **Language Support**: Add local language translations

### Advanced Analytics
- **Machine Learning**: Pattern recognition for anomaly detection
- **Predictive Modeling**: Forecast problems before they occur
- **Behavioral Analysis**: Deep dive into bee behavior patterns
- **Performance Optimization**: Tune system for your environment

---

# Appendices

## Appendix A: Specifications

### Electronic Specifications
- **Microcontroller**: nRF52840 (64MHz ARM Cortex M4)
- **RAM**: 256KB
- **Flash**: 1MB
- **Bluetooth**: 5.0 with long range support
- **Operating Temperature**: -20°C to +60°C
- **Storage Temperature**: -40°C to +85°C

### Sensor Specifications
- **Temperature**: ±1°C accuracy, -40°C to +85°C range
- **Humidity**: ±3% accuracy, 0-100% range
- **Pressure**: ±1hPa accuracy, 300-1100hPa range
- **Audio**: 20Hz-20kHz frequency response, AGC enabled

### Power Specifications
- **Battery**: 3.7V Li-Po, 1200-5000mAh capacity
- **Solar Panel**: 5V, 1-5W capacity
- **Consumption (Testing)**: ~15mA average
- **Consumption (Field)**: ~0.5mA average
- **Charging Current**: 500mA maximum

## Appendix B: Pin Configuration

### nRF52840 Pin Assignments
```
Digital Pins:
- Pin 10: SD Card CS (Chip Select)
- Pin A0: Button UP
- Pin A1: Button DOWN  
- Pin A2: Button SELECT
- Pin A3: Button BACK
- Pin A4: Audio Input (MAX9814)
- Pin A6: Battery Voltage Monitor

I2C Bus:
- SDA: BME280, OLED, RTC
- SCL: BME280, OLED, RTC

SPI Bus:
- MOSI: SD Card Data Out
- MISO: SD Card Data In
- SCK: SD Card Clock
```

## Appendix C: Troubleshooting Codes

### Error Codes
```
E001: RTC initialization failed
E002: BME280 sensor not found
E003: SD card initialization failed
E004: Display initialization failed
E005: Audio initialization failed
E006: Settings corruption detected
E007: Bluetooth initialization failed
E008: Battery critically low
E009: Temperature sensor timeout
E010: SD card write error
```

### Status Codes
```
S001: System ready, all sensors operational
S002: Field mode active, display timeout enabled
S003: Bluetooth advertising active
S004: Data logging active
S005: Alert conditions present
S006: Battery charging detected
S007: Emergency buffer mode (SD unavailable)
S008: Low power mode active
S009: Diagnostic mode active
S010: Firmware update mode
```

## Appendix D: Default Settings

### Factory Default Values
```
Temperature Offset: 0.0°C
Humidity Offset: 0.0%
Audio Sensitivity: 5
Queen Frequency Min: 200 Hz
Queen Frequency Max: 350 Hz
Swarm Frequency Min: 400 Hz
Swarm Frequency Max: 600 Hz
Stress Threshold: 70%
Log Interval: 10 minutes
Log Enabled: true
Temp Min Alert: 18.0°C
Temp Max Alert: 35.0°C
Humidity Min Alert: 40.0%
Humidity Max Alert: 80.0%
Display Brightness: 7
Field Mode: false
Display Timeout: 5 minutes
Bluetooth Mode: Manual
Manual Timeout: 30 minutes
Schedule Start: 7 (7 AM)
Schedule End: 18 (6 PM)
Device ID: 1
Hive Name: "Hive_01"
Location: "Unknown"
Beekeeper: "User"
```

## Appendix E: File Formats

### CSV Data Format
```
Field Name,Data Type,Description,Example
DateTime,String,ISO format timestamp,"2024-03-15 10:00:00"
UnixTime,Integer,Seconds since epoch,1710504000
Temp_C,Float,Temperature in Celsius,28.5
Humidity_%,Float,Relative humidity percent,65.2
Pressure_hPa,Float,Atmospheric pressure,1013.2
Sound_Hz,Integer,Dominant frequency,285
Sound_Level,Integer,Sound level 0-100%,45
Bee_State,String,Classified bee state,"NORMAL"
Battery_V,Float,Battery voltage,3.85
Alerts,Hex,Alert bitmask,"0x00"
```

### Settings Export Format
```ini
[Sensor_Calibration]
TempOffset=0.0
HumidityOffset=0.0

[Audio_Settings]
AudioSensitivity=5
QueenFreqMin=200
QueenFreqMax=350
SwarmFreqMin=400
SwarmFreqMax=600
StressThreshold=70

[Logging]
LogInterval=10
LogEnabled=true

[Alert_Thresholds]
TempMin=18.0
TempMax=35.0
HumidityMin=40.0
HumidityMax=80.0

[System]
DisplayBrightness=7
FieldModeEnabled=false
DisplayTimeoutMin=5

[Bluetooth]
Mode=1
ManualTimeoutMin=30
ScheduleStartHour=7
ScheduleEndHour=18
DeviceId=1
HiveName=Hive_01
Location=Unknown
Beekeeper=User
```

---

**Document Version**: 2.1.0  
**Last Updated**: July 2025 
**Compatible Firmware**: 2.1.x  
**Author**: Tanzania Hive Monitor Development Team  
**License**: Creative Commons Attribution-ShareAlike 4.0 International

For technical support, latest firmware, and community discussions, visit:
- **GitHub Repository**: [Link to repository]
- **User Forum**: [Link to forum]
- **Email Support**: [support email]

---

*This manual is a living document. Please check for updates regularly and contribute improvements based on your field experience.*
