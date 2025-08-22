#!/usr/bin/env python3
"""
Quick configuration script for ESP32
Simple interactive tool to send configuration commands
"""

import requests
import json
import argparse

def send_adc_config(ip, port, channel, enabled=None, sample_rate=None):
    """Send ADC configuration for a single channel"""
    url = f"http://{ip}:{port}/api/config/adc"
    
    # Build channel config
    channel_config = {"channel": channel}
    if enabled is not None:
        channel_config["enabled"] = enabled
    if sample_rate is not None:
        channel_config["sample_rate"] = sample_rate
    
    data = {"channels": [channel_config]}
    
    try:
        response = requests.post(url, json=data, timeout=10)
        response.raise_for_status()
        result = response.json()
        
        print(f"✓ ADC Channel {channel} updated successfully!")
        for change in result.get('changes', []):
            print(f"  {change['property']} = {change['value']}")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"✗ Error: {e}")
        return False

def send_uart_config(ip, port, uart_port, enabled=None, baud_rate=None):
    """Send UART configuration for a single port"""
    url = f"http://{ip}:{port}/api/config/uart"
    
    # Build port config
    port_config = {"port": uart_port}
    if enabled is not None:
        port_config["enabled"] = enabled
    if baud_rate is not None:
        port_config["baud_rate"] = baud_rate
    
    data = {"ports": [port_config]}
    
    try:
        response = requests.post(url, json=data, timeout=10)
        response.raise_for_status()
        result = response.json()
        
        print(f"✓ UART Port {uart_port} updated successfully!")
        for change in result.get('changes', []):
            print(f"  {change['property']} = {change['value']}")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"✗ Error: {e}")
        return False

def apply_config(ip, port, restart_adc=False, restart_uart=False, restart_data_logger=False):
    """Apply configuration changes"""
    url = f"http://{ip}:{port}/api/config/apply"
    
    data = {
        "restart_adc": restart_adc,
        "restart_uart": restart_uart,
        "restart_data_logger": restart_data_logger
    }
    
    try:
        response = requests.post(url, json=data, timeout=15)
        response.raise_for_status()
        result = response.json()
        
        print("✓ Configuration applied!")
        for service_result in result.get('results', []):
            service = service_result['service']
            success = service_result['success']
            status = "✓" if success else "✗"
            print(f"  {status} {service}")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"✗ Error: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Quick ESP32 Configuration Tool')
    parser.add_argument('--ip', default='192.168.86.100', help='ESP32 IP address')
    parser.add_argument('--port', type=int, default=80, help='ESP32 port')
    
    # ADC commands
    parser.add_argument('--adc-channel', type=int, help='ADC channel to configure (0-3)')
    parser.add_argument('--adc-enable', action='store_true', help='Enable ADC channel')
    parser.add_argument('--adc-disable', action='store_true', help='Disable ADC channel')
    parser.add_argument('--adc-rate', type=int, help='ADC sample rate (Hz)')
    
    # UART commands
    parser.add_argument('--uart-port', type=int, help='UART port to configure (0-1)')
    parser.add_argument('--uart-enable', action='store_true', help='Enable UART port')
    parser.add_argument('--uart-disable', action='store_true', help='Disable UART port')
    parser.add_argument('--uart-baud', type=int, help='UART baud rate')
    
    # Apply commands
    parser.add_argument('--apply', action='store_true', help='Apply configuration changes')
    parser.add_argument('--restart-adc', action='store_true', help='Restart ADC service')
    parser.add_argument('--restart-uart', action='store_true', help='Restart UART service')
    parser.add_argument('--restart-logger', action='store_true', help='Restart data logger')
    
    args = parser.parse_args()
    
    print(f"ESP32 Quick Configuration Tool")
    print(f"Target: {args.ip}:{args.port}")
    print("-" * 40)
    
    # Handle ADC configuration
    if args.adc_channel is not None:
        enabled = None
        if args.adc_enable:
            enabled = True
        elif args.adc_disable:
            enabled = False
            
        send_adc_config(args.ip, args.port, args.adc_channel, enabled, args.adc_rate)
    
    # Handle UART configuration
    if args.uart_port is not None:
        enabled = None
        if args.uart_enable:
            enabled = True
        elif args.uart_disable:
            enabled = False
            
        send_uart_config(args.ip, args.port, args.uart_port, enabled, args.uart_baud)
    
    # Handle apply configuration
    if args.apply:
        apply_config(args.ip, args.port, args.restart_adc, args.restart_uart, args.restart_logger)

if __name__ == "__main__":
    main()
