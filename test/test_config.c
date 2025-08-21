#include "unity.h"
#include "config.h"
#include "esp_log.h"

static const char* TAG = "TEST_CONFIG";

void test_config_initialization(void) {
    ESP_LOGI(TAG, "Testing config initialization");
    
    esp_err_t ret = config_init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    system_config_t* config = config_get_instance();
    TEST_ASSERT_NOT_NULL(config);
}

void test_config_validation(void) {
    ESP_LOGI(TAG, "Testing config validation");
    
    system_config_t* config = config_get_instance();
    TEST_ASSERT_NOT_NULL(config);
    
    esp_err_t ret = config_validate(config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void test_config_uart_update(void) {
    ESP_LOGI(TAG, "Testing UART config update");
    
    esp_err_t ret = config_update_uart(0, 9600, true);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    system_config_t* config = config_get_instance();
    TEST_ASSERT_NOT_NULL(config);
    TEST_ASSERT_EQUAL(9600, config->uart_config[0].baud_rate);
    TEST_ASSERT_TRUE(config->uart_config[0].enabled);
}

void test_config_adc_update(void) {
    ESP_LOGI(TAG, "Testing ADC config update");
    
    esp_err_t ret = config_update_adc(0, 1000, true);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    system_config_t* config = config_get_instance();
    TEST_ASSERT_NOT_NULL(config);
    TEST_ASSERT_EQUAL(1000, config->adc_config[0].sample_rate_hz);
    TEST_ASSERT_TRUE(config->adc_config[0].enabled);
}

void test_config_network_update(void) {
    ESP_LOGI(TAG, "Testing network config update");
    
    esp_err_t ret = config_update_network("test_ssid", "test_password", 8080);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    system_config_t* config = config_get_instance();
    TEST_ASSERT_NOT_NULL(config);
    TEST_ASSERT_EQUAL_STRING("test_ssid", config->network_config.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("test_password", config->network_config.wifi_password);
    TEST_ASSERT_EQUAL(8080, config->network_config.http_port);
}
