#include "network_manager.h"
#include "uart_manager.h"
#include "adc_manager.h"
#include "storage_manager.h"
#include "data_logger.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "cJSON.h"
#include "config.h"
#include <string.h>

// Compatibility layer - replaces original Wireless module global variables
uint16_t WIFI_NUM = 0;  // Number of WiFi APs found (replaces Wireless.c)
uint16_t BLE_NUM = 0;   // Number of BLE devices found (not implemented yet)
bool Scan_finish = 0;   // Scan completion status (replaces Wireless.c)

static const char* TAG = "NET_MGR";

// WiFi event bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_SCAN_DONE_BIT BIT2
#define WIFI_STA_START_BIT BIT3

// WiFi Scanning Configuration
#define NETWORK_MAX_SCAN_RESULTS 20

// WebSocket client tracking
typedef struct {
    httpd_handle_t server;
    int fd;
    bool active;
} websocket_client_t;

#define MAX_WEBSOCKET_CLIENTS 4

// Network Manager State
typedef struct {
    bool initialized;
    bool wifi_connected;
    bool http_server_running;
    httpd_handle_t http_server;
    EventGroupHandle_t wifi_event_group;
    int retry_count;
    network_stats_t stats;
    // WiFi Scanning (replaces original Wireless module)
    bool scan_complete;
    uint16_t wifi_ap_count;
    wifi_ap_record_t* scan_results;
    uint16_t max_scan_results;
    // WebSocket support
    websocket_client_t websocket_clients[MAX_WEBSOCKET_CLIENTS];
    TaskHandle_t websocket_task;
    QueueHandle_t websocket_queue;
    bool websocket_running;
} network_manager_state_t;

static network_manager_state_t g_network_manager = {0};

// WiFi Event Handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started");
        xEventGroupSetBits(g_network_manager.wifi_event_group, WIFI_STA_START_BIT);
        // Don't auto-connect here - let network_manager_connect_wifi handle it
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (g_network_manager.retry_count < NETWORK_MAX_RETRY) {
            esp_wifi_connect();
            g_network_manager.retry_count++;
            ESP_LOGI(TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(g_network_manager.wifi_event_group, WIFI_FAIL_BIT);
        }
        g_network_manager.wifi_connected = false;
        ESP_LOGI(TAG, "Connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        g_network_manager.retry_count = 0;
        g_network_manager.wifi_connected = true;
        xEventGroupSetBits(g_network_manager.wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        // WiFi scan completed - replaces original Wireless module functionality
        ESP_LOGI(TAG, "WiFi scan completed");
        g_network_manager.scan_complete = true;
        xEventGroupSetBits(g_network_manager.wifi_event_group, WIFI_SCAN_DONE_BIT);
    }
}

// HTTP API Handlers
static esp_err_t status_handler(httpd_req_t *req) {
    cJSON *json = cJSON_CreateObject();
    cJSON *status = cJSON_CreateString("running");
    cJSON *timestamp = cJSON_CreateNumber(esp_timer_get_time());
    cJSON *uptime = cJSON_CreateNumber(esp_timer_get_time() / 1000000);

    cJSON_AddItemToObject(json, "status", status);
    cJSON_AddItemToObject(json, "timestamp", timestamp);
    cJSON_AddItemToObject(json, "uptime_seconds", uptime);

    // Add system info
    cJSON *system = cJSON_CreateObject();
    cJSON *free_heap = cJSON_CreateNumber(esp_get_free_heap_size());
    cJSON *min_heap = cJSON_CreateNumber(esp_get_minimum_free_heap_size());
    cJSON_AddItemToObject(system, "free_heap", free_heap);
    cJSON_AddItemToObject(system, "min_free_heap", min_heap);
    cJSON_AddItemToObject(json, "system", system);

    char *json_string = cJSON_Print(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(json);

    g_network_manager.stats.api_requests++;
    return ESP_OK;
}

static esp_err_t data_latest_handler(httpd_req_t *req) {
    cJSON *json = cJSON_CreateObject();
    cJSON *timestamp = cJSON_CreateNumber(esp_timer_get_time());
    cJSON_AddItemToObject(json, "timestamp", timestamp);

    // Get UART data
    cJSON *uart_data = cJSON_CreateObject();
    for (int i = 0; i < CONFIG_UART_PORT_COUNT; i++) {
        if (uart_manager_is_channel_active(i)) {
            uart_data_packet_t packet;
            if (uart_manager_get_data(i, &packet, 0) == ESP_OK) {
                char port_name[16];
                snprintf(port_name, sizeof(port_name), "port%d", i);

                cJSON *port_data = cJSON_CreateObject();
                cJSON *data_str = cJSON_CreateString((char*)packet.data);
                cJSON *length = cJSON_CreateNumber(packet.length);
                cJSON *seq = cJSON_CreateNumber(packet.sequence);

                cJSON_AddItemToObject(port_data, "data", data_str);
                cJSON_AddItemToObject(port_data, "length", length);
                cJSON_AddItemToObject(port_data, "sequence", seq);
                cJSON_AddItemToObject(uart_data, port_name, port_data);
            }
        }
    }
    cJSON_AddItemToObject(json, "uart", uart_data);

    // Get ADC data from queue (latest samples)
    cJSON *adc_data = cJSON_CreateObject();
    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        if (adc_manager_is_channel_enabled(i)) {
            // Try to get latest queued data first
            adc_data_packet_t packet;
            bool got_queued_data = false;

            // Check if there's recent data in the queue for this channel
            size_t available = adc_manager_get_available_data();
            if (available > 0) {
                // Try to get the most recent sample for this channel
                if (adc_manager_get_data(&packet, 10) == ESP_OK && packet.channel == i) {
                    got_queued_data = true;
                }
            }

            char channel_name[16];
            snprintf(channel_name, sizeof(channel_name), "channel%d", i);
            cJSON *channel_data = cJSON_CreateObject();

            if (got_queued_data) {
                // Use queued data (from continuous sampling)
                cJSON *voltage_val = cJSON_CreateNumber(packet.filtered_voltage);
                cJSON *raw_val = cJSON_CreateNumber(packet.raw_value);
                cJSON *seq_val = cJSON_CreateNumber(packet.sequence);
                cJSON_AddItemToObject(channel_data, "voltage", voltage_val);
                cJSON_AddItemToObject(channel_data, "raw", raw_val);
                cJSON_AddItemToObject(channel_data, "sequence", seq_val);
            } else {
                // Fallback to instant reading if no queued data
                float voltage;
                if (adc_manager_get_instant_reading(i, &voltage) == ESP_OK) {
                    cJSON *voltage_val = cJSON_CreateNumber(voltage);
                    cJSON_AddItemToObject(channel_data, "voltage", voltage_val);
                    cJSON_AddItemToObject(channel_data, "source", cJSON_CreateString("instant"));
                }
            }

            cJSON_AddItemToObject(adc_data, channel_name, channel_data);
        }
    }
    cJSON_AddItemToObject(json, "adc", adc_data);

    char *json_string = cJSON_Print(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(json);

    g_network_manager.stats.api_requests++;
    return ESP_OK;
}

static esp_err_t config_get_handler(httpd_req_t *req) {
    system_config_t* config = config_get_instance();

    cJSON *json = cJSON_CreateObject();

    // Device info
    cJSON *device_name = cJSON_CreateString(config->device_name);
    cJSON_AddItemToObject(json, "device_name", device_name);

    // UART config
    cJSON *uart_config = cJSON_CreateArray();
    for (int i = 0; i < CONFIG_UART_PORT_COUNT; i++) {
        cJSON *uart = cJSON_CreateObject();
        cJSON *port = cJSON_CreateNumber(i);
        cJSON *enabled = cJSON_CreateBool(config->uart_config[i].enabled);
        cJSON *baud = cJSON_CreateNumber(config->uart_config[i].baud_rate);

        cJSON_AddItemToObject(uart, "port", port);
        cJSON_AddItemToObject(uart, "enabled", enabled);
        cJSON_AddItemToObject(uart, "baud_rate", baud);
        cJSON_AddItemToArray(uart_config, uart);
    }
    cJSON_AddItemToObject(json, "uart", uart_config);

    // ADC config
    cJSON *adc_config = cJSON_CreateArray();
    for (int i = 0; i < CONFIG_ADC_CHANNEL_COUNT; i++) {
        cJSON *adc = cJSON_CreateObject();
        cJSON *channel = cJSON_CreateNumber(i);
        cJSON *enabled = cJSON_CreateBool(config->adc_config[i].enabled);
        cJSON *sample_rate = cJSON_CreateNumber(config->adc_config[i].sample_rate_hz);

        cJSON_AddItemToObject(adc, "channel", channel);
        cJSON_AddItemToObject(adc, "enabled", enabled);
        cJSON_AddItemToObject(adc, "sample_rate", sample_rate);
        cJSON_AddItemToArray(adc_config, adc);
    }
    cJSON_AddItemToObject(json, "adc", adc_config);

    char *json_string = cJSON_Print(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(json);

    g_network_manager.stats.api_requests++;
    return ESP_OK;
}

static esp_err_t test_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Running test suite via API");

    cJSON *json = cJSON_CreateObject();
    cJSON *status = cJSON_CreateString("running");
    cJSON_AddItemToObject(json, "status", status);

    char *json_string = cJSON_Print(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(json);

    // Run test suite in background
    esp_err_t test_result = data_logger_run_full_test_suite();
    ESP_LOGI(TAG, "Test suite completed with result: %s",
             test_result == ESP_OK ? "PASS" : "FAIL");

    g_network_manager.stats.api_requests++;
    return ESP_OK;
}

// WebSocket handler based on ESP-IDF example
static esp_err_t websocket_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake done, new connection opened");

        // Register client
        int client_id = -1;
        for (int i = 0; i < MAX_WEBSOCKET_CLIENTS; i++) {
            if (!g_network_manager.websocket_clients[i].active) {
                client_id = i;
                break;
            }
        }

        if (client_id != -1) {
            g_network_manager.websocket_clients[client_id].server = req->handle;
            g_network_manager.websocket_clients[client_id].fd = httpd_req_to_sockfd(req);
            g_network_manager.websocket_clients[client_id].active = true;
            ESP_LOGI(TAG, "WebSocket client %d registered (fd: %d)", client_id,
                     g_network_manager.websocket_clients[client_id].fd);
        }

        return ESP_OK;
    }

    // Handle WebSocket frames
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    // First call to get frame length
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "WebSocket frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        // Allocate buffer for payload
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;

        // Second call to get frame payload
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got WebSocket packet with message: %s", ws_pkt.payload);
    }

    // Send welcome message back
    const char* welcome = "{\"type\":\"connected\",\"message\":\"ESP32 ADC stream ready\"}";
    httpd_ws_frame_t ws_response;
    memset(&ws_response, 0, sizeof(httpd_ws_frame_t));
    ws_response.payload = (uint8_t*)welcome;
    ws_response.len = strlen(welcome);
    ws_response.type = HTTPD_WS_TYPE_TEXT;

    ret = httpd_ws_send_frame(req, &ws_response);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
    }

    if (buf) {
        free(buf);
    }
    return ret;
}

static esp_err_t root_handler(httpd_req_t *req) {
    const char* html_page =
        "<!DOCTYPE html>"
        "<html><head><title>ESP32 Data Logger</title>"
        "<style>"
        "body { font-family: Arial, sans-serif; margin: 40px; }"
        ".container { max-width: 800px; margin: 0 auto; }"
        ".status { background: #f0f0f0; padding: 20px; border-radius: 5px; margin: 20px 0; }"
        ".button { background: #4CAF50; color: white; padding: 10px 20px; border: none; border-radius: 4px; cursor: pointer; margin: 5px; }"
        ".button:hover { background: #45a049; }"
        ".data { background: #e7f3ff; padding: 15px; border-radius: 5px; margin: 10px 0; }"
        "</style></head><body>"
        "<div class='container'>"
        "<h1>ESP32-C6 Data Logger</h1>"
        "<div class='status'>"
        "<h2>System Status</h2>"
        "<p>Data Logger: Running</p>"
        "<p>WiFi: Connected</p>"
        "<p>Storage: Active</p>"
        "</div>"
        "<div class='data'>"
        "<h2>Quick Actions</h2>"
        "<button class='button' onclick='runTest()'>Run Test Suite</button>"
        "<button class='button' onclick='getStatus()'>Get Status</button>"
        "<button class='button' onclick='getData()'>Get Latest Data</button>"
        "</div>"
        "<div id='results'></div>"
        "<script>"
        "function runTest() {"
        "  fetch('/api/test').then(r => r.json()).then(d => {"
        "    document.getElementById('results').innerHTML = '<div class=\"data\">Test Status: ' + d.status + '</div>';"
        "  });"
        "}"
        "function getStatus() {"
        "  fetch('/api/status').then(r => r.json()).then(d => {"
        "    document.getElementById('results').innerHTML = '<div class=\"data\">Uptime: ' + d.uptime_seconds + 's<br>Free Heap: ' + d.system.free_heap + ' bytes</div>';"
        "  });"
        "}"
        "function getData() {"
        "  fetch('/api/data/latest').then(r => r.json()).then(d => {"
        "    let html = '<div class=\"data\"><h3>Latest Data</h3>';"
        "    if (d.adc) {"
        "      for (let ch in d.adc) {"
        "        html += ch + ': ' + d.adc[ch].voltage + 'V<br>';"
        "      }"
        "    }"
        "    html += '</div>';"
        "    document.getElementById('results').innerHTML = html;"
        "  });"
        "}"
        "</script>"
        "</div></body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, strlen(html_page));

    g_network_manager.stats.api_requests++;
    return ESP_OK;
}

// WebSocket streaming task
static void websocket_streaming_task(void* pvParameters) {
    ESP_LOGI(TAG, "WebSocket streaming task started");

    adc_data_packet_t adc_packet;

    while (g_network_manager.websocket_running) {
        // Get ADC data from queue
        if (adc_manager_get_data(&adc_packet, 50) == ESP_OK) {
            // Create JSON message
            cJSON *json = cJSON_CreateObject();
            cJSON *type = cJSON_CreateString("data");
            cJSON *timestamp = cJSON_CreateNumber(adc_packet.timestamp_us);
            cJSON *channel = cJSON_CreateNumber(adc_packet.channel);
            cJSON *voltage = cJSON_CreateNumber(adc_packet.filtered_voltage);
            cJSON *raw = cJSON_CreateNumber(adc_packet.raw_value);
            cJSON *sequence = cJSON_CreateNumber(adc_packet.sequence);

            cJSON_AddItemToObject(json, "type", type);
            cJSON_AddItemToObject(json, "timestamp", timestamp);
            cJSON_AddItemToObject(json, "channel", channel);
            cJSON_AddItemToObject(json, "voltage", voltage);
            cJSON_AddItemToObject(json, "raw", raw);
            cJSON_AddItemToObject(json, "sequence", sequence);

            char *json_string = cJSON_Print(json);

            // Send to all active WebSocket clients
            for (int i = 0; i < MAX_WEBSOCKET_CLIENTS; i++) {
                if (g_network_manager.websocket_clients[i].active) {
                    httpd_ws_frame_t ws_pkt;
                    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
                    ws_pkt.payload = (uint8_t*)json_string;
                    ws_pkt.len = strlen(json_string);
                    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

                    esp_err_t ret = httpd_ws_send_frame_async(
                        g_network_manager.websocket_clients[i].server,
                        g_network_manager.websocket_clients[i].fd,
                        &ws_pkt);

                    if (ret != ESP_OK) {
                        ESP_LOGW(TAG, "WebSocket client %d disconnected", i);
                        g_network_manager.websocket_clients[i].active = false;
                    }
                }
            }

            free(json_string);
            cJSON_Delete(json);
        }

        // Small delay to prevent overwhelming clients
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "WebSocket streaming task stopped");
    vTaskDelete(NULL);
}

esp_err_t network_manager_init(void) {
    if (g_network_manager.initialized) {
        ESP_LOGW(TAG, "Network Manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing Network Manager");

    // Initialize WiFi event group
    g_network_manager.wifi_event_group = xEventGroupCreate();
    if (!g_network_manager.wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create WiFi event group");
        return ESP_ERR_NO_MEM;
    }

    // Initialize TCP/IP stack (now the single source of WiFi initialization)
    ESP_ERROR_CHECK(esp_netif_init());

    // Create event loop only if it doesn't exist
    esp_err_t ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(ret);
    }

    esp_netif_create_default_wifi_sta();

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Initialize statistics
    memset(&g_network_manager.stats, 0, sizeof(network_stats_t));

    // Initialize WiFi scanning (replaces original Wireless module)
    g_network_manager.scan_complete = false;
    g_network_manager.wifi_ap_count = 0;
    g_network_manager.max_scan_results = NETWORK_MAX_SCAN_RESULTS;
    g_network_manager.scan_results = malloc(sizeof(wifi_ap_record_t) * NETWORK_MAX_SCAN_RESULTS);
    if (!g_network_manager.scan_results) {
        ESP_LOGE(TAG, "Failed to allocate memory for WiFi scan results");
        return ESP_ERR_NO_MEM;
    }

    // Initialize WebSocket support
    memset(g_network_manager.websocket_clients, 0, sizeof(g_network_manager.websocket_clients));
    g_network_manager.websocket_running = false;
    g_network_manager.websocket_task = NULL;

    g_network_manager.initialized = true;
    ESP_LOGI(TAG, "Network Manager initialized");

    return ESP_OK;
}

esp_err_t network_manager_start(void) {
    if (!g_network_manager.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting Network Manager");

    // Start WiFi and wait for it to be ready
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for WiFi to be ready (check STA_START event)
    EventBits_t bits = xEventGroupWaitBits(g_network_manager.wifi_event_group,
                                          WIFI_STA_START_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          pdMS_TO_TICKS(5000));

    if (!(bits & WIFI_STA_START_BIT)) {
        ESP_LOGW(TAG, "WiFi not ready after 5 seconds, continuing anyway");
    }

    // Connect to WiFi
    system_config_t* config = config_get_instance();
    esp_err_t ret = ESP_OK;
    if (config->wifi_config.auto_connect) {
        ret = network_manager_connect_wifi(config->wifi_config.ssid, config->wifi_config.password);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to connect to WiFi: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    // Start HTTP server
    ret = network_manager_start_http_server();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Network Manager started");
    return ESP_OK;
}

esp_err_t network_manager_connect_wifi(const char* ssid, const char* password) {
    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", ssid);

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Wait for connection
    EventBits_t bits = xEventGroupWaitBits(g_network_manager.wifi_event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi SSID: %s", ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to WiFi SSID: %s", ssid);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Unexpected WiFi event");
        return ESP_ERR_INVALID_STATE;
    }
}

esp_err_t network_manager_start_http_server(void) {
    if (g_network_manager.http_server_running) {
        ESP_LOGW(TAG, "HTTP server already running");
        return ESP_OK;
    }

    system_config_t* config = config_get_instance();

    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.server_port = config->network_config.http_port;
    server_config.max_open_sockets = config->network_config.max_clients;
    server_config.task_priority = 5;
    server_config.stack_size = 8192;
    server_config.enable_so_linger = true;
    server_config.linger_timeout = 0;

    ESP_LOGI(TAG, "Starting HTTP server on port %d", server_config.server_port);

    if (httpd_start(&g_network_manager.http_server, &server_config) == ESP_OK) {
        // Register URI handlers
        httpd_uri_t status_uri = {
            .uri = "/api/status",
            .method = HTTP_GET,
            .handler = status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_network_manager.http_server, &status_uri);

        httpd_uri_t data_latest_uri = {
            .uri = "/api/data/latest",
            .method = HTTP_GET,
            .handler = data_latest_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_network_manager.http_server, &data_latest_uri);

        httpd_uri_t config_get_uri = {
            .uri = "/api/config",
            .method = HTTP_GET,
            .handler = config_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_network_manager.http_server, &config_get_uri);

        httpd_uri_t test_uri = {
            .uri = "/api/test",
            .method = HTTP_GET,
            .handler = test_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_network_manager.http_server, &test_uri);

        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(g_network_manager.http_server, &root_uri);

        // Register WebSocket handler
        httpd_uri_t websocket_uri = {
            .uri = "/ws",
            .method = HTTP_GET,
            .handler = websocket_handler,
            .user_ctx = NULL,
            .is_websocket = true
        };
        httpd_register_uri_handler(g_network_manager.http_server, &websocket_uri);

        // Start WebSocket streaming task
        g_network_manager.websocket_running = true;
        BaseType_t ret = xTaskCreate(websocket_streaming_task, "websocket_stream", 4096, NULL, 4, &g_network_manager.websocket_task);
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create WebSocket streaming task");
            g_network_manager.websocket_running = false;
        } else {
            ESP_LOGI(TAG, "WebSocket streaming task started");
        }

        g_network_manager.http_server_running = true;
        ESP_LOGI(TAG, "HTTP server started successfully with WebSocket support");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to start HTTP server");
    return ESP_FAIL;
}

bool network_manager_is_wifi_connected(void) {
    return g_network_manager.wifi_connected;
}

bool network_manager_is_http_server_running(void) {
    return g_network_manager.http_server_running;
}

esp_err_t network_manager_get_stats(network_stats_t* stats) {
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(stats, &g_network_manager.stats, sizeof(network_stats_t));
    return ESP_OK;
}

esp_err_t network_manager_print_stats(void) {
    ESP_LOGI(TAG, "=== Network Manager Statistics ===");
    ESP_LOGI(TAG, "WiFi Connected: %s", g_network_manager.wifi_connected ? "Yes" : "No");
    ESP_LOGI(TAG, "HTTP Server: %s", g_network_manager.http_server_running ? "Running" : "Stopped");
    ESP_LOGI(TAG, "API Requests: %lu", g_network_manager.stats.api_requests);
    ESP_LOGI(TAG, "WebSocket Connections: %lu", g_network_manager.stats.websocket_connections);
    ESP_LOGI(TAG, "Bytes Sent: %lu", g_network_manager.stats.bytes_sent);
    ESP_LOGI(TAG, "Connection Errors: %lu", g_network_manager.stats.connection_errors);
    ESP_LOGI(TAG, "WiFi APs Found: %d", g_network_manager.wifi_ap_count);

    return ESP_OK;
}

// WiFi Scanning Functions (replaces original Wireless module functionality)
esp_err_t network_manager_scan_wifi(uint16_t* ap_count) {
    if (!g_network_manager.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting WiFi scan...");

    // Reset scan state
    g_network_manager.scan_complete = false;
    g_network_manager.wifi_ap_count = 0;

    // Start WiFi scan
    esp_err_t ret = esp_wifi_scan_start(NULL, false);  // Non-blocking scan
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi scan: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for scan to complete
    EventBits_t bits = xEventGroupWaitBits(g_network_manager.wifi_event_group,
                                          WIFI_SCAN_DONE_BIT,
                                          pdTRUE,  // Clear bit after waiting
                                          pdFALSE,
                                          pdMS_TO_TICKS(10000));  // 10 second timeout

    if (!(bits & WIFI_SCAN_DONE_BIT)) {
        ESP_LOGE(TAG, "WiFi scan timeout");
        return ESP_ERR_TIMEOUT;
    }

    // Get scan results
    uint16_t max_records = g_network_manager.max_scan_results;
    ret = esp_wifi_scan_get_ap_records(&max_records, g_network_manager.scan_results);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get scan results: %s", esp_err_to_name(ret));
        return ret;
    }

    g_network_manager.wifi_ap_count = max_records;

    // Update compatibility layer global variables (replaces Wireless.c)
    WIFI_NUM = g_network_manager.wifi_ap_count;
    Scan_finish = 1;  // Mark scan as complete

    if (ap_count) {
        *ap_count = g_network_manager.wifi_ap_count;
    }

    ESP_LOGI(TAG, "WiFi scan completed. Found %d access points", g_network_manager.wifi_ap_count);
    printf("WIFI:%d\r\n", WIFI_NUM);  // Match original Wireless module output

    // Print scan results (like original Wireless module)
    for (int i = 0; i < g_network_manager.wifi_ap_count; i++) {
        ESP_LOGI(TAG, "AP %d: SSID=%s, RSSI=%d, Channel=%d",
                 i, g_network_manager.scan_results[i].ssid,
                 g_network_manager.scan_results[i].rssi,
                 g_network_manager.scan_results[i].primary);
    }

    return ESP_OK;
}

esp_err_t network_manager_get_scan_results(wifi_ap_record_t* ap_records, uint16_t* ap_count) {
    if (!ap_records || !ap_count) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_network_manager.initialized || !g_network_manager.scan_complete) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t copy_count = (*ap_count < g_network_manager.wifi_ap_count) ?
                          *ap_count : g_network_manager.wifi_ap_count;

    memcpy(ap_records, g_network_manager.scan_results, sizeof(wifi_ap_record_t) * copy_count);
    *ap_count = copy_count;

    return ESP_OK;
}

bool network_manager_is_scan_complete(void) {
    return g_network_manager.scan_complete;
}

uint16_t network_manager_get_wifi_count(void) {
    return g_network_manager.wifi_ap_count;
}

esp_err_t network_manager_perform_initial_scan(void) {
    if (!g_network_manager.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Performing initial WiFi scan (replaces Wireless_Init)");

    // Reset compatibility layer variables
    WIFI_NUM = 0;
    BLE_NUM = 0;  // BLE scanning not implemented yet
    Scan_finish = 0;

    // Perform WiFi scan
    uint16_t ap_count = 0;
    esp_err_t ret = network_manager_scan_wifi(&ap_count);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Initial WiFi scan failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Since BLE is disabled in original code, mark BLE scan as complete
    // BLE_NUM remains 0
    // Scan_finish is already set to 1 by network_manager_scan_wifi

    ESP_LOGI(TAG, "Initial scan complete - WiFi: %d APs, BLE: %d devices", WIFI_NUM, BLE_NUM);

    return ESP_OK;
}
