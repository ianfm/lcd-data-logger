/*
 * ESP32-C6 Data Logger Web Server
 *
 * Transforms the ESP32-C6 LCD demo into a comprehensive remote data logging
 * web server for embedded device debugging.
 */

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

// Original demo components
#include "ST7789.h"
#include "SD_SPI.h"
#include "RGB.h"
#include "Wireless.h"
#include "LVGL_Example.h"

// Data Logger components
#include "config.h"
#include "hal.h"
#include "data_logger.h"

static const char* TAG = "MAIN";



static esp_err_t system_init(void) {
    ESP_LOGI(TAG, "=== ESP32-C6 Data Logger Starting ===");

    // TODO Ian: DUPLICATION CONFLICT - config_init() calls nvs_flash_init()
    // but Wireless_Init() also calls nvs_flash_init() later
    // Initialize configuration system
    esp_err_t ret = config_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize configuration: %s", esp_err_to_name(ret));
        return ret;
    }

    // Print current configuration
    system_config_t* config = config_get_instance();
    config_print(config);

    // Initialize hardware abstraction layer (RE-ENABLING TO TEST)
    ESP_LOGI(TAG, "Re-enabling HAL initialization...");
    ret = hal_system_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize HAL: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize original demo components (RE-ENABLED)
    ESP_LOGI(TAG, "Re-enabling display initialization...");

    ESP_LOGI(TAG, "Initializing Flash...");
    Flash_Searching();

    ESP_LOGI(TAG, "Initializing RGB...");
    RGB_Init();

    // TODO Ian: POTENTIAL CONFLICT - SD_Init() here conflicts with storage_manager_init()
    // in DataLogger if both try to mount SD card filesystem
    ESP_LOGI(TAG, "Initializing SD...");
    SD_Init();

    ESP_LOGI(TAG, "Initializing LCD...");
    LCD_Init();

    ESP_LOGI(TAG, "Setting backlight...");
    BK_Light(config->display_config.brightness);

    // TODO Ian: POTENTIAL CONFLICT - LVGL_Init() here conflicts with display_manager_init()
    // in DataLogger if both try to initialize LVGL (currently display_manager is disabled)
    ESP_LOGI(TAG, "Initializing LVGL...");
    LVGL_Init();

    // Show ADC display immediately after LVGL is ready - don't wait for data logger
    ESP_LOGI(TAG, "Starting ADC display early for immediate feedback...");
    adc_display_init();
    ESP_LOGI(TAG, "ADC display started - user can see screen immediately");

    ESP_LOGI(TAG, "Display initialization complete");

    // WiFi initialization now handled by DataLogger network_manager
    // Original Wireless_Init() functionality integrated into network_manager_start()
    // This eliminates all initialization conflicts and provides better architecture
    ESP_LOGI(TAG, "WiFi will be initialized by DataLogger network manager");

    ESP_LOGI(TAG, "System initialization complete");
    return ESP_OK;
}

void app_main(void)
{
    // Initialize system
    esp_err_t ret = system_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "System initialization failed, restarting...");
        esp_restart();
    }

    // Initialize data logger (now with unified WiFi management)
    ESP_LOGI(TAG, "Initializing data logger with integrated network management...");
    ret = data_logger_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Data logger initialization failed: %s", esp_err_to_name(ret));
        // Continue with basic functionality
    }

    // Start data logger (includes WiFi scan + connection + HTTP server)
    ret = data_logger_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start data logger: %s", esp_err_to_name(ret));
    }

    // Run self test (ENABLED FOR TESTING)
    ret = data_logger_run_self_test();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Self test completed with warnings");
    }

    // Run full test suite (ENABLED FOR TESTING)
    ret = data_logger_run_full_test_suite();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Full test suite completed with failures");
    }

    // Print initial status
    data_logger_print_status();

    ESP_LOGI(TAG, "Data logger running, entering main loop");

    // Main application loop
    uint32_t status_counter = 0;
    uint32_t watchdog_counter = 0;
    uint32_t lvgl_error_count = 0;
    bool lvgl_enabled = true;

    while (1) {
        // Feed watchdog more frequently to prevent timeout
        if (++watchdog_counter >= 50) {  // Every 500ms
            watchdog_counter = 0;
            ESP_LOGD(TAG, "Main loop running, feeding watchdog");
        }

        // Handle LVGL updates (RE-ENABLED WITH SAFETY)
        if (lvgl_enabled) {
            uint32_t lvgl_start = esp_timer_get_time() / 1000;  // Convert to ms
            lv_timer_handler();
            uint32_t lvgl_duration = (esp_timer_get_time() / 1000) - lvgl_start;

            // Log if LVGL takes too long
            if (lvgl_duration > 100) {  // More than 100ms
                ESP_LOGW(TAG, "LVGL handler took %lu ms", lvgl_duration);
                lvgl_error_count++;

                // Disable LVGL if it's consistently problematic
                if (lvgl_error_count > 10) {
                    ESP_LOGE(TAG, "LVGL consistently slow, disabling to prevent watchdog timeout");
                    lvgl_enabled = false;
                }
            }
        }

        // Periodic status reporting (every 30 seconds)
        if (++status_counter >= 3000) {  // 3000 * 10ms = 30 seconds
            status_counter = 0;
            data_logger_print_status();
        }

        // Small delay to prevent watchdog timeout
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
