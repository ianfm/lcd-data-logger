# System Architecture

## High-Level Architecture

The ESP32-C6 Data Logger follows a modular, event-driven architecture optimized for reliable data capture and network delivery.

## Core Components

### 1. Data Acquisition Layer
**UART Manager**
- Multi-port UART handling with independent buffers
- Configurable baud rates and protocols
- Hardware flow control support
- Error detection and recovery

**ADC Manager**
- 3-channel analog input sampling
- Configurable sampling rates (1Hz - 1kHz)
- Voltage scaling (0-4V range)
- Moving average filtering

### 2. Data Storage Layer
**Buffer Manager**
- Ring buffers for each data source
- Configurable buffer sizes
- Overflow protection
- Memory pool management

**SD Card Manager**
- FAT32 filesystem interface
- Atomic write operations
- File rotation and cleanup
- Error recovery and retry logic

### 3. Network Layer
**WiFi Manager**
- Station mode configuration
- Auto-reconnection logic
- Network status monitoring
- Power management integration

**Web Server**
- HTTP REST API endpoints
- WebSocket real-time streaming
- Static file serving
- CORS support for web clients

### 4. User Interface Layer
**LCD Manager**
- Status display controller
- Configurable data views
- Power management (auto-sleep)
- Touch interface (if available)

**LED Manager**
- System status indication
- Error state visualization
- Network connectivity status
- Data logging activity

## Data Flow Architecture

```
[UART Ports] ──┐
[ADC Inputs] ──┼──> [Ring Buffers] ──> [SD Card] ──> [Web API]
[System Data] ─┘                           │              │
                                           ▼              ▼
                                    [File Storage]  [Real-time Stream]
                                           │              │
                                           ▼              ▼
                                    [Data Archive]  [Web Dashboard]
```

## Component Justification

### ESP-IDF Framework
- **Mature ecosystem**: Proven stability for production systems
- **Hardware abstraction**: Simplified peripheral access
- **FreeRTOS integration**: Real-time task management
- **Network stack**: Built-in WiFi and TCP/IP support

### LVGL Graphics Library
- **Lightweight**: Minimal memory footprint
- **Feature-rich**: Professional UI components
- **Touch support**: Future expansion capability
- **ESP32 optimized**: Hardware acceleration support

### FatFS Filesystem
- **Reliability**: Proven filesystem for embedded systems
- **Compatibility**: Universal SD card support
- **Performance**: Optimized for flash storage
- **Recovery**: Built-in error correction

### HTTP/WebSocket Protocol
- **Simplicity**: Easy client implementation
- **Compatibility**: Universal browser support
- **Real-time**: WebSocket for live data streaming
- **Debugging**: Standard web development tools

## Memory Management

### RAM Allocation (512KB HP SRAM)
- **System/Stack**: 128KB (25%)
- **UART Buffers**: 192KB (37.5%)
- **Network Buffers**: 96KB (18.75%)
- **Display/Graphics**: 64KB (12.5%)
- **Application**: 32KB (6.25%)

### Flash Usage (4MB)
- **Bootloader**: 64KB
- **Application**: 2MB
- **OTA Partition**: 1.5MB
- **NVS/Config**: 64KB
- **Reserved**: 384KB

## Power Management Strategy

### Normal Operation
- **CPU**: Dynamic frequency scaling
- **WiFi**: Power save mode when idle
- **Display**: Auto-brightness and sleep
- **Peripherals**: Selective power gating

### Low Power Mode
- **Display**: Complete shutdown
- **WiFi**: Beacon interval optimization
- **CPU**: Light sleep between operations
- **Data**: Reduced sampling rates

## Error Handling Strategy

### Data Integrity
- **Checksums**: Per-packet validation
- **Redundancy**: Multiple storage locations
- **Recovery**: Automatic error correction
- **Monitoring**: Continuous health checks

### Network Resilience
- **Auto-reconnect**: WiFi connection recovery
- **Buffering**: Offline data storage
- **Retry logic**: Exponential backoff
- **Fallback**: Local-only operation mode

### Hardware Failures
- **SD Card**: Graceful degradation to RAM-only
- **Display**: Headless operation capability
- **Network**: Local logging continuation
- **Power**: Clean shutdown on low voltage

## Scalability Considerations

### Data Volume
- **File rotation**: Automatic cleanup of old data
- **Compression**: Optional data compression
- **Streaming**: Real-time data export
- **Archival**: Long-term storage strategies

### Performance
- **Task priorities**: Real-time data capture priority
- **DMA usage**: Hardware-accelerated transfers
- **Caching**: Intelligent buffer management
- **Optimization**: Profile-guided optimization

---

*This architecture balances simplicity with robustness, ensuring reliable operation in embedded debugging scenarios while maintaining development velocity.*
