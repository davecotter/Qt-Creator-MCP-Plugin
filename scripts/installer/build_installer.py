#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Qt MCP Plugin Installer - Build Script

macOS: builds a native Swift/AppKit .app bundle
Windows: builds a portable PyInstaller .exe with embedded plugin files
"""

from __future__ import annotations

import glob
import os
import platform
import shutil
import subprocess
import sys

_build_scripts = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build"
)
if _build_scripts not in sys.path:
    sys.path.insert(0, _build_scripts)
import qt_config  # noqa: E402

INSTALLER_NAME = "Qt MCP Plugin Installer"
APP_BUNDLE_NAME = "Qt MCP Plugin Installer.app"
WIN_EXE_NAME = "Qt MCP Plugin Installer.exe"
PLUGIN_DYLIB_PATTERN = "libQt_MCP_Plugin*.dylib"
PLUGIN_DLL_PATTERN = "Qt_MCP_Plugin*.dll"
PLUGIN_FILES = ["Qt_MCP_Plugin.json", "Qt_MCP_Plugin_discovery.json"]


def print_step(msg):
    print(f"[INSTALLER] {msg}")


def print_success(msg):
    print(f"[OK] {msg}")


def print_error(msg):
    print(f"[ERROR] {msg}")


def get_project_root():
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def get_build_plugin_dir():
    system = platform.system().lower()
    candidates = []
    try:
        candidates.append(qt_config.get_build_plugin_dir())
    except Exception:
        pass

    root = get_project_root()
    if system == "windows":
        candidates.extend(
            [
                os.path.join(root, "build_windows", "lib", "qtcreator", "plugins", "Release"),
                os.path.join(root, "build", "lib", "qtcreator", "plugins", "Release"),
                os.path.join(root, "build_windows", "lib", "qtcreator", "plugins"),
                os.path.join(root, "build", "lib", "qtcreator", "plugins"),
            ]
        )
        pattern = PLUGIN_DLL_PATTERN
    else:
        candidates.extend(
            [
                os.path.join(
                    root,
                    "build_darwin",
                    "Qt Creator.app",
                    "Contents",
                    "PlugIns",
                    "qtcreator",
                ),
                os.path.join(root, "build_darwin", "lib", "qtcreator", "plugins"),
            ]
        )
        pattern = PLUGIN_DYLIB_PATTERN

    for plugin_dir in candidates:
        if os.path.isdir(plugin_dir) and glob.glob(os.path.join(plugin_dir, pattern)):
            return plugin_dir
    return None


def collect_plugin_payload(dest_plugins_dir):
    """Copy plugin binary + JSON into dest_plugins_dir. Returns True on success."""
    plugin_source = get_build_plugin_dir()
    if not plugin_source:
        print_error("Could not find built plugin files!")
        print_error("Please run 'python3 scripts/build/build_main.py' first.")
        return False

    print_step(f"Found plugin files in: {plugin_source}")
    os.makedirs(dest_plugins_dir, exist_ok=True)

    system = platform.system().lower()
    pattern = PLUGIN_DLL_PATTERN if system == "windows" else PLUGIN_DYLIB_PATTERN
    binaries = [
        p for p in glob.glob(os.path.join(plugin_source, pattern)) if not os.path.islink(p)
    ]
    if not binaries:
        print_error(f"No plugin binary matching {pattern} found")
        return False

    # Prefer the primary runtime name.
    preferred_names = (
        ["Qt_MCP_Plugin.dll"] if system == "windows" else ["libQt_MCP_Plugin.1.dylib"]
    )
    chosen = None
    for name in preferred_names:
        for bin_path in binaries:
            if os.path.basename(bin_path) == name:
                chosen = bin_path
                break
        if chosen:
            break
    if not chosen:
        chosen = binaries[0]

    print_step(f"Copying {os.path.basename(chosen)}...")
    shutil.copy2(chosen, os.path.join(dest_plugins_dir, os.path.basename(chosen)))

    build_dir = qt_config.get_build_dir()
    for json_file in PLUGIN_FILES:
        found = False
        for src_path in [
            os.path.join(plugin_source, json_file),
            os.path.join(build_dir, json_file),
            os.path.join(get_project_root(), "build_windows", json_file),
            os.path.join(get_project_root(), "build", json_file),
            os.path.join(get_project_root(), "build_darwin", json_file),
        ]:
            if os.path.exists(src_path):
                print_step(f"Copying {json_file}...")
                shutil.copy2(src_path, os.path.join(dest_plugins_dir, json_file))
                found = True
                break
        if not found:
            print_error(f"Missing required file: {json_file}")
            return False

    print_success("Plugin payload collected")
    return True


# ---------------------------------------------------------------------------
# macOS (Swift) path — existing behavior
# ---------------------------------------------------------------------------

def generate_app_icon():
    installer_dir = os.path.dirname(os.path.abspath(__file__))
    icon_path = os.path.join(installer_dir, "installer_icon.icns")
    if os.path.exists(icon_path):
        return icon_path

    project_root = get_project_root()
    png_icon = os.path.join(project_root, "src", "resources", "mcp.png")
    if not os.path.exists(png_icon):
        return None

    try:
        iconset_dir = os.path.join(installer_dir, "installer_icon.iconset")
        os.makedirs(iconset_dir, exist_ok=True)
        for size in [16, 32, 64, 128, 256, 512]:
            subprocess.run(
                [
                    "sips",
                    "-z",
                    str(size),
                    str(size),
                    png_icon,
                    "--out",
                    os.path.join(iconset_dir, f"icon_{size}x{size}.png"),
                ],
                capture_output=True,
                check=True,
            )
            if size <= 512:
                subprocess.run(
                    [
                        "sips",
                        "-z",
                        str(size * 2),
                        str(size * 2),
                        png_icon,
                        "--out",
                        os.path.join(iconset_dir, f"icon_{size}x{size}@2x.png"),
                    ],
                    capture_output=True,
                    check=True,
                )
        subprocess.run(
            ["iconutil", "-c", "icns", iconset_dir, "-o", icon_path],
            capture_output=True,
            check=True,
        )
        shutil.rmtree(iconset_dir)
        print_success("Generated icon")
        return icon_path
    except Exception as e:
        print_error(f"Icon generation error: {e}")
        return None


def compile_swift_app():
    import plistlib

    installer_dir = os.path.dirname(os.path.abspath(__file__))
    swift_file = os.path.join(installer_dir, "QtMCPInstaller.swift")
    if not os.path.exists(swift_file):
        print_error(f"Swift source not found: {swift_file}")
        return None

    dist_dir = os.path.join(installer_dir, "dist")
    app_path = os.path.join(dist_dir, APP_BUNDLE_NAME)
    macos_dir = os.path.join(app_path, "Contents", "MacOS")
    resources_dir = os.path.join(app_path, "Contents", "Resources")
    plugins_dir = os.path.join(resources_dir, "plugins")

    if os.path.exists(dist_dir):
        shutil.rmtree(dist_dir)
    os.makedirs(macos_dir, exist_ok=True)
    os.makedirs(plugins_dir, exist_ok=True)

    print_step("Compiling Swift installer...")
    executable_path = os.path.join(macos_dir, INSTALLER_NAME)
    result = subprocess.run(
        [
            "swiftc",
            "-O",
            "-whole-module-optimization",
            # Match Qt Creator's minos (13.0). Without an explicit target, Xcode
            # stamps the *current* OS (e.g. 27.2) and Finder grays the app out
            # on older macOS (e.g. 26.x).
            "-target",
            "arm64-apple-macos13.0",
            "-framework",
            "Cocoa",
            "-o",
            executable_path,
            swift_file,
        ],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print_error("Swift compilation failed:")
        print(result.stderr)
        return None
    print_success("Swift compilation successful")

    icon_path = generate_app_icon()
    if icon_path:
        shutil.copy2(icon_path, os.path.join(resources_dir, "installer_icon.icns"))

    plist_content = {
        "CFBundleName": INSTALLER_NAME,
        "CFBundleDisplayName": INSTALLER_NAME,
        "CFBundleIdentifier": "com.qtmcp.installer",
        "CFBundleVersion": "1.0.0",
        "CFBundleShortVersionString": "1.0.0",
        "CFBundleExecutable": INSTALLER_NAME,
        "CFBundlePackageType": "APPL",
        "CFBundleSignature": "????",
        "CFBundleIconFile": "installer_icon.icns" if icon_path else "",
        "NSHumanReadableCopyright": "Copyright 2024-2026 Qt MCP Plugin",
        "NSHighResolutionCapable": True,
        "NSRequiresAquaSystemAppearance": False,
        "LSMinimumSystemVersion": "13.0",
        "LSUIElement": False,
        "NSPrincipalClass": "NSApplication",
    }
    with open(os.path.join(app_path, "Contents", "Info.plist"), "wb") as f:
        plistlib.dump(plist_content, f)
    print_step("Created Info.plist")
    return app_path


def sign_app_bundle(app_path):
    print_step("Signing app bundle...")
    try:
        subprocess.run(["codesign", "--remove-signature", app_path], capture_output=True, check=False)
        result = subprocess.run(
            ["codesign", "--force", "--deep", "--sign", "-", app_path],
            capture_output=True,
            text=True,
        )
        if result.returncode == 0:
            print_success("App bundle signed")
            return True
        print_error(f"Signing failed: {result.stderr}")
        return False
    except Exception as e:
        print_error(f"Signing error: {e}")
        return False


def build_macos_installer():
    app_path = compile_swift_app()
    if not app_path:
        return None
    plugins_dir = os.path.join(app_path, "Contents", "Resources", "plugins")
    if not collect_plugin_payload(plugins_dir):
        return None
    sign_app_bundle(app_path)

    build_dir = qt_config.get_build_dir()
    os.makedirs(build_dir, exist_ok=True)
    final_path = os.path.join(build_dir, APP_BUNDLE_NAME)
    if os.path.exists(final_path):
        shutil.rmtree(final_path)
    shutil.move(app_path, final_path)

    dist_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dist")
    if os.path.isdir(dist_dir):
        shutil.rmtree(dist_dir)
    return final_path


# ---------------------------------------------------------------------------
# Windows (PyInstaller) path
# ---------------------------------------------------------------------------

def ensure_pyinstaller():
    try:
        import PyInstaller  # noqa: F401

        return True
    except ImportError:
        print_step("Installing PyInstaller...")
        result = subprocess.run(
            [sys.executable, "-m", "pip", "install", "pyinstaller"],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            print_error(result.stderr or result.stdout)
            return False
        return True


def make_windows_icon():
    """Create installer_icon.ico from mcp.png if possible."""
    installer_dir = os.path.dirname(os.path.abspath(__file__))
    ico_path = os.path.join(installer_dir, "installer_icon.ico")
    if os.path.exists(ico_path):
        return ico_path

    png_path = os.path.join(get_project_root(), "src", "resources", "mcp.png")
    if not os.path.exists(png_path):
        return None

    try:
        from PIL import Image

        img = Image.open(png_path).convert("RGBA")
        sizes = [(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
        img.save(ico_path, format="ICO", sizes=sizes)
        print_success(f"Generated {ico_path}")
        return ico_path
    except Exception as e:
        print_step(f"ICO generation skipped ({e})")
        return None


def build_windows_installer():
    if not ensure_pyinstaller():
        print_error("PyInstaller is required to build the Windows installer")
        return None

    installer_dir = os.path.dirname(os.path.abspath(__file__))
    script = os.path.join(installer_dir, "qt_mcp_installer.py")
    if not os.path.exists(script):
        print_error(f"Installer script not found: {script}")
        return None

    staging = os.path.join(installer_dir, "_win_staging")
    if os.path.exists(staging):
        shutil.rmtree(staging)
    plugins_dir = os.path.join(staging, "plugins")
    if not collect_plugin_payload(plugins_dir):
        return None

    work_dir = os.path.join(installer_dir, "_pyinstaller_work")
    dist_dir = os.path.join(installer_dir, "dist")
    for d in (work_dir, dist_dir):
        if os.path.exists(d):
            shutil.rmtree(d)

    icon = make_windows_icon()
    cmd = [
        sys.executable,
        "-m",
        "PyInstaller",
        "--noconfirm",
        "--clean",
        "--windowed",
        "--onefile",
        f"--name={INSTALLER_NAME}",
        f"--distpath={dist_dir}",
        f"--workpath={work_dir}",
        f"--specpath={work_dir}",
        f"--add-data={plugins_dir}{os.pathsep}plugins",
    ]
    if icon:
        cmd.append(f"--icon={icon}")
    cmd.append(script)

    print_step("Running PyInstaller (this may take a minute)...")
    print_step(" ".join(cmd))
    result = subprocess.run(cmd, cwd=installer_dir)
    if result.returncode != 0:
        print_error("PyInstaller failed")
        return None

    built_exe = os.path.join(dist_dir, WIN_EXE_NAME)
    if not os.path.isfile(built_exe):
        # Fallbacks for alternate naming
        matches = glob.glob(os.path.join(dist_dir, "*.exe"))
        if not matches:
            print_error(f"Expected installer exe not found: {built_exe}")
            return None
        built_exe = matches[0]

    build_dir = qt_config.get_build_dir()
    os.makedirs(build_dir, exist_ok=True)
    final_path = os.path.join(build_dir, WIN_EXE_NAME)
    if os.path.exists(final_path):
        os.remove(final_path)
    shutil.copy2(built_exe, final_path)
    print_success(f"Copied installer to {final_path}")

    # Cleanup staging/work
    for d in (staging, work_dir, dist_dir):
        if os.path.exists(d):
            shutil.rmtree(d, ignore_errors=True)

    return final_path


def build_installer():
    system = platform.system().lower()
    print("=" * 60)
    print("Qt MCP Plugin Installer - Build")
    print("=" * 60)
    print()

    if system == "darwin":
        final_path = build_macos_installer()
    elif system == "windows":
        final_path = build_windows_installer()
    else:
        print_error(f"Installer build is not supported on {system}")
        return False

    if not final_path:
        print_error("Installer build failed")
        return False

    print()
    print("=" * 60)
    print_success("Installer build complete!")
    print("=" * 60)
    print()
    print(f"Installer: {final_path}")
    print()
    if system == "windows":
        print("Copy this .exe to another Windows PC and run it to install")
        print("the Qt MCP Plugin into that machine's Qt Creator.")
    else:
        print("Double-click to install the Qt MCP Plugin into Qt Creator.")
    print()
    return True


def main():
    success = build_installer()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
