#include "cli_handler.h"
#include "adc_handler.h"
#include "channel_processor.h"
#include "control_handler.h"
#include "nvs_storage.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "CLI";

// Verification data (global for this module)
static verification_data_t g_verification_data = {0};

/**
 * @brief 'status' command - Display current system status
 */
static int cmd_status(int argc, char **argv)
{
    printf("\n");
    printf("=== Solar Battery Controller Status ===\n");
    printf("\n");
    
    // Battery and temperature
    uint32_t battery_mv = adc_get_battery_voltage_now();
    float temp_c = adc_get_temperature_now();
    
    printf("Battery:\n");
    printf("  Voltage: %u mV (%.2f V)\n", (unsigned int)battery_mv, battery_mv / 1000.0f);
    printf("  Temperature: %.1f °C\n", temp_c);
    printf("\n");
    
    // Channel 0 status
    bool ch0_state = channel_get_state(0);
    int32_t ch0_voltage = channel_get_filtered_voltage(0);
    printf("Channel 0:\n");
    printf("  State: %s\n", ch0_state ? "ON" : "OFF");
    printf("  Filtered Voltage: %d mV (%.2f V)\n", ch0_voltage, ch0_voltage / 1000.0f);
    printf("  Threshold ON: %d mV\n", nvs_get_ch0_th_on());
    printf("  Threshold OFF: %d mV\n", nvs_get_ch0_th_off());
    printf("\n");
    
    // Channel 1 status
    bool ch1_state = channel_get_state(1);
    int32_t ch1_voltage = channel_get_filtered_voltage(1);
    printf("Channel 1:\n");
    printf("  State: %s\n", ch1_state ? "ON" : "OFF");
    printf("  Filtered Voltage: %d mV (%.2f V)\n", ch1_voltage, ch1_voltage / 1000.0f);
    printf("  Threshold ON: %d mV\n", nvs_get_ch1_th_on());
    printf("  Threshold OFF: %d mV\n", nvs_get_ch1_th_off());
    printf("\n");
    
    // Hardware control state
    hw_control_t hw_state;
    control_get_state(&hw_state);
    printf("Hardware:\n");
    printf("  CH0 Output: %s\n", hw_state.ch0_state ? "ON" : "OFF");
    printf("  CH1 Output: %s\n", hw_state.ch1_state ? "ON" : "OFF");
    printf("  PWM Duty: %d%%\n", hw_state.pwm_duty);
    printf("  Motion Detected: %s\n", hw_state.motion_detected ? "YES" : "no");
    printf("  Charger Status: %s\n", control_get_charger_status() ? "CHARGING" : "not charging");
    printf("\n");
    
    // Configuration
    printf("Configuration:\n");
    printf("  Temp Coefficient: %.3f\n", nvs_get_temp_coefficient());
    printf("  PWM Half Duty: %d%%\n", nvs_get_pwm_half_duty());
    printf("  PWM Full Duty: %d%%\n", nvs_get_pwm_full_duty());
    printf("  Motion Timeout: %u ms\n", (unsigned int)nvs_get_motion_timeout());
    printf("\n");
    
    return 0;
}

/**
 * @brief 'set_threshold' command - Set channel thresholds
 */
static struct {
    struct arg_int *channel;
    struct arg_int *th_on;
    struct arg_int *th_off;
    struct arg_end *end;
} set_threshold_args;

static int cmd_set_threshold(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&set_threshold_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_threshold_args.end, argv[0]);
        return 1;
    }
    
    int channel = set_threshold_args.channel->ival[0];
    int th_on_mv = set_threshold_args.th_on->ival[0];
    int th_off_mv = set_threshold_args.th_off->ival[0];
    
    // Validate inputs
    if (channel < 0 || channel > 1) {
        printf("Error: Channel must be 0 or 1\n");
        return 1;
    }
    
    if (th_on_mv <= th_off_mv) {
        printf("Error: ON threshold must be greater than OFF threshold\n");
        return 1;
    }
    
    if (th_on_mv < 0 || th_on_mv > 20000) {
        printf("Error: ON threshold out of range (0-20000 mV)\n");
        return 1;
    }
    
    if (th_off_mv < 0 || th_off_mv > 20000) {
        printf("Error: OFF threshold out of range (0-20000 mV)\n");
        return 1;
    }
    
    // Set thresholds
    if (channel == 0) {
        nvs_set_ch0_thresholds(th_on_mv, th_off_mv);
    } else {
        nvs_set_ch1_thresholds(th_on_mv, th_off_mv);
    }
    
    // Save to NVS
    nvs_save_config();
    
    printf("Channel %d thresholds set: ON=%d mV, OFF=%d mV\n", channel, th_on_mv, th_off_mv);
    printf("Configuration saved to NVS\n");
    
    return 0;
}

/**
 * @brief 'set_temp_coeff' command - Set temperature coefficient
 */
static struct {
    struct arg_dbl *coefficient;
    struct arg_end *end;
} set_temp_coeff_args;

static int cmd_set_temp_coeff(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&set_temp_coeff_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_temp_coeff_args.end, argv[0]);
        return 1;
    }
    
    float coeff = (float)set_temp_coeff_args.coefficient->dval[0];
    
    // Validate range (typically -0.1 to 0.1)
    if (coeff < -0.1f || coeff > 0.1f) {
        printf("Error: Coefficient out of range (-0.1 to 0.1)\n");
        return 1;
    }
    
    nvs_set_temp_coefficient(coeff);
    nvs_save_config();
    
    printf("Temperature coefficient set to %.3f\n", coeff);
    printf("Configuration saved to NVS\n");
    
    return 0;
}

/**
 * @brief 'set_pwm' command - Set PWM duty cycles
 */
static struct {
    struct arg_int *half;
    struct arg_int *full;
    struct arg_end *end;
} set_pwm_args;

static int cmd_set_pwm(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&set_pwm_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_pwm_args.end, argv[0]);
        return 1;
    }
    
    int half_duty = set_pwm_args.half->ival[0];
    int full_duty = set_pwm_args.full->ival[0];
    
    // Validate range
    if (half_duty < 0 || half_duty > 100) {
        printf("Error: Half duty out of range (0-100%%)\n");
        return 1;
    }
    
    if (full_duty < 0 || full_duty > 100) {
        printf("Error: Full duty out of range (0-100%%)\n");
        return 1;
    }
    
    if (half_duty > full_duty) {
        printf("Error: Half duty should be less than or equal to full duty\n");
        return 1;
    }
    
    nvs_set_pwm_duties((uint8_t)half_duty, (uint8_t)full_duty);
    nvs_save_config();
    
    printf("PWM duties set: Half=%d%%, Full=%d%%\n", half_duty, full_duty);
    printf("Configuration saved to NVS\n");
    
    return 0;
}

/**
 * @brief 'motion' command - Trigger motion detection manually
 */
static int cmd_motion(int argc, char **argv)
{
    control_trigger_motion();
    printf("Motion detection triggered manually\n");
    printf("Lights will stay at full brightness for %u seconds\n", 
           nvs_get_motion_timeout() / 1000);
    return 0;
}

/**
 * @brief 'dump_verification' command - Display verification data
 */
static int cmd_dump_verification(int argc, char **argv)
{
    nvs_load_verification(&g_verification_data);
    
    printf("\n");
    printf("=== Verification Data ===\n");
    printf("  Total Cycles: %u\n", (unsigned int)g_verification_data.total_cycles);
    printf("  Last Voltage: %u mV (%.2f V)\n", 
           (unsigned int)g_verification_data.last_voltage_mv,
           g_verification_data.last_voltage_mv / 1000.0f);
    printf("  Uptime Hours: %u\n", (unsigned int)g_verification_data.uptime_hours);
    printf("  Charge Cycles: %u\n", (unsigned int)g_verification_data.charge_cycles);
    printf("\n");
    
    return 0;
}

/**
 * @brief 'reset_verification' command - Reset verification counters
 */
static int cmd_reset_verification(int argc, char **argv)
{
    memset(&g_verification_data, 0, sizeof(g_verification_data));
    nvs_save_verification(&g_verification_data);
    
    printf("Verification data reset and saved\n");
    return 0;
}

/**
 * @brief 'shutdown' command - Emergency shutdown
 */
static int cmd_shutdown(int argc, char **argv)
{
    printf("EMERGENCY SHUTDOWN - Turning off all outputs\n");
    control_emergency_shutdown();
    return 0;
}

/**
 * @brief 'help' command
 */
static int cmd_help(int argc, char **argv)
{
    printf("\n");
    printf("=== Solar Battery Controller Commands ===\n");
    printf("\n");
    printf("System Status:\n");
    printf("  status                     - Display system status\n");
    printf("  dump_verification          - Show verification data\n");
    printf("\n");
    printf("Configuration:\n");
    printf("  set_threshold <ch> <on> <off>  - Set channel thresholds (mV)\n");
    printf("                                   Example: set_threshold 0 12500 11800\n");
    printf("  set_temp_coeff <coeff>         - Set temperature coefficient\n");
    printf("                                   Example: set_temp_coeff -0.02\n");
    printf("  set_pwm <half> <full>          - Set PWM duty cycles (%%)\n");
    printf("                                   Example: set_pwm 50 100\n");
    printf("\n");
    printf("Testing:\n");
    printf("  motion                     - Trigger motion detection\n");
    printf("  shutdown                   - Emergency shutdown (all outputs off)\n");
    printf("\n");
    printf("Maintenance:\n");
    printf("  reset_verification         - Reset verification counters\n");
    printf("  restart                    - Restart the system\n");
    printf("  help                       - Show this help\n");
    printf("\n");
    
    return 0;
}

/**
 * @brief 'restart' command
 */
static int cmd_restart(int argc, char **argv)
{
    printf("Restarting system in 2 seconds...\n");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return 0;
}

/**
 * @brief Register all CLI commands
 */
static void register_commands(void)
{
    // Status command
    const esp_console_cmd_t status_cmd = {
        .command = "status",
        .help = "Display current system status",
        .hint = NULL,
        .func = &cmd_status,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&status_cmd));
    
    // Set threshold command
    set_threshold_args.channel = arg_int1(NULL, NULL, "<channel>", "Channel (0 or 1)");
    set_threshold_args.th_on = arg_int1(NULL, NULL, "<on_mv>", "ON threshold (mV)");
    set_threshold_args.th_off = arg_int1(NULL, NULL, "<off_mv>", "OFF threshold (mV)");
    set_threshold_args.end = arg_end(3);
    
    const esp_console_cmd_t set_threshold_cmd = {
        .command = "set_threshold",
        .help = "Set channel thresholds (mV)",
        .hint = NULL,
        .func = &cmd_set_threshold,
        .argtable = &set_threshold_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_threshold_cmd));
    
    // Set temperature coefficient command
    set_temp_coeff_args.coefficient = arg_dbl1(NULL, NULL, "<coeff>", "Temperature coefficient");
    set_temp_coeff_args.end = arg_end(1);
    
    const esp_console_cmd_t set_temp_coeff_cmd = {
        .command = "set_temp_coeff",
        .help = "Set temperature coefficient",
        .hint = NULL,
        .func = &cmd_set_temp_coeff,
        .argtable = &set_temp_coeff_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_temp_coeff_cmd));
    
    // Set PWM command
    set_pwm_args.half = arg_int1(NULL, NULL, "<half>", "Half duty (%)");
    set_pwm_args.full = arg_int1(NULL, NULL, "<full>", "Full duty (%)");
    set_pwm_args.end = arg_end(2);
    
    const esp_console_cmd_t set_pwm_cmd = {
        .command = "set_pwm",
        .help = "Set PWM duty cycles (%)",
        .hint = NULL,
        .func = &cmd_set_pwm,
        .argtable = &set_pwm_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_pwm_cmd));
    
    // Motion trigger command
    const esp_console_cmd_t motion_cmd = {
        .command = "motion",
        .help = "Trigger motion detection manually",
        .hint = NULL,
        .func = &cmd_motion,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&motion_cmd));
    
    // Dump verification command
    const esp_console_cmd_t dump_verification_cmd = {
        .command = "dump_verification",
        .help = "Display verification data",
        .hint = NULL,
        .func = &cmd_dump_verification,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&dump_verification_cmd));
    
    // Reset verification command
    const esp_console_cmd_t reset_verification_cmd = {
        .command = "reset_verification",
        .help = "Reset verification counters",
        .hint = NULL,
        .func = &cmd_reset_verification,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&reset_verification_cmd));
    
    // Shutdown command
    const esp_console_cmd_t shutdown_cmd = {
        .command = "shutdown",
        .help = "Emergency shutdown (turn off all outputs)",
        .hint = NULL,
        .func = &cmd_shutdown,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&shutdown_cmd));
    
    // Restart command
    const esp_console_cmd_t restart_cmd = {
        .command = "restart",
        .help = "Restart the system",
        .hint = NULL,
        .func = &cmd_restart,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&restart_cmd));
    
    // Help command
    const esp_console_cmd_t help_cmd = {
        .command = "help",
        .help = "Show available commands",
        .hint = NULL,
        .func = &cmd_help,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&help_cmd));
}

/**
 * @brief Initialize CLI console
 */
void cli_init(void)
{
    ESP_LOGI(TAG, "Initializing CLI console");
    
    // Disable buffering on stdin
    setvbuf(stdin, NULL, _IONBF, 0);
    
    // Minicom, screen, idf_monitor send CR when ENTER key is pressed
    esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    // Move the caret to the beginning of the next line on '\n'
    esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);
    
    // Configure UART
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config));
    
    // Tell VFS to use UART driver
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
    
    // Initialize console
    esp_console_config_t console_config = {
        .max_cmdline_length = 256,
        .max_cmdline_args = 8,
        .hint_color = atoi(LOG_COLOR_CYAN)
    };
    ESP_ERROR_CHECK(esp_console_init(&console_config));
    
    // Configure linenoise
    linenoiseSetMultiLine(1);
    linenoiseSetCompletionCallback(NULL);
    linenoiseSetHintsCallback(NULL);
    linenoiseHistorySetMaxLen(100);
    
    // Register commands
    register_commands();
    
    ESP_LOGI(TAG, "CLI console initialized. Type 'help' for commands.");
    
    // Print welcome message
    printf("\n");
    printf("========================================\n");
    printf("  Solar Battery Controller v1.0\n");
    printf("========================================\n");
    printf("Type 'help' for available commands\n");
    printf("Type 'status' for system status\n");
    printf("\n");
}

/**
 * @brief CLI task - processes console commands
 */
void cli_task(void *pvParameters)
{
    ESP_LOGI(TAG, "CLI task started");
    
    const char *prompt = LOG_COLOR_I "solar> " LOG_RESET_COLOR;
    
    while (1) {
        // Get a line using linenoise
        char *line = linenoise(prompt);
        
        if (line == NULL) {
            // Ctrl+C or error
            continue;
        }
        
        // Add to history
        if (strlen(line) > 0) {
            linenoiseHistoryAdd(line);
        }
        
        // Try to run the command
        int ret;
        esp_err_t err = esp_console_run(line, &ret);
        
        if (err == ESP_ERR_NOT_FOUND) {
            printf("Unrecognized command. Type 'help' for available commands.\n");
        } else if (err == ESP_ERR_INVALID_ARG) {
            // Command was empty
        } else if (err == ESP_OK && ret != ESP_OK) {
            printf("Command returned error: 0x%x (%s)\n", ret, esp_err_to_name(ret));
        } else if (err != ESP_OK) {
            printf("Internal error: %s\n", esp_err_to_name(err));
        }
        
        // Free the line
        linenoiseFree(line);
    }
}