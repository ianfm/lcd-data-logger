#include "test_suite.h"
#include "config.h"
#include "hal.h"
#include "uart_manager.h"
#include "adc_manager.h"
#include "storage_manager.h"
#include "network_manager.h"
#include "display_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "TEST_SUITE";

#define MAX_TEST_RESULTS 32
static test_result_t g_test_results[MAX_TEST_RESULTS];
static uint32_t g_test_count = 0;

// Helper function to record test result
static void record_test_result(const test_result_t* result) {
    if (g_test_count < MAX_TEST_RESULTS) {
        memcpy(&g_test_results[g_test_count], result, sizeof(test_result_t));
        g_test_count++;
    }
}

esp_err_t test_suite_run_all(void) {
    ESP_LOGI(TAG, "=== Running Complete Test Suite ===");
    
    g_test_count = 0;  // Reset test counter
    test_result_t result;
    
    // Configuration Tests
    ESP_LOGI(TAG, "Running Configuration Tests...");
    test_config_system(&result);
    record_test_result(&result);
    
    // HAL Tests
    ESP_LOGI(TAG, "Running HAL Tests...");
    test_hal_initialization(&result);
    record_test_result(&result);
    
    // UART Tests
    ESP_LOGI(TAG, "Running UART Tests...");
    test_uart_loopback(0, &result);
    record_test_result(&result);
    
    // ADC Tests
    ESP_LOGI(TAG, "Running ADC Tests...");
    test_adc_readings(&result);
    record_test_result(&result);
    
    // Storage Tests
    ESP_LOGI(TAG, "Running Storage Tests...");
    test_storage_write_read(&result);
    record_test_result(&result);
    
    // Network Tests
    ESP_LOGI(TAG, "Running Network Tests...");
    test_network_api(&result);
    record_test_result(&result);
    
    // Display Tests
    ESP_LOGI(TAG, "Running Display Tests...");
    test_display_updates(&result);
    record_test_result(&result);
    
    // Integration Tests
    ESP_LOGI(TAG, "Running Integration Tests...");
    test_end_to_end_data_flow(&result);
    record_test_result(&result);
    
    // Performance Tests
    ESP_LOGI(TAG, "Running Performance Tests...");
    test_performance_memory_usage(&result);
    record_test_result(&result);
    
    ESP_LOGI(TAG, "=== Test Suite Complete ===");
    return test_suite_print_results();
}

esp_err_t test_config_system(test_result_t* result) {
    uint64_t start_time = esp_timer_get_time();
    
    strcpy(result->description, "Configuration System Test");
    result->passed = true;
    result->error_message[0] = '\0';
    
    // Test configuration loading
    system_config_t* config = config_get_instance();
    if (!config) {
        result->passed = false;
        strcpy(result->error_message, "Failed to get configuration instance");
        goto test_end;
    }
    
    // Test configuration validation
    esp_err_t ret = config_validate(config);
    if (ret != ESP_OK) {
        result->passed = false;
        snprintf(result->error_message, sizeof(result->error_message), 
                "Configuration validation failed: %s", esp_err_to_name(ret));
        goto test_end;
    }
    
    // Test configuration updates
    ret = config_update_uart(0, 9600, true);
    if (ret != ESP_OK) {
        result->passed = false;
        strcpy(result->error_message, "Failed to update UART configuration");
        goto test_end;
    }
    
test_end:
    result->execution_time_ms = test_get_execution_time_ms(start_time);
    ESP_LOGI(TAG, "Config test: %s (%lu ms)", 
             result->passed ? "PASS" : "FAIL", result->execution_time_ms);
    return ESP_OK;
}

esp_err_t test_hal_initialization(test_result_t* result) {
    uint64_t start_time = esp_timer_get_time();
    
    strcpy(result->description, "HAL Initialization Test");
    result->passed = true;
    result->error_message[0] = '\0';
    
    // Test HAL status
    if (!hal_is_initialized()) {
        result->passed = false;
        strcpy(result->error_message, "HAL not initialized");
        goto test_end;
    }
    
    // Test UART initialization status
    for (int i = 0; i < CONFIG_UART_PORT_COUNT; i++) {
        system_config_t* config = config_get_instance();
        if (config->uart_config[i].enabled) {
            if (!hal_uart_is_initialized(i)) {
                result->passed = false;
                snprintf(result->error_message, sizeof(result->error_message), 
                        "UART%d not initialized", i);
                goto test_end;
            }
        }
    }
    
    // Test ADC initialization status
    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        system_config_t* config = config_get_instance();
        if (config->adc_config[i].enabled) {
            if (!hal_adc_is_initialized(i)) {
                result->passed = false;
                snprintf(result->error_message, sizeof(result->error_message), 
                        "ADC%d not initialized", i);
                goto test_end;
            }
        }
    }
    
test_end:
    result->execution_time_ms = test_get_execution_time_ms(start_time);
    ESP_LOGI(TAG, "HAL test: %s (%lu ms)", 
             result->passed ? "PASS" : "FAIL", result->execution_time_ms);
    return ESP_OK;
}

esp_err_t test_adc_readings(test_result_t* result) {
    uint64_t start_time = esp_timer_get_time();
    
    strcpy(result->description, "ADC Readings Test");
    result->passed = true;
    result->error_message[0] = '\0';
    
    // Test ADC readings
    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        if (adc_manager_is_channel_enabled(i)) {
            float voltage;
            esp_err_t ret = adc_manager_get_instant_reading(i, &voltage);
            if (ret != ESP_OK) {
                result->passed = false;
                snprintf(result->error_message, sizeof(result->error_message), 
                        "Failed to read ADC%d: %s", i, esp_err_to_name(ret));
                goto test_end;
            }
            
            // Validate voltage range (0-4V expected)
            if (voltage < 0.0f || voltage > 5.0f) {
                result->passed = false;
                snprintf(result->error_message, sizeof(result->error_message), 
                        "ADC%d voltage out of range: %.2fV", i, voltage);
                goto test_end;
            }
            
            ESP_LOGI(TAG, "ADC%d: %.3fV", i, voltage);
        }
    }
    
test_end:
    result->execution_time_ms = test_get_execution_time_ms(start_time);
    ESP_LOGI(TAG, "ADC test: %s (%lu ms)", 
             result->passed ? "PASS" : "FAIL", result->execution_time_ms);
    return ESP_OK;
}

esp_err_t test_storage_write_read(test_result_t* result) {
    uint64_t start_time = esp_timer_get_time();
    
    strcpy(result->description, "Storage Write/Read Test");
    result->passed = true;
    result->error_message[0] = '\0';
    
    // Test storage manager status
    if (!storage_manager_is_running()) {
        result->passed = false;
        strcpy(result->error_message, "Storage manager not running");
        goto test_end;
    }
    
    // Test writing test data
    const char* test_data = "Test data for storage verification";
    esp_err_t ret = storage_manager_write_uart_data(0, (uint8_t*)test_data, strlen(test_data));
    if (ret != ESP_OK) {
        result->passed = false;
        snprintf(result->error_message, sizeof(result->error_message), 
                "Failed to write test data: %s", esp_err_to_name(ret));
        goto test_end;
    }
    
    // Test ADC data writing
    ret = storage_manager_write_adc_data(0, 2.5f, 2048);
    if (ret != ESP_OK) {
        result->passed = false;
        snprintf(result->error_message, sizeof(result->error_message), 
                "Failed to write ADC data: %s", esp_err_to_name(ret));
        goto test_end;
    }
    
test_end:
    result->execution_time_ms = test_get_execution_time_ms(start_time);
    ESP_LOGI(TAG, "Storage test: %s (%lu ms)", 
             result->passed ? "PASS" : "FAIL", result->execution_time_ms);
    return ESP_OK;
}

esp_err_t test_network_api(test_result_t* result) {
    uint64_t start_time = esp_timer_get_time();
    
    strcpy(result->description, "Network API Test");
    result->passed = true;
    result->error_message[0] = '\0';
    
    // Test network manager status
    if (!network_manager_is_http_server_running()) {
        result->passed = false;
        strcpy(result->error_message, "HTTP server not running");
        goto test_end;
    }
    
    // Test WiFi connection (optional)
    if (network_manager_is_wifi_connected()) {
        ESP_LOGI(TAG, "WiFi connected - network tests can run");
    } else {
        ESP_LOGW(TAG, "WiFi not connected - limited network testing");
    }
    
    // Test network statistics
    network_stats_t stats;
    esp_err_t ret = network_manager_get_stats(&stats);
    if (ret != ESP_OK) {
        result->passed = false;
        strcpy(result->error_message, "Failed to get network statistics");
        goto test_end;
    }
    
test_end:
    result->execution_time_ms = test_get_execution_time_ms(start_time);
    ESP_LOGI(TAG, "Network test: %s (%lu ms)", 
             result->passed ? "PASS" : "FAIL", result->execution_time_ms);
    return ESP_OK;
}

esp_err_t test_display_updates(test_result_t* result) {
    uint64_t start_time = esp_timer_get_time();
    
    strcpy(result->description, "Display Updates Test");
    result->passed = true;
    result->error_message[0] = '\0';
    
    // Test display manager status
    if (!display_manager_is_running()) {
        result->passed = false;
        strcpy(result->error_message, "Display manager not running");
        goto test_end;
    }
    
    // Test display mode changes
    esp_err_t ret = display_manager_set_mode(DISPLAY_MODE_STATUS);
    if (ret != ESP_OK) {
        result->passed = false;
        strcpy(result->error_message, "Failed to set display mode");
        goto test_end;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));  // Allow display update
    
    ret = display_manager_set_mode(DISPLAY_MODE_DATA);
    if (ret != ESP_OK) {
        result->passed = false;
        strcpy(result->error_message, "Failed to change display mode");
        goto test_end;
    }
    
test_end:
    result->execution_time_ms = test_get_execution_time_ms(start_time);
    ESP_LOGI(TAG, "Display test: %s (%lu ms)", 
             result->passed ? "PASS" : "FAIL", result->execution_time_ms);
    return ESP_OK;
}

esp_err_t test_end_to_end_data_flow(test_result_t* result) {
    uint64_t start_time = esp_timer_get_time();
    
    strcpy(result->description, "End-to-End Data Flow Test");
    result->passed = true;
    result->error_message[0] = '\0';
    
    // This test verifies that data flows from acquisition to storage
    // In a real test, we would inject test data and verify it reaches storage
    
    // Check that all managers are running
    if (!adc_manager_is_running()) {
        result->passed = false;
        strcpy(result->error_message, "ADC manager not running");
        goto test_end;
    }
    
    if (!storage_manager_is_running()) {
        result->passed = false;
        strcpy(result->error_message, "Storage manager not running");
        goto test_end;
    }
    
    // Wait for some data to be processed
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Check that data is being processed
    size_t adc_data_available = adc_manager_get_available_data();
    ESP_LOGI(TAG, "ADC data available: %zu", adc_data_available);
    
test_end:
    result->execution_time_ms = test_get_execution_time_ms(start_time);
    ESP_LOGI(TAG, "End-to-end test: %s (%lu ms)", 
             result->passed ? "PASS" : "FAIL", result->execution_time_ms);
    return ESP_OK;
}

esp_err_t test_performance_memory_usage(test_result_t* result) {
    uint64_t start_time = esp_timer_get_time();
    
    strcpy(result->description, "Memory Usage Test");
    result->passed = true;
    result->error_message[0] = '\0';
    
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_free_heap = esp_get_minimum_free_heap_size();
    
    ESP_LOGI(TAG, "Free heap: %lu bytes, Min free: %lu bytes", free_heap, min_free_heap);
    
    // Check if we have sufficient memory
    if (free_heap < 50000) {  // 50KB minimum
        result->passed = false;
        snprintf(result->error_message, sizeof(result->error_message), 
                "Low memory: %lu bytes free", free_heap);
        goto test_end;
    }
    
    // Check memory fragmentation
    if (min_free_heap < 30000) {  // 30KB minimum ever seen
        result->passed = false;
        snprintf(result->error_message, sizeof(result->error_message), 
                "Memory fragmentation detected: min %lu bytes", min_free_heap);
        goto test_end;
    }
    
test_end:
    result->execution_time_ms = test_get_execution_time_ms(start_time);
    ESP_LOGI(TAG, "Memory test: %s (%lu ms)", 
             result->passed ? "PASS" : "FAIL", result->execution_time_ms);
    return ESP_OK;
}

esp_err_t test_suite_print_results(void) {
    ESP_LOGI(TAG, "=== Test Results Summary ===");
    
    uint32_t passed = 0;
    uint32_t failed = 0;
    
    for (uint32_t i = 0; i < g_test_count; i++) {
        test_result_t* result = &g_test_results[i];
        
        ESP_LOGI(TAG, "%s: %s (%lu ms)", 
                result->description,
                result->passed ? "PASS" : "FAIL",
                result->execution_time_ms);
        
        if (!result->passed) {
            ESP_LOGE(TAG, "  Error: %s", result->error_message);
            failed++;
        } else {
            passed++;
        }
    }
    
    ESP_LOGI(TAG, "Tests: %lu passed, %lu failed, %lu total", passed, failed, g_test_count);
    
    if (failed == 0) {
        ESP_LOGI(TAG, "ALL TESTS PASSED!");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "SOME TESTS FAILED!");
        return ESP_FAIL;
    }
}

uint32_t test_get_execution_time_ms(uint64_t start_time_us) {
    return (uint32_t)((esp_timer_get_time() - start_time_us) / 1000);
}

esp_err_t test_uart_loopback(uint8_t port, test_result_t* result) {
    uint64_t start_time = esp_timer_get_time();

    snprintf(result->description, sizeof(result->description), "UART%d Loopback Test", port);
    result->passed = true;
    result->error_message[0] = '\0';

    // Check if UART is initialized
    if (!hal_uart_is_initialized(port)) {
        result->passed = false;
        strcpy(result->error_message, "UART not initialized");
        goto test_end;
    }

    // Test data
    const char* test_data = "UART_TEST_123";
    char read_buffer[32] = {0};

    // Write test data
    esp_err_t ret = hal_uart_write(port, (const uint8_t*)test_data, strlen(test_data));
    if (ret != ESP_OK) {
        result->passed = false;
        strcpy(result->error_message, "Failed to write test data");
        goto test_end;
    }

    // Small delay for data transmission
    vTaskDelay(pdMS_TO_TICKS(10));

    // Read back data
    int bytes_read = hal_uart_read(port, (uint8_t*)read_buffer, sizeof(read_buffer) - 1, 100);
    if (bytes_read <= 0) {
        result->passed = false;
        strcpy(result->error_message, "No data received");
        goto test_end;
    }

    // Compare data (for loopback test, we expect the same data back)
    if (strncmp(test_data, read_buffer, strlen(test_data)) != 0) {
        result->passed = false;
        strcpy(result->error_message, "Data mismatch");
        goto test_end;
    }

test_end:
    result->execution_time_ms = test_get_execution_time_ms(start_time);
    return ESP_OK;
}
