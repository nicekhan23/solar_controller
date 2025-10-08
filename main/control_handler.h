#ifndef CONTROL_HANDLER_H
#define CONTROL_HANDLER_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Hardware control structure
typedef struct {
    bool ch0_state;
    bool ch1_state;
    uint8_t pwm_duty;  // 0-100%
    bool motion_detected;
} hw_control_t;

// Initialize hardware (GPIO, LEDC/PWM)
void control_init(void);

// Control task
void control_task(void *pvParameters);

// Mutex for hardware access
extern SemaphoreHandle_t hw_mutex;

#endif