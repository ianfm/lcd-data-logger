# Data Visualization and Client Integration

## Supported Visualization Platforms

### 1. Foxglove Studio
**Why Foxglove**: Professional robotics data visualization with excellent time-series support and WebSocket integration.

**Connection Setup**:
```javascript
// Foxglove WebSocket connection
const ws = new WebSocket('ws://192.168.1.100:8080/ws');
ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    // Foxglove automatically handles the data visualization
};
```

**Data Format for Foxglove**:
```json
{
  "topic": "/esp32/data",
  "timestamp": {"sec": 1640995200, "nsec": 0},
  "message": {
    "uart_data": {
      "port1": {"data": "Hello World", "timestamp": 1640995200000000},
      "port2": {"data": "Debug info", "timestamp": 1640995200001000}
    },
    "adc_data": {
      "channel1": {"voltage": 2.45, "timestamp": 1640995200000000},
      "channel2": {"voltage": 1.23, "timestamp": 1640995200001000},
      "channel3": {"voltage": 3.78, "timestamp": 1640995200002000}
    }
  }
}
```

**Foxglove Panel Configuration**:
- **Time Series**: For ADC voltage trends
- **Raw Messages**: For UART text data
- **State Transitions**: For system status changes
- **3D Visualization**: For multi-dimensional data correlation

### 2. Rerun Viewer
**Why Rerun**: Excellent for debugging workflows with rich data type support and easy Python integration.

**Python Client Example**:
```python
import rerun as rr
import requests
import json
from datetime import datetime

# Initialize Rerun
rr.init("ESP32_Data_Logger")
rr.spawn()

# Fetch data from ESP32
response = requests.get('http://192.168.1.100/api/data/latest')
data = response.json()

# Log time series data
for sample in data['adc_samples']:
    timestamp = datetime.fromtimestamp(sample['timestamp'] / 1000000)
    rr.set_time_seconds("timestamp", timestamp.timestamp())
    rr.log_scalar("adc/channel1", sample['channel1']['voltage'])
    rr.log_scalar("adc/channel2", sample['channel2']['voltage'])
    rr.log_scalar("adc/channel3", sample['channel3']['voltage'])

# Log text data
for uart_msg in data['uart_messages']:
    timestamp = datetime.fromtimestamp(uart_msg['timestamp'] / 1000000)
    rr.set_time_seconds("timestamp", timestamp.timestamp())
    rr.log_text_entry(f"uart/port{uart_msg['port']}", uart_msg['data'])
```

### 3. Python Client Libraries
**Custom Python Client** (`esp32_client.py`):
```python
import requests
import websocket
import json
import pandas as pd
import matplotlib.pyplot as plt
from datetime import datetime

class ESP32DataLogger:
    def __init__(self, host='192.168.1.100', port=80):
        self.base_url = f'http://{host}:{port}'
        self.ws_url = f'ws://{host}:{port+8000}/ws'
        self.ws = None
        
    def connect_websocket(self, callback=None):
        """Connect to real-time WebSocket stream"""
        def on_message(ws, message):
            data = json.loads(message)
            if callback:
                callback(data)
            else:
                self.default_handler(data)
                
        self.ws = websocket.WebSocketApp(self.ws_url, on_message=on_message)
        self.ws.run_forever()
    
    def get_latest_data(self):
        """Fetch latest data samples"""
        response = requests.get(f'{self.base_url}/api/data/latest')
        return response.json()
    
    def get_historical_data(self, start_time, end_time):
        """Fetch historical data range"""
        params = {'start': start_time, 'end': end_time}
        response = requests.get(f'{self.base_url}/api/data/range', params=params)
        return response.json()
    
    def download_logs(self, filename):
        """Download complete log files"""
        response = requests.get(f'{self.base_url}/api/data/download')
        with open(filename, 'wb') as f:
            f.write(response.content)
    
    def plot_adc_data(self, duration_minutes=10):
        """Plot recent ADC data"""
        data = self.get_latest_data()
        df = pd.DataFrame(data['adc_samples'])
        df['timestamp'] = pd.to_datetime(df['timestamp'], unit='us')
        
        plt.figure(figsize=(12, 8))
        for channel in ['channel1', 'channel2', 'channel3']:
            voltages = [sample[channel]['voltage'] for sample in data['adc_samples']]
            plt.plot(df['timestamp'], voltages, label=f'ADC {channel}')
        
        plt.xlabel('Time')
        plt.ylabel('Voltage (V)')
        plt.title('ESP32 ADC Data')
        plt.legend()
        plt.grid(True)
        plt.show()

# Usage example
logger = ESP32DataLogger('192.168.1.100')
logger.plot_adc_data()
```

### 4. Web Dashboard
**HTML/JavaScript Dashboard** (`web/dashboard.html`):
```html
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Data Logger Dashboard</title>
    <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
    <style>
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
        .chart { width: 100%; height: 400px; margin: 20px 0; }
        .status { background: #f0f0f0; padding: 10px; border-radius: 5px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 Data Logger Dashboard</h1>
        
        <div class="status" id="status">
            <h3>System Status</h3>
            <p>Connection: <span id="connection-status">Disconnected</span></p>
            <p>Data Rate: <span id="data-rate">0</span> samples/sec</p>
            <p>Storage: <span id="storage-usage">0</span>% used</p>
        </div>
        
        <div id="adc-chart" class="chart"></div>
        <div id="uart-log" class="chart"></div>
    </div>

    <script>
        // WebSocket connection
        const ws = new WebSocket('ws://192.168.1.100:8080/ws');
        const adcData = {x: [], y1: [], y2: [], y3: []};
        const uartLog = [];
        
        ws.onopen = () => {
            document.getElementById('connection-status').textContent = 'Connected';
        };
        
        ws.onmessage = (event) => {
            const data = JSON.parse(event.data);
            updateCharts(data);
            updateStatus(data);
        };
        
        function updateCharts(data) {
            if (data.type === 'data') {
                const timestamp = new Date(data.timestamp / 1000);
                adcData.x.push(timestamp);
                adcData.y1.push(data.sources.adc1.voltage);
                adcData.y2.push(data.sources.adc2.voltage);
                adcData.y3.push(data.sources.adc3.voltage);
                
                // Keep only last 100 points
                if (adcData.x.length > 100) {
                    adcData.x.shift();
                    adcData.y1.shift();
                    adcData.y2.shift();
                    adcData.y3.shift();
                }
                
                // Update ADC chart
                Plotly.newPlot('adc-chart', [
                    {x: adcData.x, y: adcData.y1, name: 'Channel 1', type: 'scatter'},
                    {x: adcData.x, y: adcData.y2, name: 'Channel 2', type: 'scatter'},
                    {x: adcData.x, y: adcData.y3, name: 'Channel 3', type: 'scatter'}
                ], {title: 'ADC Voltage Readings'});
                
                // Update UART log
                if (data.sources.uart1) {
                    uartLog.push({
                        timestamp: timestamp,
                        port: 1,
                        data: atob(data.sources.uart1.data)
                    });
                }
            }
        }
        
        function updateStatus(data) {
            // Update system status indicators
            const dataRate = calculateDataRate();
            document.getElementById('data-rate').textContent = dataRate;
        }
        
        function calculateDataRate() {
            // Calculate samples per second
            return Math.floor(Math.random() * 100); // Placeholder
        }
    </script>
</body>
</html>
```

## Data Export Formats

### 1. CSV Export
**Structure**:
```csv
timestamp_us,source_type,source_id,data_type,value,unit
1640995200000000,uart,1,text,"Hello World",string
1640995200001000,adc,1,voltage,2.45,volts
1640995200002000,uart,2,text,"Debug data",string
1640995200003000,adc,2,voltage,1.23,volts
```

**Python Processing**:
```python
import pandas as pd

# Load CSV data
df = pd.read_csv('esp32_log.csv')
df['timestamp'] = pd.to_datetime(df['timestamp_us'], unit='us')

# Filter ADC data
adc_data = df[df['source_type'] == 'adc']
adc_pivot = adc_data.pivot_table(
    index='timestamp', 
    columns='source_id', 
    values='value'
)

# Plot voltage trends
adc_pivot.plot(title='ADC Voltage Trends')
```

### 2. JSON Export
**Structure**:
```json
{
  "metadata": {
    "device_id": "ESP32-C6-001",
    "start_time": 1640995200000000,
    "end_time": 1640995800000000,
    "sample_count": 6000,
    "version": "1.0"
  },
  "data": [
    {
      "timestamp": 1640995200000000,
      "uart": {
        "port1": {"data": "SGVsbG8gV29ybGQ=", "length": 11},
        "port2": {"data": "RGVidWcgZGF0YQ==", "length": 9}
      },
      "adc": {
        "channel1": {"voltage": 2.45, "raw": 2048},
        "channel2": {"voltage": 1.23, "raw": 1024},
        "channel3": {"voltage": 3.78, "raw": 3145}
      },
      "system": {
        "heap_free": 245760,
        "cpu_usage": 45.2,
        "wifi_rssi": -42
      }
    }
  ]
}
```

### 3. Binary Format
**Advantages**: Compact storage, fast parsing, precise timestamps
**Use Case**: High-frequency data logging, long-term storage

**Python Reader**:
```python
import struct
from datetime import datetime

def read_binary_log(filename):
    data = []
    with open(filename, 'rb') as f:
        while True:
            # Read packet header
            header = f.read(16)  # Fixed header size
            if len(header) < 16:
                break
                
            magic, timestamp, source_id, data_type, length, checksum = \
                struct.unpack('<LQBBHB', header)
            
            if magic != 0xDEADBEEF:
                continue
                
            # Read payload
            payload = f.read(length)
            
            data.append({
                'timestamp': datetime.fromtimestamp(timestamp / 1000000),
                'source_id': source_id,
                'data_type': data_type,
                'payload': payload
            })
    
    return data
```

## Real-Time Streaming Protocols

### WebSocket Protocol Specification
**Message Types**:
- `data`: Real-time sensor data
- `status`: System status updates
- `config`: Configuration changes
- `error`: Error notifications

**Client Subscription**:
```json
{
  "type": "subscribe",
  "topics": ["uart1", "uart2", "adc1", "adc2", "adc3", "system"],
  "rate_limit": 100
}
```

**Server Response**:
```json
{
  "type": "subscription_ack",
  "topics": ["uart1", "uart2", "adc1", "adc2", "adc3", "system"],
  "client_id": "client_12345"
}
```

### HTTP Server-Sent Events (SSE)
**Alternative to WebSocket for simple clients**:
```javascript
const eventSource = new EventSource('http://192.168.1.100/api/stream');
eventSource.onmessage = (event) => {
    const data = JSON.parse(event.data);
    console.log('Received data:', data);
};
```

## Integration Examples

### Jupyter Notebook Integration
```python
import ipywidgets as widgets
from IPython.display import display
import matplotlib.pyplot as plt

# Create interactive dashboard
logger = ESP32DataLogger()

# Real-time plot widget
%matplotlib widget
fig, ax = plt.subplots()
line1, = ax.plot([], [], label='Channel 1')
line2, = ax.plot([], [], label='Channel 2')
line3, = ax.plot([], [], label='Channel 3')

def update_plot():
    data = logger.get_latest_data()
    # Update plot with new data
    
# Auto-refresh every second
timer = widgets.Timer(interval=1000)
timer.on_trait_change(update_plot, 'value')
```

### InfluxDB Integration
```python
from influxdb_client import InfluxDBClient, Point

client = InfluxDBClient(url="http://localhost:8086", token="your-token")
write_api = client.write_api()

def store_to_influxdb(data):
    points = []
    for sample in data['adc_samples']:
        point = Point("adc_voltage") \
            .tag("channel", sample['channel']) \
            .field("voltage", sample['voltage']) \
            .time(sample['timestamp'])
        points.append(point)
    
    write_api.write(bucket="esp32_data", record=points)
```

---

*This comprehensive data visualization strategy ensures compatibility with popular tools while providing flexibility for custom analysis workflows.*
