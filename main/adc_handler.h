#ifndef ADC_HANDLER_H
#define ADC_HANDLER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// ADC reading structure
typedef struct {
    uint32_t battery_voltage_mv;
    uint32_t temperature_raw;
    uint32_t timestamp_ms;
} adc_reading_t;

// Initialize ADC
void adc_init(void);

// ADC task
void adc_task(void *pvParameters);

// Queues for each channel
extern QueueHandle_t adc_queue_ch0;
extern QueueHandle_t adc_queue_ch1;

// Immediate (blocking) reads for status queries
uint32_t adc_get_battery_voltage_now(void);
float adc_get_temperature_now(void);

// Cleanup
void adc_deinit(void);

#endif