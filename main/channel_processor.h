#ifndef CHANNEL_PROCESSOR_H
#define CHANNEL_PROCESSOR_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Channel configuration
typedef struct {
    int channel_id;
    int th_on_mv;       // Voltage threshold ON
    int th_off_mv;      // Voltage threshold OFF
    float temp_coeff;   // Temperature coefficient
} channel_config_t;

// Channel state
typedef struct {
    bool output_state;
    int filtered_voltage;
    uint32_t last_change_time;
} channel_state_t;

// Command structure sent to control task
typedef struct {
    int channel_id;
    bool output_state;
    int32_t filtered_voltage;
    uint32_t timestamp_ms;
} channel_command_t;

// Command queues (one per channel)
extern QueueHandle_t ch0_command_queue;
extern QueueHandle_t ch1_command_queue;

// Processing task per channel
void channel_proc_task(void *pvParameters);

// Initialize channel processor
void channel_processor_init(void);

// Get current state (for status queries)
bool channel_get_state(int channel_id);
int32_t channel_get_filtered_voltage(int channel_id);

#endif