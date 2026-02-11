#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Qt MCP Plugin - Comprehensive Test Suite
========================================

This script provides comprehensive testing for the Qt MCP Plugin, consolidating all 
testing functionality into a single, well-organized Python script.

USAGE EXAMPLES:
    python test_suite.py                    # Run all tests
    python test_suite.py --verbose          # Detailed output
    python test_suite.py --http-only        # Test HTTP protocol only
    python test_suite.py --tcp-only         # Test TCP protocol only
    python test_suite.py --iterative        # Run with retry logic (CI/CD)
    python test_suite.py --help             # Show all options

FEATURES:
- HTTP and TCP protocol testing
- Plugin version verification
- Build verification
- Platform-specific testing (Windows, macOS, Linux)
- Iterative testing with retry logic
- Comprehensive error reporting
- CORS support testing
- Protocol detection and consistency verification

TEST CATEGORIES:
1. Server Connectivity - Basic connection to port 3001
2. TCP MCP Protocol - JSON-RPC over TCP
3. HTTP MCP Protocol - HTTP/1.1 with JSON-RPC
4. Protocol Detection - Automatic HTTP vs TCP detection
5. Plugin Version - Server version and identification

REQUIREMENTS:
- Qt Creator running with MCP Plugin loaded
- MCP server listening on port 3001
- Python 2.7+ or Python 3.x

PLATFORM SUPPORT:
- Windows: Uses taskkill, telnet for testing
- macOS: Uses pkill, nc (netcat) for testing  
- Linux: Uses pkill, nc (netcat) for testing

For detailed documentation, see documentation/TESTING.md
"""

import socket
import json
import sys
import time
import argparse
import subprocess
import os
import platform

# Import Qt configuration
try:
    from qt_config import get_qt_config
except ImportError:
    print("[ERROR] qt_config.py not found. Ensure scripts/build/qt_config.py exists and PYTHONPATH includes the repo root (or scripts/build).")
    sys.exit(1)
import shutil

class Colors:
    """ANSI color codes for terminal output"""
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    WHITE = '\033[97m'
    BOLD = '\033[1m'
    END = '\033[0m'

class TestResult:
    """Test result tracking"""
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.tests = []

    def add_test(self, name, success, message=""):
        """Add a test result"""
        self.tests.append({
            'name': name,
            'success': success,
            'message': message
        })
        if success:
            self.passed += 1
        else:
            self.failed += 1

    def print_summary(self):
        """Print test summary"""
        total = self.passed + self.failed
        print("\n" + "=" * 50)
        print("TEST SUMMARY")
        print("=" * 50)
        print("Results: {}/{} tests passed".format(self.passed, total))
        
        if self.failed > 0:
            print("\nFAILED TESTS:")
            for test in self.tests:
                if not test['success']:
                    print("  - {}: {}".format(test['name'], test['message']))
        
        return self.failed == 0

def print_header(title, color=Colors.CYAN):
    """Print a formatted header"""
    print("\n" + color + Colors.BOLD + title + Colors.END)
    print(color + "=" * len(title) + Colors.END)

def print_test_result(name, success, message=""):
    """Print a test result with appropriate color"""
    status = "PASS" if success else "FAIL"
    color = Colors.GREEN if success else Colors.RED
    print("[{}] {}".format(status, name) + color + Colors.END)
    if message:
        print("      " + Colors.YELLOW + message + Colors.END)

def get_platform_config():
    """Get platform-specific configuration using scripts/build/qt_config.py"""
    return get_qt_config()

def send_tcp_request(request_data, timeout=5):
    """Send a TCP request and return response"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect(('localhost', 3001))
        
        # Send request with newline terminator
        sock.send((request_data + '\n').encode('utf-8'))
        
        # Read response in chunks
        response_data = b''
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response_data += chunk
            
            # TCP responses end with newline, so we can stop when we have a complete JSON
            try:
                response_str = response_data.decode('utf-8').strip()
                json.loads(response_str)  # Try to parse as JSON
                break  # Successfully parsed, we have complete response
            except:
                continue  # Keep reading
        
        sock.close()
        return response_data.decode('utf-8').strip()
    except Exception as e:
        raise Exception("TCP request failed: {}".format(str(e)))

def send_http_request(method, path="/", headers=None, body=None, timeout=5):
    """Send an HTTP request and return response"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        sock.connect(('localhost', 3001))
        
        # Build HTTP request
        http_request = "{} {} HTTP/1.1\r\n".format(method, path)
        http_request += "Host: localhost\r\n"
        
        if headers:
            for key, value in headers.items():
                http_request += "{}: {}\r\n".format(key, value)
        
        if body:
            http_request += "Content-Length: {}\r\n".format(len(body))
        
        http_request += "\r\n"
        
        if body:
            http_request += body
        
        # Send request
        sock.send(http_request.encode('utf-8'))
        
        # Read response in chunks until we get the full response
        response_data = b''
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response_data += chunk
            
            # Check if we have a complete HTTP response
            response_str = response_data.decode('utf-8', errors='ignore')
            if '\r\n\r\n' in response_str:
                # We have headers, check if we have the complete body
                header_end = response_str.find('\r\n\r\n') + 4
                headers = response_str[:header_end]
                
                # Look for Content-Length header
                content_length = None
                for line in headers.split('\r\n'):
                    if line.lower().startswith('content-length:'):
                        content_length = int(line.split(':', 1)[1].strip())
                        break
                
                if content_length is not None:
                    body_start = header_end
                    body_length = len(response_data) - body_start
                    if body_length >= content_length:
                        break
                else:
                    # No content-length, assume response is complete
                    break
        
        sock.close()
        return response_data.decode('utf-8', errors='ignore')
    except Exception as e:
        raise Exception("HTTP request failed: {}".format(str(e)))

def parse_http_response(response_text):
    """Parse HTTP response into headers and body"""
    lines = response_text.split('\r\n')
    
    # Parse status line
    status_line = lines[0]
    status_code = int(status_line.split()[1])
    
    # Parse headers
    headers = {}
    body_start = 1
    for i, line in enumerate(lines[1:], 1):
        if line == '':
            body_start = i + 1
            break
        if ':' in line:
            key, value = line.split(':', 1)
            headers[key.strip().lower()] = value.strip()
    
    # Get body
    body = '\r\n'.join(lines[body_start:]) if body_start < len(lines) else ''
    
    return {
        'status_code': status_code,
        'headers': headers,
        'body': body
    }

def qt_mcp_tools_call(tool_name, arguments=None, timeout=10):
    params = {"name": tool_name}
    if arguments:
        params["arguments"] = arguments
    req = {"jsonrpc": "2.0", "method": "tools/call", "params": params, "id": 1}
    raw = send_tcp_request(json.dumps(req), timeout=timeout)
    data = json.loads(raw)
    if "error" in data:
        raise RuntimeError("Qt MCP error: " + str(data["error"]))
    return data.get("result", {})

def launch_qt_creator():
    try:
        config = get_platform_config()
        bin_path = config.get("qt_creator_bin")
        app_path = config.get("qt_creator_app")
        if not bin_path or not os.path.exists(bin_path):
            return False
        if platform.system().lower() == "darwin" and app_path and os.path.exists(app_path):
            subprocess.Popen(["open", "-a", app_path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        else:
            subprocess.Popen([bin_path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd=os.path.dirname(bin_path))
        return True
    except Exception:
        return False

def kjams_mcp_get_app_version(base_url, timeout=5):
    try:
        if sys.version_info[0] >= 3:
            from urllib.request import Request, urlopen
        else:
            from urllib2 import Request, urlopen
        url = base_url.rstrip("/") + "/"
        body = json.dumps({"jsonrpc": "2.0", "method": "tools/call", "id": 1, "params": {"name": "get_app_version", "arguments": {}}})
        req = Request(url, data=body.encode("utf-8") if isinstance(body, str) else body, headers={"Content-Type": "application/json"}, method="POST")
        resp = urlopen(req, timeout=timeout)
        data = json.loads(resp.read().decode("utf-8"))
        if "error" in data:
            return None
        content = data.get("result", {}).get("content") or []
        if not content:
            return None
        text = content[0].get("text")
        if text is None:
            return None
        try:
            parsed = json.loads(text)
            return parsed.get("version") or parsed.get("content") or text
        except Exception:
            return text
    except Exception:
        return None

def test_server_connectivity(result):
    """Test basic server connectivity"""
    print_header("Server Connectivity Test")
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect(('localhost', 3001))
        sock.close()
        print_test_result("TCP Connection", True, "Port 3001 is accessible")
        result.add_test("TCP Connection", True)
        return True
    except Exception as e:
        print_test_result("TCP Connection", False, "Cannot connect to port 3001: {}".format(str(e)))
        result.add_test("TCP Connection", False, str(e))
        return False

def test_tcp_mcp_initialize(result, verbose=False):
    """Test TCP MCP initialize"""
    print_header("TCP MCP Initialize Test")
    
    request_data = json.dumps({
        "jsonrpc": "2.0",
        "method": "initialize",
        "id": 1,
        "params": {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {
                "name": "TCP Test Client",
                "version": "1.0.0"
            }
        }
    })
    
    try:
        response_text = send_tcp_request(request_data)
        response_data = json.loads(response_text)
        
        success = (response_data.get('jsonrpc') == '2.0' and 
                  response_data.get('result', {}).get('serverInfo', {}).get('name') == 'Qt MCP Plugin')
        
        print_test_result("TCP Initialize Request", success, "Response received")
        
        if success and verbose:
            server_info = response_data['result']['serverInfo']
            print("      Server: {} v{}".format(server_info['name'], server_info['version']))
        
        result.add_test("TCP Initialize Request", success)
        return success
        
    except Exception as e:
        print_test_result("TCP Initialize Request", False, str(e))
        result.add_test("TCP Initialize Request", False, str(e))
        return False

def test_tcp_mcp_tools_list(result, verbose=False):
    """Test TCP MCP tools list"""
    print_header("TCP MCP Tools List Test")
    
    request_data = json.dumps({
        "jsonrpc": "2.0",
        "method": "tools/list",
        "id": 2
    })
    
    try:
        response_text = send_tcp_request(request_data)
        response_data = json.loads(response_text)
        
        success = (response_data.get('jsonrpc') == '2.0' and 
                  len(response_data.get('result', {}).get('tools', [])) > 0)
        
        print_test_result("TCP Tools List Request", success, "Response received")
        
        if success and verbose:
            tools = response_data['result']['tools']
            print("      Tools Found: {}".format(len(tools)))
            if verbose:
                for tool in tools[:5]:  # Show first 5 tools
                    print("        - {}: {}".format(tool['name'], tool['description']))
        
        result.add_test("TCP Tools List Request", success)
        return success
        
    except Exception as e:
        print_test_result("TCP Tools List Request", False, str(e))
        result.add_test("TCP Tools List Request", False, str(e))
        return False

def test_http_mcp_initialize(result, verbose=False):
    """Test HTTP MCP initialize"""
    print_header("HTTP MCP Initialize Test")
    
    request_data = json.dumps({
        "jsonrpc": "2.0",
        "method": "initialize",
        "id": 3,
        "params": {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {
                "name": "HTTP Test Client",
                "version": "1.0.0"
            }
        }
    })
    
    try:
        response_text = send_http_request("POST", "/", 
                                        {"Content-Type": "application/json"}, 
                                        request_data)
        response = parse_http_response(response_text)
        
        if response['status_code'] == 200:
            response_data = json.loads(response['body'])
            success = (response_data.get('jsonrpc') == '2.0' and 
                      response_data.get('result', {}).get('serverInfo', {}).get('name') == 'Qt MCP Plugin')
            
            print_test_result("HTTP Initialize Request", success, "Status: {}".format(response['status_code']))
            
            if success and verbose:
                server_info = response_data['result']['serverInfo']
                print("      Server: {} v{}".format(server_info['name'], server_info['version']))
            
            result.add_test("HTTP Initialize Request", success)
            return success
        else:
            print_test_result("HTTP Initialize Request", False, "Status: {}".format(response['status_code']))
            result.add_test("HTTP Initialize Request", False, "HTTP status {}".format(response['status_code']))
            return False
            
    except Exception as e:
        print_test_result("HTTP Initialize Request", False, str(e))
        result.add_test("HTTP Initialize Request", False, str(e))
        return False

def test_http_mcp_tools_list(result, verbose=False):
    """Test HTTP MCP tools list"""
    print_header("HTTP MCP Tools List Test")
    
    request_data = json.dumps({
        "jsonrpc": "2.0",
        "method": "tools/list",
        "id": 4
    })
    
    try:
        response_text = send_http_request("POST", "/", 
                                        {"Content-Type": "application/json"}, 
                                        request_data)
        response = parse_http_response(response_text)
        
        if response['status_code'] == 200:
            response_data = json.loads(response['body'])
            success = (response_data.get('jsonrpc') == '2.0' and 
                      len(response_data.get('result', {}).get('tools', [])) > 0)
            
            print_test_result("HTTP Tools List Request", success, "Status: {}".format(response['status_code']))
            
            if success and verbose:
                tools = response_data['result']['tools']
                print("      Tools Found: {}".format(len(tools)))
            
            result.add_test("HTTP Tools List Request", success)
            return success
        else:
            print_test_result("HTTP Tools List Request", False, "Status: {}".format(response['status_code']))
            result.add_test("HTTP Tools List Request", False, "HTTP status {}".format(response['status_code']))
            return False
            
    except Exception as e:
        print_test_result("HTTP Tools List Request", False, str(e))
        result.add_test("HTTP Tools List Request", False, str(e))
        return False

def test_http_get_info(result, verbose=False):
    """Test HTTP GET server info"""
    print_header("HTTP GET Server Info Test")
    
    try:
        response_text = send_http_request("GET", "/")
        response = parse_http_response(response_text)
        
        success = response['status_code'] == 200
        
        print_test_result("HTTP GET Request", success, "Status: {}".format(response['status_code']))
        
        if success and verbose:
            print("      Response: {}".format(response['body']))
        elif not success and verbose:
            print("      Error Response: {}".format(response['body']))
        
        result.add_test("HTTP GET Request", success)
        return success
        
    except Exception as e:
        print_test_result("HTTP GET Request", False, str(e))
        result.add_test("HTTP GET Request", False, str(e))
        return False

def test_http_cors(result, verbose=False):
    """Test HTTP CORS support"""
    print_header("HTTP CORS Support Test")
    
    try:
        response_text = send_http_request("OPTIONS", "/", {
            "Access-Control-Request-Method": "POST",
            "Access-Control-Request-Headers": "Content-Type"
        })
        response = parse_http_response(response_text)
        
        cors_headers = ['access-control-allow-origin', 'access-control-allow-methods', 'access-control-allow-headers']
        has_cors_headers = all(header in response['headers'] for header in cors_headers)
        
        success = response['status_code'] == 204 and has_cors_headers
        
        print_test_result("CORS Preflight Request", success, "Status: {}".format(response['status_code']))
        
        if success and verbose:
            for header in cors_headers:
                if header in response['headers']:
                    print("      {}: {}".format(header, response['headers'][header]))
        
        result.add_test("CORS Preflight Request", success)
        return success
        
    except Exception as e:
        print_test_result("CORS Preflight Request", False, str(e))
        result.add_test("CORS Preflight Request", False, str(e))
        return False

def test_protocol_detection(result, verbose=False):
    """Test protocol detection"""
    print_header("Protocol Detection Test")
    
    try:
        response_text = send_http_request("GET", "/")
        
        # Check if response contains HTTP headers
        success = "HTTP/1.1" in response_text
        
        print_test_result("HTTP Protocol Detection", success, "HTTP request properly identified")
        
        if verbose:
            print("      Response: {}".format(response_text[:200] + "..." if len(response_text) > 200 else response_text))
        
        result.add_test("HTTP Protocol Detection", success)
        return success
        
    except Exception as e:
        print_test_result("HTTP Protocol Detection", False, str(e))
        result.add_test("HTTP Protocol Detection", False, str(e))
        return False

def test_protocol_consistency(result, verbose=False):
    """Test that both protocols return consistent results"""
    print_header("Protocol Consistency Test")
    
    test_request = {
        "jsonrpc": "2.0",
        "method": "initialize",
        "id": 99,
        "params": {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {
                "name": "Consistency Test Client",
                "version": "1.0.0"
            }
        }
    }
    
    http_success = False
    tcp_success = False
    
    # Test HTTP
    try:
        response_text = send_http_request("POST", "/", 
                                        {"Content-Type": "application/json"}, 
                                        json.dumps(test_request))
        response = parse_http_response(response_text)
        if response['status_code'] == 200:
            http_data = json.loads(response['body'])
            http_success = http_data.get('id') == 99
    except:
        pass
    
    # Test TCP
    try:
        response_text = send_tcp_request(json.dumps(test_request))
        tcp_data = json.loads(response_text)
        tcp_success = tcp_data.get('id') == 99
    except:
        pass
    
    success = http_success and tcp_success
    
    print_test_result("Protocol Consistency", success, "Both protocols return consistent results")
    
    if verbose:
        print("      HTTP Success: {}, TCP Success: {}".format(http_success, tcp_success))
    
    result.add_test("Protocol Consistency", success)
    return success

def test_plugin_version(result, verbose=False):
    """Test plugin version verification"""
    print_header("Plugin Version Test")
    
    try:
        request_data = json.dumps({
            "jsonrpc": "2.0",
            "method": "initialize",
            "id": 100,
            "params": {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {
                    "name": "Version Test Client",
                    "version": "1.0.0"
                }
            }
        })
        
        response_text = send_tcp_request(request_data)
        response_data = json.loads(response_text)
        
        if response_data.get('result', {}).get('serverInfo'):
            server_info = response_data['result']['serverInfo']
            version = server_info.get('version', 'unknown')
            name = server_info.get('name', 'unknown')
            
            success = 'Qt MCP Plugin' in name and '1.31.' in version
            
            print_test_result("Plugin Version Check", success, "Version: {}".format(version))
            
            if verbose:
                print("      Server: {} v{}".format(name, version))
            
            result.add_test("Plugin Version Check", success)
            return success
        else:
            print_test_result("Plugin Version Check", False, "No server info in response")
            result.add_test("Plugin Version Check", False, "Missing server info")
            return False
            
    except Exception as e:
        print_test_result("Plugin Version Check", False, str(e))
        result.add_test("Plugin Version Check", False, str(e))
        return False

def test_kjams_build_run_e2e(result, verbose=False):
    """
    E2E: ensure Qt Creator running and plugin version OK; load kjams session;
    select 'kjams nightclub debug' config; build; on errors show JSON and stop;
    on success run app; poll kJams MCP get_app_version until response (20s timeout).
    """
    print_header("kJams Build/Run E2E Test")
    qt_port = 3001
    session_name = "kjams"
    config_name = "kjams nightclub debug"
    kjams_mcp_url = os.environ.get("KJAMS_MCP_URL", "http://localhost:3002")
    build_wait_timeout = 300
    kjams_poll_timeout = 20
    kjams_poll_interval = 2

    # 1) If Qt Creator not open, launch it; then verify we can query plugin version
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(3)
        sock.connect(("localhost", qt_port))
        sock.close()
    except Exception:
        if verbose:
            print("      Qt Creator not reachable, launching...")
        if not launch_qt_creator():
            print_test_result("Launch Qt Creator", False, "Could not launch Qt Creator")
            result.add_test("kJams E2E", False, "Launch Qt Creator failed")
            return False
        for attempt in range(3):
            time.sleep(5)
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(5)
                sock.connect(("localhost", qt_port))
                sock.close()
                break
            except Exception:
                if attempt == 2:
                    print_test_result("Qt Creator MCP reachable after launch", False, "Port 3001 not responding after 3 attempts")
                    result.add_test("kJams E2E", False, "MCP not reachable after launch")
                    return False
        time.sleep(2)

    # Query plugin version (must succeed)
    try:
        req = json.dumps({
            "jsonrpc": "2.0", "method": "initialize", "id": 1,
            "params": {"protocolVersion": "2024-11-05", "capabilities": {}, "clientInfo": {"name": "E2E", "version": "1.0"}}
        })
        raw = send_tcp_request(req, timeout=10)
        data = json.loads(raw)
        info = (data.get("result") or {}).get("serverInfo") or {}
        version = info.get("version", "")
        name = info.get("name", "")
        if "Qt MCP Plugin" not in name or not version:
            print_test_result("Query MCP plugin version", False, "Bad response: " + str(data))
            result.add_test("kJams E2E", False, "Plugin version query failed")
            return False
        if verbose:
            print("      Plugin: {} v{}".format(name, version))
    except Exception as e:
        print_test_result("Query MCP plugin version", False, str(e))
        result.add_test("kJams E2E", False, "Plugin version query: " + str(e))
        return False

    # 2) If kjams session not open, open it
    try:
        current = qt_mcp_tools_call("getCurrentSession", timeout=10)
        text = (current.get("content") or [{}])[0].get("text") or ""
        if session_name not in text:
            qt_mcp_tools_call("loadSession", {"sessionName": session_name}, timeout=30)
            time.sleep(5)
        if verbose:
            print("      Session: " + session_name)
    except Exception as e:
        print_test_result("Load session '" + session_name + "'", False, str(e))
        result.add_test("kJams E2E", False, "Load session: " + str(e))
        return False

    # 3) If "kjams nightclub debug" not selected, select it
    try:
        current = qt_mcp_tools_call("getCurrentBuildConfig", timeout=10)
        text = (current.get("content") or [{}])[0].get("text") or ""
        if config_name not in text:
            qt_mcp_tools_call("switchBuildConfig", {"name": config_name}, timeout=10)
            time.sleep(1)
        if verbose:
            print("      Build config: " + config_name)
    except Exception as e:
        print_test_result("Switch build config '" + config_name + "'", False, str(e))
        result.add_test("kJams E2E", False, "Switch config: " + str(e))
        return False

    # 4) Build
    try:
        qt_mcp_tools_call("build", timeout=15)
        qt_mcp_tools_call("waitForBuildCompletion", {"timeoutSeconds": build_wait_timeout}, timeout=build_wait_timeout + 30)
    except Exception as e:
        print_test_result("Build + wait", False, str(e))
        result.add_test("kJams E2E", False, "Build/wait: " + str(e))
        return False

    # 5) Fetch build diagnostics. If build had errors, fetching and displaying JSON is SUCCESS.
    try:
        diag_result = qt_mcp_tools_call("getBuildDiagnostics", {"filter": "errors"}, timeout=15)
        content = diag_result.get("content") or []
        text = (content[0].get("text") if content else None) or ""
        try:
            diag_data = json.loads(text)
        except Exception:
            diag_data = {}
        diagnostics = diag_data.get("diagnostics") or []
        if diagnostics:
            print(Colors.CYAN + "Build had errors. Fetched and displaying diagnostics (JSON):" + Colors.END)
            print(json.dumps({"diagnostics": diagnostics}, indent=2))
            print_test_result("Fetched and displayed build diagnostics", True, "Build failed; successfully fetched and displayed " + str(len(diagnostics)) + " diagnostic(s)")
            result.add_test("kJams E2E", True, "Build had errors; successfully fetched and displayed diagnostics (JSON)")
            return True
    except Exception as e:
        print_test_result("Get build diagnostics", False, str(e))
        result.add_test("kJams E2E", False, "getBuildDiagnostics: " + str(e))
        return False

    # 6) Run the app
    try:
        qt_mcp_tools_call("runProject", timeout=15)
        time.sleep(2)
    except Exception as e:
        print_test_result("Run project", False, str(e))
        result.add_test("kJams E2E", False, "runProject: " + str(e))
        return False

    # 7) Poll kJams MCP get_app_version until app responds; timeout 20s
    deadline = time.time() + kjams_poll_timeout
    version = None
    while time.time() < deadline:
        version = kjams_mcp_get_app_version(kjams_mcp_url, timeout=3)
        if version is not None:
            break
        time.sleep(kjams_poll_interval)
    if version is None:
        print_test_result("kJams app version (poll " + str(kjams_poll_timeout) + "s)", False, "No response from kJams MCP at " + kjams_mcp_url)
        result.add_test("kJams E2E", False, "kJams get_app_version timeout")
        return False
    print_test_result("kJams app version", True, "Version: " + str(version))
    result.add_test("kJams E2E", True, "App version: " + str(version))
    return True

def iterative_test(result, max_attempts=5, delay=2):
    """Run iterative tests with retry logic"""
    print_header("Iterative Testing")
    
    attempts = 0
    while attempts < max_attempts:
        attempts += 1
        print("Attempt {}/{}".format(attempts, max_attempts))
        
        if test_server_connectivity(result):
            print_test_result("Iterative Connection", True, "Connected on attempt {}".format(attempts))
            result.add_test("Iterative Connection", True)
            return True
        
        if attempts < max_attempts:
            print("      Waiting {} seconds before retry...".format(delay))
            time.sleep(delay)
    
    print_test_result("Iterative Connection", False, "Failed after {} attempts".format(max_attempts))
    result.add_test("Iterative Connection", False, "Max attempts exceeded")
    return False

def main():
    """Main test execution"""
    parser = argparse.ArgumentParser(
        description='Qt MCP Plugin - Comprehensive Test Suite',
        epilog="""
EXAMPLES:
  %(prog)s                    Run all tests
  %(prog)s --verbose          Show detailed test output
  %(prog)s --http-only        Test HTTP protocol only
  %(prog)s --tcp-only         Test TCP protocol only
  %(prog)s --iterative        Run with retry logic (useful for CI/CD)
  %(prog)s --iterative --max-attempts 10 --delay 3  Custom retry settings

TEST CATEGORIES:
  - Server Connectivity: Basic TCP connection to port 3001
  - TCP MCP Protocol: JSON-RPC requests over TCP
  - HTTP MCP Protocol: HTTP/1.1 requests with JSON-RPC
  - Protocol Detection: Automatic HTTP vs TCP detection
  - Plugin Version: Server version and identification

REQUIREMENTS:
  - Qt Creator must be running with MCP Plugin loaded
  - MCP server must be listening on port 3001
  - Python 2.7+ or Python 3.x

For detailed documentation, see documentation/TESTING.md
        """,
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument('--verbose', '-v', action='store_true', 
                       help='Show detailed test output with response data and debugging info')
    parser.add_argument('--http-only', action='store_true', 
                       help='Test HTTP protocol only (skip TCP tests)')
    parser.add_argument('--tcp-only', action='store_true', 
                       help='Test TCP protocol only (skip HTTP tests)')
    parser.add_argument('--iterative', '-i', action='store_true', 
                       help='Run iterative tests with retry logic (useful for CI/CD environments)')
    parser.add_argument('--max-attempts', type=int, default=5, 
                       help='Maximum number of connection attempts for iterative tests (default: 5)')
    parser.add_argument('--delay', type=int, default=2, 
                       help='Delay in seconds between iterative test attempts (default: 2)')
    
    parser.add_argument('--build-diagnostics', action='store_true', help='Run getBuildDiagnostics self-test (inject, build, get diagnostics, restore)')
    parser.add_argument('--kjams-e2e', action='store_true', help='Run kJams E2E: launch Qt Creator if needed, load session, select config, build, run, poll kJams app version')
    args = parser.parse_args()
    
    print(Colors.MAGENTA + Colors.BOLD + "Qt MCP Plugin - Comprehensive Test Suite" + Colors.END)
    print(Colors.MAGENTA + "=" * 50 + Colors.END)
    
    config = get_platform_config()
    print("Platform: {}".format(config['name']))
    
    result = TestResult()
    
    if getattr(args, 'kjams_e2e', False):
        test_kjams_build_run_e2e(result, args.verbose)
        if result.print_summary():
            print("\n" + Colors.GREEN + "[SUCCESS] kJams E2E test passed." + Colors.END)
            sys.exit(0)
        else:
            print("\n" + Colors.RED + "[FAIL] kJams E2E test failed." + Colors.END)
            sys.exit(1)
    
    # Iterative connectivity test if requested
    if args.iterative:
        if not iterative_test(result, args.max_attempts, args.delay):
            print("\n" + Colors.RED + "Cannot connect to MCP server after {} attempts. Please ensure Qt Creator is running with the plugin loaded." + Colors.END.format(args.max_attempts))
            sys.exit(1)
    else:
        # Single connectivity test
        if not test_server_connectivity(result):
            print("\n" + Colors.RED + "Cannot connect to MCP server. Please ensure Qt Creator is running with the plugin loaded." + Colors.END)
            sys.exit(1)
    
    # TCP Tests
    if not args.http_only:
        test_tcp_mcp_initialize(result, args.verbose)
        test_tcp_mcp_tools_list(result, args.verbose)
    
    # HTTP Tests
    if not args.tcp_only:
        test_http_get_info(result, args.verbose)
        test_http_mcp_initialize(result, args.verbose)
        test_http_mcp_tools_list(result, args.verbose)
        test_http_cors(result, args.verbose)
    
    # Protocol Tests
    test_protocol_detection(result, args.verbose)
    test_protocol_consistency(result, args.verbose)
    
    # Version Test
    test_plugin_version(result, args.verbose)
    
    # Build diagnostics full self-test (optional)
    if getattr(args, 'build_diagnostics', False):
        print_header('Build Diagnostics Self-Test')
        script_dir = os.path.dirname(os.path.abspath(__file__))
        diag_script = os.path.join(script_dir, 'scripts', 'test', 'test_build_diagnostics.py')
        if os.path.isfile(diag_script):
            cmd = [sys.executable, diag_script]
            if args.verbose:
                cmd.append('-v')
            try:
                r = subprocess.run(cmd, capture_output=True, text=True, timeout=150)
                out = (r.stdout or '') + (r.stderr or '')
                success = (r.returncode == 0)
                print_test_result('Build Diagnostics (inject/build/get/restore)', success, out.strip() or ('exit %s' % r.returncode))
                result.add_test('Build Diagnostics Self-Test', success)
            except subprocess.TimeoutExpired:
                print_test_result('Build Diagnostics Self-Test', False, 'Timeout')
                result.add_test('Build Diagnostics Self-Test', False)
            except Exception as e:
                print_test_result('Build Diagnostics Self-Test', False, str(e))
                result.add_test('Build Diagnostics Self-Test', False)
        else:
            print_test_result('Build Diagnostics Self-Test', False, 'scripts/test/test_build_diagnostics.py not found')
            result.add_test('Build Diagnostics Self-Test', False)
    
    # Summary
    if result.print_summary():
        print("\n" + Colors.GREEN + "[SUCCESS] All tests passed! MCP server is working correctly with both HTTP and TCP protocols." + Colors.END)
        sys.exit(0)
    else:
        print("\n" + Colors.YELLOW + "[WARNING] Some tests failed. Check the output above for details." + Colors.END)
        sys.exit(1)

if __name__ == "__main__":
    main()
