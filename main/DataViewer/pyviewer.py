import requests
import matplotlib.pyplot as plt
import time

class ESP32DataLogger:
    def __init__(self, host='192.168.86.100', port=80):
        self.base_url = f'http://{host}:{port}'
    
    def get_latest_data(self):
        response = requests.get(f'{self.base_url}/api/data/latest')
        return response.json()

# Real-time plotting
logger = ESP32DataLogger()
timestamps = []
adc0_values = []
adc1_values = []

plt.ion()  # Interactive mode
fig, ax = plt.subplots()

while True:
    data = logger.get_latest_data()
    timestamps.append(time.time())
    adc0_values.append(data['adc']['channel0']['voltage'])
    adc1_values.append(data['adc']['channel1']['voltage'])
    
    # Keep only last 100 points
    if len(timestamps) > 100:
        timestamps.pop(0)
        adc0_values.pop(0)
        adc1_values.pop(0)
    
    ax.clear()
    ax.plot(timestamps, adc0_values, label='ADC0')
    ax.plot(timestamps, adc1_values, label='ADC1')
    ax.legend()
    plt.pause(0.1)