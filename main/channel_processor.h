#ifndef CHANNEL_PROCESSOR_H
#define CHANNEL_PROCESSOR_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"

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

// Processing task per channel
void channel_proc_task(void *pvParameters);

#endif