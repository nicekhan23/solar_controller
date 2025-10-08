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

// Get current hardware state
void control_get_state(hw_control_t *state);

// Manual motion trigger (testing)
void control_trigger_motion(void);

// Get charger status
bool control_get_charger_status(void);

// Emergency shutdown
void control_emergency_shutdown(void);

#endif