import requests
import matplotlib
import matplotlib.pyplot as plt
import time
import json
import threading
import queue
from websocket import WebSocketApp

# Fix matplotlib backend issues for Python 3.12.3 and interactive plotting
def setup_matplotlib_backend():
    """Setup matplotlib backend with fallback options for compatibility"""
    backends_to_try = ['TkAgg', 'Qt5Agg', 'Qt4Agg', 'Agg']

    for backend in backends_to_try:
        try:
            matplotlib.use(backend)
            print(f"Using matplotlib backend: {backend}")
            return backend != 'Agg'  # Return True if interactive, False if non-interactive
        except ImportError:
            continue

    print("Warning: Could not set any matplotlib backend, using default")
    return True  # Assume interactive

# Setup backend early
INTERACTIVE_MODE = setup_matplotlib_backend()

class ESP32DataLogger:
    def __init__(self, host='192.168.86.100', port=80):
        self.base_url = f'http://{host}:{port}'
        self.ws_url = f'ws://{host}:{port}/ws'  # WebSocket runs on same HTTP server port
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

# Setup plotting with backend compatibility
def safe_plot_setup():
    """Setup matplotlib plotting with error handling"""
    try:
        if INTERACTIVE_MODE:
            plt.ion()  # Interactive mode
        fig, ax = plt.subplots(figsize=(12, 6))
        ax.set_title('ESP32 ADC Real-time Data (WebSocket)')
        ax.set_xlabel('Time (s)')
        ax.set_ylabel('Voltage (V)')
        return fig, ax, True
    except Exception as e:
        print(f"Plot setup failed: {e}")
        return None, None, False

def safe_plot_update(ax, adc_data, start_time, time_window):
    """Safely update plot with error handling"""
    try:
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
        ax.set_ylim(0, 3.3)  # 0-3.3V range for ESP32 ADC

        ax.legend()
        ax.grid(True, alpha=0.3)

        if INTERACTIVE_MODE:
            plt.draw()
            plt.pause(0.01)
        else:
            plt.savefig('adc_realtime.png', dpi=100, bbox_inches='tight')
            print("Plot saved to adc_realtime.png")

        return True
    except Exception as e:
        print(f"Plot update failed: {e}")
        return False

# Setup plotting
fig, ax, plot_success = safe_plot_setup()
if not plot_success:
    print("ERROR: Could not setup plotting. Exiting.")
    exit(1)

print(f"Plotting mode: {'Interactive' if INTERACTIVE_MODE else 'File output'}")

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
                if safe_plot_update(ax, adc_data, start_time, time_window):
                    last_plot_time = time.time()
                else:
                    print("Plot update failed, continuing...")

        except queue.Empty:
            # No new data, just refresh the plot
            try:
                if INTERACTIVE_MODE:
                    plt.pause(0.01)
                else:
                    time.sleep(0.01)
            except:
                time.sleep(0.01)  # Fallback
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

            try:
                ax.clear()
                ax.set_title('ESP32 ADC Data (HTTP Polling)')
                ax.plot(http_timestamps, http_adc_data[0], label='ADC0')
                ax.plot(http_timestamps, http_adc_data[1], label='ADC1')
                ax.legend()
                ax.grid(True, alpha=0.3)

                if INTERACTIVE_MODE:
                    plt.draw()
                    plt.pause(0.1)
                else:
                    plt.savefig('adc_http_polling.png', dpi=100, bbox_inches='tight')
                    print("HTTP plot saved to adc_http_polling.png")
                    time.sleep(0.5)  # Slower updates for file mode
            except Exception as e:
                print(f"HTTP plot update failed: {e}")
                time.sleep(0.1)

        except KeyboardInterrupt:
            print("Stopping...")
            break