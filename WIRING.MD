# Wiring Guide - Solar Battery Controller

This document provides detailed wiring instructions for the Solar Battery Controller ESP32 project.

## Table of Contents

- [Safety Warnings](#safety-warnings)
- [Required Tools](#required-tools)
- [Component Specifications](#component-specifications)
- [Step-by-Step Wiring](#step-by-step-wiring)
- [Testing Procedures](#testing-procedures)
- [Troubleshooting](#troubleshooting)

## Safety Warnings

⚠️ **IMPORTANT SAFETY INFORMATION**

- **Always disconnect power** before making any wiring changes
- **Use appropriate fuses** on all battery connections (recommended: 5A-10A depending on load)
- **Check polarity** carefully - reversed polarity can destroy components
- **Never short battery terminals** - this can cause fire or explosion
- **Work in a well-ventilated area** when working with batteries
- **Wear safety glasses** when working with electrical components
- **Use insulated tools** to prevent accidental shorts
- **Keep flammable materials away** from your work area
- **Lead-acid batteries produce explosive hydrogen gas** - ensure good ventilation

## Required Tools

### Essential Tools

- Soldering iron (temperature controlled, 300-350°C recommended)
- Solder (60/40 or lead-free)
- Wire strippers (18-24 AWG)
- Multimeter (with voltage, continuity, and resistance modes)
- Small screwdrivers (Phillips and flat head)
- Wire cutters/snips
- Heat shrink tubing or electrical tape
- Breadboard (for prototyping)
- Jumper wires (male-to-male, male-to-female)

### Recommended Tools

- Helping hands or PCB holder
- Desoldering pump or wick
- Label maker or masking tape for wire labeling
- Cable ties for wire management
- Hot glue gun for strain relief
- Oscilloscope (for PWM verification)

## Component Specifications

### ESP32 DevKit Pin Reference

```
                        ESP32 DevKit
                    ┌──────────────────┐
                    │                  │
              EN ───┤ 1             30 ├─── GPIO23
       SENSOR VP ───┤ 2   ESP32     29 ├─── GPIO22
       SENSOR VN ───┤ 3   WROOM-32  28 ├─── TX (GPIO1)
          GPIO34 ───┤ 4             27 ├─── RX (GPIO3)
          GPIO35 ───┤ 5             26 ├─── GPIO21
          GPIO32 ───┤ 6             25 ├─── GPIO19
          GPIO33 ───┤ 7             24 ├─── GPIO18
          GPIO25 ───┤ 8             23 ├─── GPIO5
          GPIO26 ───┤ 9             22 ├─── TX2 (GPIO17)
          GPIO27 ───┤10             21 ├─── RX2 (GPIO16)
          GPIO14 ───┤11             20 ├─── GPIO4
          GPIO12 ───┤12             19 ├─── GPIO2
          GPIO13 ───┤13             18 ├─── GPIO15
             GND ───┤14             17 ├─── GND
              VIN ──┤15             16 ├─── 3V3
                    └──────────────────┘
                   
* GPIO1, GPIO3, GPIO6-11 are used for flash, do not use
```

### Voltage Divider Circuit Details

**Purpose:** Reduce 12V battery voltage to safe 3.3V level for ESP32 ADC

**Component Selection:**

| Component | Value | Tolerance | Power Rating | Notes |
|-----------|-------|-----------|--------------|-------|
| R1 (Top) | 47kΩ | 1% | 1/4W | Metal film recommended |
| R2 (Bottom) | 10kΩ | 1% | 1/4W | Metal film recommended |
| C1 (Filter) | 100nF | 10% | 50V | Ceramic, X7R type |

**Circuit:**

```
Battery+ (12V)
    │
    ├───[Fuse 5A]───┐
    │               │
    │            [R1: 47kΩ]
    │               │
    │               ├────┬──── To GPIO34 (ADC)
    │               │    │
    │            [R2: 10kΩ]  [C1: 100nF]
    │               │    │
    └───────────────┴────┴──── GND
```

**Calculations:**

```
Divider Ratio = (R1 + R2) / R2 = (47k + 10k) / 10k = 5.7

At 12V battery:  ADC = 12V / 5.7 = 2.11V ✓
At 14.4V (max):  ADC = 14.4V / 5.7 = 2.53V ✓
At 20V (safety): ADC = 20V / 5.7 = 3.51V ✓ (below 3.6V max)

Max current through divider = 20V / 57kΩ = 0.35mA
Power dissipation (R1) = I² × R = (0.35mA)² × 47k = 5.8mW ✓
```

### MOSFET Driver Circuit

**Component Selection:**

| Parameter | Specification | Notes |
|-----------|--------------|-------|
| MOSFET Type | Logic-level N-channel | RDS(on) < 0.1Ω at VGS=3.3V |
| Recommended Part | IRLZ44N | 55V, 47A, RDS(on)=0.022Ω |
| Alternative | IRL540N | 100V, 28A, RDS(on)=0.044Ω |
| Gate Resistor | 100Ω | Current limiting |
| Pull-down Resistor | 10kΩ | Prevents floating gate |
| Flyback Diode | 1N5819 | Schottky, for inductive loads |

**Circuit Diagram:**

```
                         +12V (Battery+)
                           │
                           │
                      ┌────┴────┐
                      │  LOAD   │ (LED strip, etc.)
                      │         │
                      └────┬────┘
                           │
                        Drain
                           │
                      ┌────┴────┐
            ┌─────────┤ MOSFET  │
            │         │ IRLZ44N │
            │         └────┬────┘
           ─┴─             │
          ╱   ╲         Source
         ╱     ╲           │
        └───────┘          │
     D1: 1N5819        ┌───┴───┐
     (Flyback)         │  GND  │
            │          └───────┘
            │
         +12V

     Gate ────[R: 100Ω]──── ESP32 GPIO25/26
            
     Gate ────[R: 10kΩ]──── GND
            (pull-down)
```

**MOSFET Pin Identification:**

```
IRLZ44N (TO-220 Package, front view)
     ┌─────┐
     │  ┌──┤ Tab = Drain
     │  │  │
     ├──┘  │
     │     │
     └─┬─┬─┘
       │ │ │
       1 2 3
       │ │ │
     Gate │ Source
        Drain
```

## Step-by-Step Wiring

### Step 1: Prepare the ESP32

1. **Verify ESP32 functionality:**
   - Connect to USB
   - Upload blink example
   - Confirm it works before proceeding

2. **Identify critical pins:**
   - Mark GPIO34 (ADC battery)
   - Mark GPIO35 (ADC temperature)
   - Mark GPIO25 (PWM CH0)
   - Mark GPIO26 (PWM CH1)
   - Mark GPIO4 (Motion sensor)

### Step 2: Build the Voltage Divider

**On Breadboard (Prototyping):**

1. Insert R1 (47kΩ) across 5 holes
2. Insert R2 (10kΩ) connecting to R1
3. Add capacitor (100nF) parallel to R2
4. Connect junction to ESP32 GPIO34
5. Connect top of R1 to battery+ rail
6. Connect bottom (R2/C1) to GND rail

**On PCB (Permanent):**

1. Solder R1 (47kΩ) first
2. Solder R2 (10kΩ)
3. Solder capacitor (watch polarity if electrolytic)
4. Add wire from junction to ESP32 GPIO34
5. Use heat shrink on all exposed connections
6. Test with multimeter before connecting battery

**Testing the Divider:**

```bash
# With multimeter set to resistance mode:
1. Measure R1: Should read ~47kΩ ±1%
2. Measure R2: Should read ~10kΩ ±1%
3. Measure total: Should read ~57kΩ

# With power supply set to 12V:
4. Connect supply through 1kΩ safety resistor
5. Measure output: Should be ~2.1V
6. If correct, proceed to ESP32 connection
```

### Step 3: Wire the MOSFET Drivers

**For EACH channel (repeat for CH0 and CH1):**

1. **Mount MOSFET:**
   - Identify pins (Gate, Drain, Source)
   - Mount on heatsink if load > 5A
   - Use thermal paste for loads > 10A

2. **Connect Gate Circuit:**
   ```
   GPIO25 ──┬── [100Ω] ──── MOSFET Gate
            │
            └── [10kΩ] ──── GND
   ```

3. **Connect Drain:**
   ```
   MOSFET Drain ──── Load Negative
   ```

4. **Connect Source:**
   ```
   MOSFET Source ──── GND (Common ground with ESP32)
   ```

5. **Add Flyback Diode (for inductive loads):**
   ```
   Battery+ ──── Diode Anode (stripe on cathode)
   Diode Cathode ──── MOSFET Drain
   ```

**Wire Gauge Selection:**

| Current | Wire Gauge | Notes |
|---------|------------|-------|
| < 5A | 18 AWG | LED strips, small loads |
| 5-10A | 16 AWG | Medium loads |
| 10-20A | 14 AWG | Heavy loads |
| > 20A | 12 AWG | Very heavy loads |

### Step 4: Connect the Temperature Sensor

**For TMP36 (Analog Sensor):**

```
TMP36 Pin Layout (flat side facing you):
   ┌─────┐
   │  ▄  │
   │ │ │ │
   └─┬─┬─┘
     1 2 3
     │ │ │
    +V │ GND
      Vout

Connections:
Pin 1 (Vcc)  ──── +3.3V (or +5V)
Pin 2 (Vout) ──── GPIO35 (ADC)
Pin 3 (GND)  ──── GND
```

**Wire Length Considerations:**
- Keep wires < 30cm to minimize noise
- Use shielded cable for longer runs
- Add 100nF capacitor near ESP32 pin

**For DS18B20 (Digital Sensor) - Alternative:**

```
DS18B20 Pin Layout:
   ┌─────┐
   │     │
   │     │
   └─┬─┬─┘
     1 2 3
     │ │ │
    GND │ VCC
       DQ

Connections:
Pin 1 (GND)  ──── GND
Pin 2 (DQ)   ──── GPIO35 (requires 4.7kΩ pull-up to VCC)
Pin 3 (VCC)  ──── +3.3V (or +5V)

Pull-up resistor:
GPIO35 ──── [4.7kΩ] ──── +3.3V
```

### Step 5: Connect the Motion Sensor

**HC-SR501 PIR Sensor:**

```
HC-SR501 Connections:
   ┌─────────────┐
   │   ┌───┐     │
   │   │PIR│     │
   │   └───┘     │
   │             │
   │  ○ ○ ○ ○    │ ← Jumpers & potentiometers
   └──┬─┬─┬──────┘
      1 2 3
      │ │ │
     VCC│GND
       OUT

Connections:
Pin 1 (VCC) ──── +5V (or +3.3V, check sensor datasheet)
Pin 2 (OUT) ──── GPIO4
Pin 3 (GND) ──── GND

Add pull-down resistor:
GPIO4 ──── [10kΩ] ──── GND
```

**Sensor Settings:**
- Sensitivity adjustment: Middle position (test and adjust)
- Time delay: Minimum (we handle timeout in software)
- Trigger mode: Repeatable (H position)

### Step 6: Power Supply Wiring

**CRITICAL: Follow this order exactly:**

1. **Install fuse holder:**
   ```
   Battery+ ──── [Fuse 5-10A] ──── Distribution Point
   ```

2. **Connect ESP32 power:**
   - Option A: USB power (for development)
   - Option B: Buck converter from battery
   ```
   Battery+ ──── [Buck Converter] ──── 5V ──── ESP32 VIN
   ```

3. **Verify voltage before connecting ESP32:**
   - Measure buck converter output: Must be 5.0V ±0.25V
   - Measure with load (1kΩ resistor)
   - Check for ripple with oscilloscope if available

4. **Common ground connection:**
   ```
   All GND connections must connect to single point:
   - Battery GND
   - ESP32 GND
   - MOSFET Sources
   - Sensor GNDs
   ```

### Step 7: Load Connections

**LED Strip Example:**

```
              Battery+
                 │
                 │
            ┌────┴────┐
            │ LED     │
            │ Strip   │
            │ 12V     │
            └────┬────┘
                 │
                 │ (Red wire: LED Strip -)
                 │
          [MOSFET Drain]
                 │
          [MOSFET Source]
                 │
                GND
```

**Multiple Loads:**

```
For parallel loads on same channel:

Battery+ ──┬──── Load 1+ ──── Load 1- ────┐
           │                              │
           └──── Load 2+ ──── Load 2- ────┤
                                          │
                                    [MOSFET Drain]
                                          │
                                    [MOSFET Source]
                                          │
                                         GND
```

## Testing Procedures

### Pre-Power Tests

1. **Visual Inspection:**
   - Check all solder joints
   - Verify no shorts between adjacent pins
   - Confirm polarity markings
   - Check wire gauge is appropriate

2. **Continuity Tests:**
   ```
   Test with multimeter (continuity mode):
   ☐ Battery+ to ESP32 VIN (through buck converter)
   ☐ All GND points to each other
   ☐ GPIO25 to MOSFET Gate (through resistor)
   ☐ GPIO26 to MOSFET Gate (through resistor)
   ☐ GPIO34 to voltage divider junction
   ☐ GPIO35 to temperature sensor output
   ☐ GPIO4 to motion sensor output
   ```

3. **Resistance Tests:**
   ```
   Measure with multimeter (resistance mode):
   ☐ Voltage divider total: ~57kΩ
   ☐ Gate resistor: ~100Ω
   ☐ Pull-down resistor: ~10kΩ
   ☐ No shorts: Battery+ to GND should be > 1MΩ
   ```

4. **Check for shorts:**
   ```
   ☐ Battery+ to Battery- (should be open/infinite)
   ☐ Drain to Source (should be open when off)
   ☐ GPIO pins to GND (should be > 10kΩ)
   ☐ 3.3V to GND (should be > 10kΩ)
   ```

### Powered Tests (Low Voltage)

**Start with 5V power supply (NOT battery):**

1. **Power up ESP32 via USB:**
   - Confirm power LED lights
   - Upload and run serial monitor test
   - Verify "Hello World" output

2. **Test voltage divider:**
   - Connect 5V to voltage divider input
   - Measure output: Should be ~0.88V
   - Connect to GPIO34
   - Read ADC value in code
   - Verify calculated voltage is ~5V

3. **Test temperature sensor:**
   - Power sensor with 3.3V
   - Measure output voltage
   - Should be ~750mV at 25°C (TMP36)
   - Read ADC value
   - Verify temperature reading is reasonable

4. **Test motion sensor:**
   - Power sensor
   - Wait 1 minute for calibration
   - Wave hand in front
   - Verify GPIO4 reads HIGH
   - Check serial output for motion detection

### Powered Tests (Full Voltage)

**CAUTION: Use current-limited power supply or battery with fuse**

1. **Initial power-up:**
   - Set power supply to 12V, current limit 1A
   - Connect to system
   - Monitor current draw (should be < 200mA)
   - Check for excessive heating

2. **Test PWM output:**
   - Connect oscilloscope to GPIO25
   - Send PWM commands from CLI
   - Verify frequency: 5000Hz ±5%
   - Verify duty cycle matches commands

3. **Test MOSFET switching:**
   - Connect LED strip or resistive load
   - Send ON command
   - Verify MOSFET conducts
   - Measure voltage drop across MOSFET (should be < 0.2V)
   - Send OFF command
   - Verify MOSFET blocks

4. **Test under load:**
   - Connect actual load
   - Measure current draw
   - Verify MOSFET doesn't overheat
   - Check PWM dimming works correctly
   - Run for 10 minutes, monitor temperature

### System Integration Tests

1. **End-to-end test:**
   ```
   ☐ Battery voltage reading accurate (±2%)
   ☐ Temperature reading reasonable
   ☐ Motion detection works
   ☐ Thresholds trigger correctly
   ☐ PWM dimming smooth
   ☐ CLI commands work
   ☐ Configuration saves to NVS
   ```

2. **Stress test:**
   - Vary battery voltage (11V - 14V)
   - Trigger motion repeatedly
   - Change temperature
   - Run for 24 hours
   - Monitor for errors

## Troubleshooting

### No Power

**Symptom:** ESP32 doesn't turn on

**Checks:**
- Verify USB cable carries data
- Check buck converter output voltage
- Confirm fuse not blown
- Test battery voltage
- Check polarity (reverse destroys components)

### Incorrect Battery Voltage Reading

**Symptom:** Battery voltage reading is wrong or jumps around

**Causes and Solutions:**

| Reading | Likely Cause | Solution |
|---------|-------------|----------|
| 0V | Not connected | Check ADC pin connection |
| Very high | Wrong resistors | Verify R1=47k, R2=10k |
| Very low | Resistors swapped | Swap R1 and R2 |
| Noisy | Missing capacitor | Add 100nF cap |
| Unstable | Long wires | Shorten wires, add filtering |

### MOSFET Won't Turn On

**Symptom:** Load doesn't activate

**Debug steps:**

1. Check MOSFET type (must be logic-level)
2. Measure gate voltage: Should be 3.3V when ON
3. Measure source voltage: Should be 0V (GND)
4. Measure drain-source resistance:
   - OFF: Should be >1MΩ
   - ON: Should be <1Ω
5. Check load connection
6. Verify ground connections

### MOSFET Won't Turn Off

**Symptom:** Load stays on permanently

**Debug steps:**

1. Check pull-down resistor (10kΩ)
2. Verify GPIO is set to OUTPUT mode
3. Measure gate voltage: Should be 0V when OFF
4. Check for solder bridges
5. Replace MOSFET if damaged

### Temperature Reading Incorrect

**Symptom:** Shows -40°C or 125°C constantly

**Debug steps:**

1. Verify sensor power (3.3V or 5V)
2. Check pinout (easy to reverse)
3. Measure output voltage:
   - Should be ~750mV at 25°C for TMP36
4. Test sensor with multimeter
5. Replace if faulty

### Motion Sensor Not Working

**Symptom:** No detection or false triggers

**Debug steps:**

1. Wait 60 seconds after power-up (calibration)
2. Adjust sensitivity potentiometer
3. Check output voltage (should pulse HIGH)
4. Verify pull-down resistor installed
5. Test in dark environment
6. Check mounting (needs clear field of view)

### PWM Flickering

**Symptom:** Load flickers or buzzes

**Causes and Solutions:**

- **Frequency too low:** Verify 5kHz in code
- **Duty cycle too low:** Increase minimum duty
- **MOSFET ringing:** Add gate resistor (100Ω)
- **Ground loops:** Star ground configuration
- **Load incompatible:** Some LEDs require DC

### System Crashes

**Symptom:** ESP32 reboots randomly

**Common causes:**

1. **Insufficient power:**
   - Check buck converter capacity
   - Measure voltage under load
   - Add capacitor (1000µF) on VIN

2. **Brown-out:**
   - Occurs when battery voltage drops
   - Enable brown-out detection
   - Lower voltage threshold in menuconfig

3. **Stack overflow:**
   - Increase task stack sizes
   - Check for recursive functions
   - Monitor heap usage

4. **Watchdog timeout:**
   - Tasks blocked too long
   - Add vTaskDelay() in loops
   - Check for mutex deadlocks

## Additional Resources

### Wire Color Coding

Recommended wire colors for safety:

| Connection | Color | Notes |
|------------|-------|-------|
| Battery+ | Red | Always red |
| Battery- / GND | Black | Always black |
| 5V | Red | Or orange |
| 3.3V | Orange | Or yellow |
| Signal wires | Various | Use labels |
| PWM outputs | Blue/Green | Document well |

### Connector Types

Recommended connectors:

- **Power:** XT60 or Anderson Powerpole
- **Signals:** Dupont 2.54mm
- **Sensors:** JST-XH or JST-PH
- **Loads:** Screw terminals (rated for current)

### Documentation

Label everything:

1. Wire labels at both ends
2. Terminal block labels
3. Fuse ratings
4. Load specifications
5. Date of installation
6. Voltage levels

### Maintenance Schedule

- **Monthly:** Visual inspection, clean dust
- **Quarterly:** Check connections, test under load
- **Annually:** Replace fuses, check MOSFET heatsinks

---

**Document Version:** 1.0  
**Last Updated:** October 8, 2025  
**Author:** Zhanibekuly Darkhan