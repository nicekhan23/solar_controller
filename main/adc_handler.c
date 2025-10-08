#include "adc_handler.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

static const char *TAG = "ADC_HANDLER";

// Hardware configuration
#define ADC_BATTERY_CHANNEL     ADC_CHANNEL_6  // GPIO34
#define ADC_TEMP_CHANNEL        ADC_CHANNEL_7  // GPIO35
#define ADC_ATTEN               ADC_ATTEN_DB_12
#define ADC_WIDTH               ADC_BITWIDTH_12

// Voltage divider resistor values (in ohms)
#define R_TOP    47000.0f  // 47kΩ
#define R_BOT    10000.0f  // 10kΩ
#define DIVIDER_RATIO  ((R_TOP + R_BOT) / R_BOT)  // ~5.7

// Sampling configuration
#define ADC_SAMPLE_INTERVAL_MS  100   // Sample every 100ms
#define QUEUE_SIZE              10

// Oversampling for noise reduction
#define OVERSAMPLE_COUNT        8

// ADC handles
static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc1_cali_handle = NULL;
static bool calibration_available = false;

// Queues for each channel
QueueHandle_t adc_queue_ch0 = NULL;
QueueHandle_t adc_queue_ch1 = NULL;

/**
 * @brief Initialize ADC calibration
 */
static bool adc_calibration_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    ESP_LOGI(TAG, "Calibration scheme version: Curve Fitting");
    
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = atten,
        .bitwidth = ADC_WIDTH,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
    if (ret == ESP_OK) {
        calibrated = true;
        ESP_LOGI(TAG, "Curve Fitting calibration successful");
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "Calibration scheme: Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_WIDTH,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
            ESP_LOGI(TAG, "Line Fitting calibration successful");
        }
    }
#endif

    *out_handle = handle;
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Calibration failed: %s. Using raw values.", esp_err_to_name(ret));
    }
    return calibrated;
}

/**
 * @brief Read ADC with oversampling and return voltage in mV
 */
static uint32_t adc_read_voltage(adc_channel_t channel)
{
    int raw_sum = 0;
    int voltage_sum = 0;
    
    // Oversample to reduce noise
    for (int i = 0; i < OVERSAMPLE_COUNT; i++) {
        int raw = 0;
        esp_err_t ret = adc_oneshot_read(adc1_handle, channel, &raw);
        if (ret == ESP_OK) {
            raw_sum += raw;
            
            if (calibration_available) {
                int voltage = 0;
                ret = adc_cali_raw_to_voltage(adc1_cali_handle, raw, &voltage);
                if (ret == ESP_OK) {
                    voltage_sum += voltage;
                }
            }
        } else {
            ESP_LOGW(TAG, "ADC read failed on channel %d: %s", channel, esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(2)); // Small delay between samples
    }
    
    // Average the readings
    int avg_raw = raw_sum / OVERSAMPLE_COUNT;
    
    uint32_t voltage_mv;
    if (calibration_available) {
        voltage_mv = voltage_sum / OVERSAMPLE_COUNT;
    } else {
        // Fallback: approximate conversion for 12-bit ADC with 12dB attenuation
        // Max voltage ~3300mV at 4095 counts (rough approximation)
        voltage_mv = (avg_raw * 3300) / 4095;
    }
    
    ESP_LOGD(TAG, "ADC Ch%d: raw=%d, mV=%u", channel, avg_raw, (unsigned int)voltage_mv);
    
    return voltage_mv;
}

/**
 * @brief Convert ADC voltage back to actual battery voltage
 */
static uint32_t calculate_battery_voltage(uint32_t adc_mv)
{
    // Apply voltage divider ratio
    float battery_v = ((float)adc_mv / 1000.0f) * DIVIDER_RATIO;
    uint32_t battery_mv = (uint32_t)(battery_v * 1000.0f);
    
    return battery_mv;
}

/**
 * @brief Read temperature sensor (if analog)
 * For TMP36: Vout = (Temp°C * 10mV) + 500mV
 * For NTC thermistor, use Steinhart-Hart equation
 */
static float calculate_temperature(uint32_t adc_mv)
{
    // Example for TMP36 sensor
    // TMP36: 10mV per degree, 500mV at 0°C
    float temp_c = ((float)adc_mv - 500.0f) / 10.0f;
    
    // Sanity check
    if (temp_c < -40.0f || temp_c > 125.0f) {
        ESP_LOGW(TAG, "Temperature out of range: %.1f°C, using 25°C", temp_c);
        temp_c = 25.0f;
    }
    
    return temp_c;
}

/**
 * @brief Initialize ADC subsystem
 */
void adc_init(void)
{
    ESP_LOGI(TAG, "Initializing ADC");
    
    // Create queues
    adc_queue_ch0 = xQueueCreate(QUEUE_SIZE, sizeof(adc_reading_t));
    adc_queue_ch1 = xQueueCreate(QUEUE_SIZE, sizeof(adc_reading_t));
    
    if (adc_queue_ch0 == NULL || adc_queue_ch1 == NULL) {
        ESP_LOGE(TAG, "Failed to create ADC queues");
        return;
    }
    
    // Configure ADC1
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
        return;
    }
    
    // Configure battery voltage channel
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_WIDTH,
    };
    
    ret = adc_oneshot_config_channel(adc1_handle, ADC_BATTERY_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config battery channel: %s", esp_err_to_name(ret));
        return;
    }
    
    // Configure temperature channel
    ret = adc_oneshot_config_channel(adc1_handle, ADC_TEMP_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config temp channel: %s", esp_err_to_name(ret));
        return;
    }
    
    // Initialize calibration
    calibration_available = adc_calibration_init(ADC_UNIT_1, ADC_ATTEN, &adc1_cali_handle);
    
    ESP_LOGI(TAG, "ADC initialized successfully");
    ESP_LOGI(TAG, "Battery channel: ADC1_CH%d (GPIO34)", ADC_BATTERY_CHANNEL);
    ESP_LOGI(TAG, "Temperature channel: ADC1_CH%d (GPIO35)", ADC_TEMP_CHANNEL);
    ESP_LOGI(TAG, "Voltage divider ratio: %.2f", DIVIDER_RATIO);
}

/**
 * @brief ADC sampling task
 * Reads ADC channels periodically and pushes to queues
 */
void adc_task(void *pvParameters)
{
    ESP_LOGI(TAG, "ADC task started");
    
    TickType_t last_wake_time = xTaskGetTickCount();
    uint32_t sample_count = 0;
    
    while (1) {
        // Read battery voltage
        uint32_t adc_battery_mv = adc_read_voltage(ADC_BATTERY_CHANNEL);
        uint32_t battery_voltage_mv = calculate_battery_voltage(adc_battery_mv);
        
        // Read temperature
        uint32_t adc_temp_mv = adc_read_voltage(ADC_TEMP_CHANNEL);
        float temperature_c = calculate_temperature(adc_temp_mv);
        
        // Get current timestamp
        uint32_t timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Create reading structure
        adc_reading_t reading;
        reading.battery_voltage_mv = battery_voltage_mv;
        reading.temperature_raw = adc_temp_mv;
        reading.timestamp_ms = timestamp_ms;
        
        // Log periodically (every 10 samples = ~1 second)
        if (sample_count % 10 == 0) {
            ESP_LOGI(TAG, "Battery: %u mV (%.2fV), ADC: %u mV, Temp: %.1f°C", 
                     (unsigned int)battery_voltage_mv, 
                     battery_voltage_mv / 1000.0f,
                     (unsigned int)adc_battery_mv,
                     temperature_c);
        }
        
        // Push to both channel queues (they'll do their own processing)
        if (xQueueSend(adc_queue_ch0, &reading, 0) != pdTRUE) {
            ESP_LOGW(TAG, "CH0 queue full, dropping sample");
        }
        
        if (xQueueSend(adc_queue_ch1, &reading, 0) != pdTRUE) {
            ESP_LOGW(TAG, "CH1 queue full, dropping sample");
        }
        
        sample_count++;
        
        // Wait for next sample interval
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(ADC_SAMPLE_INTERVAL_MS));
    }
}

/**
 * @brief Get current battery voltage (blocking read)
 * Useful for immediate status queries
 */
uint32_t adc_get_battery_voltage_now(void)
{
    if (adc1_handle == NULL) {
        ESP_LOGE(TAG, "ADC not initialized");
        return 0;
    }
    
    uint32_t adc_mv = adc_read_voltage(ADC_BATTERY_CHANNEL);
    return calculate_battery_voltage(adc_mv);
}

/**
 * @brief Get current temperature (blocking read)
 */
float adc_get_temperature_now(void)
{
    if (adc1_handle == NULL) {
        ESP_LOGE(TAG, "ADC not initialized");
        return 25.0f;
    }
    
    uint32_t adc_mv = adc_read_voltage(ADC_TEMP_CHANNEL);
    return calculate_temperature(adc_mv);
}

/**
 * @brief Cleanup ADC resources
 */
void adc_deinit(void)
{
    if (calibration_available && adc1_cali_handle) {
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_delete_scheme_curve_fitting(adc1_cali_handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_delete_scheme_line_fitting(adc1_cali_handle);
#endif
        ESP_LOGI(TAG, "ADC calibration deleted");
    }
    
    if (adc1_handle) {
        adc_oneshot_del_unit(adc1_handle);
        ESP_LOGI(TAG, "ADC unit deleted");
    }
    
    if (adc_queue_ch0) {
        vQueueDelete(adc_queue_ch0);
    }
    
    if (adc_queue_ch1) {
        vQueueDelete(adc_queue_ch1);
    }
    
    ESP_LOGI(TAG, "ADC deinitialized");
}