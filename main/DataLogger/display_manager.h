#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Display Manager Configuration
#define DISPLAY_MAX_STATUS_ITEMS    8
#define DISPLAY_MAX_DATA_ITEMS      6

// Display Modes
typedef enum {
    DISPLAY_MODE_STATUS = 0,    // System status screen
    DISPLAY_MODE_DATA = 1,      // Data monitoring screen
    DISPLAY_MODE_NETWORK = 2,   // Network information screen
    DISPLAY_MODE_CONFIG = 3,    // Configuration screen
    DISPLAY_MODE_OFF = 4        // Display off (power save)
} display_mode_t;

// LED Status Indicators
typedef enum {
    LED_STATUS_INIT = 0,        // System initializing
    LED_STATUS_RUNNING = 1,     // Normal operation
    LED_STATUS_ERROR = 2,       // Error condition
    LED_STATUS_WIFI_CONN = 3,   // WiFi connecting
    LED_STATUS_DATA_ACT = 4     // Data activity
} led_status_t;

// Display Manager Functions
esp_err_t display_manager_init(void);
esp_err_t display_manager_deinit(void);
esp_err_t display_manager_start(void);
esp_err_t display_manager_stop(void);

// Display Control
esp_err_t display_manager_set_mode(display_mode_t mode);
display_mode_t display_manager_get_mode(void);
esp_err_t display_manager_set_brightness(uint8_t brightness);
esp_err_t display_manager_enable_auto_sleep(bool enable);

// Screen Updates
esp_err_t display_manager_update_status_screen(void);
esp_err_t display_manager_update_data_screen(void);
esp_err_t display_manager_update_network_screen(void);
esp_err_t display_manager_update_config_screen(void);
esp_err_t display_manager_force_update(void);

// LED Control
esp_err_t display_manager_set_led_status(led_status_t status);
esp_err_t display_manager_set_led_color(uint8_t red, uint8_t green, uint8_t blue);
esp_err_t display_manager_update_led_status(void);
esp_err_t display_manager_led_blink(led_status_t status, uint32_t count);

// Custom Display Functions
esp_err_t display_manager_show_message(const char* title, const char* message, uint32_t duration_ms);
esp_err_t display_manager_show_progress(const char* title, uint8_t percentage);
esp_err_t display_manager_clear_screen(void);

// Status and Monitoring
bool display_manager_is_running(void);
esp_err_t display_manager_get_stats(uint32_t* update_count, uint64_t* last_update);

#ifdef __cplusplus
}
#endif
