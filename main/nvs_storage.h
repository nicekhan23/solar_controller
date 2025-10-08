/**
 * @file nvs_storage.h
 * @brief Non-volatile storage interface for configuration and verification data
 * 
 * Manages persistent storage of system configuration parameters and
 * verification/statistics data across reboots.
 */

#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include <stdint.h>

/**
 * @struct verification_data_t
 * @brief Verification and statistics data structure
 * 
 * Tracks system operational statistics including boot cycles,
 * uptime, and charge cycles for long-term monitoring and verification.
 */
typedef struct {
    uint32_t total_cycles;
    uint32_t last_voltage_mv;
    uint32_t uptime_hours;
    uint32_t charge_cycles;
} verification_data_t;

/**
 * @brief Initialize NVS flash
 * 
 * Initializes NVS flash partition. If initialization fails due to
 * truncation or version mismatch, erases and reinitializes the partition.
 * 
 * @note Must be called early in system initialization
 */
void nvs_init(void);

/**
 * @brief Load configuration from NVS
 * 
 * Reads all configuration parameters from NVS storage.
 * Uses default values for any missing parameters.
 * Logs loaded configuration for verification.
 */

void nvs_load_config(void);

/**
 * @brief Save configuration to NVS
 * 
 * Writes current configuration parameters to NVS storage and commits.
 * All configuration changes should be followed by this call to persist data.
 */
void nvs_save_config(void);

/**
 * @brief Load verification data from NVS
 * @param data Pointer to verification_data_t structure to fill
 * 
 * Reads verification/statistics data from NVS. Initializes to zero
 * if no data exists (first boot).
 */
void nvs_load_verification(verification_data_t *data);

/**
 * @brief Save verification data to NVS
 * @param data Pointer to verification_data_t structure to save
 * 
 * Writes verification/statistics data to NVS and commits.
 * Should be called periodically to update uptime and cycle counts.
 */
void nvs_save_verification(const verification_data_t *data);

/**
 * @brief Get channel 0 ON threshold
 * @return Voltage threshold in millivolts (mV)
 */
int32_t nvs_get_ch0_th_on(void);

/**
 * @brief Get channel 0 OFF threshold
 * @return Voltage threshold in millivolts (mV)
 */
int32_t nvs_get_ch0_th_off(void);

/**
 * @brief Get channel 1 ON threshold
 * @return Voltage threshold in millivolts (mV)
 */
int32_t nvs_get_ch1_th_on(void);

/**
 * @brief Get channel 1 OFF threshold
 * @return Voltage threshold in millivolts (mV)
 */
int32_t nvs_get_ch1_th_off(void);

/**
 * @brief Get temperature compensation coefficient
 * @return Coefficient value (typically -0.1 to 0.1)
 * 
 * Coefficient represents voltage change per degree Celsius.
 * Negative values indicate voltage decreases with temperature increase.
 */
float nvs_get_temp_coefficient(void);

/**
 * @brief Get PWM half duty cycle
 * @return Duty cycle percentage (0-100)
 */
uint8_t nvs_get_pwm_half_duty(void);

/**
 * @brief Get PWM full duty cycle
 * @return Duty cycle percentage (0-100)
 */
uint8_t nvs_get_pwm_full_duty(void);

/**
 * @brief Get motion timeout duration
 * @return Timeout in milliseconds
 */
uint32_t nvs_get_motion_timeout(void);


/**
 * @brief Set channel 0 voltage thresholds
 * @param th_on_mv ON threshold in millivolts
 * @param th_off_mv OFF threshold in millivolts
 * 
 * @note Changes are not persisted until nvs_save_config() is called
 */
void nvs_set_ch0_thresholds(int32_t th_on_mv, int32_t th_off_mv);

/**
 * @brief Set channel 1 voltage thresholds
 * @param th_on_mv ON threshold in millivolts
 * @param th_off_mv OFF threshold in millivolts
 * 
 * @note Changes are not persisted until nvs_save_config() is called
 */
void nvs_set_ch1_thresholds(int32_t th_on_mv, int32_t th_off_mv);

/**
 * @brief Set temperature compensation coefficient
 * @param coefficient Temperature coefficient value
 * 
 * @note Changes are not persisted until nvs_save_config() is called
 */
void nvs_set_temp_coefficient(float coefficient);

/**
 * @brief Set PWM duty cycle percentages
 * @param half_duty Half brightness duty cycle (0-100)
 * @param full_duty Full brightness duty cycle (0-100)
 * 
 * @note Changes are not persisted until nvs_save_config() is called
 */
void nvs_set_pwm_duties(uint8_t half_duty, uint8_t full_duty);

/**
 * @brief Set motion timeout duration
 * @param timeout_ms Timeout duration in milliseconds
 * 
 * @note Changes are not persisted until nvs_save_config() is called
 */
void nvs_set_motion_timeout(uint32_t timeout_ms);

#endif