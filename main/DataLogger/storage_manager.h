#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "config.h"
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Storage Manager Configuration
#define STORAGE_QUEUE_SIZE          50
#define STORAGE_MAX_FILES           8
#define STORAGE_MAX_FILENAME_LEN    128

// Data Types
typedef enum {
    DATA_TYPE_UART = 1,
    DATA_TYPE_ADC = 2,
    DATA_TYPE_SYSTEM = 3
} data_type_t;

// Generic Data Packet Structure
typedef struct __attribute__((packed)) {
    uint32_t magic;             // Magic number for validation (0xDEADBEEF)
    uint64_t timestamp_us;      // Microsecond timestamp
    uint8_t source_id;          // Source identifier (port/channel)
    uint8_t data_type;          // Data type (UART/ADC/SYSTEM)
    uint16_t data_length;       // Payload length
    uint8_t checksum;           // Simple checksum
    uint8_t data[];             // Variable length payload
} data_packet_t;

// Log File Structure
typedef struct {
    char filename[STORAGE_MAX_FILENAME_LEN];
    FILE* file_handle;
    bool active;
    data_type_t data_type;
    size_t current_size;
    uint32_t record_count;
    uint64_t creation_time;
} log_file_t;

// Storage Statistics
typedef struct {
    uint32_t total_writes;      // Total write operations
    uint32_t write_errors;      // Write errors
    uint32_t files_created;     // Files created
    uint32_t files_rotated;     // Files rotated
    uint64_t bytes_written;     // Total bytes written
    uint64_t last_write_time;   // Last write timestamp
} storage_stats_t;

// Storage Write Request
typedef struct {
    data_packet_t packet;
    uint32_t priority;          // Write priority (0 = highest)
} storage_write_request_t;

// Storage Manager Functions
esp_err_t storage_manager_init(void);
esp_err_t storage_manager_deinit(void);
esp_err_t storage_manager_start(void);
esp_err_t storage_manager_stop(void);
bool storage_manager_is_running(void);

// Data Writing
esp_err_t storage_manager_write_uart_data(uint8_t port, const uint8_t* data, size_t length);
esp_err_t storage_manager_write_adc_data(uint8_t channel, float voltage, int raw_value);
esp_err_t storage_manager_write_system_data(const char* message);
esp_err_t storage_manager_write_packet(const data_packet_t* packet);

// File Management
esp_err_t storage_manager_flush_all(void);
esp_err_t storage_manager_rotate_files(void);
esp_err_t storage_manager_close_all_files(void);
esp_err_t storage_manager_cleanup_old_files(uint32_t retention_days);

// Statistics and Monitoring
esp_err_t storage_manager_get_stats(storage_stats_t* stats);
esp_err_t storage_manager_reset_stats(void);
esp_err_t storage_manager_print_stats(void);
esp_err_t storage_manager_get_file_list(char* buffer, size_t buffer_size);

// Configuration
esp_err_t storage_manager_set_max_file_size(uint32_t size_mb);
esp_err_t storage_manager_enable_compression(bool enable);

// Utility Functions
uint8_t storage_calculate_checksum(const uint8_t* data, size_t length);
esp_err_t storage_create_data_packet(data_type_t type, uint8_t source_id, 
                                   const uint8_t* data, size_t length, 
                                   data_packet_t** packet);
esp_err_t storage_validate_packet(const data_packet_t* packet);

// Constants
#define STORAGE_MAGIC_NUMBER        0xDEADBEEF
#define STORAGE_DEFAULT_PRIORITY    5

#ifdef __cplusplus
}
#endif
