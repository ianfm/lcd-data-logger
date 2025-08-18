# ESP32-C6 Remote Data Logger Web Server

## Project Overview

This project transforms the ESP32-C6 LCD demo application into a comprehensive remote data logging web server designed for debugging embedded devices. The system captures multiple UART streams and analog voltage inputs, stores data reliably to SD card, and serves it via a web interface compatible with popular data visualization tools.

## Hardware Platform

**Board**: Waveshare ESP32-C6-LCD-1.47
- **MCU**: ESP32-C6FH4 with dual RISC-V cores (160MHz main, 20MHz low-power)
- **Memory**: 320KB ROM, 512KB HP SRAM, 16KB LP SRAM, 4MB Flash
- **Display**: 1.47" LCD, 172×320 resolution, 262K colors, ST7789 driver
- **Storage**: MicroSD card slot (expandable storage)
- **Connectivity**: WiFi 6, Bluetooth 5.0, IEEE 802.15.4
- **Power**: USB-C (external power + debugging)
- **Additional**: RGB LED, multiple GPIO interfaces

## System Requirements

### Data Acquisition
- **UART Streams**: Up to 3 concurrent serial ports
  - Port 1: 9600 baud (primary debug interface)
  - Port 2: 115200 baud (high-speed data)
  - Port 3: Configurable baud rate
- **Analog Inputs**: 3 channels, 0-4V range
- **Sampling Rate**: Configurable, optimized for reliability over speed

### Data Storage
- **Primary Storage**: MicroSD card (FAT32 filesystem)
- **Buffer Strategy**: RAM buffering with periodic flush to SD
- **Data Integrity**: Checksums and error detection
- **File Format**: Structured binary/CSV for easy parsing

### Network Interface
- **Protocol**: HTTP REST API + WebSocket for real-time updates
- **Data Format**: JSON/MessagePack for structured data
- **Compatibility**: Foxglove Studio, Rerun, Python clients
- **Security**: Basic authentication, configurable access

### User Interface
- **LCD Display**: Configurable data subset display
- **Power Management**: Display sleep mode for power saving
- **Web Interface**: Configuration and data visualization
- **Status Indicators**: RGB LED for system status

## Key Design Principles

1. **Reliability First**: Robust data capture and storage
2. **Rapid Development**: Leverage existing ESP-IDF ecosystem
3. **Debugging Focus**: Optimized for embedded device debugging
4. **Power Efficiency**: Smart power management for continuous operation
5. **Data Integrity**: Complete and accurate data logging

## Development Timeline

**Target**: 2 days for MVP implementation

### Day 1: Core Infrastructure
- UART data capture system
- SD card logging implementation
- Basic web server setup
- LCD status display

### Day 2: Integration & Testing
- Data visualization interface
- Network protocol implementation
- Testing and debugging tools
- Documentation and deployment

## Documentation Structure

### Core Documents
- **[Architecture.md](Architecture.md)** - System architecture and design decisions
- **[Components.md](Components.md)** - Key components, libraries, and justifications
- **[Implementation-Plan.md](Implementation-Plan.md)** - Detailed 2-day development timeline
- **[Testing-Strategy.md](Testing-Strategy.md)** - Comprehensive testing and debugging approach
- **[Data-Visualization.md](Data-Visualization.md)** - Client integration and visualization tools

### Visual Diagrams
- **System Architecture Overview** - Complete system component relationships
- **Program Flow and State Machine** - Application logic and error handling
- **Data Output Structures** - Data formats and relationships
- **Input/Output Interface Design** - Hardware and software interfaces

## Quick Start Guide

### Prerequisites
- ESP32-C6-LCD-1.47 development board
- VS Code with ESP-IDF extension
- MicroSD card (Class 10, 32GB recommended)
- USB-C cable for power and debugging

### Development Setup
1. Clone the existing demo project
2. Install ESP-IDF v5.1 or later
3. Configure VS Code workspace settings
4. Install required Python packages for client tools

### Implementation Sequence
1. **Day 1**: Core infrastructure (data acquisition, storage)
2. **Day 2**: Network layer and testing
3. **Validation**: End-to-end testing with visualization tools

## Key Features Summary

### Data Acquisition
- **3 UART ports**: 9600, 115200, and configurable baud rates
- **3 ADC channels**: 0-4V range with hardware calibration
- **Real-time processing**: Interrupt-driven with DMA support
- **Data integrity**: Checksums and error detection

### Storage System
- **MicroSD logging**: Reliable file system with rotation
- **Multiple formats**: Binary, CSV, and JSON export
- **Atomic operations**: Data integrity during power loss
- **Compression**: Optional data compression for long-term storage

### Network Interface
- **WiFi 6 support**: High-speed, low-latency connectivity
- **REST API**: Standard HTTP endpoints for data access
- **WebSocket streaming**: Real-time data for live visualization
- **Multiple clients**: Foxglove, Rerun, Python, web browsers

### User Interface
- **LCD display**: Configurable data views and system status
- **RGB LED**: Visual system state indication
- **Web dashboard**: Browser-based configuration and monitoring
- **Power management**: Auto-sleep and efficiency optimization

## Expert AI Agent Guidance

This project is designed for implementation with expert embedded engineer guidance through an AI agent. The documentation provides:

1. **Structured task breakdown** for systematic development
2. **Comprehensive testing strategy** using VS Code tools
3. **Performance benchmarks** and optimization targets
4. **Error handling patterns** for robust operation
5. **Integration examples** for popular visualization tools

The 2-day timeline is achievable with focused development and leverages existing ESP-IDF ecosystem components for rapid prototyping while ensuring production-quality reliability.

---

*This comprehensive plan transforms a simple LCD demo into a professional-grade data logging solution optimized for embedded device debugging workflows.*
