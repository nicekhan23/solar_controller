#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include <stdint.h>

// Verification data structure
typedef struct {
    uint32_t total_cycles;
    uint32_t last_voltage_mv;
    uint32_t uptime_hours;
    uint32_t charge_cycles;
} verification_data_t;

// Initialize NVS
void nvs_init(void);

// Load/save configuration
void nvs_load_config(void);
void nvs_save_config(void);

// Load/save verification data
void nvs_load_verification(verification_data_t *data);
void nvs_save_verification(const verification_data_t *data);

// Getter functions
int32_t nvs_get_ch0_th_on(void);
int32_t nvs_get_ch0_th_off(void);
int32_t nvs_get_ch1_th_on(void);
int32_t nvs_get_ch1_th_off(void);
float nvs_get_temp_coefficient(void);
uint8_t nvs_get_pwm_half_duty(void);
uint8_t nvs_get_pwm_full_duty(void);
uint32_t nvs_get_motion_timeout(void);

// Setter functions
void nvs_set_ch0_thresholds(int32_t th_on_mv, int32_t th_off_mv);
void nvs_set_ch1_thresholds(int32_t th_on_mv, int32_t th_off_mv);
void nvs_set_temp_coefficient(float coefficient);
void nvs_set_pwm_duties(uint8_t half_duty, uint8_t full_duty);
void nvs_set_motion_timeout(uint32_t timeout_ms);

#endif