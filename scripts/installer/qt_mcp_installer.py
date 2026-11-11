#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Qt MCP Plugin Installer

Standalone GUI installer for the Qt MCP Plugin into an existing Qt Creator
installation. Works on Windows and macOS.
"""

from __future__ import annotations

import glob
import os
import platform
import shutil
import subprocess
import sys
import threading
import time

try:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
except ImportError:
    print("Error: tkinter is required.")
    sys.exit(1)

try:
    import plistlib
except ImportError:
    plistlib = None


APP_NAME = "Qt MCP Plugin Installer"
PLUGIN_NAME = "Qt MCP Plugin"
MIN_MACOS_VERSION = "10.15"
MIN_QT_CREATOR_VERSION = "14.0.0"

PLUGIN_JSON_FILE = "Qt_MCP_Plugin.json"
PLUGIN_DISCOVERY_FILE = "Qt_MCP_Plugin_discovery.json"

IS_WINDOWS = platform.system().lower() == "windows"
IS_MACOS = platform.system().lower() == "darwin"

if IS_WINDOWS:
    PLUGIN_BINARY_PATTERN = "Qt_MCP_Plugin*.dll"
    PLUGIN_BINARY_PRIMARY = "Qt_MCP_Plugin.dll"
else:
    PLUGIN_BINARY_PATTERN = "libQt_MCP_Plugin*.dylib"
    PLUGIN_BINARY_PRIMARY = "libQt_MCP_Plugin.1.dylib"


class CompatibilityChecker:
    @staticmethod
    def get_macos_version():
        try:
            version = platform.mac_ver()[0]
            if version:
                parts = version.split(".")
                return tuple(int(p) for p in parts[:3] if p.isdigit())
        except Exception:
            pass
        return (0, 0, 0)

    @staticmethod
    def is_macos():
        return IS_MACOS

    @staticmethod
    def is_windows():
        return IS_WINDOWS

    @staticmethod
    def is_supported_platform():
        return IS_WINDOWS or IS_MACOS

    @staticmethod
    def is_compatible_macos():
        if not IS_MACOS:
            return True
        version = CompatibilityChecker.get_macos_version()
        min_version = tuple(int(p) for p in MIN_MACOS_VERSION.split("."))
        return version >= min_version

    @staticmethod
    def get_qt_creator_version(app_path):
        if IS_MACOS and plistlib is not None:
            try:
                info_plist = os.path.join(app_path, "Contents", "Info.plist")
                if os.path.exists(info_plist):
                    with open(info_plist, "rb") as f:
                        plist = plistlib.load(f)
                        return plist.get("CFBundleShortVersionString", "") or None
            except Exception:
                pass

        if IS_WINDOWS:
            candidates = [
                os.path.join(app_path, "bin", "qtdiag.exe"),
                os.path.join(app_path, "bin", "qtcreator.exe"),
            ]
            for exe in candidates:
                if not os.path.isfile(exe):
                    continue
                try:
                    result = subprocess.run(
                        [exe, "--version"] if exe.lower().endswith("qtcreator.exe") else [exe],
                        capture_output=True,
                        text=True,
                        timeout=8,
                        check=False,
                    )
                    text = (result.stdout or "") + "\n" + (result.stderr or "")
                    for line in text.splitlines():
                        if "Qt Creator" in line:
                            parts = line.replace("Qt Creator", "").strip().split()
                            if parts and parts[0][0].isdigit():
                                return parts[0]
                except Exception:
                    continue
        return None

    @staticmethod
    def is_compatible_qt_creator(version_str):
        if not version_str:
            return IS_WINDOWS
        try:
            parts = version_str.split(".")
            version = tuple(int(p) for p in parts[:3] if p.isdigit())
            min_parts = MIN_QT_CREATOR_VERSION.split(".")
            min_version = tuple(int(p) for p in min_parts[:3] if p.isdigit())
            return version >= min_version
        except Exception:
            return IS_WINDOWS

    @staticmethod
    def get_architecture():
        machine = platform.machine().lower()
        if machine in ["arm64", "aarch64"]:
            return "ARM64" if IS_WINDOWS else "Apple Silicon (arm64)"
        if machine in ["x86_64", "amd64"]:
            return "x64" if IS_WINDOWS else "Intel (x86_64)"
        return machine


class QtCreatorFinder:
    @classmethod
    def _windows_search_roots(cls):
        home = os.path.expanduser("~")
        roots = [
            os.path.join(home, "Developer", "Qt", "Tools", "QtCreator"),
            os.path.join(home, "Qt", "Tools", "QtCreator"),
            r"C:\Qt\Tools\QtCreator",
            os.path.join(os.environ.get("LOCALAPPDATA", ""), "Programs", "Qt Creator"),
            os.path.join(os.environ.get("ProgramFiles", r"C:\Program Files"), "Qt Creator"),
        ]
        for base in [os.path.join(home, "Developer", "Qt"), r"C:\Qt"]:
            tools = os.path.join(base, "Tools", "QtCreator")
            if tools not in roots:
                roots.append(tools)
        return [p for p in roots if p]

    @classmethod
    def find_all(cls):
        found = []
        if IS_WINDOWS:
            for path in cls._windows_search_roots():
                if os.path.isdir(path) and cls._is_valid_qt_creator(path) and path not in found:
                    found.append(path)
            for base in [os.path.expanduser(r"~\Developer\Qt"), r"C:\Qt"]:
                if not os.path.isdir(base):
                    continue
                try:
                    for entry in os.listdir(base):
                        candidate = os.path.join(base, entry, "Tools", "QtCreator")
                        if (
                            os.path.isdir(candidate)
                            and cls._is_valid_qt_creator(candidate)
                            and candidate not in found
                        ):
                            found.append(candidate)
                except OSError:
                    pass
            return found

        search_paths = [
            "/Applications/Qt Creator.app",
            os.path.expanduser("~/Applications/Qt Creator.app"),
            os.path.expanduser("~/Developer/Qt/Qt Creator.app"),
            "/opt/Qt/Qt Creator.app",
            "/usr/local/Qt/Qt Creator.app",
        ]
        for path in search_paths:
            if os.path.isdir(path) and cls._is_valid_qt_creator(path) and path not in found:
                found.append(path)

        for base in [os.path.expanduser("~/Developer/Qt"), "/Applications"]:
            if not os.path.isdir(base):
                continue
            for entry in os.listdir(base):
                if "Qt Creator" in entry and entry.endswith(".app"):
                    full_path = os.path.join(base, entry)
                    if full_path not in found and cls._is_valid_qt_creator(full_path):
                        found.append(full_path)
        return found

    @classmethod
    def _is_valid_qt_creator(cls, path):
        if IS_WINDOWS:
            binary = os.path.join(path, "bin", "qtcreator.exe")
            plugin_dir = os.path.join(path, "lib", "qtcreator", "plugins")
            return os.path.isfile(binary) and os.path.isdir(plugin_dir)
        binary_path = os.path.join(path, "Contents", "MacOS", "Qt Creator")
        plugin_dir = os.path.join(path, "Contents", "PlugIns", "qtcreator")
        return os.path.isfile(binary_path) and os.path.isdir(plugin_dir)

    @classmethod
    def get_plugin_dir(cls, app_path):
        if IS_WINDOWS:
            return os.path.join(app_path, "lib", "qtcreator", "plugins")
        return os.path.join(app_path, "Contents", "PlugIns", "qtcreator")


class PluginInstaller:
    def __init__(self, qt_creator_path, plugin_source_dir):
        self.qt_creator_path = qt_creator_path
        self.plugin_source_dir = plugin_source_dir
        self.plugin_dir = QtCreatorFinder.get_plugin_dir(qt_creator_path)

    def get_existing_plugin_files(self):
        files = []
        if os.path.isdir(self.plugin_dir):
            files.extend(glob.glob(os.path.join(self.plugin_dir, PLUGIN_BINARY_PATTERN)))
            if IS_WINDOWS:
                for extra in ["Qt_MCP_Plugin.lib", "Qt_MCP_Plugin.exp", "Qt_MCP_Plugin.pdb"]:
                    path = os.path.join(self.plugin_dir, extra)
                    if os.path.exists(path):
                        files.append(path)
            for filename in [PLUGIN_JSON_FILE, PLUGIN_DISCOVERY_FILE]:
                path = os.path.join(self.plugin_dir, filename)
                if os.path.exists(path):
                    files.append(path)
        return files

    def remove_existing_plugin(self, progress_callback=None):
        files = self.get_existing_plugin_files()
        total = len(files)
        for i, filepath in enumerate(files):
            try:
                os.remove(filepath)
                if progress_callback:
                    progress_callback(
                        f"Removed: {os.path.basename(filepath)}",
                        (i + 1) / max(total, 1),
                    )
            except OSError as e:
                raise RuntimeError(f"Failed to remove {filepath}: {e}") from e
        return len(files)

    def get_source_plugin_files(self):
        files = []
        if not os.path.isdir(self.plugin_source_dir):
            return files
        primary = os.path.join(self.plugin_source_dir, PLUGIN_BINARY_PRIMARY)
        if os.path.isfile(primary):
            files.append(primary)
        else:
            files.extend(
                [
                    p
                    for p in glob.glob(os.path.join(self.plugin_source_dir, PLUGIN_BINARY_PATTERN))
                    if not os.path.islink(p)
                ]
            )
        for filename in [PLUGIN_JSON_FILE, PLUGIN_DISCOVERY_FILE]:
            path = os.path.join(self.plugin_source_dir, filename)
            if os.path.exists(path):
                files.append(path)
        return files

    def install_plugin(self, progress_callback=None):
        source_files = self.get_source_plugin_files()
        if not source_files:
            raise RuntimeError("No plugin files found to install")
        os.makedirs(self.plugin_dir, exist_ok=True)
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
                for installed_file in installed:
                    try:
                        os.remove(installed_file)
                    except OSError:
                        pass
                raise RuntimeError(f"Failed to install {filename}: {e}") from e
        return len(installed)

    def verify_installation(self):
        binaries = glob.glob(os.path.join(self.plugin_dir, PLUGIN_BINARY_PATTERN))
        if not binaries:
            return False, f"Plugin binary not found ({PLUGIN_BINARY_PATTERN})"
        json_path = os.path.join(self.plugin_dir, PLUGIN_JSON_FILE)
        if not os.path.exists(json_path):
            return False, "Plugin JSON file not found"
        return True, "Installation verified"


class InstallerApp:
    COLORS = {
        "bg": "#1e1e2e",
        "bg_secondary": "#2a2a3c",
        "fg": "#cdd6f4",
        "fg_secondary": "#9399b2",
        "accent": "#89b4fa",
        "accent_hover": "#b4befe",
        "success": "#a6e3a1",
        "error": "#f38ba8",
        "warning": "#fab387",
        "border": "#45475a",
        "progress_bg": "#313244",
    }

    def __init__(self):
        self.root = tk.Tk()
        self.root.title(APP_NAME)
        self.root.geometry("720x560")
        self.root.resizable(False, False)
        self.root.configure(bg=self.COLORS["bg"])
        self.center_window()
        self.setup_styles()
        self.qt_creator_path = tk.StringVar()
        self.found_installations = []
        self.installation_running = False
        self.create_ui()
        self.scan_for_qt_creator()

    def center_window(self):
        self.root.update_idletasks()
        width = self.root.winfo_width()
        height = self.root.winfo_height()
        x = (self.root.winfo_screenwidth() // 2) - (width // 2)
        y = (self.root.winfo_screenheight() // 2) - (height // 2)
        self.root.geometry(f"+{x}+{y}")

    def setup_styles(self):
        style = ttk.Style()
        try:
            style.theme_use("clam")
        except Exception:
            pass
        font_ui = ("Segoe UI", 10) if IS_WINDOWS else ("Helvetica", 11)
        font_title = ("Segoe UI", 18, "bold") if IS_WINDOWS else ("Helvetica", 18, "bold")
        style.configure("TFrame", background=self.COLORS["bg"])
        style.configure("Card.TFrame", background=self.COLORS["bg_secondary"])
        style.configure(
            "Title.TLabel", background=self.COLORS["bg"], foreground=self.COLORS["fg"], font=font_title
        )
        style.configure(
            "Subtitle.TLabel",
            background=self.COLORS["bg"],
            foreground=self.COLORS["fg_secondary"],
            font=font_ui,
        )
        style.configure(
            "Status.TLabel",
            background=self.COLORS["bg_secondary"],
            foreground=self.COLORS["fg"],
            font=font_ui,
        )
        style.configure(
            "Success.TLabel",
            background=self.COLORS["bg_secondary"],
            foreground=self.COLORS["success"],
            font=font_ui,
        )
        style.configure(
            "Error.TLabel",
            background=self.COLORS["bg_secondary"],
            foreground=self.COLORS["error"],
            font=font_ui,
        )
        style.configure(
            "Horizontal.TProgressbar",
            troughcolor=self.COLORS["progress_bg"],
            background=self.COLORS["accent"],
            bordercolor=self.COLORS["border"],
            lightcolor=self.COLORS["accent"],
            darkcolor=self.COLORS["accent"],
        )

    def create_ui(self):
        main = ttk.Frame(self.root, style="TFrame", padding=24)
        main.pack(fill="both", expand=True)
        ttk.Label(main, text=APP_NAME, style="Title.TLabel").pack(anchor="w")
        ttk.Label(
            main,
            text="Install the Qt MCP Plugin into an existing Qt Creator installation.",
            style="Subtitle.TLabel",
        ).pack(anchor="w", pady=(4, 20))

        card = ttk.Frame(main, style="Card.TFrame", padding=16)
        card.pack(fill="x", pady=(0, 16))
        ttk.Label(card, text="Qt Creator location", style="Status.TLabel").pack(anchor="w")
        path_row = ttk.Frame(card, style="Card.TFrame")
        path_row.pack(fill="x", pady=(8, 8))
        self.qt_combo = ttk.Combobox(path_row, textvariable=self.qt_creator_path, state="readonly", width=70)
        self.qt_combo.pack(side="left", fill="x", expand=True)
        self.qt_combo.bind("<<ComboboxSelected>>", self.on_qt_creator_selected)
        browse_btn = tk.Button(
            path_row,
            text="Browse...",
            command=self.on_browse,
            bg=self.COLORS["bg"],
            fg=self.COLORS["fg"],
            activebackground=self.COLORS["border"],
            activeforeground=self.COLORS["fg"],
            bd=0,
            padx=12,
            pady=4,
        )
        browse_btn.pack(side="left", padx=(8, 0))
        self.version_label = ttk.Label(card, text="", style="Status.TLabel")
        self.version_label.pack(anchor="w", pady=(4, 0))
        self.plugin_status_label = ttk.Label(card, text="", style="Status.TLabel")
        self.plugin_status_label.pack(anchor="w", pady=(4, 0))

        info = ttk.Frame(main, style="Card.TFrame", padding=16)
        info.pack(fill="x", pady=(0, 16))
        ttk.Label(
            info,
            text=(
                f"Platform: {platform.system()} ({CompatibilityChecker.get_architecture()})\n"
                "Removes any existing Qt MCP Plugin, then copies the bundled plugin into "
                "Qt Creator's plugins folder."
            ),
            style="Status.TLabel",
            justify="left",
        ).pack(anchor="w")

        self.progress_frame = ttk.Frame(main, style="Card.TFrame", padding=16)
        self.progress_label = ttk.Label(self.progress_frame, text="", style="Status.TLabel")
        self.progress_label.pack(anchor="w")
        self.progress_bar = ttk.Progressbar(
            self.progress_frame, orient="horizontal", mode="determinate", length=640
        )
        self.progress_bar.pack(fill="x", pady=(8, 4))
        self.step_label = ttk.Label(self.progress_frame, text="", style="Status.TLabel")
        self.step_label.pack(anchor="w")

        button_row = ttk.Frame(main, style="TFrame")
        button_row.pack(fill="x", side="bottom")
        self.cancel_btn = tk.Button(
            button_row,
            text="Cancel",
            command=self.on_cancel,
            bg=self.COLORS["bg_secondary"],
            fg=self.COLORS["fg"],
            activebackground=self.COLORS["border"],
            activeforeground=self.COLORS["fg"],
            bd=0,
            padx=24,
            pady=10,
        )
        self.cancel_btn.pack(side="left")
        self.install_btn = tk.Button(
            button_row,
            text="Install Plugin",
            command=self.on_install,
            bg=self.COLORS["accent"],
            fg=self.COLORS["bg"],
            activebackground=self.COLORS["accent_hover"],
            activeforeground=self.COLORS["bg"],
            bd=0,
            padx=30,
            pady=10,
        )
        self.install_btn.pack(side="right")
        self.button_row = button_row

    def scan_for_qt_creator(self):
        self.found_installations = QtCreatorFinder.find_all()
        if self.found_installations:
            self.qt_combo.configure(state="readonly")
            self.qt_combo["values"] = self.found_installations
            self.qt_combo.current(0)
            self.on_qt_creator_selected(None)
        else:
            self.qt_combo["values"] = []
            self.qt_creator_path.set("")
            self.version_label.configure(text="No Qt Creator installation found - use Browse...")
            self.plugin_status_label.configure(text="")
            self.install_btn.configure(state="disabled")

    def on_browse(self):
        if IS_WINDOWS:
            path = filedialog.askdirectory(
                title="Select Qt Creator folder (contains bin\\qtcreator.exe)"
            )
        else:
            path = filedialog.askdirectory(title="Select Qt Creator.app")
        if not path:
            return
        if not QtCreatorFinder._is_valid_qt_creator(path):
            messagebox.showerror(
                "Invalid Qt Creator",
                "That folder does not look like a Qt Creator installation.\n\n"
                + (
                    "Expected bin\\qtcreator.exe and lib\\qtcreator\\plugins"
                    if IS_WINDOWS
                    else "Expected a Qt Creator.app bundle with PlugIns/qtcreator"
                ),
            )
            return
        values = list(self.qt_combo["values"]) if self.qt_combo["values"] else []
        if path not in values:
            values.insert(0, path)
            self.qt_combo["values"] = values
        self.qt_combo.configure(state="readonly")
        self.qt_creator_path.set(path)
        self.on_qt_creator_selected(None)

    def on_qt_creator_selected(self, _event):
        path = self.qt_creator_path.get()
        if not path:
            return
        version = CompatibilityChecker.get_qt_creator_version(path)
        is_compatible = CompatibilityChecker.is_compatible_qt_creator(version)
        if version:
            status = "OK" if is_compatible else "Unsupported"
            self.version_label.configure(
                text=f"{status}: Qt Creator {version}",
                style="Success.TLabel" if is_compatible else "Error.TLabel",
            )
        else:
            self.version_label.configure(
                text="Version unknown (install still allowed on Windows)"
                if IS_WINDOWS
                else "Version unknown",
                style="Status.TLabel",
            )
        self.version_label.configure(background=self.COLORS["bg_secondary"])
        plugin_dir = QtCreatorFinder.get_plugin_dir(path)
        existing_files = glob.glob(os.path.join(plugin_dir, PLUGIN_BINARY_PATTERN))
        if existing_files:
            self.plugin_status_label.configure(
                text="Existing plugin found (will be replaced)", style="Status.TLabel"
            )
        else:
            self.plugin_status_label.configure(
                text="No existing plugin installed", style="Status.TLabel"
            )
        self.plugin_status_label.configure(background=self.COLORS["bg_secondary"])
        ok = is_compatible and CompatibilityChecker.is_compatible_macos()
        self.install_btn.configure(state="normal" if ok else "disabled")

    def on_cancel(self):
        if self.installation_running:
            if messagebox.askyesno(
                "Cancel Installation",
                "Installation is in progress. Are you sure you want to cancel?",
            ):
                self.root.quit()
        else:
            self.root.quit()

    def on_install(self):
        if self.is_qt_creator_running():
            result = messagebox.askyesno(
                "Qt Creator Running",
                "Qt Creator is currently running.\n\n"
                "The installer needs to quit Qt Creator to install the plugin.\n\n"
                "Would you like to quit Qt Creator and continue?",
            )
            if result:
                self.quit_qt_creator()
                time.sleep(2)
            else:
                return
        self.installation_running = True
        self.install_btn.configure(state="disabled")
        self.cancel_btn.configure(text="Close")
        self.progress_frame.pack(fill="x", pady=(0, 20), before=self.button_row)
        threading.Thread(target=self.run_installation, daemon=True).start()

    def is_qt_creator_running(self):
        try:
            if IS_WINDOWS:
                result = subprocess.run(
                    ["tasklist", "/FI", "IMAGENAME eq qtcreator.exe"],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                return "qtcreator.exe" in (result.stdout or "").lower()
            result = subprocess.run(
                ["pgrep", "-f", "Qt Creator"], capture_output=True, text=True, check=False
            )
            return result.returncode == 0
        except Exception:
            return False

    def quit_qt_creator(self):
        try:
            if IS_WINDOWS:
                subprocess.run(
                    ["taskkill", "/F", "/IM", "qtcreator.exe"], capture_output=True, check=False
                )
            else:
                subprocess.run(["pkill", "-f", "Qt Creator"], check=False)
        except Exception:
            pass

    def run_installation(self):
        try:
            qt_path = self.qt_creator_path.get()
            plugin_source = self.find_plugin_source()
            if not plugin_source:
                self.update_status("Plugin files not found in installer package", error=True)
                return
            installer = PluginInstaller(qt_path, plugin_source)
            self.update_status("Removing existing plugin...")
            self.update_progress(0.1)
            try:
                removed = installer.remove_existing_plugin(lambda msg, _prog: self.update_step(msg))
                if removed > 0:
                    self.update_step(f"Removed {removed} existing files")
            except RuntimeError as e:
                self.update_status(str(e), error=True)
                return
            self.update_status("Installing plugin files...")
            self.update_progress(0.4)
            try:
                installed = installer.install_plugin(
                    lambda msg, prog: (
                        self.update_step(msg),
                        self.update_progress(0.4 + prog * 0.4),
                    )
                )
            except RuntimeError as e:
                self.update_status(str(e), error=True)
                return
            self.update_status("Verifying installation...")
            self.update_progress(0.9)
            success, message = installer.verify_installation()
            if success:
                self.update_progress(1.0)
                self.update_status("Installation complete!")
                self.update_step(f"Installed {installed} files successfully")
                dest = QtCreatorFinder.get_plugin_dir(qt_path)
                self.root.after(
                    500,
                    lambda: messagebox.showinfo(
                        "Installation Complete",
                        f"The {PLUGIN_NAME} has been installed successfully!\n\n"
                        f"Location: {dest}\n\n"
                        "Please restart Qt Creator to load the plugin.",
                    ),
                )
            else:
                self.update_status(f"Verification failed: {message}", error=True)
        except Exception as e:
            self.update_status(f"Installation failed: {e}", error=True)
        finally:
            self.installation_running = False

    def find_plugin_source(self):
        possible_paths = []
        if hasattr(sys, "_MEIPASS"):
            possible_paths.append(os.path.join(sys._MEIPASS, "plugins"))
        exe_dir = os.path.dirname(os.path.abspath(sys.executable))
        possible_paths.extend(
            [os.path.join(exe_dir, "plugins"), os.path.join(exe_dir, "_internal", "plugins")]
        )
        if exe_dir.endswith("MacOS"):
            possible_paths.insert(
                0, os.path.join(os.path.dirname(exe_dir), "Resources", "plugins")
            )
        here = os.path.dirname(os.path.abspath(__file__))
        repo_root = os.path.dirname(os.path.dirname(here))
        possible_paths.extend(
            [
                here,
                os.path.join(here, "plugins"),
                os.path.join(repo_root, "build_windows", "lib", "qtcreator", "plugins", "Release"),
                os.path.join(repo_root, "build", "lib", "qtcreator", "plugins", "Release"),
                os.path.join(
                    repo_root,
                    "build_darwin",
                    "Qt Creator.app",
                    "Contents",
                    "PlugIns",
                    "qtcreator",
                ),
            ]
        )
        for path in possible_paths:
            if not os.path.isdir(path):
                continue
            binaries = glob.glob(os.path.join(path, PLUGIN_BINARY_PATTERN))
            if binaries:
                return path
        return None

    def update_status(self, text, error=False):
        self.root.after(0, lambda: self._update_status(text, error))

    def _update_status(self, text, error):
        self.progress_label.configure(
            text=text, style="Error.TLabel" if error else "Status.TLabel"
        )
        self.progress_label.configure(background=self.COLORS["bg_secondary"])

    def update_progress(self, value):
        self.root.after(0, lambda: self.progress_bar.configure(value=value * 100))

    def update_step(self, text):
        self.root.after(0, lambda: self.step_label.configure(text=f"  {text}"))

    def run(self):
        if not CompatibilityChecker.is_supported_platform():
            messagebox.showerror(
                "Unsupported Platform", "This installer supports Windows and macOS only."
            )
            return
        if IS_MACOS and not CompatibilityChecker.is_compatible_macos():
            messagebox.showerror(
                "Unsupported macOS", f"macOS {MIN_MACOS_VERSION} or later is required."
            )
            return
        self.root.mainloop()


def main():
    app = InstallerApp()
    app.run()


if __name__ == "__main__":
    main()
