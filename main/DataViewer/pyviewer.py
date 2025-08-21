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

# Data storage for plotting
timestamps = []
adc_data = {0: [], 1: []}  # Channel 0 and 1 data

plt.ion()  # Interactive mode
fig, ax = plt.subplots()
ax.set_title('ESP32 ADC Real-time Data (WebSocket)')
ax.set_xlabel('Time (s)')
ax.set_ylabel('Voltage (V)')

# Try to connect via WebSocket
if logger.start_websocket():
    print("Using WebSocket for real-time data")
    start_time = time.time()

    while True:
        try:
            # Get data from WebSocket queue (non-blocking)
            data = logger.data_queue.get_nowait()

            current_time = time.time() - start_time
            channel = data['channel']
            voltage = data['voltage']

            # Store data
            timestamps.append(current_time)
            if channel in adc_data:
                adc_data[channel].append(voltage)

            # Keep only last 200 points
            if len(timestamps) > 200:
                timestamps.pop(0)
                for ch in adc_data:
                    if len(adc_data[ch]) > 200:
                        adc_data[ch].pop(0)

            # Update plot every 10 samples to reduce CPU usage
            if len(timestamps) % 10 == 0:
                ax.clear()
                ax.set_title('ESP32 ADC Real-time Data (WebSocket)')
                ax.set_xlabel('Time (s)')
                ax.set_ylabel('Voltage (V)')

                # Plot each channel that has data
                for channel, values in adc_data.items():
                    if len(values) > 0:
                        # Align timestamps with data length
                        plot_timestamps = timestamps[-len(values):]
                        ax.plot(plot_timestamps, values, label=f'ADC{channel}', marker='o', markersize=2)

                ax.legend()
                ax.grid(True, alpha=0.3)
                plt.pause(0.01)  # Much faster updates

        except queue.Empty:
            # No new data, just refresh the plot
            plt.pause(0.01)
            continue
        except KeyboardInterrupt:
            print("Stopping...")
            break

else:
    print("WebSocket failed, falling back to HTTP polling")
    # Fallback to original HTTP method
    while True:
        try:
            data = logger.get_latest_data()
            current_time = time.time()

            timestamps.append(current_time)
            adc_data[0].append(data['adc']['channel0']['voltage'])
            adc_data[1].append(data['adc']['channel1']['voltage'])

            # Keep only last 100 points
            if len(timestamps) > 100:
                timestamps.pop(0)
                adc_data[0].pop(0)
                adc_data[1].pop(0)

            ax.clear()
            ax.set_title('ESP32 ADC Data (HTTP Polling)')
            ax.plot(timestamps, adc_data[0], label='ADC0')
            ax.plot(timestamps, adc_data[1], label='ADC1')
            ax.legend()
            plt.pause(0.1)

        except KeyboardInterrupt:
            print("Stopping...")
            break