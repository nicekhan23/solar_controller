/**
 * @file test_config.h
 * @brief Testing configuration for minimal hardware setup
 * 
 * Use this configuration when testing with only ESP32 and basic components
 * without full hardware (PIR, temperature sensor, charger IC, etc.)
 */

#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

// Enable test mode
#define TEST_MODE_ENABLED 1

#if TEST_MODE_ENABLED

// ============================================================================
// ADC Test Configuration
// ============================================================================

// Use simplified voltage divider (no real battery)
#define TEST_DIVIDER_RATIO  2.0f  // For 3.3V->1.65V test divider

// Simulate battery voltage range for testing
#define TEST_MIN_VOLTAGE_MV  10000  // 10.0V simulated
#define TEST_MAX_VOLTAGE_MV  14000  // 14.0V simulated

// Map ADC reading to simulated battery voltage
// ADC reads 0-1800mV, we map to 10-14V battery range
static inline uint32_t test_map_voltage(uint32_t adc_mv) {
    // Map 0-1800mV ADC to 10000-14000mV battery
    if (adc_mv < 200) adc_mv = 200;
    if (adc_mv > 1800) adc_mv = 1800;
    return 10000 + ((adc_mv - 200) * 4000) / 1600;
}

// ============================================================================
// Temperature Test Configuration
// ============================================================================

// Use fixed temperature when sensor not available
#define TEST_USE_FIXED_TEMP 1
#define TEST_FIXED_TEMP_C   25.0f

// Or simulate temperature from ADC voltage
static inline float test_map_temperature(uint32_t adc_mv) {
    // Map 200-1800mV to 15-35°C range
    if (adc_mv < 200) adc_mv = 200;
    if (adc_mv > 1800) adc_mv = 1800;
    return 15.0f + ((float)(adc_mv - 200) * 20.0f) / 1600.0f;
}

// ============================================================================
// GPIO Test Configuration
// ============================================================================

// Motion sensor - use button on GPIO4
#define TEST_MOTION_BUTTON_ENABLED 1

// Simplified LED outputs (no optocouplers/drivers)
#define TEST_DIRECT_LED_OUTPUT 1

// Charger status - use switch/jumper on GPIO27
#define TEST_CHARGER_SWITCH_ENABLED 1

// ============================================================================
// Threshold Test Configuration
// ============================================================================

// Lower thresholds to match test voltage range
#define TEST_CH0_TH_ON   12000  // 12.0V
#define TEST_CH0_TH_OFF  11000  // 11.0V
#define TEST_CH1_TH_ON   12500  // 12.5V
#define TEST_CH1_TH_OFF  11500  // 11.5V

// ============================================================================
// PWM Test Configuration
// ============================================================================

// Reduced PWM frequency for easier measurement
#define TEST_PWM_FREQUENCY  1000  // 1kHz instead of 5kHz

// Dimming test levels
#define TEST_PWM_FULL_DUTY  100   // 100%
#define TEST_PWM_HALF_DUTY  50    // 50%
#define TEST_PWM_LOW_DUTY   25    // 25%

// ============================================================================
// Serial Debug Configuration
// ============================================================================

// Enable verbose logging for testing
#define TEST_VERBOSE_LOGGING 1

// Faster status updates for testing
#define TEST_STATUS_INTERVAL_MS  2000  // 2 seconds instead of 5

// ============================================================================
// Hardware Pin Mapping for Test Board
// ============================================================================

// ADC Inputs (voltage dividers with potentiometers)
#define TEST_ADC_BATTERY_PIN    GPIO_NUM_34  // ADC1_CH6
#define TEST_ADC_TEMP_PIN       GPIO_NUM_35  // ADC1_CH7

// PWM LED Outputs (direct with current-limiting resistors)
#define TEST_LED_CH0_PIN        GPIO_NUM_25
#define TEST_LED_CH1_PIN        GPIO_NUM_26

// Digital Inputs
#define TEST_MOTION_BUTTON_PIN  GPIO_NUM_4   // Pull-down, button to 3.3V
#define TEST_CHARGER_SWITCH_PIN GPIO_NUM_27  // Switch to 3.3V or GND

// Optional test LED for system status
#define TEST_STATUS_LED_PIN     GPIO_NUM_2   // Built-in LED on many boards

#endif // TEST_MODE_ENABLED

#endif // TEST_CONFIG_H