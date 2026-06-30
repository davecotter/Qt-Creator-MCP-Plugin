#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
Qt MCP Plugin - Qt Installation Configuration
Cross-platform configuration for Qt installation paths.

Qt path is read from .qt/qt_path_{platform}.txt files.
If the file is empty/missing, auto-discovery is attempted.
"""

import os
import platform
import shutil
import socket
import subprocess
import re

# =============================================================================
# QT PATH CONFIGURATION
# =============================================================================
# 
# Qt paths are stored in .qt/qt_path_{platform}.txt files:
#   - .qt/qt_path_windows.txt
#   - .qt/qt_path_darwin.txt  
#   - .qt/qt_path_linux.txt
#
# Edit the appropriate file for your platform to set your Qt installation path.
#
# =============================================================================

def get_script_dir():
    """Get the directory containing this script"""
    return os.path.dirname(os.path.abspath(__file__))


def get_repo_root():
    """
    Get the repository root (directory containing .qt and CMakeLists.txt).
    Walks up from this script's directory so it works when the script lives in scripts/build/.
    """
    path = os.path.abspath(__file__)
    for _ in range(20):
        path = os.path.dirname(path)
        if not path or path == os.path.dirname(path):
            break
        qt_dir = os.path.join(path, ".qt")
        if os.path.isdir(qt_dir) and os.path.isfile(os.path.join(path, "CMakeLists.txt")):
            return path
    return get_script_dir()


def read_qt_path_from_file():
    """
    Read Qt path from .qt/qt_path_{platform}.txt
    
    Returns:
        str: Qt path from file, or None if not configured
    """
    system = platform.system().lower()
    repo_root = get_repo_root()
    
    # Map platform to filename
    platform_files = {
        "windows": "qt_path_windows.txt",
        "darwin": "qt_path_darwin.txt",
        "linux": "qt_path_linux.txt"
    }
    
    filename = platform_files.get(system)
    if not filename:
        return None
    
    filepath = os.path.join(repo_root, ".qt", filename)
    
    if not os.path.exists(filepath):
        return None
    
    try:
        with open(filepath, 'r') as f:
            for line in f:
                line = line.strip()
                # Skip empty lines and comments
                if line and not line.startswith('#'):
                    # Normalize path separators
                    return line.replace('\\', '/')
    except Exception:
        pass
    
    return None

def auto_discover_qt_path():
    """
    Attempt to auto-discover Qt installation path
    
    Returns:
        str: Discovered Qt path, or None if not found
    """
    system = platform.system().lower()
    
    # Common Qt installation locations to check
    if system == "windows":
        candidates = [
            os.path.expandvars(r"$USERPROFILE\Developer\Qt"),
            os.path.expandvars(r"$USERPROFILE\Qt"),
            r"C:\Qt",
            r"C:\Program Files\Qt",
        ]
    elif system == "darwin":
        candidates = [
            os.path.expanduser("~/Developer/Qt"),
            os.path.expanduser("~/Qt"),
            "/Applications/Qt",
            "/opt/Qt",
        ]
    else:  # linux
        candidates = [
            os.path.expanduser("~/Qt"),
            "/opt/Qt",
            "/usr/local/Qt",
        ]
    
    for path in candidates:
        if os.path.exists(path):
            # Verify it looks like a Qt installation
            if system == "windows":
                if os.path.exists(os.path.join(path, "Tools", "QtCreator")):
                    return path.replace('\\', '/')
            elif system == "darwin":
                if os.path.exists(os.path.join(path, "Qt Creator.app")):
                    return path
            else:
                if os.path.exists(os.path.join(path, "Tools", "QtCreator")):
                    return path
    
    return None

def get_qt_path_config_file():
    """Get the path to the Qt path config file for the current platform"""
    system = platform.system().lower()
    repo_root = get_repo_root()
    
    platform_files = {
        "windows": "qt_path_windows.txt",
        "darwin": "qt_path_darwin.txt",
        "linux": "qt_path_linux.txt"
    }
    
    filename = platform_files.get(system, "qt_path_linux.txt")
    return os.path.join(repo_root, ".qt", filename)

def get_qt_base_path():
    """
    Get the Qt installation base path.
    
    Priority:
    1. Read from .qt/qt_path_{platform}.txt
    2. Auto-discover from common locations
    3. Fail with instructions to edit the config file
    
    Returns:
        str: Qt installation path
        
    Raises:
        RuntimeError: If Qt path cannot be determined
    """
    # First, try to read from config file
    qt_path = read_qt_path_from_file()
    if qt_path and os.path.exists(qt_path):
        return qt_path
    
    # Second, try auto-discovery
    qt_path = auto_discover_qt_path()
    if qt_path:
        # Save discovered path to config file for future use
        config_file = get_qt_path_config_file()
        try:
            os.makedirs(os.path.dirname(config_file), exist_ok=True)
            with open(config_file, 'w') as f:
                f.write(qt_path + '\n')
            print(f"[OK] Auto-discovered Qt path: {qt_path}")
            print(f"[OK] Saved to: {config_file}")
        except Exception:
            pass
        return qt_path
    
    # Failed - show error with instructions
    config_file = get_qt_path_config_file()
    print("=" * 80)
    print("[ERROR] Qt installation path not configured!")
    print("=" * 80)
    print("")
    print("Please edit the following file and paste your Qt installation path:")
    print(f"  {config_file}")
    print("")
    print("The path should point to the folder containing:")
    print("  - Tools/QtCreator/ (Windows) or Qt Creator.app (macOS)")
    print("  - Version folders like 6.10.1/")
    print("  - MaintenanceTool.exe or MaintenanceTool.app")
    print("")
    print("Example paths:")
    print("  Windows: C:\\Users\\username\\Developer\\Qt")
    print("  macOS:   /Users/username/Developer/Qt")
    print("  Linux:   /opt/Qt")
    print("=" * 80)
    raise RuntimeError(f"Qt path not configured. Edit: {config_file}")

# Legacy compatibility - these are no longer used but kept for reference
CUSTOM_QT_PATHS = {}  # Deprecated - use .qt/qt_path_{platform}.txt instead
CUSTOM_HOSTNAMES = []  # Deprecated - no longer needed

# =============================================================================
# END OF PATH CONFIGURATION  
# =============================================================================

# Constants to eliminate duplicate strings
QT_CREATOR_PROCESSES = {
    "windows": "qtcreator.exe",
    "darwin": "Qt Creator", 
    "linux": "qtcreator"
}

QT_CREATOR_BINARIES = {
    "windows": "qtcreator.exe",
    "darwin": "Qt Creator",
    "linux": "qtcreator"
}

KILL_COMMANDS = {
    "windows": ["taskkill", "/F", "/IM", "qtcreator.exe"],
    "darwin": ["pkill", "-f", "Qt Creator"],
    "linux": ["pkill", "-f", "qtcreator"]
}

TEST_COMMANDS = {
    "windows": ["telnet", "localhost", "3001"],
    "darwin": ["nc", "-w", "3", "localhost", "3001"],
    "linux": ["nc", "-w", "3", "localhost", "3001"]
}

PLUGIN_EXTENSIONS = {
    "windows": ".dll",
    "darwin": ".dylib",
    "linux": ".so"
}

PLUGIN_DIRECTORIES = {
    "windows": "plugins",
    "darwin": "PlugIns/qtcreator",
    "linux": "plugins"
}

# Per-platform build directories (avoid cross-platform CMake cache clashes on shared depots)
BUILD_DIR_NAMES = {
    "windows": "build_windows",
    "darwin": "build_darwin",
    "linux": "build_linux",
}


def get_build_dir(repo_root=None):
    """Return the platform-specific out-of-tree build directory."""
    repo_root = repo_root or get_repo_root()
    system = platform.system().lower()
    return os.path.join(repo_root, BUILD_DIR_NAMES.get(system, f"build_{system}"))


def _normalize_path_for_compare(path):
    return os.path.normcase(os.path.normpath(os.path.abspath(path)))


def is_cmake_cache_stale(build_dir, repo_root=None):
    """
    Return True if CMakeCache.txt exists but was created for a different
    source tree, build directory, or platform toolchain.
    """
    cache_file = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.isfile(cache_file):
        return False

    repo_root = repo_root or get_repo_root()
    repo_norm = _normalize_path_for_compare(repo_root)
    build_norm = _normalize_path_for_compare(build_dir)

    try:
        with open(cache_file, "r", encoding="utf-8", errors="replace") as f:
            lines = f.readlines()
    except OSError:
        return True

    cached_src = None
    cached_build = None
    generator = None

    for line in lines:
        line = line.rstrip("\n")
        if line.startswith("# For build in directory:"):
            cached_build = line.split(":", 1)[1].strip()
        elif line.startswith("CMAKE_HOME_DIRECTORY:INTERNAL="):
            cached_src = line.split("=", 1)[1].strip()
        elif line.startswith("CMAKE_GENERATOR:INTERNAL="):
            generator = line.split("=", 1)[1].strip()

    if cached_build and _normalize_path_for_compare(cached_build) != build_norm:
        return True
    if cached_src and _normalize_path_for_compare(cached_src) != repo_norm:
        return True

    system = platform.system().lower()
    if generator:
        gen_lower = generator.lower()
        if system == "windows":
            if "unix makefiles" in gen_lower:
                return True
        elif system in ("darwin", "linux"):
            if "visual studio" in gen_lower:
                return True

    return False


def ensure_fresh_build_dir(build_dir=None, repo_root=None):
    """
    Remove build_dir if its CMake cache is stale (wrong platform or repo path).
    Returns True if the directory was removed.
    """
    build_dir = build_dir or get_build_dir(repo_root)
    if not is_cmake_cache_stale(build_dir, repo_root):
        return False

    print(f"[CLEAN] Stale CMake cache in {build_dir} — removing build directory")
    if os.path.isdir(build_dir):
        shutil.rmtree(build_dir)
    return True


def get_build_plugin_dir(build_dir=None):
    """Directory containing the built plugin binary and JSON files."""
    build_dir = build_dir or get_build_dir()
    system = platform.system().lower()
    if system == "darwin":
        return os.path.join(
            build_dir, "Qt Creator.app", "Contents", "PlugIns", "qtcreator"
        )
    if system == "windows":
        return os.path.join(build_dir, "lib", "qtcreator", "plugins", "Release")
    return os.path.join(build_dir, "lib", "qtcreator", "plugins")


def get_built_plugin_paths(build_dir=None):
    """Return (binary, json, discovery) paths under the platform build directory."""
    build_dir = build_dir or get_build_dir()
    json_path = os.path.join(build_dir, "Qt_MCP_Plugin.json")
    discovery_path = os.path.join(build_dir, "Qt_MCP_Plugin_discovery.json")
    system = platform.system().lower()
    if system == "windows":
        binary = os.path.join(
            build_dir, "lib", "qtcreator", "plugins", "Release", "Qt_MCP_Plugin.dll"
        )
    elif system == "darwin":
        binary = os.path.join(
            get_build_plugin_dir(build_dir), "libQt_MCP_Plugin.1.dylib"
        )
    else:
        binary = os.path.join(
            build_dir, "lib", "qtcreator", "plugins", "libQt_MCP_Plugin.so"
        )
    return binary, json_path, discovery_path



# Path construction helpers
def build_qt_creator_path(base_path, system):
    """Build Qt Creator path from base path"""
    if system == "windows":
        return "{}/Tools/QtCreator/lib".format(base_path)
    elif system == "darwin":
        # Qt Creator 19+ ships Plugin Development SDK via MaintenanceTool as
        # "Qt Creator.sdk" beside the app bundle (not inside Contents/Resources).
        sdk_path = os.path.join(base_path, "Qt Creator.sdk")
        sdk_cfg = os.path.join(sdk_path, "lib", "cmake", "QtCreator", "QtCreatorConfig.cmake")
        if os.path.isfile(sdk_cfg):
            return sdk_path
        return "{}/Qt Creator.app/Contents/Resources".format(base_path)
    else:  # linux
        return "{}/qtcreator".format(base_path)

def build_qt_creator_bin_path(base_path, system):
    """Build Qt Creator binary path from base path"""
    if system == "windows":
        return "{}/Tools/QtCreator/bin/qtcreator.exe".format(base_path)
    elif system == "darwin":
        return "{}/Qt Creator.app/Contents/MacOS/Qt Creator".format(base_path)
    else:  # linux
        return "{}/qtcreator/bin/qtcreator".format(base_path)

def discover_qt_version(qt_creator_bin):
    """
    Discover the Qt version that Qt Creator was built with
    
    Args:
        qt_creator_bin (str): Path to Qt Creator binary
        
    Returns:
        str: Qt version (e.g., "6.9.2") or None if discovery fails
    """
    try:
        system = platform.system().lower()
        
        if system == "windows":
            # Use qtdiag.exe on Windows
            qtdiag_path = qt_creator_bin.replace("qtcreator.exe", "qtdiag.exe")
        elif system == "darwin":
            # On macOS, qtdiag is in the same MacOS directory as Qt Creator
            import os.path as osp
            qtdiag_path = osp.join(osp.dirname(qt_creator_bin), "qtdiag")
        else:
            # On Linux, qtdiag is in the same directory as qtcreator
            qtdiag_path = qt_creator_bin.replace("qtcreator", "qtdiag")
        
        # Run qtdiag to get Qt version information
        result = subprocess.run([qtdiag_path], 
                              capture_output=True, text=True, timeout=30)
        
        if result.returncode == 0:
            # Parse the output to find Qt version
            for line in result.stdout.split('\n'):
                if 'Qt' in line and ('x86_64' in line or 'arm64' in line or 'gcc' in line or 'Apple LLVM' in line):
                    # Extract version from line like "Qt 6.9.2 (arm64-little_endian-lp64..."
                    match = re.search(r'Qt (\d+\.\d+\.\d+)', line)
                    if match:
                        version = match.group(1)
                        return version
        
        return None
        
    except Exception as e:
        return None

def get_qt_config():
    """
    Get comprehensive Qt installation configuration including version discovery and build-specific fields.
    
    Returns:
        dict: Complete configuration for build scripts
        
    Raises:
        RuntimeError: If Qt Creator is not found or Qt version cannot be discovered
    """
    system = platform.system().lower()
    
    # Get Qt base path from .qt/qt_path_{platform}.txt or auto-discovery
    base_path = get_qt_base_path()
    
    # Build paths using helper functions
    qt_creator_path = build_qt_creator_path(base_path, system)
    qt_creator_bin = build_qt_creator_bin_path(base_path, system)
    
    # CRITICAL: Check if Qt Creator binary exists before proceeding
    if not os.path.exists(qt_creator_bin):
        print("=" * 80)
        print("[ERROR] Qt Creator not found!")
        print("=" * 80)
        print(f"Expected Qt Creator binary at: {qt_creator_bin}")
        print(f"Current hostname: {hostname}")
        print(f"Using {'custom' if custom_path else 'standard'} paths")
        print(f"Base path: {base_path}")
        print("")
        print("SOLUTION:")
        print("1. Install Qt Creator if not already installed")
        print("2. OR update the custom path in qt_config.py")
        print("")
        print("To use a custom path, edit qt_config.py and modify:")
        print("   CUSTOM_QT_PATHS = {")
        print(f'       "darwin": "/your/custom/qt/path",  # Update this line')
        print("       ...")
        print("   }")
        print("")
        print("Then add your hostname to CUSTOM_HOSTNAMES:")
        print("   CUSTOM_HOSTNAMES = [\"your_hostname\"]")
        print("=" * 80)
        raise RuntimeError(f"Qt Creator binary not found at: {qt_creator_bin}")
    
    # Check if Qt Creator lib directory exists
    if not os.path.exists(qt_creator_path):
        print("=" * 80)
        print("[ERROR] Qt Creator lib directory not found!")
        print("=" * 80)
        print(f"Expected Qt Creator lib directory at: {qt_creator_path}")
        print(f"Qt Creator binary found at: {qt_creator_bin}")
        print("")
        print("This usually means Qt Creator is not properly installed or")
        print("the installation path is incorrect.")
        print("")
        print("SOLUTION:")
        print("1. Reinstall Qt Creator")
        print("2. OR update the custom path in qt_config.py (see above)")
        print("=" * 80)
        raise RuntimeError(f"Qt Creator lib directory not found at: {qt_creator_path}")
    
    # Discover Qt version dynamically - this is required
    qt_version = discover_qt_version(qt_creator_bin)
    if qt_version is None:
        print("=" * 80)
        print("[ERROR] Could not discover Qt version!")
        print("=" * 80)
        print(f"Qt Creator binary found at: {qt_creator_bin}")
        print(f"Qt Creator lib directory found at: {qt_creator_path}")
        print("")
        print("This usually means:")
        print("1. Qt Creator is not properly installed")
        print("2. The qtdiag tool is missing or broken")
        print("3. The installation is corrupted")
        print("")
        print("SOLUTION:")
        print("1. Reinstall Qt Creator")
        print("2. OR update the custom path in qt_config.py (see above)")
        print("=" * 80)
        raise RuntimeError("Could not discover Qt version from Qt Creator installation. Please ensure Qt Creator is properly installed and accessible.")
    
    # Platform names
    platform_names = {
        "windows": "Windows",
        "darwin": "macOS", 
        "linux": "Linux"
    }
    
    # Build base configuration
    config = {
        "name": platform_names[system],
        "qt_path": base_path,
        "qt_creator_path": qt_creator_path,
        "qt_creator_bin": qt_creator_bin,
        "process_name": QT_CREATOR_PROCESSES[system],
        "kill_command": KILL_COMMANDS[system],
        "test_command": TEST_COMMANDS[system],
        "plugin_extension": PLUGIN_EXTENSIONS[system],
        "plugin_directory": PLUGIN_DIRECTORIES[system]
    }
    
    # Add build-specific fields
    if system == "darwin":
        config["qt_creator_app"] = os.path.join(base_path, "Qt Creator.app")
        config["qt6_path"] = base_path + "/{}/macos".format(qt_version)
    elif system == "windows":
        config["qt_creator_app"] = None  # Windows doesn't use app bundles
        config["qt6_path"] = base_path + "/Qt/{}/msvc2022_64".format(qt_version)
    else:  # linux
        config["qt_creator_app"] = None  # Linux doesn't use app bundles
        config["qt6_path"] = base_path + "/{}/gcc_64".format(qt_version)
    
    return config


def get_qt_version_path():
    """
    Get the Qt version path (e.g., 6.9.2/msvc2022_64 for Windows)
    
    Returns:
        str: Qt version path for the current platform
        
    Raises:
        RuntimeError: If Qt version cannot be discovered
    """
    system = platform.system().lower()
    config = get_qt_config()
    
    # Discover Qt version dynamically - this is required
    qt_version = discover_qt_version(config["qt_creator_bin"])
    if qt_version is None:
        raise RuntimeError("Could not discover Qt version from Qt Creator installation. Please ensure Qt Creator is properly installed.")
    
    # Platform-specific compiler paths
    compiler_paths = {
        "windows": "msvc2022_64",
        "darwin": "clang_64", 
        "linux": "gcc_64"
    }
    
    return "{}/{}".format(qt_version, compiler_paths[system])

def get_cmake_prefix_path():
    """
    Get the CMake prefix path for Qt Creator
    
    Returns:
        str: CMake prefix path
    """
    config = get_qt_config()
    return config["qt_creator_path"]

def get_plugin_install_path():
    """
    Get the plugin installation path
    
    Returns:
        str: Path where plugins should be installed
    """
    config = get_qt_config()
    return os.path.join(config["qt_creator_path"], config["plugin_directory"])

def get_plugin_binary_name():
    """
    Get the plugin binary name for the current platform
    
    Returns:
        str: Plugin binary name
    """
    system = platform.system().lower()
    
    # Platform-specific plugin binary names
    plugin_names = {
        "windows": "Qt_MCP_Plugin.dll",
        "darwin": "libQt_MCP_Plugin.1.dylib",
        "linux": "libQt_MCP_Plugin.so.1"
    }
    
    return plugin_names[system]

def get_windeployqt_path():
    """
    Get the windeployqt executable path for Windows
    
    Returns:
        str: Path to windeployqt.exe
        
    Raises:
        RuntimeError: If Qt version cannot be discovered or not on Windows
    """
    system = platform.system().lower()
    if system != "windows":
        raise RuntimeError("windeployqt is only available on Windows")
    
    config = get_qt_config()
    
    # Discover Qt version dynamically - this is required
    qt_version = discover_qt_version(config["qt_creator_bin"])
    if qt_version is None:
        raise RuntimeError("Could not discover Qt version from Qt Creator installation. Please ensure Qt Creator is properly installed.")
    
    # Build windeployqt path
    return "{}/Qt/{}/msvc2022_64/bin/windeployqt.exe".format(config["qt_path"], qt_version)

def get_cmake_paths():
    """
    Get possible CMake installation paths for the current platform
    
    Returns:
        list: List of possible CMake paths to search
    """
    system = platform.system().lower()
    config = get_qt_config()
    
    if system == "windows":
        # Windows CMake paths - use Qt installation and common system paths
        cmake_paths = [
            "{}/Tools/CMake_64/bin".format(config["qt_path"]),
            "C:/Program Files/CMake/bin",
            "C:/Program Files (x86)/CMake/bin"
        ]
    elif system == "darwin":
        # macOS CMake paths
        cmake_paths = [
            "/usr/local/bin",
            "/opt/homebrew/bin", 
            "/usr/bin"
        ]
    else:  # linux
        # Linux CMake paths
        cmake_paths = [
            "/usr/bin",
            "/usr/local/bin",
            "/opt/cmake/bin"
        ]
    
    return cmake_paths

def validate_qt_installation():
    """
    Validate that Qt Creator is installed in the expected location
    
    Returns:
        tuple: (is_valid, error_message)
    """
    config = get_qt_config()
    
    # Check if Qt Creator binary exists
    if not os.path.exists(config["qt_creator_bin"]):
        return False, f"Qt Creator binary not found at: {config['qt_creator_bin']}"
    
    # Check if Qt Creator lib directory exists
    if not os.path.exists(config["qt_creator_path"]):
        return False, f"Qt Creator lib directory not found at: {config['qt_creator_path']}"
    
    return True, "Qt installation validated successfully"

if __name__ == "__main__":
    # Test the configuration
    config = get_qt_config()
    print("Qt MCP Plugin - Configuration Test")
    print("=" * 40)
    print(f"Platform: {config['name']}")
    print(f"Qt Creator Binary: {config['qt_creator_bin']}")
    print(f"Qt Creator Path: {config['qt_creator_path']}")
    print(f"Plugin Extension: {config['plugin_extension']}")
    print(f"Plugin Directory: {config['plugin_directory']}")
    print(f"CMake Prefix Path: {get_cmake_prefix_path()}")
    print(f"Plugin Install Path: {get_plugin_install_path()}")
    print(f"Plugin Binary Name: {get_plugin_binary_name()}")
    
    # Test version discovery
    qt_version = discover_qt_version(config["qt_creator_bin"])
    if qt_version:
        print(f"Discovered Qt Version: {qt_version}")
    else:
        print("Qt Version: Could not discover (will use fallback)")
    
    # Validate installation
    is_valid, message = validate_qt_installation()
    if is_valid:
        print(f"\n[OK] {message}")
    else:
        print(f"\n[ERROR] {message}")
        print("\nTo fix this, modify the paths in qt_config.py")
