# Testing and Debugging Strategy

## Development Environment Setup

### VS Code Configuration
**Required Extensions**:
- ESP-IDF Extension (official Espressif)
- C/C++ Extension (Microsoft)
- Serial Monitor Extension
- Cortex-Debug Extension
- GitLens (for version control)

**Workspace Configuration** (`.vscode/settings.json`):
```json
{
    "idf.adapterTargetName": "esp32c6",
    "idf.customExtraPaths": "",
    "idf.customExtraVars": {},
    "idf.flashType": "UART",
    "idf.openOcdConfigs": [
        "board/esp32c6-builtin.cfg"
    ],
    "idf.portWin": "COM3",
    "idf.pythonBinPath": "python.exe",
    "C_Cpp.intelliSenseEngine": "Tag Parser",
    "files.associations": {
        "*.h": "c",
        "*.c": "c"
    }
}
```

### Hardware Test Setup
**Required Equipment**:
- ESP32-C6-LCD-1.47 development board
- MicroSD card (Class 10, 32GB recommended)
- USB-C cable for power and debugging
- Logic analyzer (optional, for signal verification)
- Multimeter for voltage verification
- Breadboard and jumper wires for test connections

**Test Signal Sources**:
- Arduino Uno (UART signal generation)
- Function generator (analog signal testing)
- Variable power supply (0-4V range testing)

## Unit Testing Framework

### ESP-IDF Unity Testing
**Setup** (`test/CMakeLists.txt`):
```cmake
idf_component_register(
    SRCS "test_data_logger.c"
         "test_storage_manager.c"
         "test_network_manager.c"
    INCLUDE_DIRS "."
    REQUIRES unity data_logger storage_manager network_manager
)
```

**Test Structure**:
```c
#include "unity.h"
#include "data_logger.h"

void setUp(void) {
    // Initialize test environment
    data_logger_init();
}

void tearDown(void) {
    // Clean up after test
    data_logger_deinit();
}

void test_uart_data_capture(void) {
    // Test UART data capture functionality
    TEST_ASSERT_EQUAL(ESP_OK, uart_capture_start(UART_NUM_1));
    // Add test data
    // Verify capture
    TEST_ASSERT_EQUAL(expected_data, captured_data);
}
```

### Component-Level Testing

#### 1. UART Manager Tests
**Test Cases**:
- Multi-port initialization
- Baud rate configuration
- Data capture accuracy
- Buffer overflow handling
- Error recovery

**Test Implementation**:
```c
void test_uart_multi_port(void) {
    uart_config_t config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
    };
    
    TEST_ASSERT_EQUAL(ESP_OK, uart_manager_init(UART_NUM_1, &config));
    TEST_ASSERT_EQUAL(ESP_OK, uart_manager_init(UART_NUM_2, &config));
    
    // Send test data and verify reception
}
```

#### 2. ADC Manager Tests
**Test Cases**:
- Channel configuration
- Voltage accuracy (±1% tolerance)
- Sampling rate verification
- Filtering effectiveness

#### 3. Storage Manager Tests
**Test Cases**:
- File creation and writing
- Data integrity verification
- File rotation logic
- Error recovery from SD card failures

#### 4. Network Manager Tests
**Test Cases**:
- WiFi connection establishment
- HTTP API endpoint responses
- WebSocket real-time streaming
- Client authentication

## Integration Testing

### System-Level Test Scenarios

#### 1. End-to-End Data Flow Test
**Objective**: Verify complete data path from input to output
**Procedure**:
1. Connect test UART sources to all 3 ports
2. Apply known voltage levels to ADC inputs
3. Start data logging
4. Verify data appears in SD card files
5. Verify data available via web API
6. Check real-time WebSocket streaming

**Success Criteria**:
- All data sources captured without loss
- Timestamps accurate within 1ms
- File integrity maintained
- Network data matches stored data

#### 2. Stress Testing
**Objective**: Verify system stability under load
**Test Conditions**:
- Maximum UART data rates (115200 baud)
- Continuous ADC sampling (1kHz)
- Multiple simultaneous web clients
- Extended operation (24+ hours)

**Monitoring**:
- Memory usage (heap/stack)
- CPU utilization
- Network latency
- Data loss detection

#### 3. Error Recovery Testing
**Objective**: Verify graceful handling of error conditions
**Test Scenarios**:
- SD card removal during operation
- WiFi network disconnection
- Power supply voltage variations
- Memory exhaustion conditions

### Performance Benchmarking

#### 1. Data Throughput Measurement
**Metrics**:
- UART data capture rate (bytes/second)
- ADC sampling frequency (samples/second)
- SD card write speed (KB/second)
- Network transmission rate (packets/second)

**Tools**:
```c
// Performance measurement macros
#define PERF_START(name) \
    uint64_t start_##name = esp_timer_get_time()

#define PERF_END(name) \
    uint64_t end_##name = esp_timer_get_time(); \
    ESP_LOGI("PERF", #name ": %llu us", end_##name - start_##name)
```

#### 2. Memory Usage Analysis
**Tools**:
- ESP-IDF heap tracing
- Stack watermark monitoring
- Memory leak detection

**Implementation**:
```c
void monitor_memory_usage(void) {
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    
    ESP_LOGI("MEM", "Free heap: %d bytes, Min free: %d bytes", 
             free_heap, min_free_heap);
}
```

## Debugging Tools and Techniques

### 1. ESP-IDF Debugging Features
**GDB Integration**:
```bash
# Start OpenOCD
openocd -f board/esp32c6-builtin.cfg

# Connect GDB
xtensa-esp32c6-elf-gdb build/data-logger.elf
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) break app_main
(gdb) continue
```

**Core Dump Analysis**:
```bash
# Enable core dumps in menuconfig
idf.py menuconfig
# Component config -> ESP System Settings -> Core dump

# Analyze core dump
idf.py coredump-info build/data-logger.elf core_dump.bin
```

### 2. Real-Time Monitoring
**System Trace**:
```c
#include "esp_app_trace.h"

void trace_data_capture(const char* data, size_t len) {
    esp_apptrace_write(ESP_APPTRACE_DEST_TRAX, data, len, 100);
}
```

**Custom Logging**:
```c
#define DATA_LOG_TAG "DATA"
#define NET_LOG_TAG "NET"
#define STORAGE_LOG_TAG "STORAGE"

ESP_LOGI(DATA_LOG_TAG, "UART%d: %d bytes captured", port, len);
ESP_LOGI(NET_LOG_TAG, "Client connected: %s", client_ip);
ESP_LOGI(STORAGE_LOG_TAG, "File written: %s (%d bytes)", filename, size);
```

### 3. Web-Based Debugging Interface
**Debug API Endpoints**:
- `/api/debug/memory` - Memory usage statistics
- `/api/debug/tasks` - FreeRTOS task information
- `/api/debug/performance` - Performance metrics
- `/api/debug/logs` - Recent log entries

**Real-Time Debug Dashboard**:
- Live memory usage graphs
- Task execution timeline
- Network activity monitor
- Data capture statistics

## Automated Testing Pipeline

### 1. Continuous Integration Setup
**GitHub Actions Workflow** (`.github/workflows/test.yml`):
```yaml
name: ESP32-C6 Build and Test
on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - name: Setup ESP-IDF
      uses: espressif/esp-idf-ci-action@v1
      with:
        esp_idf_version: v5.1
    - name: Build firmware
      run: idf.py build
    - name: Run unit tests
      run: idf.py build -T test
```

### 2. Hardware-in-the-Loop Testing
**Automated Test Rig**:
- Raspberry Pi controller
- Relay-controlled power switching
- Automated signal generation
- Result verification scripts

**Test Execution**:
```python
# Automated test script
import serial
import requests
import time

def test_uart_capture():
    # Send test data via serial
    ser = serial.Serial('/dev/ttyUSB0', 9600)
    test_data = b"TEST_MESSAGE_123"
    ser.write(test_data)
    
    # Verify data via API
    time.sleep(1)
    response = requests.get('http://192.168.1.100/api/data/latest')
    assert test_data in response.content
```

## Expert Guidance Integration

### 1. Code Review Checklist
**For AI Agent Review**:
- [ ] Memory allocation patterns verified
- [ ] Error handling comprehensive
- [ ] Real-time constraints met
- [ ] Power consumption optimized
- [ ] Security considerations addressed

### 2. Performance Optimization Guidelines
**Critical Path Analysis**:
- Identify bottlenecks in data flow
- Optimize interrupt service routines
- Minimize memory allocations in hot paths
- Use DMA for large data transfers

### 3. Production Readiness Criteria
**Deployment Checklist**:
- [ ] 24-hour stress test passed
- [ ] Error recovery verified
- [ ] Performance benchmarks met
- [ ] Security audit completed
- [ ] Documentation updated

---

*This testing strategy ensures reliable operation while maintaining rapid development velocity through comprehensive automation and expert guidance integration.*
