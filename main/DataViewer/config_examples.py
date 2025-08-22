#!/usr/bin/env python3
"""
Concrete examples of how to send POST requests to configure the ESP32
"""

import requests
import json

# ESP32 IP address - change this to match your device
# For demo purposes, we'll use localhost test server
ESP32_IP = "localhost"  # Change to "192.168.86.100" for real ESP32
ESP32_PORT = 8080       # Change to 80 for real ESP32
BASE_URL = f"http://{ESP32_IP}:{ESP32_PORT}"

def example_1_update_adc_channels():
    """Example 1: Update ADC channel configuration"""
    print("=== Example 1: Update ADC Channels ===")
    
    # Configuration data to send
    config_data = {
        "channels": [
            {"channel": 0, "enabled": True, "sample_rate": 1000},   # Enable channel 0 at 1000 Hz
            {"channel": 1, "enabled": True, "sample_rate": 500},    # Enable channel 1 at 500 Hz
            {"channel": 2, "enabled": False},                       # Disable channel 2
            {"channel": 3, "enabled": True, "sample_rate": 2000}    # Enable channel 3 at 2000 Hz
        ]
    }
    
    try:
        # Send POST request
        response = requests.post(
            f"{BASE_URL}/api/config/adc",
            json=config_data,  # This automatically sets Content-Type: application/json
            timeout=10
        )
        
        # Check if request was successful
        response.raise_for_status()  # Raises an exception for bad status codes
        
        # Parse the response
        result = response.json()
        
        print("✓ ADC configuration updated successfully!")
        print(f"  Changes made: {len(result.get('changes', []))}")
        print(f"  Restart required: {result.get('restart_required', False)}")
        
        # Show what changed
        for change in result.get('changes', []):
            print(f"    Channel {change['channel']}: {change['property']} = {change['value']}")
            
        return result
        
    except requests.exceptions.RequestException as e:
        print(f"✗ Error updating ADC config: {e}")
        return None

def example_2_update_uart_ports():
    """Example 2: Update UART port configuration"""
    print("\n=== Example 2: Update UART Ports ===")
    
    # Configuration data to send
    config_data = {
        "ports": [
            {"port": 0, "enabled": True, "baud_rate": 115200},   # Enable UART0 at 115200 baud
            {"port": 1, "enabled": False}                        # Disable UART1
        ]
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/config/uart",
            json=config_data,
            timeout=10
        )
        
        response.raise_for_status()
        result = response.json()
        
        print("✓ UART configuration updated successfully!")
        print(f"  Changes made: {len(result.get('changes', []))}")
        
        for change in result.get('changes', []):
            print(f"    Port {change['port']}: {change['property']} = {change['value']}")
            
        return result
        
    except requests.exceptions.RequestException as e:
        print(f"✗ Error updating UART config: {e}")
        return None

def example_3_apply_configuration():
    """Example 3: Apply configuration changes and restart services"""
    print("\n=== Example 3: Apply Configuration Changes ===")
    
    # Specify which services to restart
    apply_data = {
        "restart_adc": True,        # Restart ADC service
        "restart_uart": True,       # Restart UART service
        "restart_data_logger": True # Restart data logger coordination
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/config/apply",
            json=apply_data,
            timeout=15  # Longer timeout since services are restarting
        )
        
        response.raise_for_status()
        result = response.json()
        
        print("✓ Configuration applied successfully!")
        print(f"  Overall success: {result.get('success', False)}")
        
        # Show results for each service
        for service_result in result.get('results', []):
            service = service_result['service']
            success = service_result['success']
            message = service_result['message']
            status = "✓" if success else "✗"
            print(f"    {status} {service}: {message}")
            
        return result
        
    except requests.exceptions.RequestException as e:
        print(f"✗ Error applying configuration: {e}")
        return None

def example_4_complete_workflow():
    """Example 4: Complete configuration workflow"""
    print("\n=== Example 4: Complete Configuration Workflow ===")
    
    # Step 1: Get current configuration
    print("1. Getting current configuration...")
    try:
        response = requests.get(f"{BASE_URL}/api/config", timeout=5)
        response.raise_for_status()
        current_config = response.json()
        print("   ✓ Current configuration retrieved")
    except requests.exceptions.RequestException as e:
        print(f"   ✗ Failed to get current config: {e}")
        return
    
    # Step 2: Update ADC settings
    print("2. Updating ADC settings...")
    adc_result = example_1_update_adc_channels()
    if not adc_result:
        return
    
    # Step 3: Update UART settings
    print("3. Updating UART settings...")
    uart_result = example_2_update_uart_ports()
    if not uart_result:
        return
    
    # Step 4: Apply all changes
    print("4. Applying configuration changes...")
    apply_result = example_3_apply_configuration()
    if not apply_result:
        return
    
    print("\n🎉 Complete configuration workflow finished successfully!")

def example_5_error_handling():
    """Example 5: Proper error handling"""
    print("\n=== Example 5: Error Handling Examples ===")
    
    # Example of invalid data (should fail validation)
    invalid_config = {
        "channels": [
            {"channel": 99, "enabled": True, "sample_rate": 50000}  # Invalid channel and sample rate
        ]
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/config/adc",
            json=invalid_config,
            timeout=10
        )
        
        # Check status code
        if response.status_code == 400:
            print("✓ Server correctly rejected invalid configuration")
            error_info = response.json()
            print(f"   Error message: {error_info.get('error', 'Unknown error')}")
        else:
            response.raise_for_status()
            print("⚠ Server accepted invalid configuration (unexpected)")
            
    except requests.exceptions.RequestException as e:
        print(f"✗ Network error: {e}")

def main():
    """Run all examples"""
    print(f"ESP32 Configuration Examples")
    print(f"Connecting to: {BASE_URL}")
    print("=" * 60)
    
    # Test basic connectivity first
    try:
        response = requests.get(f"{BASE_URL}/api/config", timeout=3)
        response.raise_for_status()
        print("✓ ESP32 is reachable and responding")
    except requests.exceptions.RequestException as e:
        print(f"✗ Cannot connect to ESP32: {e}")
        print("  Make sure the ESP32 is running and the IP address is correct")
        return
    
    # Run examples
    example_1_update_adc_channels()
    example_2_update_uart_ports()
    example_3_apply_configuration()
    example_4_complete_workflow()
    example_5_error_handling()
    
    print("\n" + "=" * 60)
    print("All examples completed!")

if __name__ == "__main__":
    main()
