/**
 * @file adc_handler.h
 * @brief ADC sampling and battery monitoring interface
 * 
 * Provides ADC sampling for battery voltage and temperature monitoring
 * with hardware calibration, oversampling, and voltage divider compensation.
 */
#ifndef ADC_HANDLER_H
#define ADC_HANDLER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @struct adc_reading_t
 * @brief ADC reading data structure
 * 
 * Contains battery voltage, temperature, and timestamp information
 * for a single ADC sampling event.
 */
typedef struct {
    uint32_t battery_voltage_mv;
    uint32_t temperature_raw;
    uint32_t timestamp_ms;
} adc_reading_t;

/**
 * @brief Initialize ADC subsystem
 * 
 * Configures ADC1 with two channels:
 * - Channel 6 (GPIO34): Battery voltage through voltage divider
 * - Channel 7 (GPIO35): Temperature sensor (TMP36)
 * 
 * Initializes hardware calibration if available and creates queues
 * for distributing readings to channel processors.
 */
void adc_init(void);

/**
 * @brief ADC sampling task
 * @param pvParameters Task parameters (unused)
 * 
 * Periodically samples battery voltage and temperature at 100ms intervals.
 * Applies oversampling for noise reduction and pushes readings to
 * per-channel queues for processing.
 * 
 * @note This task runs continuously and should be created with appropriate priority
 */
void adc_task(void *pvParameters);

// Queues for each channel
extern QueueHandle_t adc_queue_ch0;
extern QueueHandle_t adc_queue_ch1;

/**
 * @brief Get current battery voltage (blocking read)
 * @return Battery voltage in millivolts (mV)
 * 
 * Performs immediate ADC read with oversampling and voltage divider
 * compensation. Useful for status queries and CLI commands.
 */
uint32_t adc_get_battery_voltage_now(void);

/**
 * @brief Get current temperature (blocking read)
 * @return Temperature in degrees Celsius
 * 
 * Performs immediate ADC read and converts to temperature using
 * TMP36 sensor formula. Returns 25.0°C on error.
 */
float adc_get_temperature_now(void);

/**
 * @brief Cleanup ADC resources
 * 
 * Releases ADC calibration handles, deletes ADC unit, and destroys queues.
 * Should be called before system shutdown or reset.
 */
void adc_deinit(void);

#endif