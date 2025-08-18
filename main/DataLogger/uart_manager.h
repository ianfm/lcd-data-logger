#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "config.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// UART Manager Configuration
#define UART_BUFFER_SIZE            1024
#define UART_RING_BUFFER_SIZE       (8 * 1024)  // 8KB per channel
#define UART_MAX_PACKET_SIZE        256

// UART Data Packet Structure
typedef struct {
    uint64_t timestamp_us;      // Microsecond timestamp
    uint8_t port;               // UART port number
    uint16_t length;            // Data length
    uint32_t sequence;          // Sequence number
    uint8_t data[UART_MAX_PACKET_SIZE];  // Actual data
} uart_data_packet_t;

// UART Statistics
typedef struct {
    uint32_t total_packets;     // Total packets received
    uint32_t total_bytes;       // Total bytes received
    uint32_t dropped_packets;   // Packets dropped due to buffer full
    uint32_t error_count;       // UART errors
    uint64_t last_packet_time;  // Timestamp of last packet
} uart_stats_t;

// UART Channel Context
typedef struct {
    uint8_t port;               // UART port number
    bool active;                // Channel active flag
    TaskHandle_t task_handle;   // Task handle for this channel
    RingbufHandle_t ring_buffer; // Ring buffer for data
    uint32_t sequence_number;   // Current sequence number
    uint64_t last_activity;     // Last activity timestamp
    uart_stats_t stats;         // Channel statistics
} uart_channel_context_t;

// UART Manager Functions
esp_err_t uart_manager_init(void);
esp_err_t uart_manager_deinit(void);
esp_err_t uart_manager_start(void);
esp_err_t uart_manager_stop(void);

// Channel Management
esp_err_t uart_manager_start_channel(uint8_t port);
esp_err_t uart_manager_stop_channel(uint8_t port);
bool uart_manager_is_channel_active(uint8_t port);

// Data Access
esp_err_t uart_manager_get_data(uint8_t port, uart_data_packet_t* packet, uint32_t timeout_ms);
size_t uart_manager_get_available_data(uint8_t port);
esp_err_t uart_manager_flush_channel(uint8_t port);

// Statistics and Monitoring
esp_err_t uart_manager_get_stats(uint8_t port, uart_stats_t* stats);
esp_err_t uart_manager_reset_stats(uint8_t port);
esp_err_t uart_manager_print_stats(void);

// Configuration
esp_err_t uart_manager_reconfigure_channel(uint8_t port, uint32_t baud_rate);
esp_err_t uart_manager_enable_channel(uint8_t port, bool enable);

// Testing Functions
esp_err_t uart_manager_test_loopback(uint8_t port);
esp_err_t uart_manager_send_test_data(uint8_t port, const char* test_string);

#ifdef __cplusplus
}
#endif
