#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Qt MCP Plugin - Build Script Launcher (Python 2 Compatible)
Detects Python version and launches the main build script with Python 3.
"""

import os
import sys
import subprocess

def launch_build_main():
    """Launch build_main.py with Python 3 regardless of current Python version"""
    print("Launching main build script with Python 3...")
    
    # Try to find Python 3
    python3_commands = ['python3', 'py -3']
    
    for cmd in python3_commands:
        try:
            # Test if the command exists - use Python 2 compatible syntax
            cmd_parts = cmd.split()
            result = subprocess.Popen(cmd_parts + ['--version'], 
                                    stdout=subprocess.PIPE, 
                                    stderr=subprocess.PIPE)
            stdout, stderr = result.communicate()
            
            if result.returncode == 0 and '3.' in stdout:
                print("Found Python 3: " + cmd)
                
                # Launch the main build script with Python 3; run from repo root for consistent paths
                script_dir = os.path.dirname(os.path.abspath(__file__))
                repo_root = os.path.dirname(os.path.dirname(script_dir))
                build_main_py = os.path.join(script_dir, 'build_main.py')
                new_args = cmd_parts + [build_main_py] + sys.argv[1:]
                try:
                    result = subprocess.run(new_args, cwd=repo_root)
                    sys.exit(result.returncode)
                except Exception as e:
                    print("Failed to launch build_main.py: " + str(e))
                    continue
        except Exception as e:
            continue
    
    print("ERROR: Could not find Python 3. Please install Python 3.x")
    print("Or ensure 'python' points to Python 3.x, then try again.")
    sys.exit(1)

if __name__ == "__main__":
    launch_build_main()