#include "uart_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "hal.h"
#include "config.h"
#include <string.h>

static const char* TAG = "UART_MGR";

// UART Manager State
typedef struct {
    bool initialized;
    bool running;
    uart_channel_context_t channels[CONFIG_UART_PORT_COUNT];
} uart_manager_state_t;

static uart_manager_state_t g_uart_manager = {0};

// UART Task Function
static void uart_task(void* pvParameters) {
    uart_channel_context_t* channel = (uart_channel_context_t*)pvParameters;
    uint8_t* data_buffer = malloc(UART_BUFFER_SIZE);

    if (!data_buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer for UART%d", channel->port);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "UART%d task started", channel->port);

    while (channel->active) {
        // Read data from UART
        int len = hal_uart_read(channel->port, data_buffer, UART_BUFFER_SIZE, 100);

        if (len > 0) {
            // Create data packet
            uart_data_packet_t packet = {
                .timestamp_us = esp_timer_get_time(),
                .port = channel->port,
                .length = len,
                .sequence = channel->sequence_number++
            };

            // Copy data to packet
            memcpy(packet.data, data_buffer, len);

            // Send to ring buffer
            esp_err_t ret = xRingbufferSend(channel->ring_buffer, &packet,
                                          sizeof(uart_data_packet_t), pdMS_TO_TICKS(10));

            if (ret != pdTRUE) {
                ESP_LOGW(TAG, "UART%d ring buffer full, dropping data", channel->port);
                channel->stats.dropped_packets++;
            } else {
                channel->stats.total_packets++;
                channel->stats.total_bytes += len;
            }

            // Update activity timestamp
            channel->last_activity = esp_timer_get_time();
        }

        // Small delay to prevent task starvation
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    free(data_buffer);
    ESP_LOGI(TAG, "UART%d task stopped", channel->port);
    vTaskDelete(NULL);
}

esp_err_t uart_manager_init(void) {
    if (g_uart_manager.initialized) {
        ESP_LOGW(TAG, "UART Manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing UART Manager");

    // Initialize all channels
    system_config_t* config = config_get_instance();

    for (int i = 0; i < CONFIG_UART_PORT_COUNT; i++) {
        uart_channel_context_t* channel = &g_uart_manager.channels[i];

        channel->port = i;
        channel->active = false;
        channel->sequence_number = 0;
        channel->last_activity = 0;
        memset(&channel->stats, 0, sizeof(uart_stats_t));

        if (config->uart_config[i].enabled) {
            // Create ring buffer
            channel->ring_buffer = xRingbufferCreate(UART_RING_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
            if (!channel->ring_buffer) {
                ESP_LOGE(TAG, "Failed to create ring buffer for UART%d", i);
                return ESP_ERR_NO_MEM;
            }

            ESP_LOGI(TAG, "UART%d configured: %lu baud", i, config->uart_config[i].baud_rate);
        }
    }

    g_uart_manager.initialized = true;
    ESP_LOGI(TAG, "UART Manager initialized");

    return ESP_OK;
}

esp_err_t uart_manager_start(void) {
    if (!g_uart_manager.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_uart_manager.running) {
        ESP_LOGW(TAG, "UART Manager already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting UART Manager");

    system_config_t* config = config_get_instance();

    // Start enabled channels
    for (int i = 0; i < CONFIG_UART_PORT_COUNT; i++) {
        if (config->uart_config[i].enabled) {
            esp_err_t ret = uart_manager_start_channel(i);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start UART%d: %s", i, esp_err_to_name(ret));
                return ret;
            }
        }
    }

    g_uart_manager.running = true;
    ESP_LOGI(TAG, "UART Manager started");

    return ESP_OK;
}

esp_err_t uart_manager_start_channel(uint8_t port) {
    if (port >= CONFIG_UART_PORT_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    uart_channel_context_t* channel = &g_uart_manager.channels[port];

    if (channel->active) {
        ESP_LOGW(TAG, "UART%d already active", port);
        return ESP_OK;
    }

    // Create task for this channel
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "uart%d_task", port);

    BaseType_t ret = xTaskCreate(uart_task, task_name, 4096, channel, 5, &channel->task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task for UART%d", port);
        return ESP_ERR_NO_MEM;
    }

    channel->active = true;
    ESP_LOGI(TAG, "UART%d started", port);

    return ESP_OK;
}

esp_err_t uart_manager_get_data(uint8_t port, uart_data_packet_t* packet, uint32_t timeout_ms) {
    if (port >= CONFIG_UART_PORT_COUNT || !packet) {
        return ESP_ERR_INVALID_ARG;
    }

    uart_channel_context_t* channel = &g_uart_manager.channels[port];

    if (!channel->active || !channel->ring_buffer) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t item_size;
    uart_data_packet_t* received_packet = (uart_data_packet_t*)xRingbufferReceive(
        channel->ring_buffer, &item_size, pdMS_TO_TICKS(timeout_ms));

    if (received_packet) {
        memcpy(packet, received_packet, sizeof(uart_data_packet_t));
        vRingbufferReturnItem(channel->ring_buffer, received_packet);
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

esp_err_t uart_manager_get_stats(uint8_t port, uart_stats_t* stats) {
    if (port >= CONFIG_UART_PORT_COUNT || !stats) {
        return ESP_ERR_INVALID_ARG;
    }

    uart_channel_context_t* channel = &g_uart_manager.channels[port];
    memcpy(stats, &channel->stats, sizeof(uart_stats_t));

    return ESP_OK;
}

esp_err_t uart_manager_print_stats(void) {
    ESP_LOGI(TAG, "=== UART Manager Statistics ===");

    for (int i = 0; i < CONFIG_UART_PORT_COUNT; i++) {
        uart_channel_context_t* channel = &g_uart_manager.channels[i];

        ESP_LOGI(TAG, "UART%d: %s", i, channel->active ? "Active" : "Inactive");
        if (channel->active) {
            ESP_LOGI(TAG, "  Packets: %lu, Bytes: %lu, Dropped: %lu, Errors: %lu",
                    channel->stats.total_packets,
                    channel->stats.total_bytes,
                    channel->stats.dropped_packets,
                    channel->stats.error_count);
        }
    }

    return ESP_OK;
}

bool uart_manager_is_channel_active(uint8_t port) {
    if (port >= CONFIG_UART_PORT_COUNT) {
        return false;
    }
    return g_uart_manager.channels[port].active;
}

size_t uart_manager_get_available_data(uint8_t port) {
    if (port >= CONFIG_UART_PORT_COUNT) {
        return 0;
    }

    uart_channel_context_t* channel = &g_uart_manager.channels[port];
    if (!channel->ring_buffer) {
        return 0;
    }

    UBaseType_t items_waiting;
    vRingbufferGetInfo(channel->ring_buffer, NULL, NULL, NULL, NULL, &items_waiting);
    return items_waiting;
}

esp_err_t uart_manager_stop(void) {
    if (!g_uart_manager.running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping UART Manager");

    // Stop all channels
    for (int i = 0; i < CONFIG_UART_PORT_COUNT; i++) {
        if (g_uart_manager.channels[i].active) {
            uart_manager_stop_channel(i);
        }
    }

    g_uart_manager.running = false;
    ESP_LOGI(TAG, "UART Manager stopped");

    return ESP_OK;
}

esp_err_t uart_manager_stop_channel(uint8_t port) {
    if (port >= CONFIG_UART_PORT_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    uart_channel_context_t* channel = &g_uart_manager.channels[port];

    if (!channel->active) {
        return ESP_OK;
    }

    channel->active = false;

    // Wait for task to finish
    if (channel->task_handle) {
        // Task will delete itself when active becomes false
        vTaskDelay(pdMS_TO_TICKS(100));  // Give time for task to exit
        channel->task_handle = NULL;
    }

    ESP_LOGI(TAG, "UART%d stopped", port);

    return ESP_OK;
}
