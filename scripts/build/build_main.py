#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Qt MCP Plugin - Cross-Platform Build Script
Handles the complete lifecycle: quit Qt Creator, bump version, build, install, launch, test.

REQUIRED WORKFLOW (whenever you make any change to the plugin):
  1. Kill Qt Creator  - Script quits/kills Qt Creator first (plugin cannot be updated while running).
  2. Bump version     - Patch version is auto-incremented (version.cmake) so each build has a new version.
  3. Build            - CMake configure and compile the plugin.
  4. Install          - Clean old plugin files, then copy new plugin + JSON into Qt Creator.
  5. Launch Qt Creator - Start Qt Creator so it loads the new plugin.
  6. Test             - Request plugin version via MCP (initialize); verify it is the newly updated version.

Run this script for every change; do not run cmake/make directly. The script ensures the version
number is updated each time and that the running plugin is verified to be the new version.
"""

import os
import sys
import time
import subprocess
import platform
import json
import socket
import glob
import signal
import threading
import logging
import re

# Ensure scripts/build is on path so qt_config can be imported
_script_dir = os.path.dirname(os.path.abspath(__file__))
if _script_dir not in sys.path:
    sys.path.insert(0, _script_dir)

# Import Qt configuration
try:
    import qt_config
except ImportError:
    print("[ERROR] qt_config.py not found. Please ensure it exists in the project root.")
    sys.exit(1)

# Python 3 only imports - version check is handled by build.py
from pathlib import Path

# Plugin file paths - factored for maintainability
PLUGIN_BINARY_NAME = "libQt_MCP_Plugin.1.dylib"
PLUGIN_JSON_NAME = "Qt_MCP_Plugin.json"
PLUGIN_RELATIVE_PATH = "Qt Creator.app/Contents/PlugIns/qtcreator"

def print_section_break(length=80):
    """Print a section break with equals signs"""
    print("-" * length)

def print_error(message):
    """Print an error message"""
    print(f"[ERROR] {message}")

def get_plugin_paths():
    """Get the correct plugin paths based on repo root and platform."""
    plugin_binary, plugin_json, _ = qt_config.get_built_plugin_paths()
    return plugin_binary, plugin_json

# Platform configuration is now handled by qt_config.get_platform_config()


def setup_windows_environment():
    """Set up Visual Studio environment for Windows builds"""
    print("[BUILD] Setting up Visual Studio environment...")
    
    # Check for VS_PATH environment variable first
    vs_path = os.environ.get("VS_PATH")
    if vs_path and os.path.exists(vs_path):
        print(f"[OK] Found Visual Studio at: {vs_path}")
        return vs_path
    
    # Try to find Visual Studio installation
    vs_paths = [
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat"
    ]
    
    for vs_path in vs_paths:
        if os.path.exists(vs_path):
            print(f"[OK] Found Visual Studio at: {vs_path}")
            return vs_path
    
    print_error("Visual Studio not found. Please set VS_PATH environment variable or install Visual Studio 2019/2022")
    return None

# Global flag to track if we should continue running
_should_continue = True

# Set up logging - write to repo logs/ dir, pipe to console for real-time monitoring
_logs_dir = os.path.join(qt_config.get_repo_root(), "logs")
os.makedirs(_logs_dir, exist_ok=True)
logging.basicConfig(
    level=logging.WARNING,  # Only show warnings and errors on console
    format='%(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(os.path.join(_logs_dir, "build.log")),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)
# File handler gets DEBUG level for troubleshooting
for handler in logger.handlers:
    if isinstance(handler, logging.FileHandler):
        handler.setLevel(logging.DEBUG)

# Ensure all print statements are flushed immediately for real-time viewing
import functools
original_print = print
def flush_print(*args, **kwargs):
    kwargs.setdefault('flush', True)
    original_print(*args, **kwargs)
print = flush_print

def signal_handler(signum, frame):
    """Handle interruption signals gracefully"""
    global _should_continue
    logger.warning(f"Received signal {signum}, but continuing build process...")
    print(f"\n[WARNING]  Received signal {signum}, but continuing build process...")
    print("[WARNING]  Build will continue unless manually stopped")
    _should_continue = True  # Keep running despite signals

def run_command(cmd, shell=False, check=True, env=None, show_errors=True, timeout=None):
    """Run a command and return the result with explicit timeout handling"""
    global _should_continue
    cmd_str = ' '.join(cmd) if isinstance(cmd, list) else cmd
    logger.debug(f"Running command: {cmd_str}")
    logger.debug(f"Environment: {env}")
    print(f"[BUILD] Running: {cmd_str}")
    
    # Set up signal handlers to prevent interruption
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    try:
        # Use Popen for better control over long-running processes
        if timeout is None:
            # For build commands, use a very long timeout to prevent premature termination
            timeout = 3600  # 1 hour for build processes
        
        # Use Popen for real-time output streaming
        process = subprocess.Popen(cmd, shell=shell, stdout=subprocess.PIPE, 
                                 stderr=subprocess.STDOUT, text=True, env=env, bufsize=1)
        
        # Stream output in real-time
        start_time = time.time()
        output_lines = []
        
        while process.poll() is None:
            # Check timeout
            if time.time() - start_time > timeout:
                process.terminate()
                process.wait()
                if show_errors:
                    print_error(f" Command timed out after {timeout} seconds")
                return False
            
            # Check if we should continue (handle signals gracefully)
            if not _should_continue:
                print("[WARNING]  External signal received, but continuing build...")
                _should_continue = True
            
            # Read output line by line for real-time display
            try:
                line = process.stdout.readline()
                if line:
                    line = line.rstrip()
                    print(f"[BUILD] {line}")  # Show build output in real-time
                    output_lines.append(line)
            except:
                pass
            
            time.sleep(0.01)  # Small delay to prevent busy waiting
        
        # Read any remaining output
        remaining_output = process.stdout.read()
        if remaining_output:
            for line in remaining_output.splitlines():
                line = line.rstrip()
                print(f"[BUILD] {line}")
                output_lines.append(line)
        
        return_code = process.returncode
        
        if return_code != 0:
            if show_errors:
                print_error(f" Command failed with exit code {return_code}")
                print_error(f" Full output: {chr(10).join(output_lines)}")
            return False
        
        # Special check for xcopy command - it can return 0 but copy 0 files
        if cmd and 'xcopy' in str(cmd).lower():
            for line in output_lines:
                if "0 File(s) copied" in line:
                    if show_errors:
                        print_error(f" xcopy failed - no files copied")
                        print_error(f" Full output: {chr(10).join(output_lines)}")
                    return False
        
        return True
        
    except subprocess.TimeoutExpired as e:
        if show_errors:
            print_error(f" Command timed out after {timeout} seconds: {e}")
        return False
    except subprocess.CalledProcessError as e:
        if show_errors:
            print_error(f" Command failed: {e}")
        return False
    except Exception as e:
        if show_errors:
            print_error(f" Command failed with exception: {e}")
        return False

def is_process_running(process_name):
    """Check if a process is running"""
    try:
        if platform.system().lower() == "windows":
            result = subprocess.run(["tasklist", "/FI", f"IMAGENAME eq {process_name}"], 
                                  capture_output=True, text=True, check=False)
            return process_name in result.stdout
        else:
            result = subprocess.run(["pgrep", "-f", process_name], 
                                  capture_output=True, text=True, check=False)
            return result.returncode == 0
    except:
        return False

def monitor_build_process(process_name, max_monitor_time=3600):
    """Monitor build process for external interference"""
    print(f"[SEARCH] Monitoring build process for external interference...")
    start_time = time.time()
    
    while time.time() - start_time < max_monitor_time:
        if not is_process_running(process_name):
            print(f"[WARNING]  Build process {process_name} stopped unexpectedly!")
            return False
        
        # Check for system resource issues
        try:
            if platform.system().lower() == "windows":
                # Check for low memory or high CPU usage
                result = subprocess.run(["wmic", "OS", "get", "TotalVisibleMemorySize,FreePhysicalMemory", "/value"], 
                                      capture_output=True, text=True, timeout=5)
                if result.returncode == 0:
                    lines = result.stdout.strip().split('\n')
                    for line in lines:
                        if 'FreePhysicalMemory' in line:
                            free_mem = int(line.split('=')[1].strip())
                            if free_mem < 1000000:  # Less than 1GB free
                                print(f"[WARNING]  Low memory detected: {free_mem} bytes free")
        except:
            pass
        
        time.sleep(5)  # Check every 5 seconds
    
    return True

def test_mcp_connection(config):
    """Test if MCP server is responding"""
    try:
        # Use socket connection only (most reliable)
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)  # 5 second timeout for plugin loading
        result = sock.connect_ex(('localhost', 3001))
        sock.close()
        
        return result == 0
    except:
        return False

def test_mcp_version(config):
    """Test MCP server and get plugin version using initialize response"""
    try:
        # Use initialize command to get version info
        init_query = '{"jsonrpc":"2.0","method":"initialize","id":1}\n'
        response_data = send_mcp_command_socket(init_query, "initialize")
        
        if response_data:
            try:
                response = json.loads(response_data)
                if "result" in response and response["result"]:
                    server_info = response["result"].get("serverInfo", {})
                    version = server_info.get("version", "unknown")
                    name = server_info.get("name", "unknown")
                    print(f"[OK] Plugin version: {version} ({name})")
                    
                    # Verify this is a recent version (patch >= 1)
                    try:
                        version_parts = version.split('.')
                        if len(version_parts) >= 3:
                            patch_version = int(version_parts[2])
                            if patch_version >= 1:
                                print(f"[OK] Version verified: patch {patch_version} indicates recent build")
                                return True
                            else:
                                print_error(f" Version {version} appears to be old (patch {patch_version} < 1)")
                                return False
                        else:
                            print_error(f" Invalid version format: {version}")
                            return False
                    except (ValueError, IndexError):
                        print_error(f" Could not parse version: {version}")
                        return False
                else:
                    print_error(" MCP initialize returned no result")
                    return False
            except json.JSONDecodeError:
                print_error(" Invalid JSON response from MCP server")
                return False
        else:
            print_error(" MCP server not responding to initialize query")
            return False
            
    except Exception as e:
        print_error(f" MCP version test failed: {e}")
        return False

def verify_plugin_installation(config):
    """Verify that the plugin installation actually updated the files"""
    system = platform.system().lower()
    
    if system == "windows":
        # Get timestamps of built vs installed plugin
        built_plugin = qt_config.get_built_plugin_paths()[0]
        installed_plugin = os.path.join(config["qt_creator_path"], "qtcreator", "plugins", "Qt_MCP_Plugin.dll")
        
        if not os.path.exists(built_plugin):
            print_error(f" Built plugin not found: {built_plugin}")
            return False
            
        if not os.path.exists(installed_plugin):
            print_error(f" Installed plugin not found: {installed_plugin}")
            return False
        
        # Compare timestamps
        built_time = os.path.getmtime(built_plugin)
        installed_time = os.path.getmtime(installed_plugin)
        
        print(f"[DEBUG] Built plugin timestamp: {time.ctime(built_time)}")
        print(f"[DEBUG] Installed plugin timestamp: {time.ctime(installed_time)}")
        
        # Installed plugin should be newer than or equal to built plugin
        if installed_time < built_time - 5:  # Allow 5 second tolerance
            print_error(f" Installed plugin is older than built plugin!")
            print_error(f" This indicates xcopy failed to update the file")
            return False
        
        # Compare file sizes
        built_size = os.path.getsize(built_plugin)
        installed_size = os.path.getsize(installed_plugin)
        
        print(f"[DEBUG] Built plugin size: {built_size} bytes")
        print(f"[DEBUG] Installed plugin size: {installed_size} bytes")
        
        if built_size != installed_size:
            print_error(f" Plugin file sizes don't match!")
            print_error(f" This indicates xcopy failed to update the file")
            return False
        
        print("[OK] Plugin installation verified - files match")
        return True
    else:
        # For macOS/Linux, similar verification
        return True

def clean_old_plugins(config):
    """Clean up old plugin versions to ensure fresh installation - MUST succeed before continuing"""
    system = platform.system().lower()
    
    if system == "windows":
        plugin_dir = os.path.join(config["qt_creator_path"], "qtcreator", "plugins")
        plugin_files = [
            "Qt_MCP_Plugin.dll",
            "Qt_MCP_Plugin.json"
        ]
        
        print(f"[DEBUG] Cleaning plugin directory: {plugin_dir}")
        
        # Delete each plugin file and verify deletion
        for plugin_file in plugin_files:
            plugin_path = os.path.join(plugin_dir, plugin_file)
            if os.path.exists(plugin_path):
                try:
                    print(f"[DEBUG] Deleting: {plugin_path}")
                    os.remove(plugin_path)
                    
                    # Verify deletion was successful
                    if os.path.exists(plugin_path):
                        print_error(f" Failed to delete {plugin_file} - file still exists!")
                        return False
                    else:
                        print(f"[OK] Successfully removed old plugin: {plugin_file}")
                except Exception as e:
                    print_error(f" Failed to remove {plugin_file}: {e}")
                    return False
            else:
                print(f"[DEBUG] Plugin file not found (already clean): {plugin_file}")
        
        # Verify all plugin files are gone
        print("[DEBUG] Verifying all plugin files are deleted...")
        for plugin_file in plugin_files:
            plugin_path = os.path.join(plugin_dir, plugin_file)
            if os.path.exists(plugin_path):
                print_error(f" Plugin file still exists after deletion: {plugin_file}")
                return False
        
        print("[OK] All old plugin files successfully deleted")
        return True
    else:
        # For macOS/Linux, clean from app bundle
        home_dir = os.path.expanduser("~")
        if system == "darwin":
            app_bundle_dir = f"{home_dir}/Developer/Qt/Qt Creator.app/Contents/PlugIns/qtcreator"
        else:
            app_bundle_dir = "/usr/lib/qtcreator/plugins"
        
        plugin_files = [
            "libQt_MCP_Plugin*.dylib" if system == "darwin" else "libQt_MCP_Plugin*.so",
            "Qt_MCP_Plugin.json"
        ]
        
        for plugin_pattern in plugin_files:
            try:
                import glob
                for plugin_path in glob.glob(os.path.join(app_bundle_dir, plugin_pattern)):
                    os.remove(plugin_path)
                    print(f"[OK] Removed old plugin: {os.path.basename(plugin_path)}")
            except Exception as e:
                print_error(f" Failed to remove plugins matching {plugin_pattern}: {e}")
                return False
        
        return True

def bump_version():
    """Automatically bump the patch version in version.cmake"""
    version_file = "version.cmake"
    
    try:
        # Read current version
        with open(version_file, 'r') as f:
            content = f.read()
        
        # Extract current patch version
        import re
        patch_match = re.search(r'PLUGIN_VERSION_PATCH (\d+)', content)
        if patch_match:
            current_patch = int(patch_match.group(1))
            new_patch = current_patch + 1
            
            # Replace the patch version
            new_content = re.sub(
                r'PLUGIN_VERSION_PATCH \d+',
                f'PLUGIN_VERSION_PATCH {new_patch}',
                content
            )
            
            # Write back the updated version
            with open(version_file, 'w') as f:
                f.write(new_content)
            
            print(f"[OK] Version bumped from patch {current_patch} to {new_patch}")
        else:
            print_error(" Could not find PLUGIN_VERSION_PATCH in version.cmake")
            
    except Exception as e:
        print_error(f" Failed to bump version: {e}")

def get_required_qt_version(config):
    """
    Get the Qt version that Qt Creator was built with.
    Uses qtdiag.exe to get the actual Qt version, not the minimum from CMake files.
    
    Returns:
        str: Required Qt version (e.g., "6.10.1") or None if not found
    """
    # Use the existing discover_qt_version function which calls qtdiag.exe
    # This returns the ACTUAL Qt version Qt Creator was built with
    return qt_config.discover_qt_version(config["qt_creator_bin"])


def get_installed_qt_versions(config):
    """
    Get list of Qt versions installed on the system
    
    Returns:
        list: List of installed Qt version strings (e.g., ["6.5.3", "6.11.0"])
    """
    system = platform.system().lower()
    qt_base = config.get("qt_path", "")
    
    if not qt_base or not os.path.exists(qt_base):
        return []
    
    versions = []
    try:
        for entry in os.listdir(qt_base):
            # Check if directory name looks like a version number (e.g., 6.5.3, 6.11.0)
            if re.match(r'^\d+\.\d+', entry) and os.path.isdir(os.path.join(qt_base, entry)):
                versions.append(entry)
    except Exception:
        pass
    
    return sorted(versions)


def check_qt_version_compatibility(config):
    """
    Check if the required Qt version for Qt Creator is installed.
    This is the FIRST check that should run before any build steps.
    
    Returns:
        tuple: (is_compatible, required_version, installed_versions)
    """
    required_version = get_required_qt_version(config)
    installed_versions = get_installed_qt_versions(config)
    
    if required_version is None:
        # Could not determine required version - proceed with caution
        return True, None, installed_versions
    
    # Check if required version is installed
    # We need exact major.minor.patch match
    if required_version in installed_versions:
        return True, required_version, installed_versions
    
    # Check for compatible version (same major.minor)
    required_parts = required_version.split('.')
    if len(required_parts) >= 2:
        required_major_minor = f"{required_parts[0]}.{required_parts[1]}"
        for installed in installed_versions:
            if installed.startswith(required_major_minor):
                return True, required_version, installed_versions
    
    return False, required_version, installed_versions


def detect_qt_library_version(config):
    """Detect the Qt library version that Qt Creator is linked against"""
    system = platform.system().lower()
    
    if system == "darwin":
        qt_creator_bin = config.get("qt_creator_bin", "")
        if os.path.exists(qt_creator_bin):
            try:
                result = subprocess.run(["otool", "-L", qt_creator_bin], capture_output=True, text=True)
                if result.returncode == 0:
                    import re
                    # Look for QtCore framework version
                    match = re.search(r'QtCore\.framework/Versions/A/QtCore \(.*current version (\d+\.\d+\.\d+)\)', result.stdout)
                    if not match:
                        match = re.search(r'QtCore\.framework/Versions/A/QtCore \(.*current version (\d+\.\d+)\)', result.stdout)
                    if match:
                        version = match.group(1)
                        print(f"[OK] Detected Qt library version: {version}")
                        return version
            except Exception as e:
                print(f"[WARNING] Could not detect Qt library version: {e}")
    
    elif system == "windows":
        # On Windows, check Qt6Core.dll
        qt_creator_dir = os.path.dirname(config.get("qt_creator_bin", ""))
        qt6core = os.path.join(qt_creator_dir, "Qt6Core.dll")
        if os.path.exists(qt6core):
            try:
                # Use file command or check version resource
                result = subprocess.run(["powershell", "-Command", 
                    f"(Get-Item '{qt6core}').VersionInfo.FileVersion"], 
                    capture_output=True, text=True)
                if result.returncode == 0 and result.stdout.strip():
                    version = result.stdout.strip()
                    print(f"[OK] Detected Qt library version: {version}")
                    return version
            except Exception as e:
                print(f"[WARNING] Could not detect Qt library version: {e}")
    
    print("[WARNING] Could not detect Qt library version")
    return None





def find_matching_qt_sdk(config, qt_version):
    """Find a Qt SDK installation matching the required version"""
    system = platform.system().lower()
    qt_path = config.get("qt_path", "")
    
    if not qt_version or not qt_path:
        return None
    
    # Try exact version first
    if system == "darwin":
        sdk_path = os.path.join(qt_path, qt_version, "macos")
    elif system == "windows":
        sdk_path = os.path.join(qt_path, qt_version, "msvc2022_64")
    else:
        sdk_path = os.path.join(qt_path, qt_version, "gcc_64")
    
    if os.path.exists(sdk_path):
        print(f"[OK] Found matching Qt SDK at: {sdk_path}")
        return sdk_path
    
    # Try finding any installed version that matches major.minor
    version_parts = qt_version.split('.')
    if len(version_parts) >= 2:
        major_minor = f"{version_parts[0]}.{version_parts[1]}"
        try:
            for entry in os.listdir(qt_path):
                if entry.startswith(major_minor) and os.path.isdir(os.path.join(qt_path, entry)):
                    if system == "darwin":
                        candidate = os.path.join(qt_path, entry, "macos")
                    elif system == "windows":
                        candidate = os.path.join(qt_path, entry, "msvc2022_64")
                    else:
                        candidate = os.path.join(qt_path, entry, "gcc_64")
                    if os.path.exists(candidate):
                        print(f"[OK] Found compatible Qt SDK at: {candidate}")
                        return candidate
        except Exception as e:
            print(f"[WARNING] Error searching for Qt SDK: {e}")
    
    print(f"[WARNING] No matching Qt SDK found for version {qt_version}")
    return None


def detect_qt_creator_version(config):
    """Detect Qt Creator version from the installation"""
    system = platform.system().lower()
    
    if system == "darwin":
        # On macOS, check Info.plist in the app bundle
        info_plist = os.path.join(config.get("qt_creator_app", ""), "Contents", "Info.plist")
        if os.path.exists(info_plist):
            try:
                import plistlib
                with open(info_plist, 'rb') as f:
                    plist = plistlib.load(f)
                    version = plist.get("CFBundleShortVersionString", "")
                    if version:
                        print(f"[OK] Detected Qt Creator version: {version}")
                        return version
            except Exception as e:
                print(f"[WARNING] Could not read Info.plist: {e}")
        
        # Fallback: check plugin dylib compatibility version using otool
        try:
            plugin_dir = os.path.join(config.get("qt_creator_app", ""), "Contents", "PlugIns", "qtcreator")
            # Find any Qt Creator plugin to check compatibility version
            for f in os.listdir(plugin_dir):
                if f.endswith(".dylib") and f.startswith("lib") and "Qt_MCP" not in f:
                    dylib_path = os.path.join(plugin_dir, f)
                    result = subprocess.run(["otool", "-L", dylib_path], capture_output=True, text=True)
                    if result.returncode == 0:
                        # Look for compatibility version in output
                        import re
                        match = re.search(r'compatibility version (\d+\.\d+\.\d+)', result.stdout)
                        if match:
                            version = match.group(1)
                            print(f"[OK] Detected Qt Creator version from plugin: {version}")
                            return version
                    break
        except Exception as e:
            print(f"[WARNING] Could not detect version from plugins: {e}")
    
    elif system == "windows":
        # On Windows, check version from executable
        qt_creator_exe = config.get("qt_creator_bin", "")
        if os.path.exists(qt_creator_exe):
            try:
                result = subprocess.run([qt_creator_exe, "--version"], capture_output=True, text=True, timeout=10)
                if result.returncode == 0:
                    import re
                    match = re.search(r'Qt Creator (\d+\.\d+\.\d+)', result.stdout)
                    if match:
                        version = match.group(1)
                        print(f"[OK] Detected Qt Creator version: {version}")
                        return version
            except Exception as e:
                print(f"[WARNING] Could not get version from executable: {e}")
    
    print("[WARNING] Could not detect Qt Creator version, using default")
    return None


def update_json_dependencies(qt_creator_version):
    """Update the JSON template with correct Qt Creator dependency versions"""
    json_template = os.path.join(qt_config.get_repo_root(), "src", "Qt_MCP_Plugin.json.in")
    
    if not qt_creator_version:
        print("[WARNING] No Qt Creator version provided, skipping JSON update")
        return False
    
    try:
        with open(json_template, 'r') as f:
            content = f.read()
        
        import re
        # Find and replace core version
        new_content = re.sub(
            r'("Id"\s*:\s*"core"\s*,\s*"Version"\s*:\s*")[\d.]+(")',
            f'\\g<1>{qt_creator_version}\\2',
            content
        )
        # Find and replace projectexplorer version
        new_content = re.sub(
            r'("Id"\s*:\s*"projectexplorer"\s*,\s*"Version"\s*:\s*")[\d.]+(")',
            f'\\g<1>{qt_creator_version}\\2',
            new_content
        )
        
        if new_content != content:
            with open(json_template, 'w') as f:
                f.write(new_content)
            print(f"[OK] Updated JSON dependencies to version {qt_creator_version}")
            return True
        else:
            print(f"[OK] JSON dependencies already set to version {qt_creator_version}")
            return True
            
    except Exception as e:
        print_error(f" Failed to update JSON dependencies: {e}")
        return False


def regenerate_version_files():
    """Regenerate version.h from version.cmake after version bump"""
    try:
        # Read version.cmake to get current version
        with open("version.cmake", 'r') as f:
            content = f.read()
        
        import re
        # Extract version components
        major_match = re.search(r'PLUGIN_VERSION_MAJOR (\d+)', content)
        minor_match = re.search(r'PLUGIN_VERSION_MINOR (\d+)', content)
        patch_match = re.search(r'PLUGIN_VERSION_PATCH (\d+)', content)
        name_match = re.search(r'PLUGIN_NAME_VERSIONED "([^"]+)"', content)
        json_file_match = re.search(r'PLUGIN_JSON_FILE "([^"]+)"', content)
        
        if not all([major_match, minor_match, patch_match, name_match, json_file_match]):
            print_error(" Could not extract all version components from version.cmake")
            return False
        
        major = major_match.group(1)
        minor = minor_match.group(1)
        patch = patch_match.group(1)
        version = f"{major}.{minor}.{patch}"  # Construct version from components
        name = name_match.group(1)
        json_file = json_file_match.group(1)
        
        # Generate version.h content (with generated-file notice)
        version_h_content = f"""// GENERATED FILE - Do not edit. Generated by build script from version.cmake.

#pragma once

// Plugin versioning - centralized location
#define PLUGIN_VERSION_MAJOR {major}
#define PLUGIN_VERSION_MINOR {minor}
#define PLUGIN_VERSION_PATCH {patch}

#define PLUGIN_VERSION_STRING "{version}"
#define PLUGIN_NAME_VERSIONED "{name}"
#define PLUGIN_JSON_FILE "{json_file}"
"""
        
        # Write to build_<platform>/version.h
        build_dir = qt_config.get_build_dir()
        os.makedirs(build_dir, exist_ok=True)
        version_h_path = os.path.join(build_dir, "version.h")
        with open(version_h_path, 'w') as f:
            f.write(version_h_content)
        
        print(f"[OK] Regenerated version.h with version {version}")
        return True
        
    except Exception as e:
        print_error(f" Failed to regenerate version files: {e}")
        return False

def ensure_json_files_generated(build_dir, also_in_source=False):
    """
    Ensure JSON files are generated in the build directory with correct version.
    This is called after build to ensure files exist before installation.
    
    Args:
        build_dir: Build directory path
        also_in_source: If True, also generate in source directory for MOC compilation
    """
    try:
        # Read version from version.cmake
        with open("version.cmake", 'r') as f:
            content = f.read()
        
        import re
        major_match = re.search(r'PLUGIN_VERSION_MAJOR (\d+)', content)
        minor_match = re.search(r'PLUGIN_VERSION_MINOR (\d+)', content)
        patch_match = re.search(r'PLUGIN_VERSION_PATCH (\d+)', content)
        
        if not all([major_match, minor_match, patch_match]):
            print_error(" Could not extract version from version.cmake")
            return False
        
        version = f"{major_match.group(1)}.{minor_match.group(1)}.{patch_match.group(1)}"
        
        # Read JSON template
        repo_root = qt_config.get_repo_root()
        json_template = os.path.join(repo_root, "src", "Qt_MCP_Plugin.json.in")
        discovery_template = os.path.join(repo_root, "src", "Qt_MCP_Plugin_discovery.json.in")
        
        if not os.path.exists(json_template):
            print_error(f" JSON template not found: {json_template}")
            return False
        
        # Generate plugin JSON file in build directory
        with open(json_template, 'r') as f:
            json_content = f.read().replace("@PLUGIN_VERSION@", version)
        
        json_output = os.path.join(build_dir, "Qt_MCP_Plugin.json")
        os.makedirs(build_dir, exist_ok=True)
        with open(json_output, 'w') as f:
            f.write(json_content)
        print(f"[OK] Generated {json_output} with version {version}")
        
        # Also generate in source directory if requested (for MOC compilation)
        if also_in_source:
            source_json = "Qt_MCP_Plugin.json"
            with open(source_json, 'w') as f:
                f.write(json_content)
            print(f"[OK] Generated {source_json} in source directory for MOC")
        
        # Generate discovery JSON file if template exists
        if os.path.exists(discovery_template):
            with open(discovery_template, 'r') as f:
                discovery_content = f.read().replace("@PLUGIN_VERSION@", version)
            
            discovery_output = os.path.join(build_dir, "Qt_MCP_Plugin_discovery.json")
            with open(discovery_output, 'w') as f:
                f.write(discovery_content)
            print(f"[OK] Generated {discovery_output} with version {version}")
        
        return True
        
    except Exception as e:
        print_error(f" Failed to generate JSON files: {e}")
        return False


def get_mcp_timeout_for_function(function_name):
    """Get the appropriate timeout for a specific MCP function"""
    # Parse timeout from mcp.json schema
    try:
        with open('mcp.json', 'r') as f:
            mcp_data = json.load(f)
        
        tools = mcp_data.get('mcp', {}).get('tools', [])
        for tool in tools:
            if tool.get('name') == function_name:
                timeout_str = tool.get('timeout', 'default')
                if timeout_str == 'default':
                    return 5  # 5 seconds default
                elif timeout_str == '1 minute':
                    return 60
                elif timeout_str == '2 minutes':
                    return 120
                elif timeout_str == '20 minutes':
                    return 1200
                else:
                    return 5  # fallback
    except:
        pass
    
    # Fallback timeouts for common functions
    timeout_map = {
        'build': 1200,  # 20 minutes
        'debug': 60,    # 1 minute
        'loadSession': 120,  # 2 minutes
        'get_plugin_version': 1,  # Quick version check - 1 second
        'initialize': 1,  # Quick init - 1 second
        'quit': 1,  # Quick quit - 1 second
    }
    
    return timeout_map.get(function_name, 5)  # 5 seconds default

def send_mcp_command_socket(command, function_name="unknown"):
    """Send an MCP command via socket communication with function-specific timeout"""
    try:
        # Get appropriate timeout for this function
        timeout_seconds = get_mcp_timeout_for_function(function_name)
        
        # Create socket connection
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout_seconds)
        
        # Connect to MCP server
        sock.connect(('localhost', 3001))
        
        # Send command
        sock.send(command.encode())
        
        # Receive response
        response_data = sock.recv(4096).decode()
        sock.close()
        
        return response_data.strip()
        
    except socket.timeout:
        print_error(f" MCP server timeout for {function_name}")
        return ""
    except Exception as e:
        print_error(f" MCP command failed for {function_name}: {e}")
        return ""

def poll_build_status(max_poll_time=1200):
    """Poll build status until completion or timeout"""
    print("[SEARCH] Polling build status...")
    start_time = time.time()
    
    while time.time() - start_time < max_poll_time:
        try:
            # Get build status
            status_command = '{"jsonrpc":"2.0","method":"tools/call","params":{"name":"getBuildStatus"},"id":1}\n'
            response = send_mcp_command_socket(status_command, "getBuildStatus")
            
            if response:
                try:
                    status_data = json.loads(response)
                    if "result" in status_data and status_data["result"]:
                        status_text = status_data["result"]
                        
                        # Parse status for progress percentage
                        if "Building:" in status_text and "%" in status_text:
                            # Extract percentage
                            import re
                            match = re.search(r'Building: (\d+)%', status_text)
                            if match:
                                progress = int(match.group(1))
                                print(f"[PROGRESS] Build progress: {progress}%")
                                
                                if progress == 100:
                                    print("[OK] Build completed!")
                                    return True
                        elif "Status: Not building" in status_text:
                            print("[OK] Build completed!")
                            return True
                        else:
                            print(f"[STATUS] Build status: {status_text}")
                except json.JSONDecodeError:
                    print_error(" Invalid JSON response from build status")
            
            # Wait before next poll
            time.sleep(2)  # Poll every 2 seconds
            
        except Exception as e:
            print_error(f" Error polling build status: {e}")
            time.sleep(2)
    
    print_error(" Build polling timeout reached")
    return False

def send_mcp_command(config, command):
    """Send a command to the MCP server using unified socket communication"""
    try:
        # Parse the function name from the command
        function_name = "unknown"
        try:
            cmd_data = json.loads(command)
            if 'params' in cmd_data and 'name' in cmd_data['params']:
                function_name = cmd_data['params']['name']
        except:
            pass
        
        # Use unified socket communication
        return send_mcp_command_socket(command, function_name)
    except:
        return ""

def quit_qt_creator_gracefully(config):
    """Attempt to quit Qt Creator via MCP using unified socket communication"""
    print("[QUIT] Attempting to quit Qt Creator via MCP...")

    if test_mcp_connection(config):
        print("[MCP] MCP server responding, sending quit command...")
        quit_cmd = '{"jsonrpc":"2.0","method":"tools/call","params":{"name":"quit","arguments":{}},"id":1}\n'

        # Send quit command using unified socket communication
        response = send_mcp_command_socket(quit_cmd, "quit")
        if response:
            print("[OK] MCP quit command sent and acknowledged")
        else:
            print("[WARNING]  MCP quit command sent but no response received")

        # Wait for Qt Creator to quit
        for i in range(10):
            if not is_process_running(config["process_name"]):
                print("[OK] Qt Creator quit gracefully")
                return True
            print(f"Waiting for Qt Creator to quit... ({i+1}/10)")
            time.sleep(1)

        print("[WARNING]  Qt Creator still running after MCP quit")
        return False
    else:
        print_error(" MCP server not responding")
        return False

def kill_qt_creator(config, timeout_seconds=30):
    """Force kill Qt Creator with timeout - MUST succeed before continuing"""
    print("Force killing Qt Creator...")
    
    start_time = time.time()
    attempt = 0
    
    while time.time() - start_time < timeout_seconds:
        attempt += 1
        print(f"Kill attempt {attempt}...")
        
        # Method 1: Standard taskkill
        result = run_command(config["kill_command"], check=False, show_errors=False)
        if result and "SUCCESS" in str(result):
            print("[OK] Qt Creator killed successfully")
            return True
        time.sleep(1)
        
        # Check if Qt Creator is already killed
        if not is_process_running(config["process_name"]):
            print("[OK] Qt Creator killed successfully")
            return True
        
        # Method 2: More aggressive taskkill with /T (kill tree) - only if still running
        if platform.system().lower() == "windows":
            result = run_command(["taskkill", "/F", "/T", "/IM", "qtcreator.exe"], check=False, show_errors=False)
            if result and "SUCCESS" in str(result):
                print("[OK] Qt Creator killed successfully")
                return True
            time.sleep(1)
            
            # Method 3: Kill by process name pattern - only if still running
            if is_process_running(config["process_name"]):
                result = run_command(["taskkill", "/F", "/IM", "*qtcreator*"], check=False, show_errors=False)
                if result and "SUCCESS" in str(result):
                    print("[OK] Qt Creator killed successfully")
                    return True
                time.sleep(1)
            
            # Method 4: Kill by window title - only if still running
            if is_process_running(config["process_name"]):
                result = run_command(["taskkill", "/F", "/FI", "WINDOWTITLE eq Qt Creator*"], check=False, show_errors=False)
                if result and "SUCCESS" in str(result):
                    print("[OK] Qt Creator killed successfully")
                    return True
                time.sleep(1)
        else:
            # Unix-like systems
            result = run_command(["pkill", "-9", "-f", "qtcreator"], check=False, show_errors=False)
            if result and result.returncode == 0:
                print("[OK] Qt Creator killed successfully")
                return True
            time.sleep(1)
            
            if is_process_running(config["process_name"]):
                result = run_command(["killall", "-9", "qtcreator"], check=False, show_errors=False)
                if result and result.returncode == 0:
                    print("[OK] Qt Creator killed successfully")
                    return True
                time.sleep(1)
        
        # Check timeout
        elapsed = time.time() - start_time
        if elapsed >= timeout_seconds:
            print_error(f" Failed to kill Qt Creator within {timeout_seconds} seconds")
            print_error(f" Qt Creator is still running - cannot proceed with build")
            return False
        
        print(f"Waiting for Qt Creator to terminate... ({elapsed:.1f}s elapsed)")
        time.sleep(2)

    print_error(f" Failed to kill Qt Creator after {timeout_seconds} seconds")
    return False

def launch_qt_creator(config):
    """Launch Qt Creator as completely independent process"""
    try:
        if platform.system().lower() == "darwin":
            # macOS - use open command for app bundle
            subprocess.Popen(["open", config["qt_creator_app"]], 
                           stdout=subprocess.DEVNULL, 
                           stderr=subprocess.DEVNULL)
        else:
            # Windows - use DETACHED_PROCESS and CREATE_NEW_PROCESS_GROUP
            subprocess.Popen([config["qt_creator_bin"]], 
                           stdout=subprocess.DEVNULL, 
                           stderr=subprocess.DEVNULL,
                           creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP)
        return True
    except Exception as e:
        print_error(f" Failed to launch Qt Creator: {e}")
        return False

def test_installation(config):
    """Test the plugin installation"""
    print("[SEARCH] Testing MCP server...")
    print("[BUILD] Verifying plugin functionality...")

    # Try for 1 second total (2 attempts with 0.5 second intervals)
    for i in range(2):
        if test_mcp_connection(config):
            print("[OK] MCP server responding!")
            # Now test the version to ensure plugin is working
            if test_mcp_version(config):
                print("[OK] Plugin version verified!")
                print("[OK] MCP testing succeeded!")
                return True
            else:
                print_error(" Plugin version test failed")
                print_error(" MCP testing failed!")
                return False
        if i < 1:  # Don't sleep on the last attempt
            time.sleep(0.5)

    print_error(" MCP server never responded after 1 second")
    print_error(" MCP testing failed!")
    return False

def install_plugin(config):
    """Install the plugin to the app bundle only"""
    system = platform.system().lower()
    success = True
    
    if system == "darwin":  # macOS
        # Clean up previous versions from both locations (for safety)
        print("[CLEANUP] Cleaning up previous plugin versions...")
        home_dir = os.path.expanduser("~")
        cleanup_paths = [
            f"{home_dir}/Library/Application Support/QtProject/qtcreator/plugins/libQt_MCP_Plugin*.dylib",
            f"{home_dir}/Library/Application Support/QtProject/qtcreator/plugins/{PLUGIN_JSON_NAME}",
            f"{home_dir}/Library/Application Support/QtProject/qtcreator/plugins/Qt_MCP_Plugin_discovery.json",
            f"{home_dir}/Developer/Qt/Qt Creator.app/Contents/PlugIns/qtcreator/libQt_MCP_Plugin*.dylib",
            f"{home_dir}/Developer/Qt/Qt Creator.app/Contents/PlugIns/qtcreator/{PLUGIN_JSON_NAME}",
            f"{home_dir}/Developer/Qt/Qt Creator.app/Contents/PlugIns/qtcreator/Qt_MCP_Plugin_discovery.json"
        ]
        
        for path in cleanup_paths:
            try:
                run_command(["rm", "-f"] + glob.glob(path))
            except:
                pass
        
        # Install ONLY to app bundle for auto-loading
        home_dir = os.path.expanduser("~")
        app_bundle_dir = f"{home_dir}/Developer/Qt/Qt Creator.app/Contents/PlugIns/qtcreator"
        os.makedirs(app_bundle_dir, exist_ok=True)
        
        # Get the correct paths based on current working directory
        plugin_binary, plugin_json = get_plugin_paths()
        plugin_discovery = plugin_json.replace(".json", "_discovery.json")
        
        if os.path.exists(plugin_binary):
            run_command(["cp", plugin_binary, app_bundle_dir + "/"])
            print(f"[OK] Installed to app bundle: {app_bundle_dir}")
        else:
            print_error(f" Plugin binary not found: {plugin_binary}")
            success = False
        
        if os.path.exists(plugin_json):
            run_command(["cp", plugin_json, app_bundle_dir + "/"])
            print(f"[OK] Installed JSON file to app bundle")
        else:
            print_error(f" Plugin JSON not found: {plugin_json}")
            success = False
        
        if os.path.exists(plugin_discovery):
            run_command(["cp", plugin_discovery, app_bundle_dir + "/"])
            print(f"[OK] Installed discovery file to app bundle")
        else:
            print_error(f" Plugin discovery file not found: {plugin_discovery}")
            success = False
        
        if success:
            print("Plugin installation complete!")
        else:
            print_error(" Plugin installation failed!")
        
        return success
        
    elif system == "windows":
        # Install ONLY to app bundle for auto-loading
        home_dir = os.path.expanduser("~")
        app_bundle_dir = f"{home_dir}/Developer/Qt/Tools/QtCreator/lib/qtcreator/plugins"
        os.makedirs(app_bundle_dir, exist_ok=True)
        
        # Use absolute paths for xcopy
        plugin_binary, plugin_json, plugin_discovery = qt_config.get_built_plugin_paths()
        
        # Use windeployqt to automatically stage Qt dependencies
        try:
            windeployqt_exe = qt_config.get_windeployqt_path()
        except RuntimeError as e:
            print_error("Failed to get windeployqt path: {}".format(str(e)))
            sys.exit(1)
        
        if os.path.exists(plugin_binary):
            # Use xcopy for better Windows compatibility
            copy_cmd = f'xcopy "{plugin_binary}" "{app_bundle_dir}" /Y'
            if run_command(copy_cmd, shell=True):
                print(f"[OK] Installed to app bundle: {app_bundle_dir}")
            else:
                print_error(" Failed to install plugin binary")
                success = False
        else:
            print_error(f" Plugin binary not found: {plugin_binary}")
            success = False
        
        if os.path.exists(plugin_json):
            # Use xcopy for better Windows compatibility
            copy_cmd = f'xcopy "{plugin_json}" "{app_bundle_dir}" /Y'
            if run_command(copy_cmd, shell=True):
                print(f"[OK] Installed JSON file to app bundle")
            else:
                print_error(" Failed to install plugin JSON")
                success = False
        else:
            print_error(f" Plugin JSON not found: {plugin_json}")
            success = False
        
        if os.path.exists(plugin_discovery):
            # Use xcopy for better Windows compatibility
            copy_cmd = f'xcopy "{plugin_discovery}" "{app_bundle_dir}" /Y'
            if run_command(copy_cmd, shell=True):
                print(f"[OK] Installed discovery file to app bundle")
            else:
                print_error(" Failed to install plugin discovery file")
                success = False
        else:
            print_error(f" Plugin discovery file not found: {plugin_discovery}")
            success = False
        
        # HttpServer DLL should be available in Qt Creator's bin directory
        # No need to copy - plugin should link against Qt Creator's existing Qt libraries
        print("[OK] Plugin will use Qt Creator's existing HttpServer DLL")
        
        if success:
            print("Plugin installation complete!")
        else:
            print_error(" Plugin installation failed!")
        
        return success
        
    else:  # Linux
        # Install ONLY to app bundle for auto-loading
        app_bundle_dir = "/usr/lib/qtcreator/plugins"
        os.makedirs(app_bundle_dir, exist_ok=True)
        
        plugin_binary, plugin_json, plugin_discovery = qt_config.get_built_plugin_paths()
        
        if os.path.exists(plugin_binary):
            run_command(["cp", plugin_binary, app_bundle_dir + "/"])
            print(f"[OK] Installed to app bundle: {app_bundle_dir}")
        
        if os.path.exists(plugin_json):
            run_command(["cp", plugin_json, app_bundle_dir + "/"])
            print(f"[OK] Installed JSON file to app bundle")
        
        if os.path.exists(plugin_discovery):
            run_command(["cp", plugin_discovery, app_bundle_dir + "/"])
            print(f"[OK] Installed discovery file to app bundle")
        
        print("Plugin installation complete!")

def main():
    """Main build process"""
    # Store the original working directory to prevent shell errors
    original_cwd = os.getcwd()
    
    print("Qt MCP Plugin - Build, Install, Test")
    print_section_break(46)

    # Get platform configuration
    try:
        config = qt_config.get_qt_config()
    except RuntimeError as e:
        print_error("{}".format(str(e)))
        print("")
        print("BUILD FAILED - QT CREATOR NOT FOUND")
        print("=" * 50)
        print("")
        print("Please follow the instructions above to fix the Qt Creator path.")
        print("After updating qt_config.py, run the build script again.")
        print("")
        sys.exit(1)
    print(f"Detected {config['name']}")
    print(f" Qt Creator path: {config['qt_creator_path']}")
    print(f" Qt Creator binary: {config['qt_creator_bin']}")

    # Check if Qt Creator path exists
    if not os.path.exists(config["qt_creator_path"]):
        print_error(f" Qt Creator not found at: {config['qt_creator_path']}")
        print("   Please install Qt Creator or update the path in this script")
        sys.exit(1)

    # =========================================================================
    # CRITICAL FIRST CHECK: Qt Version Compatibility
    # =========================================================================
    # This MUST be the first check before any build steps.
    # Qt Creator requires a specific Qt SDK version to build plugins.
    # =========================================================================
    print("")
    print("[CHECK] Verifying Qt SDK compatibility...")
    is_compatible, required_version, installed_versions = check_qt_version_compatibility(config)
    
    if required_version:
        print(f" Qt Creator requires: Qt {required_version}")
    if installed_versions:
        print(f" Installed Qt versions: {', '.join(installed_versions)}")
    else:
        print(" Installed Qt versions: None found")
    
    if not is_compatible:
        print("")
        print_section_break()
        print("[ERROR] BUILD FAILED - QT VERSION MISMATCH")
        print_section_break()
        print("")
        print(f"Qt Creator was built with Qt {required_version}")
        print(f"but you only have: {', '.join(installed_versions) if installed_versions else 'no Qt SDK installed'}")
        print("")
        print("The Qt SDK version MUST match what Qt Creator was built with.")
        print("")
        print("ACTION REQUIRED:")
        print(f"1. Run the Qt MaintenanceTool:")
        if platform.system().lower() == "windows":
            print(f"   {config['qt_path']}\\MaintenanceTool.exe")
        else:
            print(f"   {config['qt_path']}/MaintenanceTool.app")
        print("2. Select 'Add or remove components'")
        print(f"3. Find and install Qt {required_version}")
        print("   - Include the msvc2022_64 (Windows) or macos (Mac) component")
        print("4. Re-run this build script after installation")
        print("")
        print_section_break()
        sys.exit(1)
    
    print("[OK] Qt SDK version compatible")
    print("")

    # Check: Qt Creator Plugin Development SDK (required tools) at official location only.
    # If not found, report and stop. Do not attempt workarounds.
    # Windows qt_creator_path is .../Tools/QtCreator/lib (SDK at lib/cmake/QtCreator).
    # macOS/Linux use .../Resources or .../qtcreator with SDK at lib/cmake/QtCreator under that root.
    sdk_candidates = [
        os.path.join(config["qt_creator_path"], "lib", "cmake", "QtCreator"),
        os.path.join(config["qt_creator_path"], "cmake", "QtCreator"),
    ]
    sdk_dir = None
    for cand in sdk_candidates:
        cfg = os.path.join(cand, "QtCreatorConfig.cmake")
        if os.path.isfile(cfg):
            sdk_dir = cand
            break
    if sdk_dir is None:
        print("")
        print_error("Qt Creator Plugin Development tools are not installed.")
        print("   Looked for QtCreatorConfig.cmake in:")
        for cand in sdk_candidates:
            print(f"     - {os.path.join(cand, 'QtCreatorConfig.cmake')}")
        print("   Install the Plugin Development component for Qt Creator (MaintenanceTool), then re-run.")
        sys.exit(1)
    print("[CHECK] Qt Creator Plugin Development SDK found")
    print("")

    # Step 1: ALWAYS ensure Qt Creator is NOT running before starting
    print("[SEARCH] Checking for running Qt Creator...")
    if is_process_running(config["process_name"]):
        print("[SEARCH] Qt Creator is running - killing all instances")
        if not kill_qt_creator(config):
            print("")
            print_section_break()
            print_error("BUILD FAILED - QT CREATOR TERMINATION ERROR")
            print_section_break()
            print("")
            print_error(" Could not terminate Qt Creator processes")
            print_error(" Build cannot proceed while Qt Creator is running")
            print("")
            print("ACTION REQUIRED:")
            print("1. Manually close Qt Creator")
            print("2. Check Task Manager for any remaining qtcreator.exe processes")
            print("3. Kill all Qt Creator processes manually")
            print("4. Run the build script again")
            print("")
            print_section_break()
            print("")
            sys.exit(1)
        
        # Verify Qt Creator is actually not running
        if is_process_running(config["process_name"]):
            print_error(" CRITICAL: Qt Creator still running after kill attempts")
            print("   Cannot proceed with build - Qt Creator must be completely terminated")
            print("   Please manually close Qt Creator and try again")
            sys.exit(1)
    
    # Only proceed if Qt Creator is NOT running
    if is_process_running(config["process_name"]):
        print_error(" CRITICAL: Qt Creator is still running")
        print("   Cannot proceed with build - Qt Creator must be completely terminated")
        print("   Please manually close Qt Creator and try again")
        sys.exit(1)
    
    print("[OK] Qt Creator is not running - proceeding with build")

    # Step 2: Use platform-specific build directory to avoid cross-platform conflicts
    system = platform.system().lower()
    build_dir = qt_config.get_build_dir()
    print(f"Using {build_dir} directory for {system} build...")

    # Step 3: Bump version automatically
    print("[PROGRESS] Auto-bumping version...")
    bump_version()
    
    # Step 3b: Regenerate version files to keep them in sync
    print("[PROGRESS] Regenerating version files...")
    if not regenerate_version_files():
        print_error(" Failed to regenerate version files")
        sys.exit(1)

    # Step 3c: Detect Qt Creator version and update JSON dependencies
    print("[PROGRESS] Detecting Qt Creator version...")
    qt_creator_version = detect_qt_creator_version(config)
    if qt_creator_version:
        print("[PROGRESS] Updating JSON dependencies...")
        update_json_dependencies(qt_creator_version)
    else:
        print("[WARNING] Could not detect Qt Creator version - JSON dependencies may need manual update")

    # Step 4: Set up environment for Windows builds
    env = None
    if system == "windows":
        # Windows needs Visual Studio environment
        vs_path = setup_windows_environment()
        if not vs_path:
            print_error(" Visual Studio not found")
            sys.exit(1)
        
        # Set up environment with Visual Studio paths
        env = os.environ.copy()
        
        # Find CMake executable using factored paths
        cmake_paths = qt_config.get_cmake_paths()
        
        cmake_found = False
        cmake_exe = None
        for cmake_path in cmake_paths:
            if os.path.exists(cmake_path):
                cmake_exe = os.path.join(cmake_path, "cmake.exe")
                if os.path.exists(cmake_exe):
                    env["PATH"] = cmake_path + ";" + env.get("PATH", "")
                    logger.debug(f"Added CMake to PATH: {cmake_path}")
                    cmake_found = True
                    break
        
        if not cmake_found:
            print_error(" CMake executable not found")
            sys.exit(1)
    
    # Step 5: Configure CMake
    print("[CONFIG] Configuring CMake...")
    
    qt_config.ensure_fresh_build_dir(build_dir)
    
    # First, configure CMake if needed
    if not os.path.exists(os.path.join(build_dir, "CMakeCache.txt")):
        print("[CONFIG] Running CMake configuration...")
        
        if system == "windows":
            # Debug: Print the PATH to see what's available
            print(f"[SEARCH] CMake config PATH contains: {env.get('PATH', '')[:200]}...")
            
            # Test if cmake is available
            print(f"[SEARCH] Using CMake: {cmake_exe}")
            
            # Discover Qt version dynamically - this is required
            qt_version = qt_config.discover_qt_version(config["qt_creator_bin"])
            if not qt_version:
                print_error(" Could not discover Qt version from Qt Creator installation")
                print_error(" Please ensure Qt Creator is properly installed and accessible")
                sys.exit(1)
            
            # Use Qt Creator installation root (not lib directory)
            qt_creator_root = os.path.dirname(config["qt_creator_path"])  # Remove /lib from path
            
            # Get Qt6 path for Windows - use the actual Qt installation
            qt_dir = config["qt_path"]
            
            # Try msvc2022_64 first, then msvc2019_64
            qt6_path = None
            for msvc_version in ["msvc2022_64", "msvc2019_64"]:
                candidate = f"{qt_dir}/{qt_version}/{msvc_version}"
                if os.path.exists(candidate):
                    qt6_path = candidate
                    print(f"[OK] Found Qt SDK at: {qt6_path}")
                    break
            
            if not qt6_path:
                print_error(f" Qt SDK {qt_version} not found")
                print_error(f" Looked for: {qt_dir}/{qt_version}/msvc2022_64")
                print_error(f"         or: {qt_dir}/{qt_version}/msvc2019_64")
                print("")
                print("ACTION REQUIRED:")
                print(f"1. Run the Qt MaintenanceTool: {qt_dir}/MaintenanceTool.exe")
                print("2. Install Qt {qt_version} with MSVC 2022 64-bit or MSVC 2019 64-bit")
                sys.exit(1)
            
            # Combine Qt Creator and Qt6 paths
            cmake_prefix_path = qt_creator_root + ";" + qt6_path
            cmake_generator = "Visual Studio 17 2022" if "msvc2022_64" in qt6_path.replace("\\", "/") else "Visual Studio 16 2019"
            print(f"[CONFIG] Using CMake generator: {cmake_generator}")
            cmake_cmd = [cmake_exe, "-DCMAKE_PREFIX_PATH=" + cmake_prefix_path, "-G", cmake_generator, "-A", "x64", "-DCMAKE_BUILD_TYPE=Release", "-B", build_dir]
            logger.debug(f"CMake command: {cmake_cmd}")
            logger.debug(f"Qt Creator lib path: {config['qt_creator_path']}")
            logger.debug(f"Qt Creator root path: {qt_creator_root}")
            logger.debug(f"Qt6 path: {qt6_path}")
            logger.debug(f"CMAKE_PREFIX_PATH: {cmake_prefix_path}")
            logger.debug(f"Build directory: {build_dir}")
        else:
            # For macOS, we need both Qt Creator and Qt6 paths
            if system == "darwin":
                # Dynamically detect Qt library version from Qt Creator
                qt_lib_version = detect_qt_library_version(config)
                if not qt_lib_version:
                    print_error(" Could not detect Qt library version from Qt Creator")
                    print_error(" Please ensure Qt Creator is properly installed")
                    sys.exit(1)
                # Find matching Qt SDK
                qt6_path = find_matching_qt_sdk(config, qt_lib_version)
                if not qt6_path:
                    print_error(f" Qt SDK {qt_lib_version} is not installed")
                    print("")
                    print("[ACTION REQUIRED]")
                    print(f"   1. Open MaintenanceTool.app in {config['qt_path']}")
                    print("   2. Select 'Add or remove components'")
                    print(f"   3. Install Qt {qt_lib_version} (including macOS and Qt Plugin Development)")
                    print("   4. Re-run the build after installation completes")
                    sys.exit(1)
                qtcreator_dir = f"{config['qt_creator_path']}/lib/cmake/QtCreator"
                qt6_dir = f"{qt6_path}/lib/cmake/Qt6"
                cmake_cmd = [
                    "cmake",
                    f"-DCMAKE_PREFIX_PATH={config['qt_creator_path']}",
                    f"-DQtCreator_DIR={qtcreator_dir}",
                    f"-DQt6_DIR={qt6_dir}",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-B", build_dir
                ]
            else:
                cmake_cmd = ["cmake", "-DCMAKE_PREFIX_PATH=" + config["qt_creator_path"], "-DCMAKE_BUILD_TYPE=Release", "-B", build_dir]
        
        # Convert list to string for shell execution with proper quoting
        if system == "windows":
            # On Windows, quote arguments containing semicolons or spaces
            cmake_cmd_quoted = []
            for arg in cmake_cmd:
                if ";" in arg or " " in arg:
                    cmake_cmd_quoted.append(f'"{arg}"')
                else:
                    cmake_cmd_quoted.append(arg)
            cmake_cmd_str = " ".join(cmake_cmd_quoted)
        else:
            # For Unix systems, quote paths with spaces
            cmake_cmd_quoted = []
            for arg in cmake_cmd:
                if " " in arg and arg.startswith("-D"):
                    cmake_cmd_quoted.append(f'"{arg}"')
                else:
                    cmake_cmd_quoted.append(arg)
            cmake_cmd_str = " ".join(cmake_cmd_quoted)
        
        if not run_command(cmake_cmd_str, shell=True, env=env):
            print_error(" CMake configuration failed")
            sys.exit(1)
        
        print("[OK] CMake configuration successful")
    else:
        print("[OK] CMake already configured")

    # Step 5: Always rebuild plugin to ensure latest changes are included
    plugin_binary, _ = get_plugin_paths()
    current_dir = os.getcwd()
    
    # Step 5a: Generate JSON file in source directory for MOC compilation
    print("[PROGRESS] Generating JSON file for MOC compilation...")
    if not ensure_json_files_generated(build_dir, also_in_source=True):
        print_error(" Failed to generate JSON files for build")
        sys.exit(1)
    
    print("Building plugin...")
    
    # Build the plugin
    # Use the same environment that was set up for CMake configuration
    
    if os.path.normpath(current_dir) == os.path.normpath(build_dir):
        # We're in build directory, build from here
        if system == "windows":
            # Windows build using Qt Creator's build system - Release only
            # IMPORTANT: This plugin ONLY builds in Release mode
            # Debug builds are not supported and will cause runtime issues
            print(f"[BUILD] Running: {cmake_exe} --build . --config Release")
            logger.debug("Building Release configuration only - Debug builds not supported")
            if not run_command([cmake_exe, "--build", ".", "--config", "Release"], env=env, show_errors=True):
                logger.error("Build failed")
                print_error(" Build failed")
                sys.exit(1)
        else:
            if not run_command(["cmake", "--build", "."]):
                print_error(" Build failed")
                sys.exit(1)
        print("[OK] Build successful")
    else:
        # We're in source directory, build from there
        if system == "windows":
            # Windows build using Qt Creator's build system
            # Change to build directory first
            original_dir = os.getcwd()
            os.chdir(build_dir)
            try:
                build_cmd = [cmake_exe, "--build", ".", "--config", "Release"]
                build_cmd_str = " ".join(build_cmd)
                print(f"[BUILD] Running: {build_cmd_str} (from {build_dir})")
                print("[PROTECT]  Build process protected against interruption...")
                
                # Start monitoring in a separate thread
                monitor_thread = threading.Thread(target=monitor_build_process, 
                                                args=("cmake.exe", 3600))
                monitor_thread.daemon = True
                monitor_thread.start()
                
                if not run_command(build_cmd_str, shell=True, env=env, show_errors=True, timeout=3600):
                    print_error(" Build failed")
                    sys.exit(1)
            finally:
                os.chdir(original_dir)
        else:
            # For macOS/Linux, change to build directory and run cmake --build .
            original_dir = os.getcwd()
            os.chdir(build_dir)
            try:
                build_cmd = ["cmake", "--build", "."]
                if not run_command(build_cmd):
                    print("")
                    print_section_break()
                    print_error("BUILD FAILED - PLUGIN COMPILATION ERROR")
                    print_section_break()
                    print("")
                    print_error(" Plugin compilation failed")
                    print("")
                    print("DEBUGGING REQUIRED:")
                    print("- Check CMake configuration")
                    print("- Verify Qt Creator SDK is properly installed")
                    print("- Check for missing dependencies")
                    print("- Review build logs for specific errors")
                    print("")
                    print_section_break()
                    print("")
                    sys.exit(1)
            finally:
                os.chdir(original_dir)
        print("[OK] Build successful")
    
    # Step 5a: Ensure JSON files are generated in build directory
    print("[PROGRESS] Ensuring JSON files are generated...")
    if not ensure_json_files_generated(build_dir):
        print_error(" Failed to generate JSON files")
        sys.exit(1)
    
    # Step 5b: Clean old plugin versions before installing new one
    print("Cleaning old plugin versions...")
    if not clean_old_plugins(config):
        print("")
        print_section_break()
        print_error("BUILD FAILED - PLUGIN CLEANUP ERROR")
        print_section_break()
        print("")
        print_error(" Failed to delete old plugin files")
        print("")
        print("DEBUGGING REQUIRED:")
        print("- Check file permissions")
        print("- Verify no file locks preventing deletion")
        print("- Check if Qt Creator is completely terminated")
        print("- Manually delete plugin files if necessary")
        print("")
        print_section_break()
        print("")
        sys.exit(1)
    
    # Step 5c: Install the plugin manually
    print("Installing plugin...")
    if not install_plugin(config):
        print("")
        print_section_break()
        print_error("BUILD FAILED - PLUGIN INSTALLATION ERROR")
        print_section_break()
        print("")
        print_error("CRITICAL ERROR: Plugin installation failed!")
        print("")
        print("DEBUGGING REQUIRED:")
        print("- Check Qt Creator installation path")
        print("- Verify write permissions to plugin directory")
        print("- Check if plugin binary exists in build directory")
        print("- Verify plugin JSON file was generated")
        print("")
        print_section_break()
        print("")
        sys.exit(1)
    # Verify installation actually updated the plugin
    print("Verifying plugin installation...")
    if not verify_plugin_installation(config):
        print("")
        print_section_break()
        print_error("BUILD FAILED - PLUGIN INSTALLATION VERIFICATION ERROR")
        print_section_break()
        print("")
        print_error(" Plugin installation verification failed")
        print("")
        print("DEBUGGING REQUIRED:")
        print("- Check if xcopy command succeeded")
        print("- Verify file permissions")
        print("- Check if plugin file was actually updated")
        print("- Verify no file locks preventing overwrite")
        print("")
        print_section_break()
        print("")
        sys.exit(1)
    
    print("[OK] Plugin installation successful")

    # Step 6: Launch Qt Creator as separate process
    print(" Launching Qt Creator...")
    if not launch_qt_creator(config):
        print_error(" Failed to launch Qt Creator")
        sys.exit(1)
    print("[OK] Qt Creator launched successfully")

    # Step 7: Wait for Qt Creator to start and test
    print("Waiting for Qt Creator to start and load plugins...")
    time.sleep(15)  # Give more time for plugin loading

    # Step 8: Test installation - MUST verify MCP server responds with version
    if test_installation(config):
        print("")
        
        # Step 9: Register with Cursor IDE for automatic MCP discovery
        register_with_cursor()
        
        # Step 10: Build standalone installer app (macOS only)
        installer_success = build_installer_app()
        
        print("")
        print_section_break()
        print("BUILD SUCCESSFUL!")
        print_section_break()
        print("")
        print("[SUCCESS] Plugin built and installed successfully")
        print("[SUCCESS] Qt Creator launched and loaded the plugin")
        print("[SUCCESS] MCP server is responding correctly")
        print("[SUCCESS] Plugin version verified")
        print("[SUCCESS] Registered with Cursor IDE")
        if installer_success and platform.system().lower() == "darwin":
            print("[SUCCESS] Standalone installer app created")
        print("")
        print("The MCP Plugin is now ready to use!")
        print("Restart Cursor to enable AI control of Qt Creator.")
        if installer_success and platform.system().lower() == "darwin":
            print("")
            print("A standalone installer app has been created at:")
            print(f"  {os.path.join(qt_config.get_build_dir(), 'Qt MCP Plugin Installer.app')}")
        print_section_break()
        print("")
        
        # Restore original directory to prevent shell errors
        try:
            os.chdir(original_cwd)
        except:
            pass  # Ignore if we can't restore the directory
        sys.exit(0)
    else:
        print("")
        print_section_break()
        print_error("BUILD FAILED - MCP SERVER TEST FAILED")
        print_section_break()
        print("")
        print_error("CRITICAL ERROR: Plugin version mismatch detected!")
        print("")
        print("Expected: Plugin should be running NEW version (patch >= 1)")
        print("Actual:   Plugin is running OLD version (patch 0)")
        print("")
        print("This indicates:")
        print("1. Qt Creator did not load the new plugin")
        print("2. Plugin cache issue - old version still loaded")
        print("3. Plugin installation failed silently")
        print("4. Multiple plugin locations exist")
        print("")
        print("DEBUGGING REQUIRED:")
        print("- Check if plugin was actually built with new version")
        print("- Verify plugin installation succeeded")
        print("- Check for multiple plugin locations")
        print("- Verify Qt Creator plugin cache is cleared")
        print("")
        print_section_break()
        print("")
        
        # Store the actual error for the batch file to display
        os.environ['BUILD_ERROR'] = "MCP server test failed - plugin version mismatch detected"
        
        # Restore original directory to prevent shell errors
        try:
            os.chdir(original_cwd)
        except:
            pass  # Ignore if we can't restore the directory
        sys.exit(1)

def build_installer_app():
    """Build the standalone installer app bundle"""
    system = platform.system().lower()
    
    # Only build installer on macOS for now
    if system != "darwin":
        print("[INFO] Installer app building is only supported on macOS")
        return True  # Not a failure, just skip
    
    print("")
    print_section_break()
    print("Building Installer App")
    print_section_break()
    
    # Get the path to the installer build script
    repo_root = qt_config.get_repo_root()
    installer_script = os.path.join(repo_root, "scripts", "installer", "build_installer.py")
    
    if not os.path.exists(installer_script):
        print_error(f" Installer build script not found: {installer_script}")
        return False
    
    try:
        # Run the installer build script
        result = subprocess.run(
            [sys.executable, installer_script],
            cwd=repo_root,
            capture_output=False,  # Show output in real-time
            text=True
        )
        
        if result.returncode == 0:
            print("[OK] Installer app built successfully")
            return True
        else:
            print_error(" Installer app build failed")
            return False
            
    except Exception as e:
        print_error(f" Failed to build installer app: {e}")
        return False


def register_with_cursor():
    """Register the Qt MCP server with Cursor IDE for automatic discovery"""
    import json
    
    print("[CONFIG] Registering Qt MCP server with Cursor...")
    
    # Find Cursor's mcp.json configuration file
    home_dir = os.path.expanduser("~")
    cursor_mcp_path = os.path.join(home_dir, ".cursor", "mcp.json")
    
    # Ensure .cursor directory exists
    cursor_dir = os.path.dirname(cursor_mcp_path)
    if not os.path.exists(cursor_dir):
        print(f"[WARNING] Cursor config directory not found: {cursor_dir}")
        print("[WARNING] Skipping Cursor MCP registration (Cursor may not be installed)")
        return False
    
    # Define the Qt MCP server configuration
    # Cursor's MCP client expects HTTP transport with explicit configuration
    qt_mcp_config = {
        "url": "http://localhost:3001",
        "transport": "http",
        "description": "Qt Creator MCP Plugin - AI control of Qt Creator IDE"
    }
    
    try:
        # Read existing config or create new one
        if os.path.exists(cursor_mcp_path):
            with open(cursor_mcp_path, 'r') as f:
                config = json.load(f)
        else:
            config = {"mcpServers": {}}
        
        # Ensure mcpServers key exists
        if "mcpServers" not in config:
            config["mcpServers"] = {}
        
        # Check if already registered
        if "qt-creator" in config["mcpServers"]:
            existing = config["mcpServers"]["qt-creator"]
            if existing.get("url") == qt_mcp_config["url"]:
                print("[OK] Qt MCP server already registered with Cursor")
                return True
        
        # Add or update the Qt MCP server
        config["mcpServers"]["qt-creator"] = qt_mcp_config
        
        # Write back the config
        with open(cursor_mcp_path, 'w') as f:
            json.dump(config, f, indent=2)
            f.write('\n')
        
        print(f"[OK] Registered Qt MCP server in {cursor_mcp_path}")
        print("[INFO] Restart Cursor to activate the MCP connection")
        return True
        
    except Exception as e:
        print(f"[WARNING] Failed to register with Cursor: {e}")
        print("[WARNING] You can manually add the following to ~/.cursor/mcp.json:")
        print('  "qt-creator": {"url": "http://localhost:3001"}')
        return False


if __name__ == "__main__":
    # Store the original working directory at the top level
    original_cwd = os.getcwd()
    
    try:
        main()
    except KeyboardInterrupt:
        print("\nBuild interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\nBuild failed with error: {e}")
        sys.exit(1)
    finally:
        # Always try to restore the original directory
        try:
            os.chdir(original_cwd)
        except:
            pass  # Ignore directory restoration errors
