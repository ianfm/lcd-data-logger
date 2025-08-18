#include "display_manager.h"
#include "uart_manager.h"
#include "adc_manager.h"
#include "storage_manager.h"
#include "network_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "ST7789.h"
#include "RGB.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

static const char* TAG = "DISPLAY_MGR";

// Display Manager State
typedef struct {
    bool initialized;
    bool running;
    TaskHandle_t display_task;
    display_mode_t current_mode;
    uint64_t last_update;
    uint32_t update_counter;
    lv_obj_t* main_screen;
    lv_obj_t* status_labels[DISPLAY_MAX_STATUS_ITEMS];
    lv_obj_t* data_labels[DISPLAY_MAX_DATA_ITEMS];
} display_manager_state_t;

static display_manager_state_t g_display_manager = {0};

// LED Status Patterns
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint32_t duration_ms;
} led_pattern_t;

static const led_pattern_t led_patterns[] = {
    [LED_STATUS_INIT]       = {255, 255, 0,   500},  // Yellow - Initializing
    [LED_STATUS_RUNNING]    = {0,   255, 0,   1000}, // Green - Running
    [LED_STATUS_ERROR]      = {255, 0,   0,   250},  // Red - Error
    [LED_STATUS_WIFI_CONN]  = {0,   0,   255, 100},  // Blue - WiFi connecting
    [LED_STATUS_DATA_ACT]   = {0,   255, 255, 50},   // Cyan - Data activity
};

// Display update task
static void display_task(void* pvParameters) {
    ESP_LOGI(TAG, "Display task started");

    TickType_t last_wake_time = xTaskGetTickCount();
    system_config_t* config = config_get_instance();

    while (g_display_manager.running) {
        // Update display based on current mode
        switch (g_display_manager.current_mode) {
            case DISPLAY_MODE_STATUS:
                display_manager_update_status_screen();
                break;

            case DISPLAY_MODE_DATA:
                display_manager_update_data_screen();
                break;

            case DISPLAY_MODE_NETWORK:
                display_manager_update_network_screen();
                break;

            case DISPLAY_MODE_OFF:
                // Display is off, just update LED
                break;

            default:
                g_display_manager.current_mode = DISPLAY_MODE_STATUS;
                break;
        }

        // Update LED status
        display_manager_update_led_status();

        g_display_manager.update_counter++;
        g_display_manager.last_update = esp_timer_get_time();

        // Wait for next update
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(config->display_config.refresh_rate_ms));
    }

    ESP_LOGI(TAG, "Display task stopped");
    vTaskDelete(NULL);
}

esp_err_t display_manager_init(void) {
    if (g_display_manager.initialized) {
        ESP_LOGW(TAG, "Display Manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing Display Manager");

    // Initialize display state
    g_display_manager.current_mode = DISPLAY_MODE_STATUS;
    g_display_manager.last_update = 0;
    g_display_manager.update_counter = 0;

    // Create main screen
    g_display_manager.main_screen = lv_scr_act();

    // Initialize status labels
    for (int i = 0; i < DISPLAY_MAX_STATUS_ITEMS; i++) {
        g_display_manager.status_labels[i] = lv_label_create(g_display_manager.main_screen);
        lv_obj_set_pos(g_display_manager.status_labels[i], 10, 20 + i * 25);
        lv_obj_set_size(g_display_manager.status_labels[i], 150, 20);
        lv_label_set_text(g_display_manager.status_labels[i], "");
    }

    // Initialize data labels
    for (int i = 0; i < DISPLAY_MAX_DATA_ITEMS; i++) {
        g_display_manager.data_labels[i] = lv_label_create(g_display_manager.main_screen);
        lv_obj_set_pos(g_display_manager.data_labels[i], 10, 150 + i * 20);
        lv_obj_set_size(g_display_manager.data_labels[i], 150, 18);
        lv_label_set_text(g_display_manager.data_labels[i], "");
    }

    g_display_manager.initialized = true;
    ESP_LOGI(TAG, "Display Manager initialized");

    return ESP_OK;
}

esp_err_t display_manager_start(void) {
    if (!g_display_manager.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_display_manager.running) {
        ESP_LOGW(TAG, "Display Manager already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting Display Manager");

    // Set initial LED status
    display_manager_set_led_status(LED_STATUS_INIT);

    // Create display task
    BaseType_t ret = xTaskCreate(display_task, "display_task", 4096, NULL, 3, &g_display_manager.display_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display task");
        return ESP_ERR_NO_MEM;
    }

    g_display_manager.running = true;
    ESP_LOGI(TAG, "Display Manager started");

    return ESP_OK;
}

esp_err_t display_manager_update_status_screen(void) {
    char buffer[64];

    // System status
    snprintf(buffer, sizeof(buffer), "System: Running");
    lv_label_set_text(g_display_manager.status_labels[0], buffer);

    // WiFi status
    if (network_manager_is_wifi_connected()) {
        snprintf(buffer, sizeof(buffer), "WiFi: Connected");
    } else {
        snprintf(buffer, sizeof(buffer), "WiFi: Disconnected");
    }
    lv_label_set_text(g_display_manager.status_labels[1], buffer);

    // Storage status
    if (storage_manager_is_running()) {
        snprintf(buffer, sizeof(buffer), "Storage: Active");
    } else {
        snprintf(buffer, sizeof(buffer), "Storage: Inactive");
    }
    lv_label_set_text(g_display_manager.status_labels[2], buffer);

    // Memory status
    uint32_t free_heap = esp_get_free_heap_size();
    snprintf(buffer, sizeof(buffer), "Heap: %lu KB", free_heap / 1024);
    lv_label_set_text(g_display_manager.status_labels[3], buffer);

    // Uptime
    uint64_t uptime_sec = esp_timer_get_time() / 1000000;
    snprintf(buffer, sizeof(buffer), "Uptime: %llu s", uptime_sec);
    lv_label_set_text(g_display_manager.status_labels[4], buffer);

    return ESP_OK;
}

esp_err_t display_manager_update_data_screen(void) {
    char buffer[64];
    int label_index = 0;

    // UART data status
    for (int i = 0; i < CONFIG_UART_PORT_COUNT && label_index < DISPLAY_MAX_DATA_ITEMS; i++) {
        if (uart_manager_is_channel_active(i)) {
            uart_stats_t stats;
            if (uart_manager_get_stats(i, &stats) == ESP_OK) {
                snprintf(buffer, sizeof(buffer), "UART%d: %lu pkt", i, stats.total_packets);
                lv_label_set_text(g_display_manager.data_labels[label_index++], buffer);
            }
        }
    }

    // ADC data
    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT && label_index < DISPLAY_MAX_DATA_ITEMS; i++) {
        if (adc_manager_is_channel_enabled(i)) {
            float voltage;
            if (adc_manager_get_instant_reading(i, &voltage) == ESP_OK) {
                snprintf(buffer, sizeof(buffer), "ADC%d: %.2fV", i, voltage);
                lv_label_set_text(g_display_manager.data_labels[label_index++], buffer);
            }
        }
    }

    // Clear unused labels
    for (int i = label_index; i < DISPLAY_MAX_DATA_ITEMS; i++) {
        lv_label_set_text(g_display_manager.data_labels[i], "");
    }

    return ESP_OK;
}

esp_err_t display_manager_update_network_screen(void) {
    char buffer[64];

    // Network status
    if (network_manager_is_wifi_connected()) {
        snprintf(buffer, sizeof(buffer), "WiFi: Connected");
    } else {
        snprintf(buffer, sizeof(buffer), "WiFi: Disconnected");
    }
    lv_label_set_text(g_display_manager.status_labels[0], buffer);

    // HTTP server status
    if (network_manager_is_http_server_running()) {
        snprintf(buffer, sizeof(buffer), "HTTP: Running");
    } else {
        snprintf(buffer, sizeof(buffer), "HTTP: Stopped");
    }
    lv_label_set_text(g_display_manager.status_labels[1], buffer);

    // Network statistics
    network_stats_t stats;
    if (network_manager_get_stats(&stats) == ESP_OK) {
        snprintf(buffer, sizeof(buffer), "API Req: %lu", stats.api_requests);
        lv_label_set_text(g_display_manager.status_labels[2], buffer);

        snprintf(buffer, sizeof(buffer), "Bytes Sent: %lu", stats.bytes_sent);
        lv_label_set_text(g_display_manager.status_labels[3], buffer);
    }

    return ESP_OK;
}

esp_err_t display_manager_set_led_status(led_status_t status) {
    if (status >= sizeof(led_patterns) / sizeof(led_patterns[0])) {
        return ESP_ERR_INVALID_ARG;
    }

    const led_pattern_t* pattern = &led_patterns[status];

    // Set RGB LED color (using existing RGB functions)
    Set_RGB(pattern->red, pattern->green, pattern->blue);

    return ESP_OK;
}

esp_err_t display_manager_update_led_status(void) {
    // Determine current system status and set LED accordingly
    led_status_t status = LED_STATUS_RUNNING;

    if (!network_manager_is_wifi_connected()) {
        status = LED_STATUS_WIFI_CONN;
    } else if (adc_manager_get_available_data() > 0 ||
               uart_manager_get_available_data(0) > 0 ||
               uart_manager_get_available_data(1) > 0 ||
               uart_manager_get_available_data(2) > 0) {
        status = LED_STATUS_DATA_ACT;
    }

    return display_manager_set_led_status(status);
}

esp_err_t display_manager_set_mode(display_mode_t mode) {
    if (mode >= DISPLAY_MODE_OFF + 1) {
        return ESP_ERR_INVALID_ARG;
    }

    g_display_manager.current_mode = mode;

    // Handle display power management
    system_config_t* config = config_get_instance();
    if (mode == DISPLAY_MODE_OFF) {
        BK_Light(0);  // Turn off backlight
    } else {
        BK_Light(config->display_config.brightness);
    }

    ESP_LOGI(TAG, "Display mode changed to %d", mode);
    return ESP_OK;
}

display_mode_t display_manager_get_mode(void) {
    return g_display_manager.current_mode;
}

esp_err_t display_manager_set_brightness(uint8_t brightness) {
    if (brightness > 100) {
        return ESP_ERR_INVALID_ARG;
    }

    BK_Light(brightness);

    // Update configuration
    system_config_t* config = config_get_instance();
    config->display_config.brightness = brightness;
    config_save_to_nvs(config);

    ESP_LOGI(TAG, "Display brightness set to %d%%", brightness);
    return ESP_OK;
}

esp_err_t display_manager_show_message(const char* title, const char* message, uint32_t duration_ms) {
    if (!title || !message) {
        return ESP_ERR_INVALID_ARG;
    }

    // Clear screen
    lv_obj_clean(g_display_manager.main_screen);

    // Create title label
    lv_obj_t* title_label = lv_label_create(g_display_manager.main_screen);
    lv_obj_set_pos(title_label, 10, 10);
    lv_obj_set_size(title_label, 150, 30);
    lv_label_set_text(title_label, title);

    // Create message label
    lv_obj_t* msg_label = lv_label_create(g_display_manager.main_screen);
    lv_obj_set_pos(msg_label, 10, 50);
    lv_obj_set_size(msg_label, 150, 100);
    lv_label_set_text(msg_label, message);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);

    // Force display update
    lv_refr_now(NULL);

    // Wait for duration if specified
    if (duration_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(duration_ms));

        // Restore normal display
        display_manager_init();  // Reinitialize display elements
    }

    return ESP_OK;
}

bool display_manager_is_running(void) {
    return g_display_manager.running;
}

esp_err_t display_manager_get_stats(uint32_t* update_count, uint64_t* last_update) {
    if (update_count) {
        *update_count = g_display_manager.update_counter;
    }
    if (last_update) {
        *last_update = g_display_manager.last_update;
    }
    return ESP_OK;
}
