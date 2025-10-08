#include "control_handler.h"
#include "channel_processor.h"
#include "adc_handler.h"
#include "nvs_storage.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "CONTROL";

// GPIO pin definitions
#define GPIO_LED_CH0        GPIO_NUM_25  // PWM output for Channel 0
#define GPIO_LED_CH1        GPIO_NUM_26  // PWM output for Channel 1
#define GPIO_MOTION_SENSOR  GPIO_NUM_4   // Motion sensor input
#define GPIO_CHARGER_STATUS GPIO_NUM_27  // Optional: charger status input

// LEDC (PWM) configuration
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CH0_CHANNEL        LEDC_CHANNEL_0
#define LEDC_CH1_CHANNEL        LEDC_CHANNEL_1
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT  // 13-bit resolution (0-8191)
#define LEDC_FREQUENCY          5000               // 5 kHz
#define LEDC_MAX_DUTY           8191               // (2^13 - 1)

// Battery levels for dimming logic (in mV)
#define BATTERY_FULL_THRESHOLD      13500  // 13.5V - full operation
#define BATTERY_HALF_THRESHOLD      12000  // 12.0V - half brightness
#define BATTERY_CRITICAL_THRESHOLD  11000  // 11.0V - shut off loads

// Motion sensor configuration
#define MOTION_TIMEOUT_MS           30000  // 30 seconds full brightness after motion
#define MOTION_DEBOUNCE_MS          500    // 500ms debounce

// Mutex for hardware access
SemaphoreHandle_t hw_mutex = NULL;

// Hardware state
static hw_control_t hw_state = {
    .ch0_state = false,
    .ch1_state = false,
    .pwm_duty = 0,
    .motion_detected = false
};

// Motion detection state
static volatile bool motion_active = false;
static volatile uint32_t last_motion_time = 0;

/**
 * @brief GPIO ISR handler for motion sensor
 */
static void IRAM_ATTR motion_sensor_isr_handler(void *arg)
{
    uint32_t now = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;
    
    // Simple debounce
    if (now - last_motion_time > MOTION_DEBOUNCE_MS) {
        motion_active = true;
        last_motion_time = now;
    }
}

/**
 * @brief Initialize LEDC (PWM) for LED control
 */
static void ledc_init(void)
{
    ESP_LOGI(TAG, "Initializing LEDC/PWM");
    
    // Configure timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return;
    }
    
    // Configure Channel 0
    ledc_channel_config_t ledc_ch0 = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CH0_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = GPIO_LED_CH0,
        .duty           = 0,  // Start with 0% duty
        .hpoint         = 0
    };
    ret = ledc_channel_config(&ledc_ch0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC CH0: %s", esp_err_to_name(ret));
        return;
    }
    
    // Configure Channel 1
    ledc_channel_config_t ledc_ch1 = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CH1_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = GPIO_LED_CH1,
        .duty           = 0,  // Start with 0% duty
        .hpoint         = 0
    };
    ret = ledc_channel_config(&ledc_ch1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC CH1: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "LEDC initialized: CH0=GPIO%d, CH1=GPIO%d, freq=%dHz",
             GPIO_LED_CH0, GPIO_LED_CH1, LEDC_FREQUENCY);
}

/**
 * @brief Initialize motion sensor GPIO
 */
static void motion_sensor_init(void)
{
    ESP_LOGI(TAG, "Initializing motion sensor on GPIO%d", GPIO_MOTION_SENSOR);
    
    // Configure GPIO as input with pull-down
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_MOTION_SENSOR),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_POSEDGE  // Trigger on rising edge
    };
    gpio_config(&io_conf);
    
    // Install GPIO ISR service
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_MOTION_SENSOR, motion_sensor_isr_handler, NULL);
    
    ESP_LOGI(TAG, "Motion sensor initialized");
}

/**
 * @brief Convert percentage (0-100) to LEDC duty cycle
 */
static uint32_t percent_to_duty(uint8_t percent)
{
    if (percent > 100) percent = 100;
    return (uint32_t)((LEDC_MAX_DUTY * percent) / 100);
}

/**
 * @brief Set PWM duty cycle for a channel
 */
static void set_pwm_duty(ledc_channel_t channel, uint8_t duty_percent)
{
    uint32_t duty = percent_to_duty(duty_percent);
    
    esp_err_t ret = ledc_set_duty(LEDC_MODE, channel, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty: %s", esp_err_to_name(ret));
        return;
    }
    
    ret = ledc_update_duty(LEDC_MODE, channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief Determine dimming level based on battery voltage
 * Returns duty cycle percentage (0-100)
 */
static uint8_t calculate_dimming_level(uint32_t battery_mv, bool motion_override)
{
    // Motion override: always full brightness
    if (motion_override) {
        return nvs_get_pwm_full_duty();
    }
    
    // Battery-based dimming
    if (battery_mv >= BATTERY_FULL_THRESHOLD) {
        // Full brightness - battery is healthy
        return nvs_get_pwm_full_duty();
    } else if (battery_mv >= BATTERY_HALF_THRESHOLD) {
        // Half brightness - conserve battery
        return nvs_get_pwm_half_duty();
    } else if (battery_mv >= BATTERY_CRITICAL_THRESHOLD) {
        // Quarter brightness - very low battery
        return nvs_get_pwm_half_duty() / 2;
    } else {
        // Turn off - critical battery level
        return 0;
    }
}

/**
 * @brief Check if motion timeout has expired
 */
static bool check_motion_timeout(void)
{
    if (!motion_active) {
        return false;
    }
    
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t motion_timeout = nvs_get_motion_timeout();
    
    if (now - last_motion_time > motion_timeout) {
        motion_active = false;
        ESP_LOGI(TAG, "Motion timeout expired");
        return false;
    }
    
    return true;
}

/**
 * @brief Apply hardware control commands with mutex protection
 */
static void apply_hardware_control(bool ch0_enable, bool ch1_enable, uint8_t duty_percent)
{
    // Take mutex with timeout
    if (xSemaphoreTake(hw_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire hw_mutex");
        return;
    }
    
    // Update hardware state
    hw_state.ch0_state = ch0_enable;
    hw_state.ch1_state = ch1_enable;
    hw_state.pwm_duty = duty_percent;
    
    // Apply Channel 0
    if (ch0_enable) {
        set_pwm_duty(LEDC_CH0_CHANNEL, duty_percent);
    } else {
        set_pwm_duty(LEDC_CH0_CHANNEL, 0);
    }
    
    // Apply Channel 1
    if (ch1_enable) {
        set_pwm_duty(LEDC_CH1_CHANNEL, duty_percent);
    } else {
        set_pwm_duty(LEDC_CH1_CHANNEL, 0);
    }
    
    // Release mutex
    xSemaphoreGive(hw_mutex);
}

/**
 * @brief Initialize control subsystem
 */
void control_init(void)
{
    ESP_LOGI(TAG, "Initializing control handler");
    
    // Create mutex
    hw_mutex = xSemaphoreCreateMutex();
    if (hw_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create hardware mutex");
        return;
    }
    
    // Initialize PWM
    ledc_init();
    
    // Initialize motion sensor
    motion_sensor_init();
    
    // Initialize charger status GPIO (optional)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_CHARGER_STATUS),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    ESP_LOGI(TAG, "Control handler initialized");
}

/**
 * @brief Control task - processes commands and applies hardware control
 */
void control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Control task started");
    
    channel_command_t ch0_cmd = {0};
    channel_command_t ch1_cmd = {0};
    
    uint32_t last_log_time = 0;
    
    while (1) {
        bool ch0_updated = false;
        bool ch1_updated = false;
        
        // Check for Channel 0 commands
        if (xQueueReceive(ch0_command_queue, &ch0_cmd, pdMS_TO_TICKS(10)) == pdTRUE) {
            ch0_updated = true;
        }
        
        // Check for Channel 1 commands
        if (xQueueReceive(ch1_command_queue, &ch1_cmd, pdMS_TO_TICKS(10)) == pdTRUE) {
            ch1_updated = true;
        }
        
        // Get current battery voltage for dimming calculation
        uint32_t battery_mv = adc_get_battery_voltage_now();
        
        // Check motion sensor timeout
        bool motion_override = check_motion_timeout();
        hw_state.motion_detected = motion_override;
        
        // Calculate dimming level
        uint8_t duty_percent = calculate_dimming_level(battery_mv, motion_override);
        
        // Apply control based on channel commands and battery level
        bool ch0_enable = ch0_cmd.output_state && (duty_percent > 0);
        bool ch1_enable = ch1_cmd.output_state && (duty_percent > 0);
        
        // Apply hardware changes
        if (ch0_updated || ch1_updated || motion_override) {
            apply_hardware_control(ch0_enable, ch1_enable, duty_percent);
        }
        
        // Periodic logging (every 5 seconds)
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now - last_log_time > 5000) {
            ESP_LOGI(TAG, "Status: CH0=%s, CH1=%s, Duty=%d%%, Battery=%umV, Motion=%s",
                     ch0_enable ? "ON" : "OFF",
                     ch1_enable ? "ON" : "OFF",
                     duty_percent,
                     (unsigned int)battery_mv,
                     motion_override ? "ACTIVE" : "idle");
            last_log_time = now;
        }
        
        // Task delay
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Get current hardware state (for status queries)
 */
void control_get_state(hw_control_t *state)
{
    if (state == NULL) return;
    
    if (xSemaphoreTake(hw_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        *state = hw_state;
        xSemaphoreGive(hw_mutex);
    }
}

/**
 * @brief Force motion detection (for testing)
 */
void control_trigger_motion(void)
{
    motion_active = true;
    last_motion_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ESP_LOGI(TAG, "Motion triggered manually");
}

/**
 * @brief Get charger status (if connected)
 */
bool control_get_charger_status(void)
{
    // Read charger status pin
    // Typically: HIGH = charging, LOW = not charging
    return gpio_get_level(GPIO_CHARGER_STATUS);
}

/**
 * @brief Emergency shutdown (turn off all outputs)
 */
void control_emergency_shutdown(void)
{
    ESP_LOGW(TAG, "EMERGENCY SHUTDOWN");
    
    if (xSemaphoreTake(hw_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Turn off all outputs
        set_pwm_duty(LEDC_CH0_CHANNEL, 0);
        set_pwm_duty(LEDC_CH1_CHANNEL, 0);
        
        hw_state.ch0_state = false;
        hw_state.ch1_state = false;
        hw_state.pwm_duty = 0;
        
        xSemaphoreGive(hw_mutex);
    }
}