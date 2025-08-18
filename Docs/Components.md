# Key Components and Libraries

## Core ESP-IDF Components

### 1. UART Driver (driver/uart.h)
**Purpose**: Multi-port serial data acquisition
**Justification**: Hardware-accelerated UART with DMA support for reliable high-speed data capture

**Configuration**:
```c
// UART1: Primary debug interface (9600 baud)
uart_config_t uart1_config = {
    .baud_rate = 9600,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 122,
};

// UART2: High-speed data (115200 baud)
uart_config_t uart2_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 122,
};
```

**Benefits**:
- Hardware FIFO buffering (128 bytes)
- Interrupt-driven operation
- DMA support for large transfers
- Built-in error detection

### 2. ADC Driver (driver/adc.h)
**Purpose**: Analog voltage measurement (0-4V range)
**Justification**: High-resolution ADC with calibration support for accurate voltage readings

**Configuration**:
```c
adc_oneshot_unit_init_cfg_t init_config = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
};

adc_oneshot_chan_cfg_t config = {
    .bitwidth = ADC_BITWIDTH_12,
    .atten = ADC_ATTEN_DB_11,  // 0-3.3V range
};
```

**Features**:
- 12-bit resolution (4096 levels)
- Multiple attenuation settings
- Built-in calibration
- DMA support for continuous sampling

### 3. SPI Master Driver (driver/spi_master.h)
**Purpose**: LCD and SD card communication
**Justification**: High-speed SPI interface for display updates and storage operations

**Configuration**:
```c
spi_bus_config_t buscfg = {
    .miso_io_num = PIN_NUM_MISO,
    .mosi_io_num = PIN_NUM_MOSI,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4096,
};
```

### 4. WiFi Driver (esp_wifi.h)
**Purpose**: Network connectivity for data transmission
**Justification**: WiFi 6 support with power management for reliable connectivity

**Configuration**:
```c
wifi_config_t wifi_config = {
    .sta = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        .pmf_cfg = {
            .capable = true,
            .required = false
        },
    },
};
```

## Third-Party Libraries

### 1. LVGL Graphics Library (v8.3.0)
**Purpose**: LCD user interface and data visualization
**Justification**: Lightweight, feature-rich GUI library optimized for embedded systems

**Key Features**:
- Memory-efficient rendering
- Touch input support (future expansion)
- Rich widget library
- Animation support
- Theme customization

**Integration Benefits**:
- ESP32 hardware acceleration
- FreeRTOS task integration
- Low memory footprint
- Professional UI components

### 2. FatFS Filesystem
**Purpose**: SD card file management
**Justification**: Reliable filesystem with wear leveling and error recovery

**Configuration**:
```c
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
};
```

**Features**:
- Long filename support
- Directory operations
- File locking
- Error recovery

### 3. HTTP Server (esp_http_server.h)
**Purpose**: REST API and web interface serving
**Justification**: Built-in HTTP server with WebSocket support

**Configuration**:
```c
httpd_config_t config = HTTPD_DEFAULT_CONFIG();
config.task_priority = tskIDLE_PRIORITY + 5;
config.stack_size = 8192;
config.max_uri_handlers = 20;
config.max_resp_headers = 8;
```

### 4. JSON Library (cJSON)
**Purpose**: Data serialization for web API
**Justification**: Lightweight JSON parser/generator for structured data exchange

**Usage Example**:
```c
cJSON *json = cJSON_CreateObject();
cJSON *timestamp = cJSON_CreateNumber(esp_timer_get_time());
cJSON *data = cJSON_CreateArray();
cJSON_AddItemToObject(json, "timestamp", timestamp);
cJSON_AddItemToObject(json, "data", data);
```

## Custom Components

### 1. Data Logger Core
**Files**: `data_logger.h`, `data_logger.c`
**Purpose**: Central data coordination and buffering
**Responsibilities**:
- Multi-source data aggregation
- Ring buffer management
- Data validation and checksums
- Timestamp synchronization

### 2. Storage Manager
**Files**: `storage_manager.h`, `storage_manager.c`
**Purpose**: Reliable data persistence
**Responsibilities**:
- File rotation and cleanup
- Atomic write operations
- Error recovery
- Compression (optional)

### 3. Network Manager
**Files**: `network_manager.h`, `network_manager.c`
**Purpose**: Network communication and API serving
**Responsibilities**:
- WiFi connection management
- HTTP request handling
- WebSocket real-time streaming
- Client authentication

### 4. Display Manager
**Files**: `display_manager.h`, `display_manager.c`
**Purpose**: LCD interface and status display
**Responsibilities**:
- Real-time data visualization
- System status indication
- Power management
- User interaction

### 5. System Monitor
**Files**: `system_monitor.h`, `system_monitor.c`
**Purpose**: Health monitoring and diagnostics
**Responsibilities**:
- Resource usage tracking
- Error detection and reporting
- Performance metrics
- Watchdog management

## Library Selection Rationale

### Rapid Development Focus
1. **ESP-IDF Ecosystem**: Mature, well-documented, production-ready
2. **LVGL**: Professional UI without custom graphics code
3. **Built-in HTTP Server**: No external dependencies
4. **FatFS**: Proven reliability for embedded storage

### Reliability Requirements
1. **Hardware Drivers**: Direct ESP32 hardware access
2. **Error Handling**: Built-in recovery mechanisms
3. **Memory Management**: Predictable allocation patterns
4. **Real-time Performance**: Interrupt-driven operation

### Debugging Optimization
1. **Standard Protocols**: HTTP/WebSocket for universal client support
2. **JSON Format**: Human-readable data exchange
3. **Logging Integration**: ESP-IDF logging framework
4. **Development Tools**: VS Code integration and debugging support

### Future Expansion
1. **Modular Design**: Easy component addition/modification
2. **Configuration System**: Runtime parameter adjustment
3. **OTA Updates**: Remote firmware updates
4. **Protocol Flexibility**: Multiple client interface options

---

*This component selection balances rapid development needs with long-term reliability and maintainability requirements.*
