import requests
import matplotlib.pyplot as plt
import time
import json
import threading
import queue
from websocket import WebSocketApp

class ESP32DataLogger:
    def __init__(self, host='192.168.86.100', port=80):
        self.base_url = f'http://{host}:{port}'
        self.ws_url = f'ws://{host}:{port}/ws'
        self.data_queue = queue.Queue()
        self.ws = None
        self.running = False

    def get_latest_data(self):
        """Fallback HTTP method"""
        response = requests.get(f'{self.base_url}/api/data/latest')
        return response.json()

    def on_message(self, _ws, message):
        """WebSocket message handler"""
        try:
            data = json.loads(message)
            if data.get('type') == 'data':
                self.data_queue.put(data)
        except json.JSONDecodeError:
            print(f"Invalid JSON received: {message}")

    def on_error(self, _ws, error):
        print(f"WebSocket error: {error}")

    def on_close(self, _ws, _close_status_code, _close_msg):
        print("WebSocket connection closed")
        self.running = False

    def on_open(self, ws):
        print("WebSocket connection opened")
        self.running = True
        # Send initial message to register as client
        ws.send('{"type":"connect","message":"Python client connected"}')

    def start_websocket(self):
        """Start WebSocket connection in a separate thread"""
        # websocket.enableTrace(True)  # Enable for debugging
        self.ws = WebSocketApp(self.ws_url,
                              on_open=self.on_open,
                              on_message=self.on_message,
                              on_error=self.on_error,
                              on_close=self.on_close)

        # Run WebSocket in a separate thread
        ws_thread = threading.Thread(target=self.ws.run_forever)
        ws_thread.daemon = True
        ws_thread.start()

        # Wait for connection
        time.sleep(2)
        return self.running

# Real-time plotting with WebSocket
logger = ESP32DataLogger()

# Data storage for plotting - separate timestamps for each channel
adc_data = {
    0: {'timestamps': [], 'voltages': []},
    1: {'timestamps': [], 'voltages': []}
}

plt.ion()  # Interactive mode
fig, ax = plt.subplots()
ax.set_title('ESP32 ADC Real-time Data (WebSocket)')
ax.set_xlabel('Time (s)')
ax.set_ylabel('Voltage (V)')

# Try to connect via WebSocket
if logger.start_websocket():
    print("Using WebSocket for real-time data")
    start_time = time.time()

    last_plot_time = time.time()
    plot_interval = 0.1  # Update plot every 100ms
    time_window = 10.0   # Show last 10 seconds of data

    while True:
        try:
            # Get data from WebSocket queue (non-blocking)
            data = logger.data_queue.get_nowait()

            current_time = time.time() - start_time
            channel = data['channel']
            voltage = data['voltage']

            # Store data per channel
            if channel in adc_data:
                adc_data[channel]['timestamps'].append(current_time)
                adc_data[channel]['voltages'].append(voltage)

                # Keep only data within time window
                cutoff_time = current_time - time_window
                while (len(adc_data[channel]['timestamps']) > 0 and
                       adc_data[channel]['timestamps'][0] < cutoff_time):
                    adc_data[channel]['timestamps'].pop(0)
                    adc_data[channel]['voltages'].pop(0)

            # Update plot at regular intervals
            if time.time() - last_plot_time > plot_interval:
                ax.clear()
                ax.set_title('ESP32 ADC Real-time Data (WebSocket)')
                ax.set_xlabel('Time (s)')
                ax.set_ylabel('Voltage (V)')

                # Plot each channel as line plot
                for channel_num, channel_data in adc_data.items():
                    if len(channel_data['voltages']) > 0:
                        ax.plot(channel_data['timestamps'], channel_data['voltages'],
                               label=f'ADC{channel_num}', marker='o', markersize=1, linewidth=1)

                # Set consistent axis limits
                current_time_now = time.time() - start_time
                ax.set_xlim(current_time_now - time_window, current_time_now)
                ax.set_ylim(0, 1.0)  # Assuming 0-1V range for ADC

                ax.legend()
                ax.grid(True, alpha=0.3)
                plt.pause(0.01)
                last_plot_time = time.time()

        except queue.Empty:
            # No new data, just refresh the plot
            plt.pause(0.01)
            continue
        except KeyboardInterrupt:
            print("Stopping...")
            break

else:
    print("WebSocket failed, falling back to HTTP polling")
    # Fallback to original HTTP method - reset data structure for HTTP
    http_timestamps = []
    http_adc_data = {0: [], 1: []}

    while True:
        try:
            data = logger.get_latest_data()
            current_time = time.time()

            http_timestamps.append(current_time)
            http_adc_data[0].append(data['adc']['channel0']['voltage'])
            http_adc_data[1].append(data['adc']['channel1']['voltage'])

            # Keep only last 100 points
            if len(http_timestamps) > 100:
                http_timestamps.pop(0)
                http_adc_data[0].pop(0)
                http_adc_data[1].pop(0)

            ax.clear()
            ax.set_title('ESP32 ADC Data (HTTP Polling)')
            ax.plot(http_timestamps, http_adc_data[0], label='ADC0')
            ax.plot(http_timestamps, http_adc_data[1], label='ADC1')
            ax.legend()
            plt.pause(0.1)

        except KeyboardInterrupt:
            print("Stopping...")
            break