#pragma once

#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Hardware Abstraction Layer for ESP32-C6-LCD-1.47

// UART Hardware Abstraction
typedef struct {
    uart_port_t port;
    gpio_num_t tx_pin;
    gpio_num_t rx_pin;
    uart_config_t config;
    bool initialized;
} hal_uart_t;

// ADC Hardware Abstraction
typedef struct {
    adc_channel_t channel;
    adc_cali_handle_t cali_handle;
    bool calibrated;
    bool initialized;
} hal_adc_t;

// System Hardware State
typedef struct {
    hal_uart_t uart_ports[CONFIG_UART_PORT_COUNT];
    hal_adc_t adc_channels[CONFIG_ADC_CHANNEL_COUNT];
    adc_oneshot_unit_handle_t shared_adc_handle;  // Shared ADC unit for all channels
    bool adc_unit_initialized;
    bool system_initialized;
} hal_system_t;

// Hardware Initialization Functions
esp_err_t hal_system_init(void);
esp_err_t hal_system_deinit(void);

// UART Hardware Functions
esp_err_t hal_uart_init(uint8_t port, uint32_t baud_rate);
esp_err_t hal_uart_deinit(uint8_t port);
esp_err_t hal_uart_write(uint8_t port, const uint8_t* data, size_t length);
int hal_uart_read(uint8_t port, uint8_t* buffer, size_t buffer_size, uint32_t timeout_ms);
esp_err_t hal_uart_flush(uint8_t port);
bool hal_uart_is_initialized(uint8_t port);

// ADC Hardware Functions
esp_err_t hal_adc_init(uint8_t channel);
esp_err_t hal_adc_deinit(uint8_t channel);
esp_err_t hal_adc_read_raw(uint8_t channel, int* raw_value);
esp_err_t hal_adc_read_voltage(uint8_t channel, float* voltage);
esp_err_t hal_adc_raw_to_voltage(uint8_t channel, int raw_value, float* voltage);
bool hal_adc_is_initialized(uint8_t channel);
bool hal_adc_is_calibrated(uint8_t channel);

// GPIO Helper Functions
esp_err_t hal_gpio_set_direction(gpio_num_t pin, gpio_mode_t mode);
esp_err_t hal_gpio_set_level(gpio_num_t pin, uint32_t level);
int hal_gpio_get_level(gpio_num_t pin);

// System Status Functions
bool hal_is_initialized(void);
esp_err_t hal_get_system_info(char* buffer, size_t buffer_size);
esp_err_t hal_reset_system(void);

// Hardware Test Functions (for debugging)
esp_err_t hal_test_uart_loopback(uint8_t port);
esp_err_t hal_test_adc_reading(uint8_t channel);
esp_err_t hal_test_all_hardware(void);

// Pin Mapping Definitions (ESP32-C6-LCD-1.47 specific)
#define HAL_UART_PORT_MAP { \
    {UART_NUM_0, CONFIG_UART1_TX_PIN, CONFIG_UART1_RX_PIN}, \
    {UART_NUM_1, CONFIG_UART2_TX_PIN, CONFIG_UART2_RX_PIN}  \
}

#define HAL_ADC_CHANNEL_MAP { \
    CONFIG_ADC1_PIN, \
    CONFIG_ADC2_PIN, \
    CONFIG_ADC3_PIN, \
    CONFIG_ADC4_PIN \
}

// Hardware Validation Macros
#define HAL_VALIDATE_UART_PORT(port) \
    ((port) < CONFIG_UART_PORT_COUNT)

#define HAL_VALIDATE_ADC_CHANNEL(ch) \
    ((ch) < CONFIG_ADC_CHANNEL_COUNT)

// Error Codes
#define HAL_ERR_NOT_INITIALIZED     (ESP_ERR_INVALID_STATE)
#define HAL_ERR_ALREADY_INITIALIZED (ESP_ERR_INVALID_STATE + 1)
#define HAL_ERR_HARDWARE_FAULT      (ESP_ERR_INVALID_STATE + 2)
#define HAL_ERR_CALIBRATION_FAILED  (ESP_ERR_INVALID_STATE + 3)

#ifdef __cplusplus
}
#endif
