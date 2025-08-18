#include "storage_manager.h"
#include "uart_manager.h"
#include "adc_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static const char* TAG = "STORAGE_MGR";

// Storage Manager State
typedef struct {
    bool initialized;
    bool running;
    TaskHandle_t storage_task;
    QueueHandle_t write_queue;
    log_file_t current_files[STORAGE_MAX_FILES];
    uint32_t total_files_created;
    uint64_t total_bytes_written;
    storage_stats_t stats;
} storage_manager_state_t;

static storage_manager_state_t g_storage_manager = {0};

// Generate filename with timestamp
static esp_err_t generate_filename(const char* prefix, char* filename, size_t max_len) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    snprintf(filename, max_len, "%s/%s_%04d%02d%02d_%02d%02d%02d.bin",
             CONFIG_SD_MOUNT_POINT, prefix,
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    return ESP_OK;
}

// Write data packet to file
static esp_err_t write_data_packet(log_file_t* log_file, const data_packet_t* packet) {
    if (!log_file || !log_file->file_handle || !packet) {
        return ESP_ERR_INVALID_ARG;
    }

    // Write packet header
    size_t written = fwrite(packet, sizeof(data_packet_t), 1, log_file->file_handle);
    if (written != 1) {
        ESP_LOGE(TAG, "Failed to write packet header to %s", log_file->filename);
        return ESP_FAIL;
    }

    log_file->current_size += sizeof(data_packet_t);
    log_file->record_count++;

    // Flush periodically for data integrity
    if (log_file->record_count % 10 == 0) {
        fflush(log_file->file_handle);
    }

    return ESP_OK;
}

// Storage task - handles data writing
static void storage_task(void* pvParameters) {
    ESP_LOGI(TAG, "Storage task started");

    storage_write_request_t request;

    while (g_storage_manager.running) {
        // Wait for write requests
        if (xQueueReceive(g_storage_manager.write_queue, &request, pdMS_TO_TICKS(100)) == pdTRUE) {

            // Find appropriate log file
            log_file_t* log_file = NULL;
            for (int i = 0; i < STORAGE_MAX_FILES; i++) {
                if (g_storage_manager.current_files[i].active &&
                    g_storage_manager.current_files[i].data_type == request.packet.data_type) {
                    log_file = &g_storage_manager.current_files[i];
                    break;
                }
            }

            // Create new file if needed
            if (!log_file) {
                for (int i = 0; i < STORAGE_MAX_FILES; i++) {
                    if (!g_storage_manager.current_files[i].active) {
                        log_file = &g_storage_manager.current_files[i];

                        // Generate filename based on data type
                        const char* prefix = (request.packet.data_type == DATA_TYPE_UART) ? "uart" : "adc";
                        generate_filename(prefix, log_file->filename, sizeof(log_file->filename));

                        // Open file
                        log_file->file_handle = fopen(log_file->filename, "wb");
                        if (!log_file->file_handle) {
                            ESP_LOGE(TAG, "Failed to create file: %s", log_file->filename);
                            g_storage_manager.stats.write_errors++;
                            continue;
                        }

                        log_file->active = true;
                        log_file->data_type = request.packet.data_type;
                        log_file->current_size = 0;
                        log_file->record_count = 0;
                        log_file->creation_time = esp_timer_get_time();

                        ESP_LOGI(TAG, "Created new log file: %s", log_file->filename);
                        g_storage_manager.total_files_created++;
                        break;
                    }
                }
            }

            // Write data
            if (log_file) {
                esp_err_t ret = write_data_packet(log_file, &request.packet);
                if (ret == ESP_OK) {
                    g_storage_manager.stats.total_writes++;
                    g_storage_manager.total_bytes_written += sizeof(data_packet_t);
                } else {
                    g_storage_manager.stats.write_errors++;
                }

                // Check if file rotation is needed
                system_config_t* config = config_get_instance();
                if (log_file->current_size >= (config->storage_config.max_file_size_mb * 1024 * 1024)) {
                    ESP_LOGI(TAG, "Rotating file: %s (size: %zu bytes)",
                            log_file->filename, log_file->current_size);

                    fclose(log_file->file_handle);
                    log_file->active = false;
                    log_file->file_handle = NULL;
                }
            }
        }

        // Periodic maintenance
        static uint32_t maintenance_counter = 0;
        if (++maintenance_counter >= 100) {  // Every ~10 seconds
            maintenance_counter = 0;
            // Flush all open files
            for (int i = 0; i < STORAGE_MAX_FILES; i++) {
                if (g_storage_manager.current_files[i].active &&
                    g_storage_manager.current_files[i].file_handle) {
                    fflush(g_storage_manager.current_files[i].file_handle);
                }
            }
        }
    }

    ESP_LOGI(TAG, "Storage task stopped");
    vTaskDelete(NULL);
}

esp_err_t storage_manager_init(void) {
    if (g_storage_manager.initialized) {
        ESP_LOGW(TAG, "Storage Manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing Storage Manager");

    // Create write queue
    g_storage_manager.write_queue = xQueueCreate(STORAGE_QUEUE_SIZE, sizeof(storage_write_request_t));
    if (!g_storage_manager.write_queue) {
        ESP_LOGE(TAG, "Failed to create storage write queue");
        return ESP_ERR_NO_MEM;
    }

    // Initialize file structures
    memset(g_storage_manager.current_files, 0, sizeof(g_storage_manager.current_files));
    memset(&g_storage_manager.stats, 0, sizeof(storage_stats_t));

    g_storage_manager.total_files_created = 0;
    g_storage_manager.total_bytes_written = 0;

    g_storage_manager.initialized = true;
    ESP_LOGI(TAG, "Storage Manager initialized");

    return ESP_OK;
}

esp_err_t storage_manager_start(void) {
    if (!g_storage_manager.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_storage_manager.running) {
        ESP_LOGW(TAG, "Storage Manager already running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting Storage Manager");

    // Create storage task
    BaseType_t ret = xTaskCreate(storage_task, "storage_task", 8192, NULL, 4, &g_storage_manager.storage_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create storage task");
        return ESP_ERR_NO_MEM;
    }

    g_storage_manager.running = true;
    ESP_LOGI(TAG, "Storage Manager started");

    return ESP_OK;
}

esp_err_t storage_manager_write_uart_data(uint8_t port, const uint8_t* data, size_t length) {
    if (!data || length == 0 || length > 256) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_storage_manager.running) {
        return ESP_ERR_INVALID_STATE;
    }

    // Create data packet
    data_packet_t* packet = malloc(sizeof(data_packet_t) + length);
    if (!packet) {
        return ESP_ERR_NO_MEM;
    }

    packet->magic = STORAGE_MAGIC_NUMBER;
    packet->timestamp_us = esp_timer_get_time();
    packet->source_id = port;
    packet->data_type = DATA_TYPE_UART;
    packet->data_length = length;
    packet->checksum = storage_calculate_checksum(data, length);
    memcpy(packet->data, data, length);

    // Create write request
    storage_write_request_t request = {
        .priority = STORAGE_DEFAULT_PRIORITY
    };
    memcpy(&request.packet, packet, sizeof(data_packet_t));

    // Send to queue
    esp_err_t ret = ESP_OK;
    if (xQueueSend(g_storage_manager.write_queue, &request, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGW(TAG, "Storage queue full, dropping UART data");
        ret = ESP_ERR_TIMEOUT;
    }

    free(packet);
    return ret;
}

esp_err_t storage_manager_write_adc_data(uint8_t channel, float voltage, int raw_value) {
    if (!g_storage_manager.running) {
        return ESP_ERR_INVALID_STATE;
    }

    // Create ADC data structure
    struct {
        float voltage;
        int raw_value;
    } adc_data = {voltage, raw_value};

    // Create data packet
    data_packet_t* packet = malloc(sizeof(data_packet_t) + sizeof(adc_data));
    if (!packet) {
        return ESP_ERR_NO_MEM;
    }

    packet->magic = STORAGE_MAGIC_NUMBER;
    packet->timestamp_us = esp_timer_get_time();
    packet->source_id = channel;
    packet->data_type = DATA_TYPE_ADC;
    packet->data_length = sizeof(adc_data);
    packet->checksum = storage_calculate_checksum((uint8_t*)&adc_data, sizeof(adc_data));
    memcpy(packet->data, &adc_data, sizeof(adc_data));

    // Create write request
    storage_write_request_t request = {
        .priority = STORAGE_DEFAULT_PRIORITY
    };
    memcpy(&request.packet, packet, sizeof(data_packet_t));

    // Send to queue
    esp_err_t ret = ESP_OK;
    if (xQueueSend(g_storage_manager.write_queue, &request, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGW(TAG, "Storage queue full, dropping ADC data");
        ret = ESP_ERR_TIMEOUT;
    }

    free(packet);
    return ret;
}

uint8_t storage_calculate_checksum(const uint8_t* data, size_t length) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

esp_err_t storage_manager_get_stats(storage_stats_t* stats) {
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(stats, &g_storage_manager.stats, sizeof(storage_stats_t));
    stats->files_created = g_storage_manager.total_files_created;
    stats->bytes_written = g_storage_manager.total_bytes_written;

    return ESP_OK;
}

esp_err_t storage_manager_print_stats(void) {
    ESP_LOGI(TAG, "=== Storage Manager Statistics ===");
    ESP_LOGI(TAG, "Total writes: %lu", g_storage_manager.stats.total_writes);
    ESP_LOGI(TAG, "Write errors: %lu", g_storage_manager.stats.write_errors);
    ESP_LOGI(TAG, "Files created: %lu", g_storage_manager.total_files_created);
    ESP_LOGI(TAG, "Bytes written: %llu", g_storage_manager.total_bytes_written);

    ESP_LOGI(TAG, "Active files:");
    for (int i = 0; i < STORAGE_MAX_FILES; i++) {
        if (g_storage_manager.current_files[i].active) {
            ESP_LOGI(TAG, "  %s: %zu bytes, %lu records",
                    g_storage_manager.current_files[i].filename,
                    g_storage_manager.current_files[i].current_size,
                    g_storage_manager.current_files[i].record_count);
        }
    }

    return ESP_OK;
}

bool storage_manager_is_running(void) {
    return g_storage_manager.running;
}

esp_err_t storage_manager_stop(void) {
    if (!g_storage_manager.running) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping Storage Manager");

    g_storage_manager.running = false;

    // Close all open files
    for (int i = 0; i < STORAGE_MAX_FILES; i++) {
        if (g_storage_manager.current_files[i].active &&
            g_storage_manager.current_files[i].file_handle) {
            fclose(g_storage_manager.current_files[i].file_handle);
            g_storage_manager.current_files[i].active = false;
            g_storage_manager.current_files[i].file_handle = NULL;
        }
    }

    ESP_LOGI(TAG, "Storage Manager stopped");
    return ESP_OK;
}
