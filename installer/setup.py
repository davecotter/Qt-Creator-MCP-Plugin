#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Qt MCP Plugin Installer - py2app Setup Script

This script builds the installer into a standalone macOS .app bundle.
Usage: python3 setup.py py2app
"""

import os
import sys
from setuptools import setup

# App name
APP_NAME = "Qt MCP Plugin Installer"
APP_BUNDLE_ID = "com.qtmcp.installer"

# Main script
APP = ['qt_mcp_installer.py']

# Data files to include
DATA_FILES = []

# py2app options
OPTIONS = {
    'argv_emulation': False,  # Better compatibility on modern macOS
    'iconfile': 'installer_icon.icns' if os.path.exists('installer_icon.icns') else None,
    'plist': {
        'CFBundleName': APP_NAME,
        'CFBundleDisplayName': APP_NAME,
        'CFBundleIdentifier': APP_BUNDLE_ID,
        'CFBundleVersion': '1.0.0',
        'CFBundleShortVersionString': '1.0.0',
        'NSHumanReadableCopyright': 'Copyright © 2024-2026 Qt MCP Plugin',
        'NSHighResolutionCapable': True,
        'NSRequiresAquaSystemAppearance': False,  # Support dark mode
        'LSMinimumSystemVersion': '10.15',
        'CFBundleDocumentTypes': [],
        'LSUIElement': False,  # Show in Dock
    },
    'packages': [],
    'includes': ['tkinter', 'tkinter.ttk', 'plistlib', 'glob', 'shutil', 'threading'],
    'excludes': [
        'matplotlib', 'numpy', 'scipy', 'pandas', 'PIL', 
        'PyQt5', 'PyQt6', 'PySide2', 'PySide6',
        'wx', 'IPython', 'notebook', 'jupyter',
        'test', 'tests', 'unittest'
    ],
    'strip': True,
    'optimize': 2,
    'semi_standalone': False,
    'site_packages': False,
}

setup(
    name=APP_NAME,
    app=APP,
    data_files=DATA_FILES,
    options={'py2app': OPTIONS},
    setup_requires=['py2app'],
)

