#!/usr/bin/env python3
"""
Demo script to show the button functionality of the ESP32 ADC viewer
This creates a simple plot with simulated data and interactive buttons
"""

import matplotlib
# Configure matplotlib backend and window behavior
matplotlib.use('TkAgg')
matplotlib.rcParams['figure.raise_window'] = False
matplotlib.rcParams['figure.max_open_warning'] = 0

import matplotlib.pyplot as plt
from matplotlib.widgets import Button
import numpy as np

# Channel visibility state
channel_visible = {0: True, 1: True, 2: True, 3: True}
buttons = {}

# Global flag to control the demo
running = True

def on_window_close(event):
    """Handle window close event"""
    global running
    print("Demo window closed by user, terminating...")
    running = False
    plt.close('all')

def toggle_channel(channel_num):
    """Toggle visibility of a specific channel"""
    def callback(event):
        channel_visible[channel_num] = not channel_visible[channel_num]
        # Update button text and color
        if channel_visible[channel_num]:
            buttons[channel_num].label.set_text(f'ADC{channel_num}: ON')
            buttons[channel_num].color = 'lightgreen'
        else:
            buttons[channel_num].label.set_text(f'ADC{channel_num}: OFF')
            buttons[channel_num].color = 'lightcoral'
        # Redraw the button
        buttons[channel_num].ax.figure.canvas.draw_idle()
        update_plot()
    return callback

def update_plot():
    """Update the plot based on channel visibility"""
    ax.clear()
    ax.set_title('ESP32 ADC Demo - Click buttons to toggle channels')
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Voltage (V)')
    
    colors = ['blue', 'red', 'green', 'orange']
    
    # Generate some demo data
    t = np.linspace(0, 10, 100)
    
    for ch in range(4):
        if channel_visible[ch]:
            # Create different waveforms for each channel
            if ch == 0:
                y = 1.5 + 0.5 * np.sin(2 * np.pi * 0.5 * t)  # 0.5 Hz sine
            elif ch == 1:
                y = 2.0 + 0.3 * np.sin(2 * np.pi * 1.0 * t)  # 1 Hz sine
            elif ch == 2:
                y = 1.0 + 0.4 * np.sin(2 * np.pi * 1.5 * t)  # 1.5 Hz sine
            else:
                y = 2.5 + 0.2 * np.sin(2 * np.pi * 2.0 * t)  # 2 Hz sine
            
            ax.plot(t, y, label=f'ADC{ch}', color=colors[ch], linewidth=2)
    
    ax.set_ylim(0, 3.3)
    ax.legend()
    ax.grid(True, alpha=0.3)
    plt.draw()

# Create the main plot
plt.ion()
fig, ax = plt.subplots(figsize=(14, 8))
plt.subplots_adjust(bottom=0.15, right=0.85)

# Create toggle buttons for each channel
button_width = 0.08
button_height = 0.04
button_spacing = 0.02

for i in range(4):
    # Position buttons vertically on the right side
    button_x = 0.87
    button_y = 0.8 - (i * (button_height + button_spacing))
    
    # Create button axes
    button_ax = plt.axes([button_x, button_y, button_width, button_height])
    
    # Create button with initial state
    button = Button(button_ax, f'ADC{i}: ON', color='lightgreen')
    button.on_clicked(toggle_channel(i))
    buttons[i] = button

# Connect window close event
fig.canvas.mpl_connect('close_event', on_window_close)

# Configure window behavior to prevent always-on-top
try:
    manager = fig.canvas.manager
    if hasattr(manager, 'window') and hasattr(manager.window, 'wm_attributes'):
        manager.window.wm_attributes('-topmost', False)
except Exception:
    pass  # Ignore if window configuration fails

# Initial plot
update_plot()

print("Demo started! Click the buttons on the right to toggle ADC channels on/off.")
print("Close the plot window to exit.")

plt.show(block=True)
