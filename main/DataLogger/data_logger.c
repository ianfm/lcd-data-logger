#include "data_logger.h"
#include "uart_manager.h"
#include "adc_manager.h"
#include "storage_manager.h"
#include "network_manager.h"
#include "display_manager.h"
#include "test_suite.h"
#include "hal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "DATA_LOGGER";

// Data coordination task
static TaskHandle_t g_data_coordination_task = NULL;
static bool g_data_logger_running = false;

// Data coordination task - bridges data acquisition and storage
static void data_coordination_task(void* pvParameters) {
    ESP_LOGI(TAG, "Data coordination task started");

    uart_data_packet_t uart_packet;
    adc_data_packet_t adc_packet;

    while (g_data_logger_running) {
        // Process UART data
        for (int port = 0; port < CONFIG_UART_PORT_COUNT; port++) {
            if (uart_manager_is_channel_active(port)) {
                if (uart_manager_get_data(port, &uart_packet, 10) == ESP_OK) {
                    // Forward to storage
                    storage_manager_write_uart_data(uart_packet.port,
                                                   uart_packet.data,
                                                   uart_packet.length);
                }
            }
        }

        // Process ADC data
        if (adc_manager_is_running()) {
            if (adc_manager_get_data(&adc_packet, 10) == ESP_OK) {
                // Forward to storage
                storage_manager_write_adc_data(adc_packet.channel,
                                             adc_packet.filtered_voltage,
                                             adc_packet.raw_value);
            }
        }

        // Small delay to prevent task starvation
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGI(TAG, "Data coordination task stopped");
    vTaskDelete(NULL);
}

esp_err_t data_logger_init(void) {
    ESP_LOGI(TAG, "Initializing Data Logger Core");

    // Initialize UART Manager
    esp_err_t ret = uart_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize UART Manager: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize ADC Manager
    ret = adc_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC Manager: %s", esp_err_to_name(ret));
        return ret;
    }

    // TODO Ian: POTENTIAL CONFLICT - storage_manager uses SD card filesystem
    // which may conflict with SD_Init() in main.c if both try to mount same SD card
    // Currently no direct conflict as storage_manager doesn't mount SD, just uses files
    // Initialize Storage Manager
    ret = storage_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Storage Manager: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize Network Manager (now the single source of WiFi functionality)
    ret = network_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Network Manager: %s", esp_err_to_name(ret));
        return ret;
    }

    // TODO Ian: POTENTIAL CONFLICT - display_manager_init() would conflict with LVGL_Init()
    // in main.c if both try to initialize LVGL system (currently disabled to avoid conflict)
    // Initialize Display Manager (disabled to avoid conflict with original LVGL demo)
    // ret = display_manager_init();
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to initialize Display Manager: %s", esp_err_to_name(ret));
    //     return ret;
    // }

    ESP_LOGI(TAG, "Data Logger Core initialized");
    return ESP_OK;
}

esp_err_t data_logger_start(void) {
    ESP_LOGI(TAG, "Starting Data Logger");

    // Start Storage Manager first
    esp_err_t ret = storage_manager_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Storage Manager: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start UART Manager
    ret = uart_manager_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start UART Manager: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start ADC Manager
    ret = adc_manager_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start ADC Manager: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start Network Manager
    ret = network_manager_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Network Manager: %s", esp_err_to_name(ret));
        // Continue without network - not critical for basic operation
    }

    // Start Display Manager (disabled to avoid conflict with original LVGL demo)
    // ret = display_manager_start();
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to start Display Manager: %s", esp_err_to_name(ret));
    //     // Continue without display - not critical for basic operation
    // }

    // Start data coordination task
    BaseType_t task_ret = xTaskCreate(data_coordination_task, "data_coord", 4096, NULL, 5, &g_data_coordination_task);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create data coordination task");
        return ESP_ERR_NO_MEM;
    }

    g_data_logger_running = true;
    ESP_LOGI(TAG, "Data Logger started successfully");
    return ESP_OK;
}

esp_err_t data_logger_stop(void) {
    ESP_LOGI(TAG, "Stopping Data Logger");

    // Stop managers
    adc_manager_stop();
    // uart_manager_stop(); // Will implement this function

    ESP_LOGI(TAG, "Data Logger stopped");
    return ESP_OK;
}

esp_err_t data_logger_deinit(void) {
    ESP_LOGI(TAG, "Deinitializing Data Logger");

    // Stop first
    data_logger_stop();

    // Deinitialize managers
    // adc_manager_deinit(); // Will implement this function
    // uart_manager_deinit(); // Will implement this function

    ESP_LOGI(TAG, "Data Logger deinitialized");
    return ESP_OK;
}

esp_err_t data_logger_print_status(void) {
    ESP_LOGI(TAG, "=== Data Logger Status ===");
    ESP_LOGI(TAG, "Running: %s", g_data_logger_running ? "Yes" : "No");

    // Print component status
    uart_manager_print_stats();
    adc_manager_print_stats();
    storage_manager_print_stats();
    network_manager_print_stats();

    // Display status
    if (display_manager_is_running()) {
        uint32_t update_count;
        uint64_t last_update;
        display_manager_get_stats(&update_count, &last_update);
        ESP_LOGI(TAG, "Display: %lu updates, last: %llu us", update_count, last_update);
    }

    return ESP_OK;
}

esp_err_t data_logger_run_self_test(void) {
    ESP_LOGI(TAG, "Running Data Logger Self Test");

    // Test configuration
    system_config_t* config = config_get_instance();
    if (!config) {
        ESP_LOGE(TAG, "Self Test FAILED: Configuration not available");
        return ESP_FAIL;
    }

    // Test HAL
    if (!hal_is_initialized()) {
        ESP_LOGE(TAG, "Self Test FAILED: HAL not initialized");
        return ESP_FAIL;
    }

    // Test managers
    if (!adc_manager_is_running()) {
        ESP_LOGW(TAG, "Self Test WARNING: ADC Manager not running");
    }

    if (!storage_manager_is_running()) {
        ESP_LOGW(TAG, "Self Test WARNING: Storage Manager not running");
    }

    if (!network_manager_is_wifi_connected()) {
        ESP_LOGW(TAG, "Self Test WARNING: WiFi not connected");
    }

    ESP_LOGI(TAG, "Self Test PASSED");
    return ESP_OK;
}

bool data_logger_is_running(void) {
    return g_data_logger_running;
}

esp_err_t data_logger_run_full_test_suite(void) {
    ESP_LOGI(TAG, "Running Full Test Suite");

    // Show test message on display (disabled - using original LVGL demo)
    // if (display_manager_is_running()) {
    //     display_manager_show_message("Testing", "Running test suite...", 2000);
    // }

    // Run comprehensive test suite
    esp_err_t ret = test_suite_run_all();

    // Show results on display (disabled - using original LVGL demo)
    // if (display_manager_is_running()) {
    //     if (ret == ESP_OK) {
    //         display_manager_show_message("Tests", "All tests PASSED!", 3000);
    //     } else {
    //         display_manager_show_message("Tests", "Some tests FAILED!", 3000);
    //     }
    // }

    return ret;
}
