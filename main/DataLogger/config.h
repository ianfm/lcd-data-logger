#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Hardware Configuration
#define CONFIG_UART_PORT_COUNT          2  // ESP32-C6 has only UART0 and UART1
#define CONFIG_ADC_CHANNEL_COUNT        2  // ESP32-C6 - using only first 2 ADC channels
#define CONFIG_MAX_DEVICE_NAME_LEN      32
#define CONFIG_MAX_WIFI_SSID_LEN        32
#define CONFIG_MAX_WIFI_PASSWORD_LEN    64

// Default UART Configuration
#define CONFIG_UART1_DEFAULT_BAUD       9600
#define CONFIG_UART2_DEFAULT_BAUD       115200
#define CONFIG_UART3_DEFAULT_BAUD       38400

// Default ADC Configuration
#define CONFIG_ADC_DEFAULT_SAMPLE_RATE  100  // Hz
#define CONFIG_ADC_VOLTAGE_RANGE        4.0f // 0-4V
#define CONFIG_ADC_FILTER_ALPHA         0.1f // Moving average filter

// Storage Configuration
#define CONFIG_SD_MOUNT_POINT           "/sdcard"
#define CONFIG_LOG_FILE_PREFIX          "datalog"
#define CONFIG_MAX_FILE_SIZE_MB         100
#define CONFIG_BUFFER_FLUSH_INTERVAL_MS 1000

// Network Configuration
#define CONFIG_HTTP_SERVER_PORT         80
#define CONFIG_WEBSOCKET_PORT           8080
#define CONFIG_MAX_CLIENTS              5

// Display Configuration
#define CONFIG_LCD_REFRESH_RATE_MS      100
#define CONFIG_LCD_AUTO_SLEEP_SEC       300

// GPIO Pin Definitions (based on ESP32-C6-LCD-1.47)
#define CONFIG_UART1_TX_PIN             GPIO_NUM_20
#define CONFIG_UART1_RX_PIN             GPIO_NUM_19
#define CONFIG_UART2_TX_PIN             GPIO_NUM_18
#define CONFIG_UART2_RX_PIN             GPIO_NUM_17
#define CONFIG_UART3_TX_PIN             GPIO_NUM_16
#define CONFIG_UART3_RX_PIN             GPIO_NUM_10

#define CONFIG_ADC1_PIN                 ADC_CHANNEL_0  // GPIO0
#define CONFIG_ADC2_PIN                 ADC_CHANNEL_1  // GPIO1
#define CONFIG_ADC3_PIN                 ADC_CHANNEL_2  // GPIO2

// System Configuration Structure
typedef struct {
    // Device Information
    char device_name[CONFIG_MAX_DEVICE_NAME_LEN];
    uint32_t device_id;
    
    // UART Configuration
    struct {
        bool enabled;
        uint32_t baud_rate;
        uint8_t data_bits;
        uint8_t stop_bits;
        uint8_t parity;
        bool flow_control;
    } uart_config[CONFIG_UART_PORT_COUNT];
    
    // ADC Configuration
    struct {
        bool enabled;
        uint16_t sample_rate_hz;
        float voltage_scale;
        float filter_alpha;
        uint8_t attenuation;
    } adc_config[CONFIG_ADC_CHANNEL_COUNT];
    
    // WiFi Configuration
    struct {
        char ssid[CONFIG_MAX_WIFI_SSID_LEN];
        char password[CONFIG_MAX_WIFI_PASSWORD_LEN];
        bool auto_connect;
        int8_t power_save_mode;
    } wifi_config;
    
    // Storage Configuration
    struct {
        bool auto_start;
        uint32_t max_file_size_mb;
        uint32_t buffer_flush_interval_ms;
        bool compress_files;
        uint8_t retention_days;
    } storage_config;
    
    // Display Configuration
    struct {
        bool enabled;
        uint8_t brightness;
        uint32_t refresh_rate_ms;
        uint32_t auto_sleep_sec;
        uint8_t display_mode;
    } display_config;
    
    // Network Configuration
    struct {
        uint16_t http_port;
        uint16_t websocket_port;
        uint8_t max_clients;
        bool enable_cors;
        bool require_auth;
        char auth_token[64];
    } network_config;
    
    // System Configuration
    struct {
        uint8_t log_level;
        bool enable_watchdog;
        uint32_t task_stack_size;
        uint8_t task_priority;
    } system_config;
    
} system_config_t;

// Configuration Management Functions
esp_err_t config_init(void);
esp_err_t config_load_defaults(system_config_t* config);
esp_err_t config_load_from_nvs(system_config_t* config);
esp_err_t config_save_to_nvs(const system_config_t* config);
esp_err_t config_load_from_file(const char* filename, system_config_t* config);
esp_err_t config_save_to_file(const char* filename, const system_config_t* config);
esp_err_t config_validate(const system_config_t* config);
esp_err_t config_print(const system_config_t* config);

// Configuration Access Functions
system_config_t* config_get_instance(void);
esp_err_t config_update_uart(uint8_t port, uint32_t baud_rate, bool enabled);
esp_err_t config_update_adc(uint8_t channel, uint16_t sample_rate, bool enabled);
esp_err_t config_update_wifi(const char* ssid, const char* password);
esp_err_t config_update_display(uint8_t brightness, bool enabled);

// Configuration Validation Macros
#define CONFIG_VALIDATE_UART_PORT(port) \
    ((port) < CONFIG_UART_PORT_COUNT)

#define CONFIG_VALIDATE_ADC_CHANNEL(ch) \
    ((ch) < CONFIG_ADC_CHANNEL_COUNT)

#define CONFIG_VALIDATE_BAUD_RATE(baud) \
    ((baud) >= 300 && (baud) <= 921600)

#define CONFIG_VALIDATE_SAMPLE_RATE(rate) \
    ((rate) >= 1 && (rate) <= 10000)

#define CONFIG_VALIDATE_BRIGHTNESS(brightness) \
    ((brightness) <= 100)

// Default Configuration Values
#define CONFIG_DEFAULT_DEVICE_NAME      "ESP32-DataLogger"
#define CONFIG_DEFAULT_LOG_LEVEL        3  // ESP_LOG_INFO
#define CONFIG_DEFAULT_TASK_STACK_SIZE  4096
#define CONFIG_DEFAULT_TASK_PRIORITY    5

#ifdef __cplusplus
}
#endif
