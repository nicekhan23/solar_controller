# Solar Battery Controller

![ESP32](https://img.shields.io/badge/ESP32-Supported-green)
![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.x-blue)
![License](https://img.shields.io/badge/License-MIT-yellow)

A professional-grade solar battery management system for ESP32, featuring dual-channel control, temperature compensation, motion detection, and real-time monitoring via serial CLI.

## 📋 Table of Contents

- [Features](#features)
- [System Architecture](#system-architecture)
- [Hardware Requirements](#hardware-requirements)
- [Wiring Diagram](#wiring-diagram)
- [Software Installation](#software-installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [CLI Commands](#cli-commands)
- [Technical Specifications](#technical-specifications)
- [Troubleshooting](#troubleshooting)
- [Development](#development)
- [License](#license)

## ✨ Features

### Core Functionality
- **Dual-Channel Control**: Independent battery voltage monitoring and load control for two channels
- **Moving Average Filter**: 16-sample moving average for noise reduction (1.6s window)
- **Hysteresis Control**: Prevents rapid on/off cycling with separate ON/OFF thresholds
- **Temperature Compensation**: Automatic voltage threshold adjustment based on ambient temperature
- **Battery Protection**: Multi-level protection with automatic dimming and shutdown

### Advanced Features
- **Motion Detection**: PIR sensor integration with configurable timeout
- **PWM Dimming**: 13-bit resolution (8192 levels) at 5kHz for flicker-free operation
- **Non-Volatile Storage**: Configuration and statistics persistence using NVS
- **Serial CLI**: Full-featured command-line interface for monitoring and configuration
- **Real-Time Monitoring**: Live voltage, temperature, and system status updates
- **Watchdog System**: Automatic health monitoring with low-battery warnings

### Safety Features
- **Over-discharge Protection**: Automatic load disconnection below critical voltage
- **Debounce Logic**: 5-second minimum between state changes
- **Mutex Protection**: Thread-safe hardware access
- **Emergency Shutdown**: Manual override capability

## 🏗️ System Architecture

```
┌────────────────────────────────────────────────────────────┐
│                        ESP32 Main                          │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  ┌──────────┐     ┌────────────┐    ┌──────────┐           │
│  │   ADC    │───▶│  Channel   │──▶│ Control  │           │
│  │   Task   │     │ Processor  │    │   Task   │           │
│  │          │     │   (CH0)    │    │          │───▶ PWM  │
│  │  100ms   │     │            │    │          │           │
│  │ Sampling │     │  Moving    │    │  Mutex   │           │
│  │          │     │  Average   │    │ Protected│           │
│  │          │     │ Hysteresis │    │          │───▶ PWM  │
│  │          │     │   Temp     │    │          │           │
│  │          │     │   Comp     │    │          │           │
│  │          │     └────────────┘    └──────────┘           │
│  │          │     ┌────────────┐         ▲                 │
│  │          │───▶│  Channel   │─────────┘                 │
│  │          │     │ Processor  │                           │
│  │          │     │   (CH1)    │                           │
│  └──────────┘     └────────────┘                           │
│       │                                                    │
│       │           ┌────────────┐                           │
│       └─────────▶│    CLI     │◀────── UART              │
│                   │   Task     │                           │
│                   └────────────┘                           │
│                                                            │
│  ┌──────────┐     ┌────────────┐                           │
│  │  Uptime  │     │  Watchdog  │                           │
│  │   Task   │     │    Task    │                           │
│  └──────────┘     └────────────┘                           │
│                                                            │
│                   ┌────────────┐                           │
│                   │    NVS     │                           │
│                   │  Storage   │                           │
│                   └────────────┘                           │
└────────────────────────────────────────────────────────────┘
```

### Task Overview

| Task | Priority | Stack | Function |
|------|----------|-------|----------|
| ADC Task | 5 | 2048 | Battery voltage & temperature sampling |
| Channel Processors | 4 | 3072 | Signal processing & state logic |
| Control Task | 5 | 2048 | Hardware output management |
| CLI Task | 3 | 4096 | User interface |
| Uptime Task | 2 | 2048 | Statistics tracking |
| Watchdog Task | 2 | 2048 | System health monitoring |

## 🔧 Hardware Requirements

### Essential Components

| Component | Specification | Quantity | Notes |
|-----------|---------------|----------|-------|
| ESP32 DevKit | ESP32-WROOM-32 | 1 | Any ESP32 variant |
| Voltage Divider Resistors | 47kΩ, 10kΩ | 1 set | 1% tolerance recommended |
| MOSFETs | Logic-level N-channel | 2 | IRLZ44N or similar |
| Gate Resistors | 100Ω | 2 | Current limiting |
| Smoothing Capacitor | 100nF ceramic | 1 | ADC noise filtering |
| PIR Motion Sensor | HC-SR501 or similar | 1 | 3.3V compatible |
| Temperature Sensor | TMP36 (analog) | 1 | Or DS18B20 (1-Wire) |

### Optional Components

| Component | Purpose |
|-----------|---------|
| Solar Charge Controller | BQ24074 or equivalent |
| Optocouplers | Galvanic isolation |
| Level Shifters | 5V device compatibility |
| Fuses | Overcurrent protection |
| TVS Diodes | Surge protection |

### Power Supply

- **Operating Voltage**: 3.3V (ESP32)
- **Battery Voltage Range**: 10V - 20V (configurable)
- **Current Consumption**: ~150mA @ 3.3V (typical)
- **Load Current**: Depends on MOSFET rating

## 📐 Wiring Diagram

### GPIO Pin Assignment

```
ESP32 DevKit
┌─────────────────────┐
│                     │
│  GPIO34 (ADC1_CH6) ◄├─── Battery Voltage (via divider)
│  GPIO35 (ADC1_CH7) ◄├─── Temperature Sensor
│                     │
│  GPIO25 (PWM)      ─┤───▶ MOSFET Gate CH0
│  GPIO26 (PWM)      ─┤───▶ MOSFET Gate CH1
│                     │
│  GPIO4  (Input)    ◄├─── PIR Motion Sensor
│  GPIO27 (Input)    ◄├─── Charger Status (optional)
│                     │
│  TX (GPIO1)        ─┤───▶ Serial Console
│  RX (GPIO3)        ◄├─── Serial Console
│                     │
│  GND               ─┤───▶ Common Ground
│  3.3V              ─┤───▶ Sensor Power
│                     │
└─────────────────────┘
```

### Voltage Divider Circuit

```
Battery+ ─┬─── 47kΩ ───┬─── 10kΩ ───┬─── GND
          │            │            │
          │            └─── 100nF ──┘
          │                 │
          │                 └────────▶ GPIO34 (ADC)
          │
          └─ Protected by fuse
```

**Calculation:**
- Divider Ratio: (47k + 10k) / 10k = 5.7
- Max Battery Voltage: 20V
- ADC Input: 20V / 5.7 ≈ 3.5V (safe for 3.6V max)

### MOSFET Driver Circuit

```
GPIO25 ──── 100Ω ──── Gate
                      │
                    Source ──── GND
                      │
                    Drain ──── Load- (LED strip, etc.)
                      
Load+ ──────────────────────── Battery+

Flyback Diode (for inductive loads):
         Drain
           │
          ─┴─  
          ╱ ╲  Schottky diode
         ╱   ╲
        └─────┘
           │
        Battery+
```

### Motion Sensor Connection

```
HC-SR501 PIR
┌─────────────┐
│             │
│  VCC       ─┼─── 3.3V or 5V
│  OUT       ─┼───▶ GPIO4 (with pull-down)
│  GND       ─┼─── GND
│             │
└─────────────┘
```

## 💾 Software Installation

### Prerequisites

```bash
# Install ESP-IDF (v5.0 or later)
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32

# Set up environment (add to ~/.bashrc)
alias get_idf='. $HOME/esp/esp-idf/export.sh'
```

### Building the Project

```bash
# Clone or download the project
cd ~/workspace
# Copy all project files to solar_controller/

# Set up ESP-IDF environment
get_idf

# Navigate to project
cd solar_controller

# Configure (optional - adjust settings)
idf.py menuconfig

# Build
idf.py build

# Flash to ESP32
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output
idf.py -p /dev/ttyUSB0 monitor

# Or flash and monitor in one command
idf.py -p /dev/ttyUSB0 flash monitor
```

### Finding Serial Port

```bash
# Linux
ls /dev/ttyUSB* /dev/ttyACM*

# macOS
ls /dev/cu.*

# Windows
# Check Device Manager -> Ports (COM & LPT)
```

## ⚙️ Configuration

### Default Configuration

| Parameter | Default Value | Range | Unit |
|-----------|---------------|-------|------|
| CH0 ON Threshold | 12500 | 10000-20000 | mV |
| CH0 OFF Threshold | 11800 | 10000-20000 | mV |
| CH1 ON Threshold | 12500 | 10000-20000 | mV |
| CH1 OFF Threshold | 11800 | 10000-20000 | mV |
| Temperature Coefficient | -0.02 | -0.1 to 0.1 | V/°C |
| PWM Half Duty | 50 | 0-100 | % |
| PWM Full Duty | 100 | 0-100 | % |
| Motion Timeout | 30000 | 1000-300000 | ms |

### Battery Voltage Thresholds

For typical 12V lead-acid batteries:

| State | Voltage | Action |
|-------|---------|--------|
| Full Charge | ≥13.5V | 100% brightness |
| Normal | ≥12.0V | 50% brightness |
| Low | ≥11.0V | 25% brightness |
| Critical | <11.0V | Load disconnect |

### Temperature Compensation

Lead-acid batteries require voltage adjustment based on temperature:

- **Reference Temperature**: 25°C
- **Coefficient**: -0.02 V/°C (typical)
- **Example**: At 35°C, threshold decreases by 0.2V

```
Adjusted Voltage = Base Voltage + (Temperature - 25°C) × Coefficient
```

## 🖥️ Usage

### First Boot

Upon first power-up, the system will:

1. Initialize NVS with default configuration
2. Perform ADC calibration
3. Start all tasks
4. Display welcome message on serial console

```
========================================
  Solar Battery Controller v1.0
========================================
Type 'help' for available commands
Type 'status' for system status

solar> 
```

### Monitoring

Real-time status is logged every few seconds:

```
I (1234) ADC_HANDLER: Battery: 12450 mV (12.45V), Temp: 23.5°C
I (1235) CHAN_PROC: CH0: State=ON, Voltage=12430 mV
I (1236) CONTROL: Status: CH0=ON, CH1=OFF, Duty=100%, Battery=12450mV
```

### Basic Operation

1. **Check Status**:
   ```
   solar> status
   ```

2. **Adjust Thresholds**:
   ```
   solar> set_threshold 0 13000 12000
   ```

3. **Test Motion Detection**:
   ```
   solar> motion
   ```

4. **View Statistics**:
   ```
   solar> dump_verification
   ```

## 📟 CLI Commands

### System Status Commands

#### `status`
Display comprehensive system status including battery voltage, temperature, channel states, and configuration.

**Example:**
```
solar> status

=== Solar Battery Controller Status ===

Battery:
  Voltage: 12450 mV (12.45 V)
  Temperature: 23.5 °C

Channel 0:
  State: ON
  Filtered Voltage: 12430 mV (12.43 V)
  Threshold ON: 12500 mV
  Threshold OFF: 11800 mV

Channel 1:
  State: OFF
  Filtered Voltage: 12430 mV (12.43 V)
  Threshold ON: 12500 mV
  Threshold OFF: 11800 mV

Hardware:
  CH0 Output: ON
  CH1 Output: OFF
  PWM Duty: 100%
  Motion Detected: no
  Charger Status: not charging

Configuration:
  Temp Coefficient: -0.020
  PWM Half Duty: 50%
  PWM Full Duty: 100%
  Motion Timeout: 30000 ms
```

#### `dump_verification`
Display verification data and statistics.

**Example:**
```
solar> dump_verification

=== Verification Data ===
  Total Cycles: 42
  Last Voltage: 12450 mV (12.45 V)
  Uptime Hours: 168
  Charge Cycles: 15
```

### Configuration Commands

#### `set_threshold <channel> <on_mv> <off_mv>`
Set voltage thresholds for a channel.

**Parameters:**
- `channel`: 0 or 1
- `on_mv`: ON threshold in millivolts (10000-20000)
- `off_mv`: OFF threshold in millivolts (10000-20000)

**Example:**
```
solar> set_threshold 0 13000 12000
Channel 0 thresholds set: ON=13000 mV, OFF=12000 mV
Configuration saved to NVS
```

#### `set_temp_coeff <coefficient>`
Set temperature compensation coefficient.

**Parameters:**
- `coefficient`: -0.1 to 0.1 (typical: -0.02)

**Example:**
```
solar> set_temp_coeff -0.025
Temperature coefficient set to -0.025
Configuration saved to NVS
```

#### `set_pwm <half> <full>`
Set PWM duty cycles for half and full brightness.

**Parameters:**
- `half`: Half brightness duty (0-100%)
- `full`: Full brightness duty (0-100%)

**Example:**
```
solar> set_pwm 40 90
PWM duties set: Half=40%, Full=90%
Configuration saved to NVS
```

### Testing Commands

#### `motion`
Manually trigger motion detection.

**Example:**
```
solar> motion
Motion detection triggered manually
Lights will stay at full brightness for 30 seconds
```

#### `shutdown`
Emergency shutdown - turn off all outputs immediately.

**Example:**
```
solar> shutdown
EMERGENCY SHUTDOWN - Turning off all outputs
```

### Maintenance Commands

#### `reset_verification`
Reset all verification counters to zero.

**Example:**
```
solar> reset_verification
Verification data reset and saved
```

#### `restart`
Restart the ESP32.

**Example:**
```
solar> restart
Restarting system in 2 seconds...
```

#### `help`
Display command help.

## 📊 Technical Specifications

### ADC Specifications

| Parameter | Value |
|-----------|-------|
| Resolution | 12-bit (4096 levels) |
| Attenuation | 12 dB |
| Voltage Range | 0 - 3.6V |
| Sampling Rate | 100ms (10 Hz) |
| Oversampling | 8x |
| Effective Resolution | ~15-bit after oversampling |

### PWM Specifications

| Parameter | Value |
|-----------|-------|
| Resolution | 13-bit (8192 levels) |
| Frequency | 5 kHz |
| Duty Cycle Range | 0 - 100% |
| Update Rate | 100ms |

### Processing Specifications

| Parameter | Value |
|-----------|-------|
| Moving Average Window | 16 samples (1.6s) |
| Debounce Time | 5 seconds |
| Temperature Refresh | Per ADC sample (100ms) |
| State Update Rate | 100ms |

### Memory Usage

| Component | Approximate Size |
|-----------|------------------|
| Program Flash | ~250 KB |
| RAM (runtime) | ~50 KB |
| NVS Storage | ~4 KB |
| Stack (all tasks) | ~26 KB |

## 🔍 Troubleshooting

### Common Issues

#### ADC Readings Incorrect

**Symptoms:** Battery voltage reading is wrong

**Solutions:**
1. Verify voltage divider resistor values with multimeter
2. Check ADC calibration in logs
3. Measure actual voltage divider output
4. Adjust divider ratio in `adc_handler.c` if needed

```c
// In adc_handler.c
#define R_TOP    47000.0f  // Verify with multimeter
#define R_BOT    10000.0f  // Verify with multimeter
```

#### Rapid Channel Switching

**Symptoms:** Output toggles on/off repeatedly

**Solutions:**
1. Increase hysteresis gap between ON/OFF thresholds
2. Increase debounce time
3. Check battery voltage stability

```bash
solar> set_threshold 0 12800 11800  # Larger gap
```

#### Temperature Readings Invalid

**Symptoms:** Temperature shows -40°C or 125°C constantly

**Solutions:**
1. Check TMP36 wiring (VCC, GND, Signal)
2. Verify sensor is getting 3.3V or 5V power
3. Check ADC channel configuration
4. Replace sensor if faulty

#### CLI Not Responding

**Symptoms:** No response to typed commands

**Solutions:**
1. Check baud rate (115200)
2. Verify USB cable supports data (not charging-only)
3. Check serial port permissions: `sudo usermod -a -G dialout $USER`
4. Try different terminal program (screen, minicom, putty)

#### PWM Not Working

**Symptoms:** Lights don't dim or no output

**Solutions:**
1. Check MOSFET gate connections
2. Verify MOSFET is logic-level type
3. Check PWM frequency compatibility with load
4. Measure PWM signal with oscilloscope

### Debugging Tools

#### Enable Debug Logging

```bash
# In menuconfig
idf.py menuconfig

# Navigate to:
Component config → Log output → Default log verbosity → Debug
```

#### Monitor Specific Component

```bash
# Add to your component:
ESP_LOGD(TAG, "Debug message: value=%d", value);
```

#### Check Task Status

Add this to a CLI command to view task information:

```c
vTaskList(buffer);
printf("%s\n", buffer);
```

### Performance Optimization

#### Reduce Logging

Comment out periodic logging in production:

```c
// In adc_handler.c, channel_processor.c, control_handler.c
// Comment out ESP_LOGI statements in loops
```

#### Adjust Task Priorities

If system is sluggish:

```c
// In main.c
#define PRIORITY_ADC        6  // Increase if sampling is delayed
#define PRIORITY_CONTROL    6  // Increase if output is laggy
```

## 🛠️ Development

### Project Structure

```
solar_controller/
├── CMakeLists.txt              # Root build configuration
├── sdkconfig.defaults          # Default SDK configuration
├── README.md                   # This file
├── WIRING.md                   # Detailed wiring guide
├── CALIBRATION.md              # Calibration procedures
└── main/
    ├── CMakeLists.txt          # Component build config
    ├── main.c                  # Application entry point
    ├── adc_handler.c/h         # ADC sampling
    ├── channel_processor.c/h   # Signal processing
    ├── control_handler.c/h     # Hardware control
    ├── cli_handler.c/h         # Command-line interface
    └── nvs_storage.c/h         # Configuration storage
```

### Adding New Features

#### Add a New CLI Command

1. **Define command arguments** in `cli_handler.c`:
```c
static struct {
    struct arg_int *param1;
    struct arg_end *end;
} my_command_args;
```

2. **Implement command function**:
```c
static int cmd_my_command(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&my_command_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, my_command_args.end, argv[0]);
        return 1;
    }
    
    // Your code here
    
    return 0;
}
```

3. **Register command** in `register_commands()`:
```c
my_command_args.param1 = arg_int1(NULL, NULL, "<value>", "Description");
my_command_args.end = arg_end(1);

const esp_console_cmd_t my_cmd = {
    .command = "mycommand",
    .help = "My command help text",
    .hint = NULL,
    .func = &cmd_my_command,
    .argtable = &my_command_args
};
ESP_ERROR_CHECK(esp_console_cmd_register(&my_cmd));
```

#### Add a New Sensor

1. **Define sensor interface** in new files `sensor_handler.c/h`
2. **Create sensor task** or integrate into ADC task
3. **Update CMakeLists.txt** to include new files
4. **Add sensor data to reading structure**
5. **Process data in channel processor**

### Code Style Guidelines

- Use descriptive variable names
- Comment complex algorithms
- Follow ESP-IDF coding standards
- Use `ESP_LOGx` for all logging
- Add error checking for all ESP-IDF functions
- Document all public functions in headers

### Testing

#### Unit Testing

Create test cases in `main/test/`:

```c
#include "unity.h"
#include "channel_processor.h"

TEST_CASE("Moving average calculation", "[processor]")
{
    // Test code
    TEST_ASSERT_EQUAL(expected, actual);
}
```

Run tests:
```bash
idf.py test
```

#### Hardware Testing

1. **ADC Calibration**: Use known reference voltage
2. **PWM Verification**: Use oscilloscope or LED
3. **Load Testing**: Run for 24+ hours
4. **Temperature Range**: Test -10°C to 40°C

### Contributing

1. Fork the repository
2. Create feature branch
3. Make changes
4. Test thoroughly
5. Submit pull request with description

## 📄 License

MIT License - See LICENSE file for details

## 🙏 Acknowledgments

- ESP-IDF framework by Espressif Systems
- Community contributors
- Open source hardware community

## 📞 Support

For issues, questions, or contributions:
- Open an issue on GitHub
- Check troubleshooting section
- Review ESP-IDF documentation

---

**Version:** 1.0.0  
**Last Updated:** October 8 2025  
**Author:** Zhanibekuly Darkhan