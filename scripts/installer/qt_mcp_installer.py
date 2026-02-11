#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Qt MCP Plugin Installer
A standalone macOS application for installing the Qt MCP Plugin into Qt Creator.

Features:
- Auto-detects Qt Creator installation
- Removes previous plugin versions
- Installs new plugin with progress indication
- System compatibility checking
- Beautiful native-looking UI
"""

import os
import sys
import platform
import subprocess
import shutil
import threading
import time
import glob
import re
import plistlib
from pathlib import Path

# Ensure we can import tkinter
try:
    import tkinter as tk
    from tkinter import ttk, messagebox
except ImportError:
    print("Error: tkinter is required. Please install python3-tk.")
    sys.exit(1)


# =============================================================================
# Constants
# =============================================================================

APP_NAME = "Qt MCP Plugin Installer"
PLUGIN_NAME = "Qt MCP Plugin"
MIN_MACOS_VERSION = "10.15"  # Catalina or later
MIN_QT_CREATOR_VERSION = "14.0.0"  # Qt Creator 14+

# Plugin file patterns
PLUGIN_DYLIB_PATTERN = "libQt_MCP_Plugin*.dylib"
PLUGIN_JSON_FILE = "Qt_MCP_Plugin.json"
PLUGIN_DISCOVERY_FILE = "Qt_MCP_Plugin_discovery.json"


# =============================================================================
# Compatibility Checker
# =============================================================================

class CompatibilityChecker:
    """Check system and Qt Creator compatibility."""
    
    @staticmethod
    def get_macos_version():
        """Get the macOS version as a tuple."""
        try:
            version = platform.mac_ver()[0]
            if version:
                parts = version.split('.')
                return tuple(int(p) for p in parts[:3] if p.isdigit())
        except:
            pass
        return (0, 0, 0)
    
    @staticmethod
    def is_macos():
        """Check if running on macOS."""
        return platform.system().lower() == "darwin"
    
    @staticmethod
    def is_compatible_macos():
        """Check if macOS version is compatible."""
        version = CompatibilityChecker.get_macos_version()
        min_version = tuple(int(p) for p in MIN_MACOS_VERSION.split('.'))
        return version >= min_version
    
    @staticmethod
    def get_qt_creator_version(app_path):
        """Get Qt Creator version from Info.plist."""
        try:
            info_plist = os.path.join(app_path, "Contents", "Info.plist")
            if os.path.exists(info_plist):
                with open(info_plist, 'rb') as f:
                    plist = plistlib.load(f)
                    return plist.get("CFBundleShortVersionString", "")
        except:
            pass
        return None
    
    @staticmethod
    def is_compatible_qt_creator(version_str):
        """Check if Qt Creator version is compatible."""
        if not version_str:
            return False
        try:
            parts = version_str.split('.')
            version = tuple(int(p) for p in parts[:3] if p.isdigit())
            min_parts = MIN_QT_CREATOR_VERSION.split('.')
            min_version = tuple(int(p) for p in min_parts[:3] if p.isdigit())
            return version >= min_version
        except:
            return False
    
    @staticmethod
    def get_architecture():
        """Get CPU architecture."""
        machine = platform.machine().lower()
        if machine in ['arm64', 'aarch64']:
            return 'Apple Silicon (arm64)'
        elif machine in ['x86_64', 'amd64']:
            return 'Intel (x86_64)'
        return machine


# =============================================================================
# Qt Creator Finder
# =============================================================================

class QtCreatorFinder:
    """Find Qt Creator installations on the system."""
    
    # Standard locations to search for Qt Creator
    SEARCH_PATHS = [
        "/Applications/Qt Creator.app",
        os.path.expanduser("~/Applications/Qt Creator.app"),
        os.path.expanduser("~/Developer/Qt/Qt Creator.app"),
        "/opt/Qt/Qt Creator.app",
        "/usr/local/Qt/Qt Creator.app",
    ]
    
    @classmethod
    def find_all(cls):
        """Find all Qt Creator installations."""
        found = []
        
        # Check standard paths
        for path in cls.SEARCH_PATHS:
            if os.path.isdir(path) and cls._is_valid_qt_creator(path):
                found.append(path)
        
        # Also check ~/Developer/Qt for versioned installations
        qt_dev_dir = os.path.expanduser("~/Developer/Qt")
        if os.path.isdir(qt_dev_dir):
            for entry in os.listdir(qt_dev_dir):
                if entry == "Qt Creator.app":
                    full_path = os.path.join(qt_dev_dir, entry)
                    if full_path not in found and cls._is_valid_qt_creator(full_path):
                        found.append(full_path)
        
        # Check /Applications for any Qt Creator variants
        apps_dir = "/Applications"
        if os.path.isdir(apps_dir):
            for entry in os.listdir(apps_dir):
                if "Qt Creator" in entry and entry.endswith(".app"):
                    full_path = os.path.join(apps_dir, entry)
                    if full_path not in found and cls._is_valid_qt_creator(full_path):
                        found.append(full_path)
        
        return found
    
    @classmethod
    def _is_valid_qt_creator(cls, path):
        """Check if path is a valid Qt Creator installation."""
        # Check for the binary
        binary_path = os.path.join(path, "Contents", "MacOS", "Qt Creator")
        # Check for plugin directory
        plugin_dir = os.path.join(path, "Contents", "PlugIns", "qtcreator")
        return os.path.isfile(binary_path) and os.path.isdir(plugin_dir)
    
    @classmethod
    def get_plugin_dir(cls, app_path):
        """Get the plugin directory for a Qt Creator installation."""
        return os.path.join(app_path, "Contents", "PlugIns", "qtcreator")


# =============================================================================
# Plugin Installer
# =============================================================================

class PluginInstaller:
    """Handle plugin installation and removal."""
    
    def __init__(self, qt_creator_path, plugin_source_dir):
        self.qt_creator_path = qt_creator_path
        self.plugin_source_dir = plugin_source_dir
        self.plugin_dir = QtCreatorFinder.get_plugin_dir(qt_creator_path)
    
    def get_existing_plugin_files(self):
        """Find existing plugin files."""
        files = []
        if os.path.isdir(self.plugin_dir):
            # Find dylib files
            for pattern in [PLUGIN_DYLIB_PATTERN]:
                files.extend(glob.glob(os.path.join(self.plugin_dir, pattern)))
            # Find JSON files
            for filename in [PLUGIN_JSON_FILE, PLUGIN_DISCOVERY_FILE]:
                path = os.path.join(self.plugin_dir, filename)
                if os.path.exists(path):
                    files.append(path)
        return files
    
    def remove_existing_plugin(self, progress_callback=None):
        """Remove existing plugin installation."""
        files = self.get_existing_plugin_files()
        total = len(files)
        
        for i, filepath in enumerate(files):
            try:
                os.remove(filepath)
                if progress_callback:
                    progress_callback(f"Removed: {os.path.basename(filepath)}", (i + 1) / max(total, 1))
            except OSError as e:
                raise RuntimeError(f"Failed to remove {filepath}: {e}")
        
        return len(files)
    
    def get_source_plugin_files(self):
        """Find plugin files to install from source directory."""
        files = []
        if os.path.isdir(self.plugin_source_dir):
            # Find dylib files
            for pattern in [PLUGIN_DYLIB_PATTERN]:
                files.extend(glob.glob(os.path.join(self.plugin_source_dir, pattern)))
            # Find JSON files
            for filename in [PLUGIN_JSON_FILE, PLUGIN_DISCOVERY_FILE]:
                path = os.path.join(self.plugin_source_dir, filename)
                if os.path.exists(path):
                    files.append(path)
        return files
    
    def install_plugin(self, progress_callback=None):
        """Install the plugin files."""
        source_files = self.get_source_plugin_files()
        if not source_files:
            raise RuntimeError("No plugin files found to install")
        
        total = len(source_files)
        installed = []
        
        for i, src_path in enumerate(source_files):
            filename = os.path.basename(src_path)
            dst_path = os.path.join(self.plugin_dir, filename)
            
            try:
                shutil.copy2(src_path, dst_path)
                installed.append(dst_path)
                if progress_callback:
                    progress_callback(f"Installed: {filename}", (i + 1) / total)
            except OSError as e:
                # Rollback on failure
                for installed_file in installed:
                    try:
                        os.remove(installed_file)
                    except:
                        pass
                raise RuntimeError(f"Failed to install {filename}: {e}")
        
        return len(installed)
    
    def verify_installation(self):
        """Verify the plugin was installed correctly."""
        # Check for the main dylib
        dylib_files = glob.glob(os.path.join(self.plugin_dir, PLUGIN_DYLIB_PATTERN))
        if not dylib_files:
            return False, "Plugin dylib not found"
        
        # Check for JSON file
        json_path = os.path.join(self.plugin_dir, PLUGIN_JSON_FILE)
        if not os.path.exists(json_path):
            return False, "Plugin JSON file not found"
        
        return True, "Installation verified"


# =============================================================================
# GUI Application
# =============================================================================

class InstallerApp:
    """Main installer application with GUI."""
    
    # Color scheme - modern dark theme inspired by Qt Creator
    COLORS = {
        'bg': '#1e1e2e',           # Dark background
        'bg_secondary': '#2a2a3c', # Secondary background
        'fg': '#cdd6f4',           # Light text
        'fg_secondary': '#9399b2', # Secondary text
        'accent': '#89b4fa',       # Blue accent
        'accent_hover': '#b4befe', # Lighter blue on hover
        'success': '#a6e3a1',      # Green for success
        'error': '#f38ba8',        # Red for errors
        'warning': '#fab387',      # Orange for warnings
        'border': '#45475a',       # Border color
        'progress_bg': '#313244',  # Progress bar background
    }
    
    def __init__(self):
        self.root = tk.Tk()
        self.root.title(APP_NAME)
        self.root.geometry("700x550")
        self.root.resizable(False, False)
        
        # Configure root window
        self.root.configure(bg=self.COLORS['bg'])
        
        # Center window on screen
        self.center_window()
        
        # Configure styles
        self.setup_styles()
        
        # State variables
        self.qt_creator_path = tk.StringVar()
        self.found_installations = []
        self.installation_running = False
        
        # Build UI
        self.create_ui()
        
        # Initial scan for Qt Creator
        self.scan_for_qt_creator()
    
    def center_window(self):
        """Center the window on the screen."""
        self.root.update_idletasks()
        width = self.root.winfo_width()
        height = self.root.winfo_height()
        x = (self.root.winfo_screenwidth() // 2) - (width // 2)
        y = (self.root.winfo_screenheight() // 2) - (height // 2)
        self.root.geometry(f'+{x}+{y}')
    
    def setup_styles(self):
        """Configure ttk styles for modern look."""
        style = ttk.Style()
        
        # Try to use a modern theme base
        try:
            style.theme_use('clam')
        except:
            pass
        
        # Configure custom styles
        style.configure('TFrame', background=self.COLORS['bg'])
        style.configure('Secondary.TFrame', background=self.COLORS['bg_secondary'])
        
        style.configure('TLabel', 
                       background=self.COLORS['bg'], 
                       foreground=self.COLORS['fg'],
                       font=('SF Pro Display', 12))
        
        style.configure('Title.TLabel',
                       background=self.COLORS['bg'],
                       foreground=self.COLORS['fg'],
                       font=('SF Pro Display', 24, 'bold'))
        
        style.configure('Subtitle.TLabel',
                       background=self.COLORS['bg'],
                       foreground=self.COLORS['fg_secondary'],
                       font=('SF Pro Display', 13))
        
        style.configure('Section.TLabel',
                       background=self.COLORS['bg'],
                       foreground=self.COLORS['accent'],
                       font=('SF Pro Display', 14, 'bold'))
        
        style.configure('Status.TLabel',
                       background=self.COLORS['bg'],
                       foreground=self.COLORS['fg_secondary'],
                       font=('SF Pro Display', 11))
        
        style.configure('Success.TLabel',
                       background=self.COLORS['bg'],
                       foreground=self.COLORS['success'],
                       font=('SF Pro Display', 12))
        
        style.configure('Error.TLabel',
                       background=self.COLORS['bg'],
                       foreground=self.COLORS['error'],
                       font=('SF Pro Display', 12))
        
        # Button styles
        style.configure('Accent.TButton',
                       font=('SF Pro Display', 13, 'bold'),
                       padding=(20, 12))
        
        style.map('Accent.TButton',
                 background=[('active', self.COLORS['accent_hover']),
                            ('!active', self.COLORS['accent'])],
                 foreground=[('active', self.COLORS['bg']),
                            ('!active', self.COLORS['bg'])])
        
        # Progress bar style
        style.configure('Custom.Horizontal.TProgressbar',
                       background=self.COLORS['accent'],
                       troughcolor=self.COLORS['progress_bg'],
                       borderwidth=0,
                       lightcolor=self.COLORS['accent'],
                       darkcolor=self.COLORS['accent'])
        
        # Combobox style
        style.configure('TCombobox',
                       fieldbackground=self.COLORS['bg_secondary'],
                       background=self.COLORS['bg_secondary'],
                       foreground=self.COLORS['fg'],
                       arrowcolor=self.COLORS['fg'],
                       font=('SF Pro Display', 12))
    
    def create_ui(self):
        """Create the main user interface."""
        # Main container with padding
        main_frame = ttk.Frame(self.root, style='TFrame')
        main_frame.pack(fill='both', expand=True, padx=40, pady=30)
        
        # Header section
        self.create_header(main_frame)
        
        # System info section
        self.create_system_info(main_frame)
        
        # Qt Creator selection section
        self.create_qt_creator_selection(main_frame)
        
        # Progress section (hidden initially)
        self.create_progress_section(main_frame)
        
        # Action buttons
        self.create_action_buttons(main_frame)
    
    def create_header(self, parent):
        """Create the header section."""
        header_frame = ttk.Frame(parent, style='TFrame')
        header_frame.pack(fill='x', pady=(0, 20))
        
        # App icon placeholder (could be replaced with actual icon)
        icon_label = ttk.Label(header_frame, text="⚙️", font=('', 48), style='TLabel')
        icon_label.pack(anchor='center')
        
        # Title
        title = ttk.Label(header_frame, text=APP_NAME, style='Title.TLabel')
        title.pack(anchor='center', pady=(10, 5))
        
        # Subtitle
        subtitle = ttk.Label(header_frame, 
                            text="Install the MCP Plugin for AI-powered Qt Creator control",
                            style='Subtitle.TLabel')
        subtitle.pack(anchor='center')
    
    def create_system_info(self, parent):
        """Create system information section."""
        info_frame = ttk.Frame(parent, style='Secondary.TFrame')
        info_frame.pack(fill='x', pady=(0, 20), ipadx=15, ipady=10)
        
        # System label
        section_label = ttk.Label(info_frame, text="System Information", style='Section.TLabel')
        section_label.configure(background=self.COLORS['bg_secondary'])
        section_label.pack(anchor='w', padx=10, pady=(5, 10))
        
        # macOS version
        macos_version = platform.mac_ver()[0]
        is_compatible_macos = CompatibilityChecker.is_compatible_macos()
        status_icon = "✓" if is_compatible_macos else "✗"
        status_color = 'Success.TLabel' if is_compatible_macos else 'Error.TLabel'
        
        self.macos_label = ttk.Label(info_frame, 
                                     text=f"{status_icon} macOS {macos_version}",
                                     style=status_color)
        self.macos_label.configure(background=self.COLORS['bg_secondary'])
        self.macos_label.pack(anchor='w', padx=20)
        
        # Architecture
        arch = CompatibilityChecker.get_architecture()
        arch_label = ttk.Label(info_frame, text=f"• {arch}", style='Status.TLabel')
        arch_label.configure(background=self.COLORS['bg_secondary'])
        arch_label.pack(anchor='w', padx=20)
        
        # Compatibility note if needed
        if not is_compatible_macos:
            note = ttk.Label(info_frame, 
                           text=f"  ⚠️ Requires macOS {MIN_MACOS_VERSION} or later",
                           style='Error.TLabel')
            note.configure(background=self.COLORS['bg_secondary'])
            note.pack(anchor='w', padx=20)
    
    def create_qt_creator_selection(self, parent):
        """Create Qt Creator selection section."""
        self.qt_frame = ttk.Frame(parent, style='Secondary.TFrame')
        self.qt_frame.pack(fill='x', pady=(0, 20), ipadx=15, ipady=10)
        
        # Section label
        section_label = ttk.Label(self.qt_frame, text="Qt Creator Installation", style='Section.TLabel')
        section_label.configure(background=self.COLORS['bg_secondary'])
        section_label.pack(anchor='w', padx=10, pady=(5, 10))
        
        # Selection frame
        select_frame = ttk.Frame(self.qt_frame, style='Secondary.TFrame')
        select_frame.pack(fill='x', padx=10, pady=5)
        
        # Dropdown for Qt Creator installations
        self.qt_combo = ttk.Combobox(select_frame, 
                                     textvariable=self.qt_creator_path,
                                     state='readonly',
                                     font=('SF Pro Display', 11),
                                     width=50)
        self.qt_combo.pack(side='left', fill='x', expand=True)
        self.qt_combo.bind('<<ComboboxSelected>>', self.on_qt_creator_selected)
        
        # Refresh button
        refresh_btn = tk.Button(select_frame, text="🔄", 
                               font=('', 14),
                               bg=self.COLORS['bg_secondary'],
                               fg=self.COLORS['fg'],
                               activebackground=self.COLORS['border'],
                               activeforeground=self.COLORS['fg'],
                               bd=0,
                               highlightthickness=0,
                               command=self.scan_for_qt_creator)
        refresh_btn.pack(side='left', padx=(10, 0))
        
        # Version info label
        self.version_label = ttk.Label(self.qt_frame, text="", style='Status.TLabel')
        self.version_label.configure(background=self.COLORS['bg_secondary'])
        self.version_label.pack(anchor='w', padx=20, pady=(5, 0))
        
        # Plugin status label
        self.plugin_status_label = ttk.Label(self.qt_frame, text="", style='Status.TLabel')
        self.plugin_status_label.configure(background=self.COLORS['bg_secondary'])
        self.plugin_status_label.pack(anchor='w', padx=20)
    
    def create_progress_section(self, parent):
        """Create the progress indication section."""
        self.progress_frame = ttk.Frame(parent, style='Secondary.TFrame')
        # Don't pack initially - shown during installation
        
        # Progress label
        self.progress_label = ttk.Label(self.progress_frame, 
                                        text="Installing...", 
                                        style='Status.TLabel')
        self.progress_label.configure(background=self.COLORS['bg_secondary'])
        self.progress_label.pack(anchor='w', padx=15, pady=(10, 5))
        
        # Progress bar
        self.progress_bar = ttk.Progressbar(self.progress_frame,
                                            style='Custom.Horizontal.TProgressbar',
                                            length=400,
                                            mode='determinate')
        self.progress_bar.pack(fill='x', padx=15, pady=5)
        
        # Step detail label
        self.step_label = ttk.Label(self.progress_frame, text="", style='Status.TLabel')
        self.step_label.configure(background=self.COLORS['bg_secondary'])
        self.step_label.pack(anchor='w', padx=15, pady=(0, 10))
    
    def create_action_buttons(self, parent):
        """Create the action buttons."""
        btn_frame = ttk.Frame(parent, style='TFrame')
        btn_frame.pack(fill='x', pady=(10, 0))
        
        # Cancel button
        self.cancel_btn = tk.Button(btn_frame, text="Cancel",
                                    font=('SF Pro Display', 13),
                                    bg=self.COLORS['bg_secondary'],
                                    fg=self.COLORS['fg'],
                                    activebackground=self.COLORS['border'],
                                    activeforeground=self.COLORS['fg'],
                                    bd=0,
                                    highlightthickness=1,
                                    highlightbackground=self.COLORS['border'],
                                    padx=25, pady=10,
                                    command=self.on_cancel)
        self.cancel_btn.pack(side='left')
        
        # Install button
        self.install_btn = tk.Button(btn_frame, text="Install Plugin",
                                     font=('SF Pro Display', 13, 'bold'),
                                     bg=self.COLORS['accent'],
                                     fg=self.COLORS['bg'],
                                     activebackground=self.COLORS['accent_hover'],
                                     activeforeground=self.COLORS['bg'],
                                     bd=0,
                                     highlightthickness=0,
                                     padx=30, pady=10,
                                     command=self.on_install)
        self.install_btn.pack(side='right')
    
    def scan_for_qt_creator(self):
        """Scan for Qt Creator installations."""
        self.found_installations = QtCreatorFinder.find_all()
        
        if self.found_installations:
            self.qt_combo['values'] = self.found_installations
            self.qt_combo.current(0)
            self.on_qt_creator_selected(None)
        else:
            self.qt_combo['values'] = []
            self.qt_creator_path.set("")
            self.version_label.configure(text="⚠️ No Qt Creator installation found")
            self.plugin_status_label.configure(text="")
            self.install_btn.configure(state='disabled')
    
    def on_qt_creator_selected(self, event):
        """Handle Qt Creator selection change."""
        path = self.qt_creator_path.get()
        if not path:
            return
        
        # Get Qt Creator version
        version = CompatibilityChecker.get_qt_creator_version(path)
        is_compatible = CompatibilityChecker.is_compatible_qt_creator(version)
        
        if version:
            status_icon = "✓" if is_compatible else "⚠️"
            self.version_label.configure(
                text=f"{status_icon} Qt Creator {version}",
                style='Success.TLabel' if is_compatible else 'Error.TLabel'
            )
            self.version_label.configure(background=self.COLORS['bg_secondary'])
        else:
            self.version_label.configure(text="• Version unknown", style='Status.TLabel')
            self.version_label.configure(background=self.COLORS['bg_secondary'])
        
        # Check for existing plugin
        plugin_dir = QtCreatorFinder.get_plugin_dir(path)
        existing_files = glob.glob(os.path.join(plugin_dir, PLUGIN_DYLIB_PATTERN))
        
        if existing_files:
            self.plugin_status_label.configure(
                text="• Existing plugin found (will be replaced)",
                style='Status.TLabel'
            )
        else:
            self.plugin_status_label.configure(
                text="• No existing plugin installed",
                style='Status.TLabel'
            )
        self.plugin_status_label.configure(background=self.COLORS['bg_secondary'])
        
        # Enable install button if compatible
        if is_compatible and CompatibilityChecker.is_compatible_macos():
            self.install_btn.configure(state='normal')
        else:
            self.install_btn.configure(state='disabled')
    
    def on_cancel(self):
        """Handle cancel button click."""
        if self.installation_running:
            if messagebox.askyesno("Cancel Installation", 
                                   "Installation is in progress. Are you sure you want to cancel?"):
                self.root.quit()
        else:
            self.root.quit()
    
    def on_install(self):
        """Handle install button click."""
        # Check if Qt Creator is running
        if self.is_qt_creator_running():
            result = messagebox.askyesno(
                "Qt Creator Running",
                "Qt Creator is currently running.\n\n"
                "The installer needs to quit Qt Creator to install the plugin.\n\n"
                "Would you like to quit Qt Creator and continue?"
            )
            if result:
                self.quit_qt_creator()
                time.sleep(2)  # Wait for Qt Creator to quit
            else:
                return
        
        # Start installation in background thread
        self.installation_running = True
        self.install_btn.configure(state='disabled')
        self.cancel_btn.configure(text="Close")
        
        # Show progress section
        self.progress_frame.pack(fill='x', pady=(0, 20), ipadx=15, ipady=10)
        
        # Run installation in thread
        thread = threading.Thread(target=self.run_installation)
        thread.daemon = True
        thread.start()
    
    def is_qt_creator_running(self):
        """Check if Qt Creator is running."""
        try:
            result = subprocess.run(['pgrep', '-f', 'Qt Creator'],
                                   capture_output=True, text=True)
            return result.returncode == 0
        except:
            return False
    
    def quit_qt_creator(self):
        """Attempt to quit Qt Creator."""
        try:
            subprocess.run(['pkill', '-f', 'Qt Creator'], check=False)
        except:
            pass
    
    def run_installation(self):
        """Run the installation process."""
        try:
            qt_path = self.qt_creator_path.get()
            
            # Find plugin source directory
            plugin_source = self.find_plugin_source()
            if not plugin_source:
                self.update_status("❌ Plugin files not found", error=True)
                return
            
            installer = PluginInstaller(qt_path, plugin_source)
            
            # Step 1: Remove existing plugin
            self.update_status("Removing existing plugin...")
            self.update_progress(0.1)
            
            try:
                removed = installer.remove_existing_plugin(
                    lambda msg, prog: self.update_step(msg)
                )
                if removed > 0:
                    self.update_step(f"Removed {removed} existing files")
            except RuntimeError as e:
                self.update_status(f"❌ {str(e)}", error=True)
                return
            
            # Step 2: Install new plugin
            self.update_status("Installing plugin files...")
            self.update_progress(0.4)
            
            try:
                installed = installer.install_plugin(
                    lambda msg, prog: (self.update_step(msg), self.update_progress(0.4 + prog * 0.4))
                )
            except RuntimeError as e:
                self.update_status(f"❌ {str(e)}", error=True)
                return
            
            # Step 3: Verify installation
            self.update_status("Verifying installation...")
            self.update_progress(0.9)
            
            success, message = installer.verify_installation()
            
            if success:
                self.update_progress(1.0)
                self.update_status("✅ Installation complete!")
                self.update_step(f"Installed {installed} files successfully")
                
                # Show completion message
                self.root.after(500, lambda: messagebox.showinfo(
                    "Installation Complete",
                    f"The {PLUGIN_NAME} has been installed successfully!\n\n"
                    f"Location: {qt_path}\n\n"
                    "Please restart Qt Creator to load the plugin."
                ))
            else:
                self.update_status(f"❌ Verification failed: {message}", error=True)
            
        except Exception as e:
            self.update_status(f"❌ Installation failed: {str(e)}", error=True)
        finally:
            self.installation_running = False
    
    def find_plugin_source(self):
        """Find the plugin source directory."""
        # Check various possible locations
        possible_paths = [
            # Same directory as installer
            os.path.dirname(os.path.abspath(__file__)),
            # Parent directory of installer
            os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            # Build directories
            os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 
                        'build_darwin', 'Qt Creator.app', 'Contents', 'PlugIns', 'qtcreator'),
            # Bundle resources (for packaged app)
            os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Resources'),
        ]
        
        # Add the embedded plugin directory if this is a bundled app
        if hasattr(sys, '_MEIPASS'):  # PyInstaller bundle
            possible_paths.insert(0, os.path.join(sys._MEIPASS, 'plugins'))
        
        # Check for embedded plugins in app bundle
        if sys.executable:
            exe_dir = os.path.dirname(sys.executable)
            # Check if we're in a macOS app bundle
            if exe_dir.endswith('MacOS'):
                resources_dir = os.path.join(os.path.dirname(exe_dir), 'Resources', 'plugins')
                possible_paths.insert(0, resources_dir)
        
        for path in possible_paths:
            if os.path.isdir(path):
                dylib_files = glob.glob(os.path.join(path, PLUGIN_DYLIB_PATTERN))
                if dylib_files:
                    return path
        
        return None
    
    def update_status(self, text, error=False):
        """Update the status label."""
        self.root.after(0, lambda: self._update_status(text, error))
    
    def _update_status(self, text, error):
        """Internal status update on main thread."""
        self.progress_label.configure(
            text=text,
            style='Error.TLabel' if error else 'Status.TLabel'
        )
        self.progress_label.configure(background=self.COLORS['bg_secondary'])
    
    def update_progress(self, value):
        """Update the progress bar."""
        self.root.after(0, lambda: self.progress_bar.configure(value=value * 100))
    
    def update_step(self, text):
        """Update the step detail label."""
        self.root.after(0, lambda: self.step_label.configure(text=f"  {text}"))
    
    def run(self):
        """Run the application."""
        # Check basic compatibility before running
        if not CompatibilityChecker.is_macos():
            messagebox.showerror(
                "Unsupported Platform",
                "This installer only supports macOS.\n\n"
                "Please use the appropriate installer for your platform."
            )
            return
        
        self.root.mainloop()


# =============================================================================
# Entry Point
# =============================================================================

def main():
    """Main entry point."""
    app = InstallerApp()
    app.run()


if __name__ == "__main__":
    main()

