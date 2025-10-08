#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "nvs_flash.h"

// Application modules
#include "nvs_storage.h"
#include "adc_handler.h"
#include "channel_processor.h"
#include "control_handler.h"
#include "cli_handler.h"

static const char *TAG = "MAIN";

// Task priorities
#define PRIORITY_ADC        5
#define PRIORITY_PROCESSOR  4
#define PRIORITY_CONTROL    5
#define PRIORITY_CLI        3

// Task stack sizes (in words, not bytes)
#define STACK_SIZE_ADC      2048
#define STACK_SIZE_PROCESSOR 3072
#define STACK_SIZE_CONTROL  2048
#define STACK_SIZE_CLI      4096

// Task handles
static TaskHandle_t adc_task_handle = NULL;
static TaskHandle_t ch0_proc_task_handle = NULL;
static TaskHandle_t ch1_proc_task_handle = NULL;
static TaskHandle_t control_task_handle = NULL;
static TaskHandle_t cli_task_handle = NULL;

// Channel configurations
static channel_config_t ch0_config;
static channel_config_t ch1_config;

/**
 * @brief Print system information
 */
static void print_system_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    printf("\n");
    printf("========================================\n");
    printf("  Solar Battery Controller\n");
    printf("========================================\n");
    printf("Chip: %s\n", CONFIG_IDF_TARGET);
    printf("Cores: %d\n", chip_info.cores);
    printf("Features: WiFi%s%s\n",
           (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");
    printf("Silicon Revision: %d\n", chip_info.revision);
    printf("Flash: %dMB %s\n",
           spi_flash_get_chip_size() / (1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    printf("Free heap: %d bytes\n", (int)esp_get_free_heap_size());
    printf("========================================\n");
    printf("\n");
}

/**
 * @brief Initialize all subsystems
 */
static void initialize_subsystems(void)
{
    ESP_LOGI(TAG, "Initializing subsystems...");
    
    // 1. Initialize NVS
    ESP_LOGI(TAG, "Step 1/5: Initializing NVS");
    nvs_init();
    nvs_load_config();
    
    // Load verification data
    verification_data_t verification;
    nvs_load_verification(&verification);
    
    // Increment total cycles on startup
    verification.total_cycles++;
    nvs_save_verification(&verification);
    
    ESP_LOGI(TAG, "Boot count: %u", (unsigned int)verification.total_cycles);
    
    // 2. Initialize ADC
    ESP_LOGI(TAG, "Step 2/5: Initializing ADC");
    adc_init();
    
    // 3. Initialize channel processors
    ESP_LOGI(TAG, "Step 3/5: Initializing channel processors");
    channel_processor_init();
    
    // 4. Initialize hardware control
    ESP_LOGI(TAG, "Step 4/5: Initializing hardware control");
    control_init();
    
    // 5. Initialize CLI
    ESP_LOGI(TAG, "Step 5/5: Initializing CLI console");
    cli_init();
    
    ESP_LOGI(TAG, "All subsystems initialized successfully");
}

/**
 * @brief Configure channels from NVS settings
 */
static void configure_channels(void)
{
    ESP_LOGI(TAG, "Configuring channels from NVS");
    
    // Channel 0 configuration
    ch0_config.channel_id = 0;
    ch0_config.th_on_mv = nvs_get_ch0_th_on();
    ch0_config.th_off_mv = nvs_get_ch0_th_off();
    ch0_config.temp_coeff = nvs_get_temp_coefficient();
    
    ESP_LOGI(TAG, "Channel 0: ON=%dmV, OFF=%dmV, temp_coeff=%.3f",
             ch0_config.th_on_mv, ch0_config.th_off_mv, ch0_config.temp_coeff);
    
    // Channel 1 configuration
    ch1_config.channel_id = 1;
    ch1_config.th_on_mv = nvs_get_ch1_th_on();
    ch1_config.th_off_mv = nvs_get_ch1_th_off();
    ch1_config.temp_coeff = nvs_get_temp_coefficient();
    
    ESP_LOGI(TAG, "Channel 1: ON=%dmV, OFF=%dmV, temp_coeff=%.3f",
             ch1_config.th_on_mv, ch1_config.th_off_mv, ch1_config.temp_coeff);
}

/**
 * @brief Create all application tasks
 */
static void create_tasks(void)
{
    ESP_LOGI(TAG, "Creating application tasks...");
    
    // Create ADC task
    BaseType_t ret = xTaskCreate(
        adc_task,
        "adc_task",
        STACK_SIZE_ADC,
        NULL,
        PRIORITY_ADC,
        &adc_task_handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create ADC task");
        return;
    }
    ESP_LOGI(TAG, "ADC task created");
    
    // Create Channel 0 processor task
    ret = xTaskCreate(
        channel_proc_task,
        "ch0_proc",
        STACK_SIZE_PROCESSOR,
        &ch0_config,
        PRIORITY_PROCESSOR,
        &ch0_proc_task_handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CH0 processor task");
        return;
    }
    ESP_LOGI(TAG, "Channel 0 processor task created");
    
    // Create Channel 1 processor task
    ret = xTaskCreate(
        channel_proc_task,
        "ch1_proc",
        STACK_SIZE_PROCESSOR,
        &ch1_config,
        PRIORITY_PROCESSOR,
        &ch1_proc_task_handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CH1 processor task");
        return;
    }
    ESP_LOGI(TAG, "Channel 1 processor task created");
    
    // Create control task
    ret = xTaskCreate(
        control_task,
        "control",
        STACK_SIZE_CONTROL,
        NULL,
        PRIORITY_CONTROL,
        &control_task_handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create control task");
        return;
    }
    ESP_LOGI(TAG, "Control task created");
    
    // Create CLI task
    ret = xTaskCreate(
        cli_task,
        "cli",
        STACK_SIZE_CLI,
        NULL,
        PRIORITY_CLI,
        &cli_task_handle
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CLI task");
        return;
    }
    ESP_LOGI(TAG, "CLI task created");
    
    ESP_LOGI(TAG, "All tasks created successfully");
}

/**
 * @brief Periodic uptime update task
 * Updates verification data every hour
 */
static void uptime_task(void *pvParameters)
{
    verification_data_t verification;
    uint32_t last_hour = 0;
    
    while (1) {
        // Wait 1 hour
        vTaskDelay(pdMS_TO_TICKS(3600000));  // 1 hour in ms
        
        // Load current verification data
        nvs_load_verification(&verification);
        
        // Increment uptime
        verification.uptime_hours++;
        
        // Update last voltage
        verification.last_voltage_mv = adc_get_battery_voltage_now();
        
        // Save back to NVS
        nvs_save_verification(&verification);
        
        last_hour++;
        ESP_LOGI(TAG, "Uptime: %u hours, Battery: %u mV",
                 (unsigned int)verification.uptime_hours,
                 (unsigned int)verification.last_voltage_mv);
    }
}

/**
 * @brief Watchdog task - monitors system health
 */
static void watchdog_task(void *pvParameters)
{
    uint32_t last_check = 0;
    const uint32_t check_interval_ms = 60000;  // Check every minute
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
        
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Check heap size
        uint32_t free_heap = esp_get_free_heap_size();
        if (free_heap < 10000) {  // Less than 10KB free
            ESP_LOGW(TAG, "Low heap warning: %u bytes free", (unsigned int)free_heap);
        }
        
        // Check battery voltage
        uint32_t battery_mv = adc_get_battery_voltage_now();
        if (battery_mv < 10500) {  // Below 10.5V - critical
            ESP_LOGE(TAG, "CRITICAL: Battery voltage very low: %u mV", (unsigned int)battery_mv);
            // Could trigger emergency shutdown here
            // control_emergency_shutdown();
        } else if (battery_mv < 11000) {
            ESP_LOGW(TAG, "Warning: Battery voltage low: %u mV", (unsigned int)battery_mv);
        }
        
        // Log periodic health status
        if ((now - last_check) > 300000) {  // Every 5 minutes
            ESP_LOGI(TAG, "Health check: heap=%u bytes, battery=%u mV, uptime=%u min",
                     (unsigned int)free_heap,
                     (unsigned int)battery_mv,
                     (unsigned int)(now / 60000));
            last_check = now;
        }
    }
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    // Print system information
    print_system_info();
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Solar Battery Controller Starting");
    ESP_LOGI(TAG, "========================================");
    
    // Initialize all subsystems
    initialize_subsystems();
    
    // Configure channels from NVS
    configure_channels();
    
    // Create all application tasks
    create_tasks();
    
    // Create uptime tracking task
    BaseType_t ret = xTaskCreate(
        uptime_task,
        "uptime",
        2048,
        NULL,
        2,
        NULL
    );
    if (ret == pdPASS) {
        ESP_LOGI(TAG, "Uptime tracking task created");
    }
    
    // Create watchdog task
    ret = xTaskCreate(
        watchdog_task,
        "watchdog",
        2048,
        NULL,
        2,
        NULL
    );
    if (ret == pdPASS) {
        ESP_LOGI(TAG, "Watchdog task created");
    }
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  System Running");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Free heap: %d bytes", (int)esp_get_free_heap_size());
    ESP_LOGI(TAG, "Type 'help' in console for commands");
    
    // Main task is done, FreeRTOS scheduler is running
    // All work happens in the created tasks
}