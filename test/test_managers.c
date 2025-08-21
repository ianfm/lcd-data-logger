#include "unity.h"
#include "uart_manager.h"
#include "adc_manager.h"
#include "storage_manager.h"
#include "network_manager.h"
#include "display_manager.h"
#include "esp_log.h"

static const char* TAG = "TEST_MANAGERS";

void test_uart_manager(void) {
    ESP_LOGI(TAG, "Testing UART manager");
    
    // Initialize UART manager
    esp_err_t ret = uart_manager_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Check if manager is running
    TEST_ASSERT_TRUE(uart_manager_is_running());
    
    // Test data transmission (if UART is available)
    if (uart_manager_is_port_enabled(0)) {
        const char* test_data = "UART_TEST";
        ret = uart_manager_send_data(0, (const uint8_t*)test_data, strlen(test_data));
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
}

void test_adc_manager(void) {
    ESP_LOGI(TAG, "Testing ADC manager");
    
    // Initialize ADC manager
    esp_err_t ret = adc_manager_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Check if manager is running
    TEST_ASSERT_TRUE(adc_manager_is_running());
    
    // Test ADC reading (if channel is available)
    if (adc_manager_is_channel_enabled(0)) {
        float voltage;
        ret = adc_manager_get_instant_reading(0, &voltage);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_GREATER_OR_EQUAL(0.0f, voltage);
        TEST_ASSERT_LESS_OR_EQUAL(5.0f, voltage);
    }
}

void test_storage_manager(void) {
    ESP_LOGI(TAG, "Testing storage manager");
    
    // Initialize storage manager
    esp_err_t ret = storage_manager_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Check if manager is running
    TEST_ASSERT_TRUE(storage_manager_is_running());
    
    // Test data writing
    const char* test_data = "STORAGE_TEST";
    ret = storage_manager_write_uart_data(0, (const uint8_t*)test_data, strlen(test_data));
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Test ADC data writing
    ret = storage_manager_write_adc_data(0, 2.5f, 2048);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void test_network_manager(void) {
    ESP_LOGI(TAG, "Testing network manager");
    
    // Initialize network manager
    esp_err_t ret = network_manager_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Check if HTTP server is running
    TEST_ASSERT_TRUE(network_manager_is_http_server_running());
    
    // Test network statistics
    network_stats_t stats;
    ret = network_manager_get_stats(&stats);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // WiFi connection is optional for testing
    if (network_manager_is_wifi_connected()) {
        ESP_LOGI(TAG, "WiFi connected during test");
    } else {
        ESP_LOGW(TAG, "WiFi not connected during test");
    }
}

void test_display_manager(void) {
    ESP_LOGI(TAG, "Testing display manager");
    
    // Initialize display manager
    esp_err_t ret = display_manager_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Check if manager is running
    TEST_ASSERT_TRUE(display_manager_is_running());
    
    // Test display mode changes
    ret = display_manager_set_mode(DISPLAY_MODE_STATUS);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    vTaskDelay(pdMS_TO_TICKS(100)); // Allow display update
    
    ret = display_manager_set_mode(DISPLAY_MODE_DATA);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}
