#include "unity.h"
#include "data_logger.h"
#include "config.h"
#include "hal.h"
#include "test_suite.h"
#include "esp_log.h"

static const char* TAG = "TEST_DATA_LOGGER";

void setUp(void) {
    // Setup before each test
    ESP_LOGI(TAG, "Setting up test");
}

void tearDown(void) {
    // Cleanup after each test
    ESP_LOGI(TAG, "Tearing down test");
}

void test_data_logger_initialization(void) {
    ESP_LOGI(TAG, "Testing data logger initialization");
    
    // Initialize configuration first
    esp_err_t ret = config_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Initialize HAL
    ret = hal_system_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Initialize data logger
    ret = data_logger_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Check if data logger is running
    TEST_ASSERT_TRUE(data_logger_is_running());
}

void test_data_logger_self_test(void) {
    ESP_LOGI(TAG, "Testing data logger self test");
    
    // Run self test
    esp_err_t ret = data_logger_run_self_test();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void test_data_logger_full_test_suite(void) {
    ESP_LOGI(TAG, "Testing full test suite");
    
    // Run full test suite
    esp_err_t ret = data_logger_run_full_test_suite();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void test_configuration_system(void) {
    ESP_LOGI(TAG, "Testing configuration system");
    
    test_result_t result;
    esp_err_t ret = test_config_system(&result);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(result.passed);
}

void test_hal_initialization(void) {
    ESP_LOGI(TAG, "Testing HAL initialization");
    
    test_result_t result;
    esp_err_t ret = test_hal_initialization(&result);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(result.passed);
}

void test_adc_readings(void) {
    ESP_LOGI(TAG, "Testing ADC readings");
    
    test_result_t result;
    esp_err_t ret = test_adc_readings(&result);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(result.passed);
}

void test_storage_operations(void) {
    ESP_LOGI(TAG, "Testing storage operations");
    
    test_result_t result;
    esp_err_t ret = test_storage_write_read(&result);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(result.passed);
}

void test_network_functionality(void) {
    ESP_LOGI(TAG, "Testing network functionality");
    
    test_result_t result;
    esp_err_t ret = test_network_api(&result);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(result.passed);
}

void test_display_updates(void) {
    ESP_LOGI(TAG, "Testing display updates");
    
    test_result_t result;
    esp_err_t ret = test_display_updates(&result);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(result.passed);
}

void test_end_to_end_data_flow(void) {
    ESP_LOGI(TAG, "Testing end-to-end data flow");
    
    test_result_t result;
    esp_err_t ret = test_end_to_end_data_flow(&result);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(result.passed);
}

void test_memory_usage(void) {
    ESP_LOGI(TAG, "Testing memory usage");
    
    test_result_t result;
    esp_err_t ret = test_performance_memory_usage(&result);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(result.passed);
}

void test_uart_loopback(void) {
    ESP_LOGI(TAG, "Testing UART loopback");
    
    test_result_t result;
    esp_err_t ret = test_uart_loopback(0, &result);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(result.passed);
}
