#include "channel_processor.h"
#include "adc_handler.h"
#include "nvs_storage.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <math.h>

static const char *TAG = "CHAN_PROC";

// Moving average configuration
#define MA_SIZE 16  // Number of samples to average (16 * 100ms = 1.6s smoothing)

// Minimum time between state changes (debounce)
#define MIN_STATE_CHANGE_MS  5000  // 5 seconds

// Queue for output commands (to control_task)
QueueHandle_t ch0_command_queue = NULL;
QueueHandle_t ch1_command_queue = NULL;

/**
 * @brief Moving average filter structure
 */
typedef struct {
    int32_t buffer[MA_SIZE];
    int index;
    int64_t sum;
    int count;
    bool initialized;
} moving_average_t;

/**
 * @brief Channel state structure
 */
typedef struct {
    int channel_id;
    moving_average_t ma_filter;
    channel_state_t state;
    int32_t th_on_mv;
    int32_t th_off_mv;
    float temp_coefficient;
    float last_temperature;
} channel_context_t;

/**
 * @brief Initialize moving average filter
 */
static void ma_init(moving_average_t *ma)
{
    memset(ma->buffer, 0, sizeof(ma->buffer));
    ma->index = 0;
    ma->sum = 0;
    ma->count = 0;
    ma->initialized = false;
}

/**
 * @brief Add sample to moving average
 */
static void ma_add(moving_average_t *ma, int32_t value)
{
    if (!ma->initialized) {
        // First fill: initialize all slots with first value
        for (int i = 0; i < MA_SIZE; i++) {
            ma->buffer[i] = value;
        }
        ma->sum = (int64_t)value * MA_SIZE;
        ma->count = MA_SIZE;
        ma->initialized = true;
        return;
    }
    
    // Remove old value, add new value
    ma->sum -= ma->buffer[ma->index];
    ma->buffer[ma->index] = value;
    ma->sum += value;
    
    // Move to next position
    ma->index = (ma->index + 1) % MA_SIZE;
    
    if (ma->count < MA_SIZE) {
        ma->count++;
    }
}

/**
 * @brief Get moving average value
 */
static int32_t ma_get(moving_average_t *ma)
{
    if (ma->count == 0) {
        return 0;
    }
    return (int32_t)(ma->sum / ma->count);
}

/**
 * @brief Apply hysteresis logic
 * Returns true if output should be ON
 */
static bool apply_hysteresis(bool current_state, int32_t value, int32_t th_on, int32_t th_off)
{
    if (current_state) {
        // Currently ON: turn OFF only if below OFF threshold
        return value >= th_off;
    } else {
        // Currently OFF: turn ON only if above ON threshold
        return value >= th_on;
    }
}

/**
 * @brief Apply temperature compensation to thresholds
 * Adjusts thresholds based on temperature
 * Lead-acid batteries need higher voltage at lower temps
 */
static void apply_temperature_compensation(channel_context_t *ctx, float temp_c)
{
    // Get base thresholds from NVS
    int32_t base_th_on, base_th_off;
    
    if (ctx->channel_id == 0) {
        base_th_on = nvs_get_ch0_th_on();
        base_th_off = nvs_get_ch0_th_off();
    } else {
        base_th_on = nvs_get_ch1_th_on();
        base_th_off = nvs_get_ch1_th_off();
    }
    
    // Temperature compensation
    // Coefficient is typically negative (voltage decreases with temp increase)
    // Example: -0.02 means voltage decreases 20mV per °C above 25°C
    float temp_delta = temp_c - 25.0f;  // Reference temperature 25°C
    int32_t compensation_mv = (int32_t)(ctx->temp_coefficient * temp_delta * 1000.0f);
    
    ctx->th_on_mv = base_th_on + compensation_mv;
    ctx->th_off_mv = base_th_off + compensation_mv;
    
    // Log if temperature changed significantly
    if (fabsf(temp_c - ctx->last_temperature) > 2.0f) {
        ESP_LOGI(TAG, "CH%d: Temp=%.1f°C, compensation=%dmV, TH_ON=%dmV, TH_OFF=%dmV",
                 ctx->channel_id, temp_c, compensation_mv, ctx->th_on_mv, ctx->th_off_mv);
        ctx->last_temperature = temp_c;
    }
}

/**
 * @brief Process channel logic
 */
static void process_channel(channel_context_t *ctx, const adc_reading_t *reading)
{
    // Add to moving average
    ma_add(&ctx->ma_filter, reading->battery_voltage_mv);
    
    // Get filtered voltage
    int32_t filtered_voltage = ma_get(&ctx->ma_filter);
    ctx->state.filtered_voltage = filtered_voltage;
    
    // Calculate temperature from raw ADC
    // For TMP36: Vout = (Temp°C * 10mV) + 500mV
    float temp_c = ((float)reading->temperature_raw - 500.0f) / 10.0f;
    
    // Sanity check temperature
    if (temp_c < -40.0f || temp_c > 125.0f) {
        temp_c = 25.0f;  // Default to room temp if invalid
    }
    
    // Apply temperature compensation to thresholds
    apply_temperature_compensation(ctx, temp_c);
    
    // Apply hysteresis
    bool new_state = apply_hysteresis(
        ctx->state.output_state,
        filtered_voltage,
        ctx->th_on_mv,
        ctx->th_off_mv
    );
    
    // Check if state changed
    if (new_state != ctx->state.output_state) {
        // Check debounce time
        uint32_t time_since_last_change = reading->timestamp_ms - ctx->state.last_change_time;
        
        if (time_since_last_change >= MIN_STATE_CHANGE_MS) {
            // State change allowed
            ctx->state.output_state = new_state;
            ctx->state.last_change_time = reading->timestamp_ms;
            
            ESP_LOGI(TAG, "CH%d: State change to %s (voltage=%dmV, TH_ON=%dmV, TH_OFF=%dmV)",
                     ctx->channel_id,
                     new_state ? "ON" : "OFF",
                     filtered_voltage,
                     ctx->th_on_mv,
                     ctx->th_off_mv);
        } else {
            ESP_LOGD(TAG, "CH%d: State change blocked by debounce (time=%ums < %ums)",
                     ctx->channel_id,
                     (unsigned int)time_since_last_change,
                     MIN_STATE_CHANGE_MS);
        }
    }
    
    // Log periodic status (every ~10 seconds at 100ms sampling)
    static uint32_t log_counter[2] = {0, 0};
    log_counter[ctx->channel_id]++;
    
    if (log_counter[ctx->channel_id] % 100 == 0) {
        ESP_LOGI(TAG, "CH%d: State=%s, Voltage=%dmV (raw=%umV), Temp=%.1f°C",
                 ctx->channel_id,
                 ctx->state.output_state ? "ON" : "OFF",
                 filtered_voltage,
                 (unsigned int)reading->battery_voltage_mv,
                 temp_c);
    }
}

/**
 * @brief Channel processing task
 * One instance runs per channel
 */
void channel_proc_task(void *pvParameters)
{
    channel_config_t *config = (channel_config_t *)pvParameters;
    
    if (config == NULL) {
        ESP_LOGE(TAG, "NULL config passed to channel_proc_task");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Channel %d processor started", config->channel_id);
    
    // Create channel context
    channel_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.channel_id = config->channel_id;
    ctx.th_on_mv = config->th_on_mv;
    ctx.th_off_mv = config->th_off_mv;
    ctx.temp_coefficient = config->temp_coeff;
    ctx.last_temperature = 25.0f;
    
    // Initialize moving average
    ma_init(&ctx.ma_filter);
    
    // Initialize state
    ctx.state.output_state = false;
    ctx.state.filtered_voltage = 0;
    ctx.state.last_change_time = 0;
    
    // Select appropriate queue based on channel
    QueueHandle_t input_queue = (config->channel_id == 0) ? adc_queue_ch0 : adc_queue_ch1;
    QueueHandle_t output_queue = (config->channel_id == 0) ? ch0_command_queue : ch1_command_queue;
    
    if (input_queue == NULL) {
        ESP_LOGE(TAG, "CH%d: Input queue not initialized", config->channel_id);
        vTaskDelete(NULL);
        return;
    }
    
    adc_reading_t reading;
    
    while (1) {
        // Wait for ADC reading
        if (xQueueReceive(input_queue, &reading, portMAX_DELAY) == pdTRUE) {
            // Process the reading
            process_channel(&ctx, &reading);
            
            // Send command to control task if output queue exists
            if (output_queue != NULL) {
                channel_command_t cmd;
                cmd.channel_id = ctx.channel_id;
                cmd.output_state = ctx.state.output_state;
                cmd.filtered_voltage = ctx.state.filtered_voltage;
                cmd.timestamp_ms = reading.timestamp_ms;
                
                if (xQueueSend(output_queue, &cmd, 0) != pdTRUE) {
                    ESP_LOGW(TAG, "CH%d: Command queue full", config->channel_id);
                }
            }
        }
    }
}

/**
 * @brief Initialize channel processor queues
 */
void channel_processor_init(void)
{
    ESP_LOGI(TAG, "Initializing channel processor");
    
    // Create command queues for control task
    ch0_command_queue = xQueueCreate(5, sizeof(channel_command_t));
    ch1_command_queue = xQueueCreate(5, sizeof(channel_command_t));
    
    if (ch0_command_queue == NULL || ch1_command_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create command queues");
        return;
    }
    
    ESP_LOGI(TAG, "Channel processor initialized");
}

/**
 * @brief Get current channel state (for status queries)
 */
bool channel_get_state(int channel_id)
{
    // This is a simplified version
    // In a real implementation, you might want to store state in a shared structure
    // protected by a mutex
    
    if (channel_id == 0 && ch0_command_queue != NULL) {
        channel_command_t cmd;
        if (xQueuePeek(ch0_command_queue, &cmd, 0) == pdTRUE) {
            return cmd.output_state;
        }
    } else if (channel_id == 1 && ch1_command_queue != NULL) {
        channel_command_t cmd;
        if (xQueuePeek(ch1_command_queue, &cmd, 0) == pdTRUE) {
            return cmd.output_state;
        }
    }
    
    return false;
}

/**
 * @brief Get filtered voltage for a channel
 */
int32_t channel_get_filtered_voltage(int channel_id)
{
    if (channel_id == 0 && ch0_command_queue != NULL) {
        channel_command_t cmd;
        if (xQueuePeek(ch0_command_queue, &cmd, 0) == pdTRUE) {
            return cmd.filtered_voltage;
        }
    } else if (channel_id == 1 && ch1_command_queue != NULL) {
        channel_command_t cmd;
        if (xQueuePeek(ch1_command_queue, &cmd, 0) == pdTRUE) {
            return cmd.filtered_voltage;
        }
    }
    
    return 0;
}