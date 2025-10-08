# Calibration Guide - Solar Battery Controller

This document provides detailed calibration procedures to ensure accurate readings and optimal performance.

## Table of Contents

- [Overview](#overview)
- [Required Equipment](#required-equipment)
- [ADC Calibration](#adc-calibration)
- [Voltage Divider Calibration](#voltage-divider-calibration)
- [Temperature Sensor Calibration](#temperature-sensor-calibration)
- [PWM Calibration](#pwm-calibration)
- [Threshold Tuning](#threshold-tuning)
- [Motion Sensor Adjustment](#motion-sensor-adjustment)
- [Verification Procedures](#verification-procedures)

## Overview

Proper calibration ensures:
- Accurate battery voltage readings (±1%)
- Reliable temperature compensation
- Correct load switching thresholds
- Optimal battery protection
- Consistent PWM dimming

**Calibration Frequency:**
- Initial setup: Complete calibration
- After component changes: Relevant sections
- Annually: Verification check
- After battery replacement: Threshold tuning

## Required Equipment

### Essential Equipment

| Equipment | Specification | Purpose |
|-----------|--------------|---------|
| Digital Multimeter | 4-digit, ±0.5% accuracy | Voltage measurements |
| Power Supply | 0-20V, 2A adjustable | Calibration source |
| Thermometer | ±0.5°C accuracy | Temperature reference |
| Known Resistors | 1% tolerance | Verification |

### Recommended Equipment

| Equipment | Specification | Purpose |
|-----------|--------------|---------|
| Oscilloscope | 20MHz+ bandwidth | PWM waveform analysis |
| Current Meter | DC, 0-10A | Load current verification |
| Precision Voltage Reference | 5V or 10V, ±0.1% | ADC calibration |
| LCR Meter | For capacitance/inductance | Component verification |

### Software Tools

- Serial terminal (PuTTY, minicom, or idf.py monitor)
- Spreadsheet for recording measurements
- Calculator for adjustments

## ADC Calibration

The ESP32 has built-in ADC calibration, but we'll verify its accuracy.

### Step 1: Verify ESP32 ADC Calibration

The firmware automatically attempts hardware calibration on startup. Check the serial output:

```
I (123) ADC_HANDLER: Calibration scheme version: Curve Fitting
I (124) ADC_HANDLER: Curve Fitting calibration successful
```

If calibration fails:
```
W (123) ADC_HANDLER: Calibration failed: ESP_ERR_NOT_SUPPORTED. Using raw values.
```

### Step 2: ADC Linearity Test

**Test multiple voltage points:**

1. **Disconnect battery** from voltage divider
2. **Connect adjustable power supply** to voltage divider input
3. **Set power supply to each test voltage**
4. **Record ADC readings using CLI:**

```bash
solar> status
```

**Test Points:**

| Supply Voltage | Expected ADC Input | Record ADC Reading | Calculate Error |
|----------------|-------------------|-------------------|-----------------|
| 10.0V | 1754mV | ___________ | ___________ |
| 11.0V | 1930mV | ___________ | ___________ |
| 12.0V | 2105mV | ___________ | ___________ |
| 13.0V | 2281mV | ___________ | ___________ |
| 14.0V | 2456mV | ___________ | ___________ |
| 15.0V | 2632mV | ___________ | ___________ |

**Calculate Expected ADC Input:**
```
ADC_mV = Supply_Voltage / Divider_Ratio
ADC_mV = Supply_Voltage / 5.7
```

**Acceptable Error:** ±2% (±50mV at 12V reading)

### Step 3: Correct ADC Readings (if needed)

If error is consistent across all voltages, adjust the divider ratio in `adc_handler.c`:

**Original code:**
```c
#define R_TOP    47000.0f  // 47kΩ
#define R_BOT    10000.0f  // 10kΩ
```

**Calculate actual ratio:**
```
If readings are consistently HIGH by 5%:
Actual_Ratio = Nominal_Ratio × (1 + Error%)
Actual_Ratio = 5.7 × 1.05 = 5.985

Adjust R_TOP:
R_TOP = (Actual_Ratio - 1) × R_BOT
R_TOP = (5.985 - 1) × 10000 = 49850Ω

Update code:
#define R_TOP    49850.0f  // Calibrated value
```

**If readings are consistently LOW:**
```
Actual_Ratio = Nominal_Ratio × (1 - Error%)
```

### Step 4: Non-linearity Correction

If error varies across voltage range (non-linear), implement correction curve:

**Add to adc_handler.c:**

```c
static uint32_t apply_calibration_curve(uint32_t raw_voltage)
{
    // Example: Polynomial correction
    // Calibrated_V = A + B×V + C×V²
    float v = raw_voltage / 1000.0f;
    float calibrated = -0.050f + 1.02f * v + 0.001f * v * v;
    return (uint32_t)(calibrated * 1000.0f);
}
```

Determine coefficients using least-squares fit to your test data.

## Voltage Divider Calibration

### Step 1: Measure Actual Resistor Values

1. **Remove resistors from circuit**
2. **Measure with accurate multimeter:**

```
R1 (47kΩ nominal): ____________ Ω
R2 (10kΩ nominal): ____________ Ω

Actual Ratio = (R1 + R2) / R2 = ____________
```

### Step 2: Verify Divider Output

**Setup:**
```
Power Supply (12.00V) ──┬── [R1] ──┬── [R2] ──┬── GND
                        │          │          │
                     DMM+     DMM+ (2nd)   DMM-
                     Reading1   Reading2
```

**Measurements:**

| Input Voltage (PS) | Expected Output | Measured Output | Error % |
|--------------------|-----------------|-----------------|---------|
| 12.00V | V_out = 12.00/5.7 = 2.105V | __________ | _____ |
| 13.50V | 2.368V | __________ | _____ |
| 14.40V | 2.526V | __________ | _____ |

**Error Calculation:**
```
Error% = ((Measured - Expected) / Expected) × 100
```

### Step 3: Adjust Software Constants

Update `adc_handler.c` with measured values:

```c
// Original
#define R_TOP    47000.0f  
#define R_BOT    10000.0f  

// Calibrated (example with measured values)
#define R_TOP    47120.0f  // Measured value
#define R_BOT    10050.0f  // Measured value
#define DIVIDER_RATIO  ((R_TOP + R_BOT) / R_BOT)  // ~5.69
```

### Step 4: Capacitor Verification

**Verify filtering capacitor:**

1. Measure capacitor value: C1 = __________ nF (should be ~100nF)
2. Check for proper operation:
   - No ripple > 10mV on ADC input
   - Fast settling time (< 10ms)

**If ripple is excessive:**
- Increase capacitor to 220nF or 470nF
- Add second 10nF ceramic in parallel
- Check ground connection quality

## Temperature Sensor Calibration

### TMP36 Calibration

**TMP36 Formula:**
```
Temperature (°C) = (Vout - 500mV) / 10mV/°C
```

### Step 1: Reference Temperature Measurement

**Setup:**
1. Place TMP36 and reference thermometer in insulated container
2. Wait 10 minutes for thermal equilibrium
3. Record both readings

**Ice Water Bath (0°C):**
```
Reference Thermometer: __________ °C
TMP36 ADC Reading:     __________ mV
Calculated Temp:       __________ °C
Error:                 __________ °C
```

**Room Temperature (~25°C):**
```
Reference Thermometer: __________ °C
TMP36 ADC Reading:     __________ mV
Calculated Temp:       __________ °C
Error:                 __________ °C
```

**Hot Water Bath (~50°C):**
```
Reference Thermometer: __________ °C
TMP36 ADC Reading:     __________ mV
Calculated Temp:       __________ °C
Error:                 __________ °C
```

### Step 2: Calculate Calibration Constants

**Linear correction:**
```
Actual_Temp = Offset + Slope × Raw_Temp

Where:
Offset = Average error at all test points
Slope = If error increases with temperature, adjust slope
```

**Example calculation:**

If sensor reads 2°C high consistently:
```
Offset = -2.0°C
Slope = 1.0 (no change)
```

If sensor reads 0°C at ice point, 3°C high at 50°C:
```
Offset = 0°C
Slope = 50/(50+3) = 0.943
```

### Step 3: Update Temperature Calculation

**Modify `adc_handler.c`:**

```c
static float calculate_temperature(uint32_t adc_mv)
{
    // Original TMP36 formula
    float temp_c = ((float)adc_mv - 500.0f) / 10.0f;
    
    // Apply calibration
    const float TEMP_OFFSET = -2.0f;  // Your measured offset
    const float TEMP_SLOPE = 1.0f;    // Your measured slope
    
    temp_c = TEMP_OFFSET + TEMP_SLOPE * temp_c;
    
    // Sanity check
    if (temp_c < -40.0f || temp_c > 125.0f) {
        ESP_LOGW(TAG, "Temperature out of range: %.1f°C, using 25°C", temp_c);
        temp_c = 25.0f;
    }
    
    return temp_c;
}
```

### Step 4: Verify Temperature Compensation

**Test temperature compensation effectiveness:**

1. Set known temperature (e.g., 25°C)
2. Note battery threshold: __________ mV
3. Change temperature to 35°C
4. New threshold should be: Original + (10°C × -0.02V/°C × 1000) = __________ mV
5. Verify with CLI `status` command

**Temperature Coefficient Check:**

| Temperature | Expected Threshold Adjustment | Actual Adjustment | Match? |
|-------------|------------------------------|-------------------|--------|
| 15°C | +200mV | __________ | ☐ |
| 25°C | 0mV (reference) | 0mV | ☐ |
| 35°C | -200mV | __________ | ☐ |

## PWM Calibration

### Step 1: Verify PWM Frequency

**Using Oscilloscope:**

1. Connect oscilloscope probe to GPIO25 or GPIO26
2. Trigger on rising edge
3. Measure frequency

```
Expected Frequency: 5000 Hz
Measured Frequency: __________ Hz
Error:              __________ Hz (should be < ±250Hz)
```

**If frequency is incorrect:**

Check `control_handler.c`:
```c
#define LEDC_FREQUENCY          5000  // Adjust if needed
```

### Step 2: Duty Cycle Accuracy

**Test multiple duty cycles:**

| Command | Expected Duty | Measured Duty | Error % |
|---------|--------------|---------------|---------|
| 0% | 0% | __________ | _____ |
| 25% | 25% | __________ | _____ |
| 50% | 50% | __________ | _____ |
| 75% | 75% | __________ | _____ |
| 100% | 100% | __________ | _____ |

**Using CLI:**
```bash
solar> set_pwm 25 100    # Set half=25%, full=100%
solar> status            # Check current duty
```

**Oscilloscope Measurement:**
```
Duty Cycle = (High Time / Period) × 100%

Period = 1 / 5000Hz = 200µs
High Time at 50% = 100µs
```

### Step 3: Verify PWM Linearity

**Brightness vs Duty Cycle:**

Use light meter or photodiode to measure actual brightness:

| Duty Cycle | Relative Brightness | Expected (Linear) | Deviation |
|------------|-------------------|------------------|-----------|
| 10% | __________ | 10% | _____ |
| 25% | __________ | 25% | _____ |
| 50% | __________ | 50% | _____ |
| 75% | __________ | 75% | _____ |
| 100% | 100% (reference) | 100% | 0% |

**Note:** LED brightness may not be linear. Consider gamma correction if needed.

### Step 4: Gamma Correction (Optional)

If brightness is non-linear, apply gamma correction:

**Add to `control_handler.c`:**

```c
static uint8_t apply_gamma_correction(uint8_t duty_percent)
{
    // Gamma = 2.2 typical for LEDs
    const float gamma = 2.2f;
    float normalized = duty_percent / 100.0f;
    float corrected = powf(normalized, gamma);
    return (uint8_t)(corrected * 100.0f);
}
```

## Threshold Tuning

### Step 1: Determine Battery Characteristics

**Measure your specific battery:**

| State | Voltage | Notes |
|-------|---------|-------|
| Fully Charged (resting) | __________ | After 2hrs no load |
| Float Charge | __________ | Under charge |
| 90% SOC | __________ | Light load |
| 50% SOC | __________ | Nominal load |
| 20% SOC | __________ | Heavy load |
| 10% SOC | __________ | Critical |

**Typical Lead-Acid Values:**
- 12V 100% SOC: 12.7V
- 12V 50% SOC: 12.2V
- 12V 20% SOC: 11.8V
- 12V 0% SOC: 10.5V (do not discharge below!)

### Step 2: Set Conservative Thresholds

**Initial recommended thresholds:**

```bash
solar> set_threshold 0 12500 11800
solar> set_threshold 1 12500 11800
```

**Rationale:**
- ON threshold: 12.5V (battery well-charged)
- OFF threshold: 11.8V (20% SOC, safe margin)
- Hysteresis: 700mV (prevents oscillation)

### Step 3: Load Testing

**Monitor battery behavior under actual load:**

1. Start with full battery
2. Enable load via threshold
3. Record voltage every 30 minutes
4. Note when load turns off
5. Verify battery not over-discharged

**Test Data Log:**

| Time | Battery Voltage | Load State | Battery SOC (est) |
|------|----------------|------------|------------------|
| 00:00 | __________ | ON | ~100% |
| 00:30 | __________ | ON | _____ |
| 01:00 | __________ | ON | _____ |
| 01:30 | __________ | ON | _____ |
| 02:00 | __________ | ??? | _____ |

### Step 4: Optimize Thresholds

**Adjust based on testing:**

**If load turns off too early:**
```bash
solar> set_threshold 0 12500 11500  # Lower OFF threshold
```

**If battery over-discharged:**
```bash
solar> set_threshold 0 12500 12000  # Raise OFF threshold
```

**If rapid on/off cycling:**
```bash
solar> set_threshold 0 13000 11800  # Increase hysteresis
```

### Step 5: Temperature-Adjusted Thresholds

**Test at different temperatures:**

| Temperature | Auto-Adjusted Threshold | Battery Actual | Correct? |
|-------------|------------------------|----------------|----------|
| 5°C | __________ | __________ | ☐ |
| 15°C | __________ | __________ | ☐ |
| 25°C | __________ (reference) | __________ | ☐ |
| 35°C | __________ | __________ | ☐ |
| 45°C | __________ | __________ | ☐ |

**Adjust temperature coefficient if needed:**

```bash
# Default: -0.02V/°C
solar> set_temp_coeff -0.02

# If battery voltage drops more with temp:
solar> set_temp_coeff -0.025

# If battery voltage drops less with temp:
solar> set_temp_coeff -0.015
```

## Motion Sensor Adjustment

### Step 1: Physical Adjustment

**HC-SR501 has two potentiometers:**

1. **Sensitivity (Sx):** Detection range
   - Clockwise: Increase range (3-7 meters)
   - Counter-clockwise: Decrease range
   
2. **Time Delay (Tx):** Hardware timeout
   - Set to MINIMUM (fully counter-clockwise)
   - We handle timeout in software

### Step 2: Detection Zone Testing

**Test detection pattern:**

```
Mark detection boundaries:

        ↑ 
        │ Forward
        │
    ────┼────  Sensor
        │
        │
        
Test Points:
- 1m directly ahead: ☐ Detected
- 2m directly ahead: ☐ Detected
- 3m directly ahead: ☐ Detected
- 1m at 45° angle:  ☐ Detected
- 1m at 90° angle:  ☐ Detected (should not)
```

**Adjust sensitivity:**
- Too sensitive: Reduce (turn CCW)
- Not sensitive enough: Increase (turn CW)
- False triggers: Reduce sensitivity, shield from drafts

### Step 3: Software Timeout Calibration

**Test different timeout values:**

```bash
# Test 15 seconds
solar> set_motion_timeout 15000

# Test 30 seconds (default)
solar> set_motion_timeout 30000

# Test 60 seconds
solar> set_motion_timeout 60000
```

**Walk test:**
1. Trigger motion
2. Stand still in view
3. Measure how long lights stay on
4. Adjust timeout to desired behavior

### Step 4: Debounce Verification

Check `control_handler.c` for debounce time:

```c
#define MOTION_DEBOUNCE_MS          500    // 500ms debounce
```

**If false triggers occur:**
- Increase debounce to 1000ms
- Check for electrical noise sources
- Add shielding to sensor cable

## Verification Procedures

### Complete System Verification

**Checklist for final validation:**

#### Voltage Accuracy
```
☐ Battery voltage reading within ±1% of multimeter
☐ Voltage stable (< 50mV variation without load changes)
☐ All test voltages 10-15V read correctly
☐ ADC values reasonable (1754mV - 2632mV range)
```

#### Temperature Accuracy
```
☐ Temperature reading within ±2°C of reference
☐ Temperature stable (< 1°C variation at constant temp)
☐ Temperature compensation adjusts thresholds correctly
☐ No temperature-related false triggers
```

#### Threshold Operation
```
☐ Load turns ON when voltage > ON threshold
☐ Load turns OFF when voltage < OFF threshold
☐ No oscillation (rapid on/off)
☐ Hysteresis gap appropriate (> 500mV)
☐ Debounce prevents rapid state changes
```

#### PWM Operation
```
☐ Frequency = 5000Hz ±5%
☐ Duty cycle accuracy within ±2%
☐ No visible flicker at any duty cycle
☐ Smooth dimming transitions
☐ Full range 0-100% works
```

#### Motion Detection
```
☐ Detects motion reliably in target zone
☐ No false triggers from environment
☐ Timeout works as configured
☐ Manual trigger command works
☐ Full brightness during motion period
```

#### System Integration
```
☐ All CLI commands work correctly
☐ Configuration saves to NVS
☐ Settings persist after reboot
☐ Status command shows accurate info
☐ No crashes or errors in 24hr test
```

### Long-Term Verification

**24-Hour Test Protocol:**

1. **Setup monitoring:**
   - Connect serial logger
   - Record battery voltage every minute
   - Log all state changes
   - Monitor temperature

2. **Run through scenarios:**
   - Full battery → gradual discharge
   - Motion detection multiple times
   - Temperature variation (if possible)
   - Power cycle (reboot)

3. **Verify data:**
   ```
   ☐ Voltage readings consistent
   ☐ No unexpected reboots
   ☐ All thresholds triggered correctly
   ☐ Temperature compensation working
   ☐ Motion sensor responsive
   ☐ PWM stable throughout test
   ```

4. **Check verification data:**
   ```bash
   solar> dump_verification
   
   ☐ Total cycles incremented
   ☐ Last voltage recorded
   ☐ Uptime tracking accurate
   ☐ No memory leaks (heap stable)
   ```

### Calibration Record Template

**Document your calibration:**

```
===========================================
CALIBRATION RECORD
===========================================

Date: __________________
Technician: __________________
System ID: __________________

VOLTAGE DIVIDER
  R1 (measured): __________ Ω
  R2 (measured): __________ Ω
  Ratio: __________
  Code updated: ☐ Yes ☐ No

ADC CALIBRATION
  Test voltage: __________ V
  ADC reading: __________ mV
  Error: __________ %
  Acceptable: ☐ Yes ☐ No

TEMPERATURE SENSOR
  Offset: __________ °C
  Slope: __________
  Code updated: ☐ Yes ☐ No

PWM VERIFICATION
  Frequency: __________ Hz
  50% duty accuracy: __________ %
  Acceptable: ☐ Yes ☐ No

THRESHOLDS
  CH0 ON: __________ mV
  CH0 OFF: __________ mV
  CH1 ON: __________ mV
  CH1 OFF: __________ mV
  Temp coeff: __________

MOTION SENSOR
  Sensitivity: __________ (pot position)
  Range: __________ meters
  Timeout: __________ seconds

FINAL VERIFICATION
  24-hour test: ☐ Pass ☐ Fail
  All checks pass: ☐ Yes ☐ No
  System approved: ☐ Yes ☐ No

Notes:
_________________________________________
_________________________________________
_________________________________________

Signature: _______________  Date: ________
===========================================
```

## Troubleshooting Calibration Issues

### Readings Jump Around

**Symptom:** Voltage or temperature readings unstable

**Solutions:**
1. Add or increase filter capacitor
2. Shorten wire lengths
3. Use shielded cable
4. Improve grounding (star topology)
5. Increase moving average window size
6. Check for EMI sources nearby

### Consistent Offset Error

**Symptom:** Reading always high or low by same amount

**Solutions:**
1. Measure actual resistor values
2. Update software constants
3. Verify reference voltage
4. Check ADC calibration
5. Apply offset correction in code

### Non-Linear Error

**Symptom:** Error varies across measurement range

**Solutions:**
1. Implement correction curve
2. Check for component heating
3. Verify power supply regulation
4. Test components individually
5. Consider replacing out-of-spec parts

### Temperature Drift

**Symptom:** Readings change with temperature

**Solutions:**
1. Use temperature-stable resistors (metal film)
2. Implement temperature compensation
3. Add thermal mass to sensor
4. Shield from direct sunlight/drafts
5. Calibrate at operating temperature

---

**Document Version:** 1.0  
**Last Updated:** October 8, 2025  
**Author:** Zhanibekuly Darkhan