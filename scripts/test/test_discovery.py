#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Qt MCP Plugin - Test Discovery Script
Quick way to discover available testing options and run common test scenarios.
"""

import os
import sys

def print_discovery_info():
    """Print discovery information for the test suite"""
    print("=" * 60)
    print("Qt MCP Plugin - Test Discovery")
    print("=" * 60)
    print()
    
    print("MAIN TEST SCRIPT:")
    print("  test_suite.py - Comprehensive test suite")
    print()
    
    print("QUICK COMMANDS:")
    print("  python test_suite.py --help           # Show all options")
    print("  python test_suite.py                  # Run all tests")
    print("  python test_suite.py --verbose        # Detailed output")
    print("  python test_suite.py --iterative      # CI/CD friendly")
    print()
    
    print("PROTOCOL-SPECIFIC TESTING:")
    print("  python test_suite.py --http-only      # HTTP protocol only")
    print("  python test_suite.py --tcp-only       # TCP protocol only")
    print()
    
    print("ITERATIVE TESTING (for CI/CD):")
    print("  python test_suite.py --iterative --max-attempts 10 --delay 3")
    print()
    
    print("TEST CATEGORIES:")
    print("  1. Server Connectivity - Basic connection to port 3001")
    print("  2. TCP MCP Protocol - JSON-RPC over TCP")
    print("  3. HTTP MCP Protocol - HTTP/1.1 with JSON-RPC")
    print("  4. Protocol Detection - Automatic HTTP vs TCP detection")
    print("  5. Plugin Version - Server version and identification")
    print()
    
    print("REQUIREMENTS:")
    print("  - Qt Creator running with MCP Plugin loaded")
    print("  - MCP server listening on port 3001")
    print("  - Python 2.7+ or Python 3.x")
    print()
    
    print("DOCUMENTATION:")
    print("  documentation/TESTING.md - Detailed testing documentation")
    print("  test_suite.py --help - Command-line options")
    print()
    
    print("PLATFORM SUPPORT:")
    print("  - Windows: Uses taskkill, telnet")
    print("  - macOS: Uses pkill, nc (netcat)")
    print("  - Linux: Uses pkill, nc (netcat)")
    print()
    
    # Check if test suite exists
    if os.path.exists("test_suite.py"):
        print("STATUS: [OK] test_suite.py found")
        
        # Try to get help
        try:
            import subprocess
            result = subprocess.run([sys.executable, "test_suite.py", "--help"], 
                                  capture_output=True, text=True, timeout=5)
            if result.returncode == 0:
                print("STATUS: [OK] test_suite.py is executable")
            else:
                print("STATUS: [WARNING] test_suite.py may have issues")
        except:
            print("STATUS: [WARNING] Could not verify test_suite.py execution")
    else:
        print("STATUS: [ERROR] test_suite.py not found")
    
    print()
    print("=" * 60)

if __name__ == "__main__":
    print_discovery_info()
