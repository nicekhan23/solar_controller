# Solar Panel Battery Controller

ESP32-based solar battery controller with dual-channel processing.

## Features
- ADC battery voltage monitoring
- Moving average filtering
- Hysteresis control
- Temperature compensation
- Motion sensor integration
- PWM dimming
- Serial CLI
- NVS configuration storage

## Hardware Connections
- Battery voltage: GPIO34 (ADC1_CH6) via 47kΩ/10kΩ divider
- Temperature: GPIO35 (ADC1_CH7)
- Motion sensor: GPIO4
- Light CH0: GPIO25 (PWM)
- Light CH1: GPIO26 (PWM)

## Building

idf.py build
idf.py -p /dev/ttyUSB0 flash monitor

## CLI Commands
- `status` - Show current readings
- `set_threshold <ch> <on_mv> <off_mv>` - Set thresholds
- `dump_verification` - Show verification data