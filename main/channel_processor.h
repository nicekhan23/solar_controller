/**
 * @file channel_processor.h
 * @brief Channel processing logic with hysteresis and temperature compensation
 * 
 * Implements per-channel battery voltage monitoring with moving average filtering,
 * hysteresis control, temperature compensation, and state debouncing.
 */

#ifndef CHANNEL_PROCESSOR_H
#define CHANNEL_PROCESSOR_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @struct channel_config_t
 * @brief Channel configuration parameters
 * 
 * Defines the operational parameters for a single channel including
 * voltage thresholds and temperature compensation coefficient.
 */
typedef struct {
    int channel_id;
    int th_on_mv;       // Voltage threshold ON
    int th_off_mv;      // Voltage threshold OFF
    float temp_coeff;   // Temperature coefficient
} channel_config_t;

/**
 * @struct channel_state_t
 * @brief Current channel state
 * 
 * Tracks the current output state, filtered voltage reading,
 * and timestamp of the last state change for debouncing.
 */
typedef struct {
    bool output_state;
    int filtered_voltage;
    uint32_t last_change_time;
} channel_state_t;

/**
 * @struct channel_command_t
 * @brief Command structure sent to control task
 * 
 * Contains the desired output state and associated voltage information
 * to be applied by the hardware control task.
 */
typedef struct {
    int channel_id;
    bool output_state;
    int32_t filtered_voltage;
    uint32_t timestamp_ms;
} channel_command_t;

// Command queues (one per channel)
extern QueueHandle_t ch0_command_queue;
extern QueueHandle_t ch1_command_queue;

/**
 * @brief Channel processing task
 * @param pvParameters Pointer to channel_config_t structure
 * 
 * Processes ADC readings for a single channel:
 * - Applies moving average filter (1.6s window)
 * - Implements hysteresis logic
 * - Applies temperature compensation to thresholds
 * - Enforces minimum 5-second state change debounce
 * - Sends commands to control task
 * 
 * @note One instance of this task should run per channel
 */
void channel_proc_task(void *pvParameters);

/**
 * @brief Initialize channel processor subsystem
 * 
 * Creates command queues for communication with the control task.
 * Must be called before creating channel processing tasks.
 */
void channel_processor_init(void);

/**
 * @brief Get current channel state
 * @param channel_id Channel identifier (0 or 1)
 * @return true if output is ON, false if OFF
 * 
 * Non-blocking query of current channel output state by peeking
 * at the most recent command in the queue.
 */
bool channel_get_state(int channel_id);

/**
 * @brief Get filtered voltage for a channel
 * @param channel_id Channel identifier (0 or 1)
 * @return Filtered voltage in millivolts (mV), or 0 if unavailable
 * 
 * Non-blocking query of the current filtered voltage value
 * after moving average processing.
 */
int32_t channel_get_filtered_voltage(int channel_id);

#endif