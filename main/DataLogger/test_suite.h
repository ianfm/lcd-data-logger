#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Test Suite for Data Logger System

// Test Result Structure
typedef struct {
    bool passed;
    char description[128];
    uint32_t execution_time_ms;
    char error_message[256];
} test_result_t;

// Test Categories
typedef enum {
    TEST_CATEGORY_CONFIG = 0,
    TEST_CATEGORY_HAL = 1,
    TEST_CATEGORY_UART = 2,
    TEST_CATEGORY_ADC = 3,
    TEST_CATEGORY_STORAGE = 4,
    TEST_CATEGORY_NETWORK = 5,
    TEST_CATEGORY_DISPLAY = 6,
    TEST_CATEGORY_INTEGRATION = 7
} test_category_t;

// Test Suite Functions
esp_err_t test_suite_run_all(void);
esp_err_t test_suite_run_category(test_category_t category);
esp_err_t test_suite_print_results(void);

// Individual Test Functions
esp_err_t test_config_system(test_result_t* result);
esp_err_t test_hal_initialization(test_result_t* result);

esp_err_t test_adc_readings(test_result_t* result);
esp_err_t test_storage_write_read(test_result_t* result);
esp_err_t test_network_api(test_result_t* result);
esp_err_t test_display_updates(test_result_t* result);
esp_err_t test_end_to_end_data_flow(test_result_t* result);
esp_err_t test_uart_loopback(uint8_t port, test_result_t* result);

// Performance Tests
esp_err_t test_performance_uart_throughput(test_result_t* result);
esp_err_t test_performance_adc_sampling(test_result_t* result);
esp_err_t test_performance_storage_speed(test_result_t* result);
esp_err_t test_performance_memory_usage(test_result_t* result);

// Stress Tests
esp_err_t test_stress_continuous_operation(uint32_t duration_minutes, test_result_t* result);
esp_err_t test_stress_memory_pressure(test_result_t* result);
esp_err_t test_stress_high_data_rate(test_result_t* result);

// Error Recovery Tests
esp_err_t test_error_recovery_sd_card_removal(test_result_t* result);
esp_err_t test_error_recovery_wifi_disconnect(test_result_t* result);
esp_err_t test_error_recovery_buffer_overflow(test_result_t* result);

// Utility Functions
esp_err_t test_generate_test_data(uint8_t* buffer, size_t length);
esp_err_t test_verify_data_integrity(const uint8_t* expected, const uint8_t* actual, size_t length);
uint32_t test_get_execution_time_ms(uint64_t start_time_us);

#ifdef __cplusplus
}
#endif
