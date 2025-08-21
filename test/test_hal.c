#include "unity.h"
#include "hal.h"
#include "config.h"
#include "esp_log.h"

static const char* TAG = "TEST_HAL";

void test_hal_system_initialization(void) {
    ESP_LOGI(TAG, "Testing HAL system initialization");
    
    // Initialize config first
    esp_err_t ret = config_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Initialize HAL
    ret = hal_system_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Check if HAL is initialized
    TEST_ASSERT_TRUE(hal_is_initialized());
}

void test_hal_uart_operations(void) {
    ESP_LOGI(TAG, "Testing HAL UART operations");
    
    // Check if UART 0 is initialized
    if (hal_uart_is_initialized(0)) {
        const char* test_data = "TEST";
        esp_err_t ret = hal_uart_write(0, (const uint8_t*)test_data, strlen(test_data));
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        
        // Small delay for transmission
        vTaskDelay(pdMS_TO_TICKS(10));
        
        uint8_t read_buffer[32];
        int bytes_read = hal_uart_read(0, read_buffer, sizeof(read_buffer), 100);
        TEST_ASSERT_GREATER_OR_EQUAL(0, bytes_read);
    } else {
        TEST_IGNORE_MESSAGE("UART 0 not initialized, skipping test");
    }
}

void test_hal_adc_operations(void) {
    ESP_LOGI(TAG, "Testing HAL ADC operations");
    
    // Check if ADC 0 is initialized
    if (hal_adc_is_initialized(0)) {
        int raw_value;
        esp_err_t ret = hal_adc_read_raw(0, &raw_value);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_GREATER_OR_EQUAL(0, raw_value);
        TEST_ASSERT_LESS_OR_EQUAL(4095, raw_value); // 12-bit ADC
        
        float voltage;
        ret = hal_adc_read_voltage(0, &voltage);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_GREATER_OR_EQUAL(0.0f, voltage);
        TEST_ASSERT_LESS_OR_EQUAL(4.0f, voltage); // Assuming 4V max
    } else {
        TEST_IGNORE_MESSAGE("ADC 0 not initialized, skipping test");
    }
}

void test_hal_gpio_operations(void) {
    ESP_LOGI(TAG, "Testing HAL GPIO operations");
    
    // Test GPIO set/get operations
    esp_err_t ret = hal_gpio_set_level(2, 1); // GPIO 2
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    int level = hal_gpio_get_level(2);
    TEST_ASSERT_EQUAL(1, level);
    
    ret = hal_gpio_set_level(2, 0);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    level = hal_gpio_get_level(2);
    TEST_ASSERT_EQUAL(0, level);
}
