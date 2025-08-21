#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ADC Manager Configuration - OPTIMIZED FOR MATCHED RATES
#define ADC_QUEUE_SIZE              10     // Smaller queue since rates are matched
#define ADC_MAX_SAMPLE_RATE         10000  // 10kHz maximum
#define ADC_MIN_SAMPLE_RATE         1      // 1Hz minimum

// ADC Data Packet Structure
typedef struct {
    uint64_t timestamp_us;      // Microsecond timestamp
    uint8_t channel;            // ADC channel number
    int raw_value;              // Raw ADC reading
    float voltage;              // Converted voltage
    float filtered_voltage;     // Filtered voltage
    uint32_t sequence;          // Sequence number
} adc_data_packet_t;

// ADC Statistics
typedef struct {
    uint32_t total_samples;     // Total samples taken
    uint32_t dropped_samples;   // Samples dropped due to queue full
    uint32_t error_count;       // ADC read errors
    float min_voltage;          // Minimum voltage recorded
    float max_voltage;          // Maximum voltage recorded
    float avg_voltage;          // Running average voltage
    uint64_t last_sample_time;  // Timestamp of last sample
} adc_stats_t;

// ADC Channel Context
typedef struct {
    uint8_t channel;            // Channel number
    uint32_t sequence_number;   // Current sequence number
    bool filter_initialized;    // Filter initialization flag
    float filtered_value;       // Current filtered value
    uint64_t last_sample_time;  // Last sample timestamp
    adc_stats_t stats;          // Channel statistics
} adc_channel_context_t;

// ADC Manager Functions
esp_err_t adc_manager_init(void);
esp_err_t adc_manager_deinit(void);
esp_err_t adc_manager_start(void);
esp_err_t adc_manager_stop(void);

// Data Access
esp_err_t adc_manager_get_data(adc_data_packet_t* packet, uint32_t timeout_ms);
size_t adc_manager_get_available_data(void);
esp_err_t adc_manager_flush_data(void);

// Channel Management
esp_err_t adc_manager_enable_channel(uint8_t channel, bool enable);
esp_err_t adc_manager_set_sample_rate(uint8_t channel, uint16_t sample_rate_hz);
esp_err_t adc_manager_set_filter_alpha(uint8_t channel, float alpha);

// Statistics and Monitoring
esp_err_t adc_manager_get_stats(uint8_t channel, adc_stats_t* stats);
esp_err_t adc_manager_reset_stats(uint8_t channel);
esp_err_t adc_manager_print_stats(void);

// Calibration and Testing
esp_err_t adc_manager_calibrate_channel(uint8_t channel);
esp_err_t adc_manager_test_channel(uint8_t channel);
esp_err_t adc_manager_get_instant_reading(uint8_t channel, float* voltage);

// Configuration
esp_err_t adc_manager_reconfigure_channel(uint8_t channel, uint16_t sample_rate, float filter_alpha);
bool adc_manager_is_running(void);
bool adc_manager_is_channel_enabled(uint8_t channel);

#ifdef __cplusplus
}
#endif
