#include "adc_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "hal.h"
#include "config.h"
#include <string.h>
#include <math.h>

static const char* TAG = "ADC_MGR";

// ADC Manager State
typedef struct {
    bool initialized;
    bool running;
    adc_channel_context_t channels[CONFIG_ADC_CHANNEL_COUNT];
    TaskHandle_t sampling_task;
    QueueHandle_t data_queue;
} adc_manager_state_t;

static adc_manager_state_t g_adc_manager = {0};

// Moving average filter implementation
static float apply_moving_average(adc_channel_context_t* channel, float new_value) {
    system_config_t* config = config_get_instance();
    float alpha = config->adc_config[channel->channel].filter_alpha;

    if (channel->filter_initialized) {
        channel->filtered_value = alpha * new_value + (1.0f - alpha) * channel->filtered_value;
    } else {
        channel->filtered_value = new_value;
        channel->filter_initialized = true;
    }

    return channel->filtered_value;
}

// ADC Sampling Task
static void adc_sampling_task(void* pvParameters) {
    ESP_LOGI(TAG, "ADC sampling task started");

    system_config_t* config = config_get_instance();
    TickType_t last_wake_time = xTaskGetTickCount();

    while (g_adc_manager.running) {
        uint64_t timestamp = esp_timer_get_time();

        // Sample all enabled channels
        for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
            if (!config->adc_config[i].enabled) {
                continue;
            }

            adc_channel_context_t* channel = &g_adc_manager.channels[i];

            // Read raw ADC value
            int raw_value;
            esp_err_t ret = hal_adc_read_raw(i, &raw_value);

            if (ret == ESP_OK) {
                // Convert to voltage
                float voltage;
                ret = hal_adc_read_voltage(i, &voltage);

                if (ret == ESP_OK) {
                    // Apply filtering
                    float filtered_voltage = apply_moving_average(channel, voltage);

                    // Create data packet
                    adc_data_packet_t packet = {
                        .timestamp_us = timestamp,
                        .channel = i,
                        .raw_value = raw_value,
                        .voltage = voltage,
                        .filtered_voltage = filtered_voltage,
                        .sequence = channel->sequence_number++
                    };

                    // Send to queue
                    if (xQueueSend(g_adc_manager.data_queue, &packet, 0) != pdTRUE) {
                        channel->stats.dropped_samples++;
                        ESP_LOGW(TAG, "ADC%d queue full, dropping sample", i);
                    } else {
                        channel->stats.total_samples++;
                        channel->last_sample_time = timestamp;

                        // Update min/max values
                        if (voltage < channel->stats.min_voltage || channel->stats.total_samples == 1) {
                            channel->stats.min_voltage = voltage;
                        }
                        if (voltage > channel->stats.max_voltage || channel->stats.total_samples == 1) {
                            channel->stats.max_voltage = voltage;
                        }

                        // Update running average
                        channel->stats.avg_voltage =
                            (channel->stats.avg_voltage * (channel->stats.total_samples - 1) + voltage) /
                            channel->stats.total_samples;
                    }
                } else {
                    channel->stats.error_count++;
                }
            } else {
                channel->stats.error_count++;
            }
        }

        // Calculate delay for desired sample rate
        uint16_t sample_rate = config->adc_config[0].sample_rate_hz;  // Use first channel's rate
        TickType_t delay_ticks = pdMS_TO_TICKS(1000 / sample_rate);

        vTaskDelayUntil(&last_wake_time, delay_ticks);
    }

    ESP_LOGI(TAG, "ADC sampling task stopped");
    vTaskDelete(NULL);
}

esp_err_t adc_manager_init(void) {
    if (g_adc_manager.initialized) {
        ESP_LOGW(TAG, "ADC Manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing ADC Manager");

    // Create data queue
    g_adc_manager.data_queue = xQueueCreate(ADC_QUEUE_SIZE, sizeof(adc_data_packet_t));
    if (!g_adc_manager.data_queue) {
        ESP_LOGE(TAG, "Failed to create ADC data queue");
        return ESP_ERR_NO_MEM;
    }

    // Initialize channel contexts
    system_config_t* config = config_get_instance();

    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        adc_channel_context_t* channel = &g_adc_manager.channels[i];

        channel->channel = i;
        channel->sequence_number = 0;
        channel->filter_initialized = false;
        channel->filtered_value = 0.0f;
        channel->last_sample_time = 0;
        memset(&channel->stats, 0, sizeof(adc_stats_t));

        if (config->adc_config[i].enabled) {
            ESP_LOGI(TAG, "ADC%d configured: %d Hz sample rate",
                    i, config->adc_config[i].sample_rate_hz);
        }
    }

    g_adc_manager.initialized = true;
    ESP_LOGI(TAG, "ADC Manager initialized");

    return ESP_OK;
}

esp_err_t adc_manager_start(void) {
    if (!g_adc_manager.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_adc_manager.running) {
        ESP_LOGW(TAG, "ADC Manager already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting ADC Manager");

    // Create sampling task
    BaseType_t ret = xTaskCreate(adc_sampling_task, "adc_sampling", 4096, NULL, 6, &g_adc_manager.sampling_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create ADC sampling task");
        return ESP_ERR_NO_MEM;
    }

    g_adc_manager.running = true;
    ESP_LOGI(TAG, "ADC Manager started");

    return ESP_OK;
}

esp_err_t adc_manager_stop(void) {
    if (!g_adc_manager.running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping ADC Manager");

    g_adc_manager.running = false;

    // Wait for task to finish
    if (g_adc_manager.sampling_task) {
        // Task will delete itself
        g_adc_manager.sampling_task = NULL;
    }

    ESP_LOGI(TAG, "ADC Manager stopped");
    return ESP_OK;
}

esp_err_t adc_manager_get_data(adc_data_packet_t* packet, uint32_t timeout_ms) {
    if (!packet) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_adc_manager.data_queue) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueReceive(g_adc_manager.data_queue, packet, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

esp_err_t adc_manager_get_stats(uint8_t channel, adc_stats_t* stats) {
    if (channel >= CONFIG_ADC_CHANNEL_COUNT || !stats) {
        return ESP_ERR_INVALID_ARG;
    }

    adc_channel_context_t* ch = &g_adc_manager.channels[channel];
    memcpy(stats, &ch->stats, sizeof(adc_stats_t));

    return ESP_OK;
}

esp_err_t adc_manager_print_stats(void) {
    ESP_LOGI(TAG, "=== ADC Manager Statistics ===");

    system_config_t* config = config_get_instance();

    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        adc_channel_context_t* channel = &g_adc_manager.channels[i];

        ESP_LOGI(TAG, "ADC%d: %s", i, config->adc_config[i].enabled ? "Enabled" : "Disabled");
        if (config->adc_config[i].enabled) {
            ESP_LOGI(TAG, "  Samples: %lu, Dropped: %lu, Errors: %lu",
                    channel->stats.total_samples,
                    channel->stats.dropped_samples,
                    channel->stats.error_count);
            ESP_LOGI(TAG, "  Voltage: %.3fV (min: %.3fV, max: %.3fV, avg: %.3fV)",
                    channel->filtered_value,
                    channel->stats.min_voltage,
                    channel->stats.max_voltage,
                    channel->stats.avg_voltage);
        }
    }

    return ESP_OK;
}

esp_err_t adc_manager_get_instant_reading(uint8_t channel, float* voltage) {
    if (channel >= CONFIG_ADC_CHANNEL_COUNT || !voltage) {
        return ESP_ERR_INVALID_ARG;
    }

    return hal_adc_read_voltage(channel, voltage);
}

bool adc_manager_is_running(void) {
    return g_adc_manager.running;
}

bool adc_manager_is_channel_enabled(uint8_t channel) {
    if (channel >= CONFIG_ADC_CHANNEL_COUNT) {
        return false;
    }

    system_config_t* config = config_get_instance();
    return config->adc_config[channel].enabled;
}

size_t adc_manager_get_available_data(void) {
    if (!g_adc_manager.data_queue) {
        return 0;
    }

    return uxQueueMessagesWaiting(g_adc_manager.data_queue);
}

esp_err_t adc_manager_deinit(void) {
    if (!g_adc_manager.initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing ADC Manager");

    // Stop first
    adc_manager_stop();

    // Clean up queue
    if (g_adc_manager.data_queue) {
        vQueueDelete(g_adc_manager.data_queue);
        g_adc_manager.data_queue = NULL;
    }

    // Clean up channel contexts
    memset(&g_adc_manager.channels, 0, sizeof(g_adc_manager.channels));

    g_adc_manager.initialized = false;
    ESP_LOGI(TAG, "ADC Manager deinitialized");

    return ESP_OK;
}
