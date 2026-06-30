# Qt MCP Plugin Installer

A standalone native macOS application for installing the Qt MCP Plugin into Qt Creator.

## Features

- **Native macOS App**: Built with Swift/AppKit for maximum compatibility
- **Auto-detection**: Automatically finds Qt Creator installations on your system
- **Safe installation**: Removes existing plugin versions before installing
- **Progress indication**: Shows detailed progress during installation
- **Compatibility checking**: Verifies system and Qt Creator compatibility
- **Modern UI**: Native Cocoa interface with full dark mode support

## Building the Installer

The installer is automatically built as part of the main build process:

```bash
python3 scripts/build/build_main.py
```

Build artifacts are written to `build_darwin/` (macOS), `build_windows/`, or `build_linux/`.

To build the installer separately:

```bash
cd scripts/installer
python3 build_installer.py
```

## Requirements

- macOS 10.15 (Catalina) or later
- Xcode Command Line Tools (for Swift compiler)

## Files

- `QtMCPInstaller.swift` - Native Swift/AppKit installer application
- `build_installer.py` - Script to compile and bundle the .app
- `qt_mcp_installer.py` - Alternative Python/tkinter implementation (backup)
- `installer_icon.icns` - App icon (generated from mcp.png)

## How It Works

1. **Detection**: Scans standard locations for Qt Creator installations
2. **Compatibility Check**: Verifies macOS version and Qt Creator version
3. **Plugin Removal**: Safely removes any existing plugin files
4. **Installation**: Copies new plugin files to Qt Creator's plugin directory
5. **Verification**: Confirms all files were installed correctly

## Distribution

After building, the `Qt MCP Plugin Installer.app` can be:
- Zipped and shared directly
- Notarized and distributed (requires Apple Developer ID)
- Uploaded to a website for download

## Supported Qt Creator Locations

The installer searches for Qt Creator in:
- `/Applications/Qt Creator.app`
- `~/Applications/Qt Creator.app`
- `~/Developer/Qt/Qt Creator.app`
- `/opt/Qt/Qt Creator.app`
- `/usr/local/Qt/Qt Creator.app`

## Troubleshooting

### Qt Creator Not Found
If the installer doesn't find your Qt Creator installation, ensure:
1. Qt Creator is installed as a proper .app bundle
2. The installation contains `Contents/PlugIns/qtcreator` directory

### Installation Fails
If installation fails:
1. Make sure Qt Creator is not running
2. Check you have write permissions to the Qt Creator bundle
3. Try running the installer with administrator privileges

### Plugin Not Loading
After installation, if the plugin doesn't load:
1. Restart Qt Creator completely
2. Check Help → About Plugins for any errors
3. Verify the plugin files are in the correct location

