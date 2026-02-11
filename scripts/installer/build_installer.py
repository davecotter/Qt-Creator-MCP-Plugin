#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Qt MCP Plugin Installer - Build Script

Builds a native macOS installer app using Swift/AppKit.
"""

import os
import sys
import shutil
import subprocess
import glob
import plistlib

# =============================================================================
# Configuration
# =============================================================================

INSTALLER_NAME = "Qt MCP Plugin Installer"
APP_BUNDLE_NAME = "Qt MCP Plugin Installer.app"
PLUGIN_DYLIB_PATTERN = "libQt_MCP_Plugin*.dylib"
PLUGIN_FILES = ["Qt_MCP_Plugin.json", "Qt_MCP_Plugin_discovery.json"]


def print_step(msg):
    print(f"[INSTALLER] {msg}")

def print_success(msg):
    print(f"[✓] {msg}")

def print_error(msg):
    print(f"[✗] {msg}")


def get_project_root():
    # scripts/installer/build_installer.py -> repo root (2 levels up from installer dir)
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def get_build_plugin_dir():
    project_root = get_project_root()
    
    # Single build/ dir; plugin lives under build/lib/qtcreator/... or build/.../PlugIns
    build_dir = os.path.join(
        project_root, 'build', 'Qt Creator.app',
        'Contents', 'PlugIns', 'qtcreator'
    )
    if os.path.isdir(build_dir):
        if glob.glob(os.path.join(build_dir, PLUGIN_DYLIB_PATTERN)):
            return build_dir
    
    return None


def generate_app_icon():
    installer_dir = os.path.dirname(os.path.abspath(__file__))
    icon_path = os.path.join(installer_dir, 'installer_icon.icns')
    
    if os.path.exists(icon_path):
        return icon_path
    
    project_root = get_project_root()
    png_icon = os.path.join(project_root, 'src', 'resources', 'mcp.png')
    
    if os.path.exists(png_icon):
        try:
            iconset_dir = os.path.join(installer_dir, 'installer_icon.iconset')
            os.makedirs(iconset_dir, exist_ok=True)
            
            for size in [16, 32, 64, 128, 256, 512]:
                subprocess.run([
                    'sips', '-z', str(size), str(size), png_icon,
                    '--out', os.path.join(iconset_dir, f'icon_{size}x{size}.png')
                ], capture_output=True, check=True)
                
                if size <= 512:
                    subprocess.run([
                        'sips', '-z', str(size * 2), str(size * 2), png_icon,
                        '--out', os.path.join(iconset_dir, f'icon_{size}x{size}@2x.png')
                    ], capture_output=True, check=True)
            
            subprocess.run([
                'iconutil', '-c', 'icns', iconset_dir, '-o', icon_path
            ], capture_output=True, check=True)
            
            shutil.rmtree(iconset_dir)
            print_success(f"Generated icon")
            return icon_path
            
        except Exception as e:
            print_error(f"Icon generation error: {e}")
    
    return None


def compile_swift_app():
    """Compile the Swift installer into a native app bundle."""
    installer_dir = os.path.dirname(os.path.abspath(__file__))
    swift_file = os.path.join(installer_dir, 'QtMCPInstaller.swift')
    
    if not os.path.exists(swift_file):
        print_error(f"Swift source not found: {swift_file}")
        return None
    
    # Create app bundle structure
    dist_dir = os.path.join(installer_dir, 'dist')
    app_path = os.path.join(dist_dir, APP_BUNDLE_NAME)
    macos_dir = os.path.join(app_path, 'Contents', 'MacOS')
    resources_dir = os.path.join(app_path, 'Contents', 'Resources')
    plugins_dir = os.path.join(resources_dir, 'plugins')
    
    # Clean and create directories
    if os.path.exists(dist_dir):
        shutil.rmtree(dist_dir)
    
    os.makedirs(macos_dir, exist_ok=True)
    os.makedirs(plugins_dir, exist_ok=True)
    
    # Compile Swift
    print_step("Compiling Swift installer...")
    executable_path = os.path.join(macos_dir, INSTALLER_NAME)
    
    result = subprocess.run([
        'swiftc',
        '-O',  # Optimize
        '-whole-module-optimization',
        '-framework', 'Cocoa',
        '-o', executable_path,
        swift_file
    ], capture_output=True, text=True)
    
    if result.returncode != 0:
        print_error("Swift compilation failed:")
        print(result.stderr)
        return None
    
    print_success("Swift compilation successful")
    
    # Generate and copy icon
    icon_path = generate_app_icon()
    if icon_path:
        shutil.copy2(icon_path, os.path.join(resources_dir, 'installer_icon.icns'))
    
    # Create Info.plist
    plist_content = {
        'CFBundleName': INSTALLER_NAME,
        'CFBundleDisplayName': INSTALLER_NAME,
        'CFBundleIdentifier': 'com.qtmcp.installer',
        'CFBundleVersion': '1.0.0',
        'CFBundleShortVersionString': '1.0.0',
        'CFBundleExecutable': INSTALLER_NAME,
        'CFBundlePackageType': 'APPL',
        'CFBundleSignature': '????',
        'CFBundleIconFile': 'installer_icon.icns' if icon_path else '',
        'NSHumanReadableCopyright': 'Copyright © 2024-2026 Qt MCP Plugin',
        'NSHighResolutionCapable': True,
        'NSRequiresAquaSystemAppearance': False,
        'LSMinimumSystemVersion': '10.15',
        'LSUIElement': False,
        'NSPrincipalClass': 'NSApplication',
    }
    
    plist_path = os.path.join(app_path, 'Contents', 'Info.plist')
    with open(plist_path, 'wb') as f:
        plistlib.dump(plist_content, f)
    
    print_step("Created Info.plist")
    
    return app_path


def embed_plugin_files(app_path):
    plugin_source = get_build_plugin_dir()
    
    if not plugin_source:
        print_error("Could not find built plugin files!")
        print_error("Please run 'python3 scripts/build/build_main.py' first.")
        return False
    
    print_step(f"Found plugin files in: {plugin_source}")
    
    plugins_dir = os.path.join(app_path, 'Contents', 'Resources', 'plugins')
    
    # Find the main versioned dylib (e.g., libQt_MCP_Plugin.1.dylib)
    # Skip symlinks and unversioned names
    dylib_files = glob.glob(os.path.join(plugin_source, PLUGIN_DYLIB_PATTERN))
    main_dylib = None
    
    for src_file in dylib_files:
        # Skip symlinks
        if os.path.islink(src_file):
            continue
        basename = os.path.basename(src_file)
        # Prefer the versioned dylib (contains .1. or similar version number)
        if '.1.' in basename or basename.endswith('.1.dylib'):
            main_dylib = src_file
            break
    
    # Fallback to any non-symlink dylib
    if not main_dylib:
        for src_file in dylib_files:
            if not os.path.islink(src_file):
                main_dylib = src_file
                break
    
    if main_dylib:
        basename = os.path.basename(main_dylib)
        dst_file = os.path.join(plugins_dir, basename)
        print_step(f"Copying {basename}...")
        shutil.copy2(main_dylib, dst_file)
    else:
        print_error("No main dylib found!")
        return False
    
    # Copy JSON files
    project_root = get_project_root()
    build_dir = os.path.join(project_root, 'build')
    
    for json_file in PLUGIN_FILES:
        for src_path in [
            os.path.join(plugin_source, json_file),
            os.path.join(build_dir, json_file),
        ]:
            if os.path.exists(src_path):
                print_step(f"Copying {json_file}...")
                shutil.copy2(src_path, os.path.join(plugins_dir, json_file))
                break
    
    embedded = glob.glob(os.path.join(plugins_dir, PLUGIN_DYLIB_PATTERN))
    if not embedded:
        print_error("No plugin dylib files embedded!")
        return False
    
    print_success(f"Embedded {len(embedded)} dylib file(s)")
    return True


def sign_app_bundle(app_path):
    print_step("Signing app bundle...")
    try:
        subprocess.run(['codesign', '--remove-signature', app_path],
                      capture_output=True, check=False)
        result = subprocess.run(
            ['codesign', '--force', '--deep', '--sign', '-', app_path],
            capture_output=True, text=True
        )
        if result.returncode == 0:
            print_success("App bundle signed")
            return True
        else:
            print_error(f"Signing failed: {result.stderr}")
            return False
    except Exception as e:
        print_error(f"Signing error: {e}")
        return False


def move_to_build_dir(app_path):
    project_root = get_project_root()
    build_dir = os.path.join(project_root, 'build')
    os.makedirs(build_dir, exist_ok=True)
    final_path = os.path.join(build_dir, APP_BUNDLE_NAME)
    
    if os.path.exists(final_path):
        print_step("Removing previous installer...")
        shutil.rmtree(final_path)
    
    print_step(f"Moving to build dir...")
    shutil.move(app_path, final_path)
    
    return final_path


def cleanup():
    installer_dir = os.path.dirname(os.path.abspath(__file__))
    dist_dir = os.path.join(installer_dir, 'dist')
    if os.path.isdir(dist_dir):
        shutil.rmtree(dist_dir)


def build_installer():
    print("=" * 60)
    print("Qt MCP Plugin Installer - Build (Native Swift)")
    print("=" * 60)
    print()
    
    # Compile Swift app
    app_path = compile_swift_app()
    if not app_path:
        print_error("Failed to compile Swift app")
        return False
    
    # Embed plugin files
    if not embed_plugin_files(app_path):
        print_error("Failed to embed plugin files")
        return False
    
    # Sign
    sign_app_bundle(app_path)
    
    # Move to workspace root
    final_path = move_to_build_dir(app_path)
    
    # Cleanup
    cleanup()
    
    print()
    print("=" * 60)
    print_success("Installer build complete!")
    print("=" * 60)
    print()
    print(f"Installer app: {final_path}")
    print()
    print("Double-click to install the Qt MCP Plugin into Qt Creator.")
    print()
    
    return True


def main():
    success = build_installer()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
