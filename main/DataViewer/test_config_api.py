#!/usr/bin/env python3
"""
Test script for the new ESP32 Configuration API endpoints
Demonstrates how to remotely configure ADC and UART settings
"""

import requests
import json
import argparse
import time

class ESP32ConfigAPI:
    def __init__(self, host='192.168.86.100', port=80):
        self.base_url = f"http://{host}:{port}"
        
    def get_current_config(self):
        """Get current configuration from ESP32"""
        try:
            response = requests.get(f"{self.base_url}/api/config", timeout=5)
            response.raise_for_status()
            return response.json()
        except requests.exceptions.RequestException as e:
            print(f"Error getting config: {e}")
            return None
    
    def update_adc_config(self, channel_configs):
        """Update ADC configuration
        
        Args:
            channel_configs: List of dicts with 'channel', 'enabled', 'sample_rate'
        """
        payload = {"channels": channel_configs}
        
        try:
            response = requests.post(
                f"{self.base_url}/api/config/adc",
                json=payload,
                headers={'Content-Type': 'application/json'},
                timeout=10
            )
            response.raise_for_status()
            return response.json()
        except requests.exceptions.RequestException as e:
            print(f"Error updating ADC config: {e}")
            return None
    
    def update_uart_config(self, port_configs):
        """Update UART configuration
        
        Args:
            port_configs: List of dicts with 'port', 'enabled', 'baud_rate'
        """
        payload = {"ports": port_configs}
        
        try:
            response = requests.post(
                f"{self.base_url}/api/config/uart",
                json=payload,
                headers={'Content-Type': 'application/json'},
                timeout=10
            )
            response.raise_for_status()
            return response.json()
        except requests.exceptions.RequestException as e:
            print(f"Error updating UART config: {e}")
            return None
    
    def apply_config_changes(self, restart_adc=False, restart_uart=False, restart_data_logger=False):
        """Apply configuration changes and restart services"""
        payload = {
            "restart_adc": restart_adc,
            "restart_uart": restart_uart,
            "restart_data_logger": restart_data_logger
        }
        
        try:
            response = requests.post(
                f"{self.base_url}/api/config/apply",
                json=payload,
                headers={'Content-Type': 'application/json'},
                timeout=15
            )
            response.raise_for_status()
            return response.json()
        except requests.exceptions.RequestException as e:
            print(f"Error applying config: {e}")
            return None

def main():
    parser = argparse.ArgumentParser(description='Test ESP32 Configuration API')
    parser.add_argument('--ip', default='192.168.86.100', help='ESP32 IP address')
    parser.add_argument('--port', type=int, default=80, help='ESP32 port')
    parser.add_argument('--demo', action='store_true', help='Run configuration demo')
    args = parser.parse_args()
    
    api = ESP32ConfigAPI(args.ip, args.port)
    
    print(f"Testing ESP32 Configuration API at {args.ip}:{args.port}")
    print("=" * 60)
    
    # Get current configuration
    print("1. Getting current configuration...")
    config = api.get_current_config()
    if config:
        print("✓ Current configuration retrieved successfully")
        # Handle the actual config structure from ESP32
        adc_config = config.get('adc', {})
        uart_config = config.get('uart', {})

        if isinstance(adc_config, dict) and 'channels' in adc_config:
            print(f"  ADC Channels: {len(adc_config['channels'])}")
        else:
            print(f"  ADC Config: {type(adc_config)} (structure may vary)")

        if isinstance(uart_config, dict) and 'ports' in uart_config:
            print(f"  UART Ports: {len(uart_config['ports'])}")
        else:
            print(f"  UART Config: {type(uart_config)} (structure may vary)")
    else:
        print("✗ Failed to get current configuration")
        return
    
    if not args.demo:
        print("\nUse --demo flag to run configuration change demo")
        return
    
    print("\n2. Testing ADC configuration update...")
    # Test ADC configuration update
    adc_changes = [
        {"channel": 0, "enabled": True, "sample_rate": 1000},
        {"channel": 1, "enabled": True, "sample_rate": 500},
        {"channel": 2, "enabled": False},
        {"channel": 3, "enabled": True, "sample_rate": 2000}
    ]
    
    result = api.update_adc_config(adc_changes)
    if result and result.get('success'):
        print("✓ ADC configuration updated successfully")
        print(f"  Changes made: {len(result.get('changes', []))}")
        for change in result.get('changes', []):
            print(f"    Channel {change['channel']}: {change['property']} = {change['value']}")
    else:
        print("✗ ADC configuration update failed")
        if result:
            print(f"  Error: {result.get('message', 'Unknown error')}")
    
    print("\n3. Testing UART configuration update...")
    # Test UART configuration update
    uart_changes = [
        {"port": 0, "enabled": True, "baud_rate": 115200},
        {"port": 1, "enabled": False}
    ]
    
    result = api.update_uart_config(uart_changes)
    if result and result.get('success'):
        print("✓ UART configuration updated successfully")
        print(f"  Changes made: {len(result.get('changes', []))}")
        for change in result.get('changes', []):
            print(f"    Port {change['port']}: {change['property']} = {change['value']}")
    else:
        print("✗ UART configuration update failed")
        if result:
            print(f"  Error: {result.get('message', 'Unknown error')}")
    
    print("\n4. Applying configuration changes...")
    # Apply changes and restart services
    result = api.apply_config_changes(
        restart_adc=True,
        restart_uart=True,
        restart_data_logger=True
    )
    
    if result and result.get('success'):
        print("✓ Configuration changes applied successfully")
        for service_result in result.get('results', []):
            service = service_result['service']
            success = service_result['success']
            message = service_result['message']
            status = "✓" if success else "✗"
            print(f"  {status} {service}: {message}")
    else:
        print("✗ Failed to apply configuration changes")
        if result:
            print(f"  Error: {result.get('message', 'Unknown error')}")
    
    print("\n5. Verifying new configuration...")
    time.sleep(2)  # Wait for services to restart
    
    new_config = api.get_current_config()
    if new_config:
        print("✓ New configuration retrieved successfully")
        print("  Updated ADC settings:")
        adc_config = new_config.get('adc', {})
        if isinstance(adc_config, dict) and 'channels' in adc_config:
            for i, channel in enumerate(adc_config['channels']):
                print(f"    Channel {i}: enabled={channel.get('enabled')}, rate={channel.get('sample_rate_hz')}Hz")
        else:
            print("    ADC config structure varies from expected format")

        print("  Updated UART settings:")
        uart_config = new_config.get('uart', {})
        if isinstance(uart_config, dict) and 'ports' in uart_config:
            for i, port in enumerate(uart_config['ports']):
                print(f"    Port {i}: enabled={port.get('enabled')}, baud={port.get('baud_rate')}")
        else:
            print("    UART config structure varies from expected format")
    else:
        print("✗ Failed to verify new configuration")
    
    print("\n" + "=" * 60)
    print("Configuration API test completed!")

if __name__ == "__main__":
    main()
