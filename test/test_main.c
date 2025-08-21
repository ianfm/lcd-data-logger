#include "unity.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "TEST_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32-C6 Data Logger Test Suite");
    
    // Initialize Unity test framework
    unity_run_menu();
}
