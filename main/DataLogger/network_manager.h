#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "config.h"
#include "cJSON.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Network Manager Configuration
#define NETWORK_MAX_RETRY           5
#define NETWORK_WEBSOCKET_BUFFER    1024
#define NETWORK_MAX_CLIENTS         5

// Network Statistics
typedef struct {
    uint32_t api_requests;          // Total API requests
    uint32_t websocket_connections; // WebSocket connections
    uint32_t bytes_sent;            // Total bytes sent
    uint32_t bytes_received;        // Total bytes received
    uint32_t connection_errors;     // Connection errors
    uint64_t last_activity;         // Last network activity
} network_stats_t;

// WebSocket Message Types
typedef enum {
    WS_MSG_DATA = 1,
    WS_MSG_STATUS = 2,
    WS_MSG_CONFIG = 3,
    WS_MSG_ERROR = 4
} websocket_msg_type_t;

// Network Manager Functions
esp_err_t network_manager_init(void);
esp_err_t network_manager_deinit(void);
esp_err_t network_manager_start(void);
esp_err_t network_manager_stop(void);

// WiFi Management
esp_err_t network_manager_connect_wifi(const char* ssid, const char* password);
esp_err_t network_manager_disconnect_wifi(void);
bool network_manager_is_wifi_connected(void);
esp_err_t network_manager_get_ip_info(char* ip_str, size_t max_len);

// WiFi Scanning (replaces original Wireless module functionality)
esp_err_t network_manager_scan_wifi(uint16_t* ap_count);
esp_err_t network_manager_get_scan_results(wifi_ap_record_t* ap_records, uint16_t* ap_count);
bool network_manager_is_scan_complete(void);
uint16_t network_manager_get_wifi_count(void);
esp_err_t network_manager_perform_initial_scan(void);  // Replaces Wireless_Init() functionality

// Compatibility layer - global variables (replaces Wireless.c exports)
extern uint16_t WIFI_NUM;
extern uint16_t BLE_NUM;
extern bool Scan_finish;

// HTTP Server Management
esp_err_t network_manager_start_http_server(void);
esp_err_t network_manager_stop_http_server(void);
bool network_manager_is_http_server_running(void);

// WebSocket Management
esp_err_t network_manager_start_websocket_server(void);
esp_err_t network_manager_stop_websocket_server(void);
esp_err_t network_manager_broadcast_data(const char* json_data);
esp_err_t network_manager_send_websocket_message(int fd, websocket_msg_type_t type, const char* data);

// Statistics and Monitoring
esp_err_t network_manager_get_stats(network_stats_t* stats);
esp_err_t network_manager_reset_stats(void);
esp_err_t network_manager_print_stats(void);

// Configuration
esp_err_t network_manager_set_cors_enabled(bool enabled);
esp_err_t network_manager_set_auth_required(bool required);
esp_err_t network_manager_set_auth_token(const char* token);

// Utility Functions
esp_err_t network_manager_create_json_response(const char* status, const char* message, char** json_str);
esp_err_t network_manager_parse_json_request(const char* json_str, cJSON** json_obj);

#ifdef __cplusplus
}
#endif
