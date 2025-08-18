#include "config.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "CONFIG";
static system_config_t g_system_config;
static bool g_config_initialized = false;

#define NVS_NAMESPACE "datalogger"
#define CONFIG_FILE_PATH CONFIG_SD_MOUNT_POINT "/config.json"

esp_err_t config_init(void) {
    if (g_config_initialized) {
        return ESP_OK;
    }
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Load default configuration
    config_load_defaults(&g_system_config);
    
    // Try to load from NVS, fall back to defaults if not found
    if (config_load_from_nvs(&g_system_config) != ESP_OK) {
        ESP_LOGI(TAG, "No saved configuration found, using defaults");
        config_save_to_nvs(&g_system_config);
    }
    
    g_config_initialized = true;
    ESP_LOGI(TAG, "Configuration system initialized");
    
    return ESP_OK;
}

esp_err_t config_load_defaults(system_config_t* config) {
    if (!config) return ESP_ERR_INVALID_ARG;
    
    memset(config, 0, sizeof(system_config_t));
    
    // Device Information
    strncpy(config->device_name, CONFIG_DEFAULT_DEVICE_NAME, sizeof(config->device_name) - 1);
    config->device_id = esp_random();
    
    // UART Configuration
    for (int i = 0; i < CONFIG_UART_PORT_COUNT; i++) {
        config->uart_config[i].enabled = true;
        config->uart_config[i].data_bits = 8;
        config->uart_config[i].stop_bits = 1;
        config->uart_config[i].parity = 0; // None
        config->uart_config[i].flow_control = false;
    }
    config->uart_config[0].baud_rate = CONFIG_UART1_DEFAULT_BAUD;
    config->uart_config[1].baud_rate = CONFIG_UART2_DEFAULT_BAUD;
    config->uart_config[2].baud_rate = CONFIG_UART3_DEFAULT_BAUD;
    
    // ADC Configuration
    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        config->adc_config[i].enabled = true;
        config->adc_config[i].sample_rate_hz = CONFIG_ADC_DEFAULT_SAMPLE_RATE;
        config->adc_config[i].voltage_scale = CONFIG_ADC_VOLTAGE_RANGE;
        config->adc_config[i].filter_alpha = CONFIG_ADC_FILTER_ALPHA;
        config->adc_config[i].attenuation = 3; // ADC_ATTEN_DB_11 for 0-3.3V
    }
    
    // WiFi Configuration
    strncpy(config->wifi_config.ssid, "Good Machine", sizeof(config->wifi_config.ssid) - 1);
    strncpy(config->wifi_config.password, "1500trains", sizeof(config->wifi_config.password) - 1);
    config->wifi_config.auto_connect = true;
    config->wifi_config.power_save_mode = 1; // WIFI_PS_MIN_MODEM
    
    // Storage Configuration
    config->storage_config.auto_start = true;
    config->storage_config.max_file_size_mb = CONFIG_MAX_FILE_SIZE_MB;
    config->storage_config.buffer_flush_interval_ms = CONFIG_BUFFER_FLUSH_INTERVAL_MS;
    config->storage_config.compress_files = false;
    config->storage_config.retention_days = 7;
    
    // Display Configuration
    config->display_config.enabled = true;
    config->display_config.brightness = 50;
    config->display_config.refresh_rate_ms = CONFIG_LCD_REFRESH_RATE_MS;
    config->display_config.auto_sleep_sec = CONFIG_LCD_AUTO_SLEEP_SEC;
    config->display_config.display_mode = 0; // Default display mode
    
    // Network Configuration
    config->network_config.http_port = CONFIG_HTTP_SERVER_PORT;
    config->network_config.websocket_port = CONFIG_WEBSOCKET_PORT;
    config->network_config.max_clients = CONFIG_MAX_CLIENTS;
    config->network_config.enable_cors = true;
    config->network_config.require_auth = false;
    memset(config->network_config.auth_token, 0, sizeof(config->network_config.auth_token));
    
    // System Configuration
    config->system_config.log_level = CONFIG_DEFAULT_LOG_LEVEL;
    config->system_config.enable_watchdog = true;
    config->system_config.task_stack_size = CONFIG_DEFAULT_TASK_STACK_SIZE;
    config->system_config.task_priority = CONFIG_DEFAULT_TASK_PRIORITY;
    
    return ESP_OK;
}

esp_err_t config_load_from_nvs(system_config_t* config) {
    if (!config) return ESP_ERR_INVALID_ARG;
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    size_t required_size = sizeof(system_config_t);
    err = nvs_get_blob(nvs_handle, "config", config, &required_size);
    
    nvs_close(nvs_handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Configuration loaded from NVS");
        return config_validate(config);
    }
    
    return err;
}

esp_err_t config_save_to_nvs(const system_config_t* config) {
    if (!config) return ESP_ERR_INVALID_ARG;
    
    esp_err_t err = config_validate(config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Configuration validation failed");
        return err;
    }
    
    nvs_handle_t nvs_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    err = nvs_set_blob(nvs_handle, "config", config, sizeof(system_config_t));
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Configuration saved to NVS");
        }
    }
    
    nvs_close(nvs_handle);
    return err;
}

esp_err_t config_validate(const system_config_t* config) {
    if (!config) return ESP_ERR_INVALID_ARG;
    
    // Validate UART configuration
    for (int i = 0; i < CONFIG_UART_PORT_COUNT; i++) {
        if (config->uart_config[i].enabled) {
            if (!CONFIG_VALIDATE_BAUD_RATE(config->uart_config[i].baud_rate)) {
                ESP_LOGE(TAG, "Invalid baud rate for UART%d: %lu", i, config->uart_config[i].baud_rate);
                return ESP_ERR_INVALID_ARG;
            }
        }
    }
    
    // Validate ADC configuration
    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        if (config->adc_config[i].enabled) {
            if (!CONFIG_VALIDATE_SAMPLE_RATE(config->adc_config[i].sample_rate_hz)) {
                ESP_LOGE(TAG, "Invalid sample rate for ADC%d: %d", i, config->adc_config[i].sample_rate_hz);
                return ESP_ERR_INVALID_ARG;
            }
        }
    }
    
    // Validate display configuration
    if (!CONFIG_VALIDATE_BRIGHTNESS(config->display_config.brightness)) {
        ESP_LOGE(TAG, "Invalid brightness: %d", config->display_config.brightness);
        return ESP_ERR_INVALID_ARG;
    }
    
    return ESP_OK;
}

system_config_t* config_get_instance(void) {
    if (!g_config_initialized) {
        config_init();
    }
    return &g_system_config;
}

esp_err_t config_update_uart(uint8_t port, uint32_t baud_rate, bool enabled) {
    if (!CONFIG_VALIDATE_UART_PORT(port)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (enabled && !CONFIG_VALIDATE_BAUD_RATE(baud_rate)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    g_system_config.uart_config[port].baud_rate = baud_rate;
    g_system_config.uart_config[port].enabled = enabled;
    
    return config_save_to_nvs(&g_system_config);
}

esp_err_t config_update_adc(uint8_t channel, uint16_t sample_rate, bool enabled) {
    if (!CONFIG_VALIDATE_ADC_CHANNEL(channel)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (enabled && !CONFIG_VALIDATE_SAMPLE_RATE(sample_rate)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    g_system_config.adc_config[channel].sample_rate_hz = sample_rate;
    g_system_config.adc_config[channel].enabled = enabled;
    
    return config_save_to_nvs(&g_system_config);
}

esp_err_t config_update_wifi(const char* ssid, const char* password) {
    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (strlen(ssid) >= CONFIG_MAX_WIFI_SSID_LEN || 
        strlen(password) >= CONFIG_MAX_WIFI_PASSWORD_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(g_system_config.wifi_config.ssid, ssid, sizeof(g_system_config.wifi_config.ssid) - 1);
    strncpy(g_system_config.wifi_config.password, password, sizeof(g_system_config.wifi_config.password) - 1);
    
    return config_save_to_nvs(&g_system_config);
}

esp_err_t config_update_display(uint8_t brightness, bool enabled) {
    if (!CONFIG_VALIDATE_BRIGHTNESS(brightness)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    g_system_config.display_config.brightness = brightness;
    g_system_config.display_config.enabled = enabled;
    
    return config_save_to_nvs(&g_system_config);
}

esp_err_t config_print(const system_config_t* config) {
    if (!config) return ESP_ERR_INVALID_ARG;
    
    ESP_LOGI(TAG, "=== System Configuration ===");
    ESP_LOGI(TAG, "Device: %s (ID: 0x%08lX)", config->device_name, config->device_id);
    
    ESP_LOGI(TAG, "UART Ports:");
    for (int i = 0; i < CONFIG_UART_PORT_COUNT; i++) {
        ESP_LOGI(TAG, "  Port %d: %s, %lu baud", i, 
                config->uart_config[i].enabled ? "Enabled" : "Disabled",
                config->uart_config[i].baud_rate);
    }
    
    ESP_LOGI(TAG, "ADC Channels:");
    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        ESP_LOGI(TAG, "  Channel %d: %s, %d Hz", i,
                config->adc_config[i].enabled ? "Enabled" : "Disabled",
                config->adc_config[i].sample_rate_hz);
    }
    
    ESP_LOGI(TAG, "WiFi: SSID=%s, Auto-connect=%s", 
            config->wifi_config.ssid,
            config->wifi_config.auto_connect ? "Yes" : "No");
    
    ESP_LOGI(TAG, "Display: %s, Brightness=%d%%", 
            config->display_config.enabled ? "Enabled" : "Disabled",
            config->display_config.brightness);
    
    ESP_LOGI(TAG, "Network: HTTP=%d, WebSocket=%d", 
            config->network_config.http_port,
            config->network_config.websocket_port);
    
    return ESP_OK;
}
