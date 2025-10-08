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

#endif