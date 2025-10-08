#include "nvs_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NVS_STORAGE";

// NVS namespace for our application
#define NVS_NAMESPACE "solar_ctrl"

// Configuration keys
#define KEY_CH0_TH_ON       "ch0_th_on"
#define KEY_CH0_TH_OFF      "ch0_th_off"
#define KEY_CH1_TH_ON       "ch1_th_on"
#define KEY_CH1_TH_OFF      "ch1_th_off"
#define KEY_TEMP_COEFF      "temp_coeff"
#define KEY_PWM_HALF_DUTY   "pwm_half"
#define KEY_PWM_FULL_DUTY   "pwm_full"
#define KEY_MOTION_TIMEOUT  "motion_to"

// Verification data keys
#define KEY_TOTAL_CYCLES    "tot_cycles"
#define KEY_LAST_VOLTAGE    "last_volt"
#define KEY_UPTIME_HOURS    "uptime_hrs"
#define KEY_CHARGE_CYCLES   "chg_cycles"

// Default configuration values (in mV)
#define DEFAULT_CH0_TH_ON    12500  // 12.5V turn on
#define DEFAULT_CH0_TH_OFF   11800  // 11.8V turn off
#define DEFAULT_CH1_TH_ON    12500
#define DEFAULT_CH1_TH_OFF   11800
#define DEFAULT_TEMP_COEFF   -0.02f
#define DEFAULT_PWM_HALF     50     // 50% duty
#define DEFAULT_PWM_FULL     100    // 100% duty
#define DEFAULT_MOTION_TO    30000  // 30 seconds in ms

// Global configuration structure
typedef struct {
    int32_t ch0_th_on_mv;
    int32_t ch0_th_off_mv;
    int32_t ch1_th_on_mv;
    int32_t ch1_th_off_mv;
    float temp_coefficient;
    uint8_t pwm_half_duty;
    uint8_t pwm_full_duty;
    uint32_t motion_timeout_ms;
} app_config_t;

static app_config_t g_config;

/**
 * @brief Initialize NVS flash
 */
void nvs_init(void)
{
    ESP_LOGI(TAG, "Initializing NVS");
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated or version changed
        ESP_LOGW(TAG, "NVS partition issue, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "NVS initialized successfully");
}

/**
 * @brief Load configuration from NVS, use defaults if not found
 */
void nvs_load_config(void)
{
    nvs_handle_t handle;
    esp_err_t err;
    
    ESP_LOGI(TAG, "Loading configuration from NVS");
    
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, using defaults");
        // Set defaults
        g_config.ch0_th_on_mv = DEFAULT_CH0_TH_ON;
        g_config.ch0_th_off_mv = DEFAULT_CH0_TH_OFF;
        g_config.ch1_th_on_mv = DEFAULT_CH1_TH_ON;
        g_config.ch1_th_off_mv = DEFAULT_CH1_TH_OFF;
        g_config.temp_coefficient = DEFAULT_TEMP_COEFF;
        g_config.pwm_half_duty = DEFAULT_PWM_HALF;
        g_config.pwm_full_duty = DEFAULT_PWM_FULL;
        g_config.motion_timeout_ms = DEFAULT_MOTION_TO;
        return;
    }
    
    // Load each configuration item, use default if not found
    int32_t val_i32;
    uint8_t val_u8;
    uint32_t val_u32;
    
    // Channel 0 thresholds
    if (nvs_get_i32(handle, KEY_CH0_TH_ON, &val_i32) == ESP_OK) {
        g_config.ch0_th_on_mv = val_i32;
    } else {
        g_config.ch0_th_on_mv = DEFAULT_CH0_TH_ON;
    }
    
    if (nvs_get_i32(handle, KEY_CH0_TH_OFF, &val_i32) == ESP_OK) {
        g_config.ch0_th_off_mv = val_i32;
    } else {
        g_config.ch0_th_off_mv = DEFAULT_CH0_TH_OFF;
    }
    
    // Channel 1 thresholds
    if (nvs_get_i32(handle, KEY_CH1_TH_ON, &val_i32) == ESP_OK) {
        g_config.ch1_th_on_mv = val_i32;
    } else {
        g_config.ch1_th_on_mv = DEFAULT_CH1_TH_ON;
    }
    
    if (nvs_get_i32(handle, KEY_CH1_TH_OFF, &val_i32) == ESP_OK) {
        g_config.ch1_th_off_mv = val_i32;
    } else {
        g_config.ch1_th_off_mv = DEFAULT_CH1_TH_OFF;
    }
    
    // Temperature coefficient (stored as int32, convert to float)
    if (nvs_get_i32(handle, KEY_TEMP_COEFF, &val_i32) == ESP_OK) {
        g_config.temp_coefficient = (float)val_i32 / 1000.0f;
    } else {
        g_config.temp_coefficient = DEFAULT_TEMP_COEFF;
    }
    
    // PWM duties
    if (nvs_get_u8(handle, KEY_PWM_HALF_DUTY, &val_u8) == ESP_OK) {
        g_config.pwm_half_duty = val_u8;
    } else {
        g_config.pwm_half_duty = DEFAULT_PWM_HALF;
    }
    
    if (nvs_get_u8(handle, KEY_PWM_FULL_DUTY, &val_u8) == ESP_OK) {
        g_config.pwm_full_duty = val_u8;
    } else {
        g_config.pwm_full_duty = DEFAULT_PWM_FULL;
    }
    
    // Motion timeout
    if (nvs_get_u32(handle, KEY_MOTION_TIMEOUT, &val_u32) == ESP_OK) {
        g_config.motion_timeout_ms = val_u32;
    } else {
        g_config.motion_timeout_ms = DEFAULT_MOTION_TO;
    }
    
    nvs_close(handle);
    
    ESP_LOGI(TAG, "Configuration loaded:");
    ESP_LOGI(TAG, "  CH0: ON=%d mV, OFF=%d mV", g_config.ch0_th_on_mv, g_config.ch0_th_off_mv);
    ESP_LOGI(TAG, "  CH1: ON=%d mV, OFF=%d mV", g_config.ch1_th_on_mv, g_config.ch1_th_off_mv);
    ESP_LOGI(TAG, "  Temp coeff: %.3f", g_config.temp_coefficient);
    ESP_LOGI(TAG, "  PWM: half=%d%%, full=%d%%", g_config.pwm_half_duty, g_config.pwm_full_duty);
    ESP_LOGI(TAG, "  Motion timeout: %u ms", (unsigned int)g_config.motion_timeout_ms);
}

/**
 * @brief Save current configuration to NVS
 */
void nvs_save_config(void)
{
    nvs_handle_t handle;
    esp_err_t err;
    
    ESP_LOGI(TAG, "Saving configuration to NVS");
    
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(err));
        return;
    }
    
    // Save all configuration items
    nvs_set_i32(handle, KEY_CH0_TH_ON, g_config.ch0_th_on_mv);
    nvs_set_i32(handle, KEY_CH0_TH_OFF, g_config.ch0_th_off_mv);
    nvs_set_i32(handle, KEY_CH1_TH_ON, g_config.ch1_th_on_mv);
    nvs_set_i32(handle, KEY_CH1_TH_OFF, g_config.ch1_th_off_mv);
    
    // Convert float to int32 for storage (multiply by 1000)
    int32_t temp_coeff_i32 = (int32_t)(g_config.temp_coefficient * 1000.0f);
    nvs_set_i32(handle, KEY_TEMP_COEFF, temp_coeff_i32);
    
    nvs_set_u8(handle, KEY_PWM_HALF_DUTY, g_config.pwm_half_duty);
    nvs_set_u8(handle, KEY_PWM_FULL_DUTY, g_config.pwm_full_duty);
    nvs_set_u32(handle, KEY_MOTION_TIMEOUT, g_config.motion_timeout_ms);
    
    // Commit changes
    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Configuration saved successfully");
    }
    
    nvs_close(handle);
}

/**
 * @brief Load verification data from NVS
 */
void nvs_load_verification(verification_data_t *data)
{
    if (data == NULL) {
        ESP_LOGE(TAG, "NULL pointer passed to nvs_load_verification");
        return;
    }
    
    nvs_handle_t handle;
    esp_err_t err;
    
    ESP_LOGI(TAG, "Loading verification data from NVS");
    
    // Initialize with zeros
    memset(data, 0, sizeof(verification_data_t));
    
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No verification data found, initializing to zero");
        return;
    }
    
    uint32_t val;
    
    if (nvs_get_u32(handle, KEY_TOTAL_CYCLES, &val) == ESP_OK) {
        data->total_cycles = val;
    }
    
    if (nvs_get_u32(handle, KEY_LAST_VOLTAGE, &val) == ESP_OK) {
        data->last_voltage_mv = val;
    }
    
    if (nvs_get_u32(handle, KEY_UPTIME_HOURS, &val) == ESP_OK) {
        data->uptime_hours = val;
    }
    
    if (nvs_get_u32(handle, KEY_CHARGE_CYCLES, &val) == ESP_OK) {
        data->charge_cycles = val;
    }
    
    nvs_close(handle);
    
    ESP_LOGI(TAG, "Verification data loaded:");
    ESP_LOGI(TAG, "  Total cycles: %u", (unsigned int)data->total_cycles);
    ESP_LOGI(TAG, "  Last voltage: %u mV", (unsigned int)data->last_voltage_mv);
    ESP_LOGI(TAG, "  Uptime: %u hours", (unsigned int)data->uptime_hours);
    ESP_LOGI(TAG, "  Charge cycles: %u", (unsigned int)data->charge_cycles);
}

/**
 * @brief Save verification data to NVS
 */
void nvs_save_verification(const verification_data_t *data)
{
    if (data == NULL) {
        ESP_LOGE(TAG, "NULL pointer passed to nvs_save_verification");
        return;
    }
    
    nvs_handle_t handle;
    esp_err_t err;
    
    ESP_LOGD(TAG, "Saving verification data to NVS");
    
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing verification data: %s", esp_err_to_name(err));
        return;
    }
    
    // Save all verification items
    nvs_set_u32(handle, KEY_TOTAL_CYCLES, data->total_cycles);
    nvs_set_u32(handle, KEY_LAST_VOLTAGE, data->last_voltage_mv);
    nvs_set_u32(handle, KEY_UPTIME_HOURS, data->uptime_hours);
    nvs_set_u32(handle, KEY_CHARGE_CYCLES, data->charge_cycles);
    
    // Commit changes
    err = nvs_commit(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit verification data: %s", esp_err_to_name(err));
    } else {
        ESP_LOGD(TAG, "Verification data saved successfully");
    }
    
    nvs_close(handle);
}

/**
 * @brief Get current channel 0 ON threshold
 */
int32_t nvs_get_ch0_th_on(void)
{
    return g_config.ch0_th_on_mv;
}

/**
 * @brief Get current channel 0 OFF threshold
 */
int32_t nvs_get_ch0_th_off(void)
{
    return g_config.ch0_th_off_mv;
}

/**
 * @brief Get current channel 1 ON threshold
 */
int32_t nvs_get_ch1_th_on(void)
{
    return g_config.ch1_th_on_mv;
}

/**
 * @brief Get current channel 1 OFF threshold
 */
int32_t nvs_get_ch1_th_off(void)
{
    return g_config.ch1_th_off_mv;
}

/**
 * @brief Get temperature coefficient
 */
float nvs_get_temp_coefficient(void)
{
    return g_config.temp_coefficient;
}

/**
 * @brief Get PWM half duty
 */
uint8_t nvs_get_pwm_half_duty(void)
{
    return g_config.pwm_half_duty;
}

/**
 * @brief Get PWM full duty
 */
uint8_t nvs_get_pwm_full_duty(void)
{
    return g_config.pwm_full_duty;
}

/**
 * @brief Get motion timeout
 */
uint32_t nvs_get_motion_timeout(void)
{
    return g_config.motion_timeout_ms;
}

/**
 * @brief Set channel 0 thresholds
 */
void nvs_set_ch0_thresholds(int32_t th_on_mv, int32_t th_off_mv)
{
    g_config.ch0_th_on_mv = th_on_mv;
    g_config.ch0_th_off_mv = th_off_mv;
    ESP_LOGI(TAG, "CH0 thresholds updated: ON=%d mV, OFF=%d mV", th_on_mv, th_off_mv);
}

/**
 * @brief Set channel 1 thresholds
 */
void nvs_set_ch1_thresholds(int32_t th_on_mv, int32_t th_off_mv)
{
    g_config.ch1_th_on_mv = th_on_mv;
    g_config.ch1_th_off_mv = th_off_mv;
    ESP_LOGI(TAG, "CH1 thresholds updated: ON=%d mV, OFF=%d mV", th_on_mv, th_off_mv);
}

/**
 * @brief Set temperature coefficient
 */
void nvs_set_temp_coefficient(float coefficient)
{
    g_config.temp_coefficient = coefficient;
    ESP_LOGI(TAG, "Temperature coefficient updated: %.3f", coefficient);
}

/**
 * @brief Set PWM duties
 */
void nvs_set_pwm_duties(uint8_t half_duty, uint8_t full_duty)
{
    g_config.pwm_half_duty = half_duty;
    g_config.pwm_full_duty = full_duty;
    ESP_LOGI(TAG, "PWM duties updated: half=%d%%, full=%d%%", half_duty, full_duty);
}

/**
 * @brief Set motion timeout
 */
void nvs_set_motion_timeout(uint32_t timeout_ms)
{
    g_config.motion_timeout_ms = timeout_ms;
    ESP_LOGI(TAG, "Motion timeout updated: %u ms", (unsigned int)timeout_ms);
}