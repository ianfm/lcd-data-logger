#!/usr/bin/env python3
"""
Simple test server to demonstrate POST requests
This simulates the ESP32 configuration API for learning purposes
"""

from http.server import HTTPServer, BaseHTTPRequestHandler
import json
import threading
import time

class ConfigHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        """Handle GET requests"""
        if self.path == '/api/config':
            # Simulate current configuration
            config = {
                "adc": [
                    {"channel": 0, "enabled": True, "sample_rate_hz": 1000},
                    {"channel": 1, "enabled": True, "sample_rate_hz": 1000},
                    {"channel": 2, "enabled": False, "sample_rate_hz": 1000},
                    {"channel": 3, "enabled": False, "sample_rate_hz": 1000}
                ],
                "uart": [
                    {"port": 0, "enabled": True, "baud_rate": 115200},
                    {"port": 1, "enabled": False, "baud_rate": 9600}
                ]
            }
            
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps(config).encode())
            
        else:
            self.send_response(404)
            self.end_headers()
    
    def do_POST(self):
        """Handle POST requests"""
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length)
        
        try:
            data = json.loads(post_data.decode('utf-8'))
        except json.JSONDecodeError:
            self.send_response(400)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            error_response = {"success": False, "error": "Invalid JSON"}
            self.wfile.write(json.dumps(error_response).encode())
            return
        
        print(f"Received POST to {self.path}")
        print(f"Data: {json.dumps(data, indent=2)}")
        
        if self.path == '/api/config/adc':
            self.handle_adc_config(data)
        elif self.path == '/api/config/uart':
            self.handle_uart_config(data)
        elif self.path == '/api/config/apply':
            self.handle_apply_config(data)
        else:
            self.send_response(404)
            self.end_headers()
    
    def handle_adc_config(self, data):
        """Simulate ADC configuration update"""
        changes = []
        
        if 'channels' in data:
            for channel_config in data['channels']:
                channel = channel_config.get('channel')
                if channel is not None and 0 <= channel <= 3:
                    if 'enabled' in channel_config:
                        changes.append({
                            "channel": channel,
                            "property": "enabled",
                            "value": channel_config['enabled']
                        })
                    if 'sample_rate' in channel_config:
                        changes.append({
                            "channel": channel,
                            "property": "sample_rate",
                            "value": channel_config['sample_rate']
                        })
        
        response = {
            "success": len(changes) > 0,
            "restart_required": len(changes) > 0,
            "changes": changes,
            "message": "ADC configuration updated successfully" if changes else "No valid changes found"
        }
        
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(response).encode())
    
    def handle_uart_config(self, data):
        """Simulate UART configuration update"""
        changes = []
        
        if 'ports' in data:
            for port_config in data['ports']:
                port = port_config.get('port')
                if port is not None and 0 <= port <= 1:
                    if 'enabled' in port_config:
                        changes.append({
                            "port": port,
                            "property": "enabled",
                            "value": port_config['enabled']
                        })
                    if 'baud_rate' in port_config:
                        changes.append({
                            "port": port,
                            "property": "baud_rate",
                            "value": port_config['baud_rate']
                        })
        
        response = {
            "success": len(changes) > 0,
            "restart_required": len(changes) > 0,
            "changes": changes,
            "message": "UART configuration updated successfully" if changes else "No valid changes found"
        }
        
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(response).encode())
    
    def handle_apply_config(self, data):
        """Simulate configuration apply"""
        results = []
        
        if data.get('restart_adc'):
            results.append({
                "service": "adc",
                "success": True,
                "message": "ADC service restarted successfully"
            })
        
        if data.get('restart_uart'):
            results.append({
                "service": "uart", 
                "success": True,
                "message": "UART service restarted successfully"
            })
        
        if data.get('restart_data_logger'):
            results.append({
                "service": "data_logger",
                "success": True,
                "message": "Data logger restarted successfully"
            })
        
        response = {
            "success": True,
            "results": results,
            "message": "Configuration applied successfully"
        }
        
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.end_headers()
        self.wfile.write(json.dumps(response).encode())
    
    def log_message(self, format, *args):
        """Override to reduce log spam"""
        pass

def run_test_server():
    """Run the test server"""
    server_address = ('localhost', 8080)
    httpd = HTTPServer(server_address, ConfigHandler)
    print(f"Test server running at http://localhost:8080")
    print("Press Ctrl+C to stop")
    
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down test server...")
        httpd.shutdown()

if __name__ == "__main__":
    run_test_server()
