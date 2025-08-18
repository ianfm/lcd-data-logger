#include "hal.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include <string.h>

static const char* TAG = "HAL";
static hal_system_t g_hal_system = {0};

// Pin mapping tables
static const struct {
    uart_port_t port;
    gpio_num_t tx_pin;
    gpio_num_t rx_pin;
} uart_pin_map[CONFIG_UART_PORT_COUNT] = HAL_UART_PORT_MAP;

static const adc_channel_t adc_channel_map[CONFIG_ADC_CHANNEL_COUNT] = HAL_ADC_CHANNEL_MAP;

esp_err_t hal_system_init(void) {
    if (g_hal_system.system_initialized) {
        ESP_LOGW(TAG, "HAL already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing Hardware Abstraction Layer");
    
    // Initialize UART ports based on configuration
    system_config_t* config = config_get_instance();
    for (int i = 0; i < CONFIG_UART_PORT_COUNT; i++) {
        if (config->uart_config[i].enabled) {
            esp_err_t ret = hal_uart_init(i, config->uart_config[i].baud_rate);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to initialize UART%d: %s", i, esp_err_to_name(ret));
                return ret;
            }
        }
    }
    
    // Initialize ADC channels based on configuration
    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        if (config->adc_config[i].enabled) {
            esp_err_t ret = hal_adc_init(i);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to initialize ADC%d: %s", i, esp_err_to_name(ret));
                return ret;
            }
        }
    }
    
    g_hal_system.system_initialized = true;
    ESP_LOGI(TAG, "HAL initialization complete");
    
    return ESP_OK;
}

esp_err_t hal_system_deinit(void) {
    if (!g_hal_system.system_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Deinitializing HAL");
    
    // Deinitialize UART ports
    for (int i = 0; i < CONFIG_UART_PORT_COUNT; i++) {
        if (g_hal_system.uart_ports[i].initialized) {
            hal_uart_deinit(i);
        }
    }
    
    // Deinitialize ADC channels
    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        if (g_hal_system.adc_channels[i].initialized) {
            hal_adc_deinit(i);
        }
    }
    
    memset(&g_hal_system, 0, sizeof(hal_system_t));
    ESP_LOGI(TAG, "HAL deinitialization complete");
    
    return ESP_OK;
}

esp_err_t hal_uart_init(uint8_t port, uint32_t baud_rate) {
    if (!HAL_VALIDATE_UART_PORT(port)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (g_hal_system.uart_ports[port].initialized) {
        ESP_LOGW(TAG, "UART%d already initialized", port);
        return ESP_OK;
    }
    
    hal_uart_t* uart = &g_hal_system.uart_ports[port];
    uart->port = uart_pin_map[port].port;
    uart->tx_pin = uart_pin_map[port].tx_pin;
    uart->rx_pin = uart_pin_map[port].rx_pin;
    
    // Configure UART parameters
    uart->config.baud_rate = baud_rate;
    uart->config.data_bits = UART_DATA_8_BITS;
    uart->config.parity = UART_PARITY_DISABLE;
    uart->config.stop_bits = UART_STOP_BITS_1;
    uart->config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart->config.rx_flow_ctrl_thresh = 122;
    uart->config.source_clk = UART_SCLK_DEFAULT;
    
    // Install UART driver
    esp_err_t ret = uart_driver_install(uart->port, 1024, 1024, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART%d driver: %s", port, esp_err_to_name(ret));
        return ret;
    }
    
    // Configure UART parameters
    ret = uart_param_config(uart->port, &uart->config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART%d: %s", port, esp_err_to_name(ret));
        uart_driver_delete(uart->port);
        return ret;
    }
    
    // Set UART pins
    ret = uart_set_pin(uart->port, uart->tx_pin, uart->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART%d pins: %s", port, esp_err_to_name(ret));
        uart_driver_delete(uart->port);
        return ret;
    }
    
    uart->initialized = true;
    ESP_LOGI(TAG, "UART%d initialized: %lu baud, TX=%d, RX=%d", 
             port, baud_rate, uart->tx_pin, uart->rx_pin);
    
    return ESP_OK;
}

esp_err_t hal_uart_deinit(uint8_t port) {
    if (!HAL_VALIDATE_UART_PORT(port)) {
        return ESP_ERR_INVALID_ARG;
    }
    
    hal_uart_t* uart = &g_hal_system.uart_ports[port];
    if (!uart->initialized) {
        return ESP_OK;
    }
    
    esp_err_t ret = uart_driver_delete(uart->port);
    if (ret == ESP_OK) {
        uart->initialized = false;
        ESP_LOGI(TAG, "UART%d deinitialized", port);
    }
    
    return ret;
}

int hal_uart_read(uint8_t port, uint8_t* buffer, size_t buffer_size, uint32_t timeout_ms) {
    if (!HAL_VALIDATE_UART_PORT(port) || !buffer) {
        return -1;
    }
    
    hal_uart_t* uart = &g_hal_system.uart_ports[port];
    if (!uart->initialized) {
        return -1;
    }
    
    return uart_read_bytes(uart->port, buffer, buffer_size, pdMS_TO_TICKS(timeout_ms));
}

esp_err_t hal_uart_write(uint8_t port, const uint8_t* data, size_t length) {
    if (!HAL_VALIDATE_UART_PORT(port) || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    hal_uart_t* uart = &g_hal_system.uart_ports[port];
    if (!uart->initialized) {
        return HAL_ERR_NOT_INITIALIZED;
    }
    
    int written = uart_write_bytes(uart->port, data, length);
    return (written == length) ? ESP_OK : ESP_FAIL;
}

esp_err_t hal_adc_init(uint8_t channel) {
    if (!HAL_VALIDATE_ADC_CHANNEL(channel)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (g_hal_system.adc_channels[channel].initialized) {
        ESP_LOGW(TAG, "ADC%d already initialized", channel);
        return ESP_OK;
    }

    hal_adc_t* adc = &g_hal_system.adc_channels[channel];
    adc->channel = adc_channel_map[channel];

    // Initialize shared ADC unit if not already done
    if (!g_hal_system.adc_unit_initialized) {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };

        esp_err_t ret = adc_oneshot_new_unit(&init_config, &g_hal_system.shared_adc_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create ADC unit: %s", esp_err_to_name(ret));
            return ret;
        }
        g_hal_system.adc_unit_initialized = true;
        ESP_LOGI(TAG, "Shared ADC unit initialized");
    }
    
    // Configure ADC channel
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,  // 0-3.3V range
    };

    esp_err_t ret = adc_oneshot_config_channel(g_hal_system.shared_adc_handle, adc->channel, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC%d: %s", channel, esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize calibration
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = adc->channel,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &adc->cali_handle);
    if (ret == ESP_OK) {
        adc->calibrated = true;
        ESP_LOGI(TAG, "ADC%d calibration enabled", channel);
    } else {
        ESP_LOGW(TAG, "ADC%d calibration not available: %s", channel, esp_err_to_name(ret));
        adc->calibrated = false;
    }
    
    adc->initialized = true;
    ESP_LOGI(TAG, "ADC%d initialized (channel %d)", channel, adc->channel);
    
    return ESP_OK;
}

esp_err_t hal_adc_deinit(uint8_t channel) {
    if (!HAL_VALIDATE_ADC_CHANNEL(channel)) {
        return ESP_ERR_INVALID_ARG;
    }

    hal_adc_t* adc = &g_hal_system.adc_channels[channel];
    if (!adc->initialized) {
        return ESP_OK;
    }

    // Delete calibration handle
    if (adc->cali_handle) {
        esp_err_t ret = adc_cali_delete_scheme_curve_fitting(adc->cali_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete ADC calibration: %s", esp_err_to_name(ret));
        }
        adc->cali_handle = NULL;
    }

    adc->initialized = false;
    adc->calibrated = false;
    ESP_LOGI(TAG, "ADC%d deinitialized", channel);

    // Check if all ADC channels are deinitialized, then delete shared unit
    bool all_deinit = true;
    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        if (g_hal_system.adc_channels[i].initialized) {
            all_deinit = false;
            break;
        }
    }

    if (all_deinit && g_hal_system.adc_unit_initialized) {
        esp_err_t ret = adc_oneshot_del_unit(g_hal_system.shared_adc_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete shared ADC unit: %s", esp_err_to_name(ret));
            return ret;
        }
        g_hal_system.shared_adc_handle = NULL;
        g_hal_system.adc_unit_initialized = false;
        ESP_LOGI(TAG, "Shared ADC unit deinitialized");
    }

    return ESP_OK;
}

esp_err_t hal_adc_read_raw(uint8_t channel, int* raw_value) {
    if (!HAL_VALIDATE_ADC_CHANNEL(channel) || !raw_value) {
        return ESP_ERR_INVALID_ARG;
    }

    hal_adc_t* adc = &g_hal_system.adc_channels[channel];
    if (!adc->initialized || !g_hal_system.adc_unit_initialized) {
        return HAL_ERR_NOT_INITIALIZED;
    }

    return adc_oneshot_read(g_hal_system.shared_adc_handle, adc->channel, raw_value);
}

esp_err_t hal_adc_read_voltage(uint8_t channel, float* voltage) {
    if (!HAL_VALIDATE_ADC_CHANNEL(channel) || !voltage) {
        return ESP_ERR_INVALID_ARG;
    }
    
    hal_adc_t* adc = &g_hal_system.adc_channels[channel];
    if (!adc->initialized) {
        return HAL_ERR_NOT_INITIALIZED;
    }
    
    int raw_value;
    esp_err_t ret = adc_oneshot_read(g_hal_system.shared_adc_handle, adc->channel, &raw_value);
    if (ret != ESP_OK) {
        return ret;
    }
    
    if (adc->calibrated) {
        int voltage_mv;
        ret = adc_cali_raw_to_voltage(adc->cali_handle, raw_value, &voltage_mv);
        if (ret == ESP_OK) {
            *voltage = voltage_mv / 1000.0f;  // Convert mV to V
        }
    } else {
        // Simple linear conversion without calibration
        *voltage = (raw_value / 4095.0f) * 3.3f;  // 12-bit ADC, 3.3V reference
    }
    
    return ret;
}

bool hal_uart_is_initialized(uint8_t port) {
    if (!HAL_VALIDATE_UART_PORT(port)) {
        return false;
    }
    return g_hal_system.uart_ports[port].initialized;
}

bool hal_adc_is_initialized(uint8_t channel) {
    if (!HAL_VALIDATE_ADC_CHANNEL(channel)) {
        return false;
    }
    return g_hal_system.adc_channels[channel].initialized;
}

bool hal_is_initialized(void) {
    return g_hal_system.system_initialized;
}
