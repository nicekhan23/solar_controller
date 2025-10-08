/**
 * @file control_handler.h
 * @brief Hardware control interface for PWM outputs and GPIO
 * 
 * Manages hardware outputs including LED PWM control, motion sensor input,
 * and battery-based dimming logic.
 */

#ifndef CONTROL_HANDLER_H
#define CONTROL_HANDLER_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/**
 * @struct hw_control_t
 * @brief Hardware control state structure
 * 
 * Represents the current state of all hardware outputs including
 * channel states, PWM duty cycle, and motion detection status.
 */
typedef struct {
    bool ch0_state;
    bool ch1_state;
    uint8_t pwm_duty;  // 0-100%
    bool motion_detected;
} hw_control_t;

/**
 * @brief Initialize control subsystem
 * 
 * Configures hardware peripherals:
 * - LEDC (PWM) timers and channels for LED control
 * - Motion sensor GPIO with interrupt
 * - Charger status GPIO input
 * - Creates mutex for thread-safe hardware access
 */
void control_init(void);

/**
 * @brief Control task
 * @param pvParameters Task parameters (unused)
 * 
 * Main hardware control loop that:
 * - Receives commands from channel processors
 * - Monitors battery voltage for dimming decisions
 * - Handles motion sensor timeout
 * - Applies PWM duty cycles to hardware outputs
 * - Logs periodic status updates
 * 
 * @note Runs continuously at 100ms intervals
 */
void control_task(void *pvParameters);

// Mutex for hardware access
extern SemaphoreHandle_t hw_mutex;

/**
 * @brief Get current hardware state
 * @param state Pointer to hw_control_t structure to fill
 * 
 * Thread-safe query of current hardware state protected by mutex.
 * Used by CLI and status reporting functions.
 */
void control_get_state(hw_control_t *state);

/**
 * @brief Force motion detection trigger
 * 
 * Manually activates motion detection for testing purposes.
 * Lights will remain at full brightness for the configured timeout period.
 */
void control_trigger_motion(void);

/**
 * @brief Get charger status
 * @return true if charging, false otherwise
 * 
 * Reads the charger status GPIO pin. Typically HIGH indicates
 * charging in progress, LOW indicates not charging.
 */
bool control_get_charger_status(void);


/**
 * @brief Emergency shutdown
 * 
 * Immediately turns off all outputs by setting PWM duty to 0%.
 * Thread-safe operation protected by mutex. Used for critical
 * battery conditions or emergency stop commands.
 */
void control_emergency_shutdown(void);

#endif