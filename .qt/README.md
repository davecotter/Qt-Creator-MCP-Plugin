# Qt Path Configuration

This folder contains platform-specific Qt installation paths.

## Files

- `qt_path_windows.txt` - Qt path for Windows
- `qt_path_darwin.txt` - Qt path for macOS  
- `qt_path_linux.txt` - Qt path for Linux

## Format

Each file should contain a single line with the absolute path to your Qt installation.
Lines starting with `#` are comments and are ignored.

The path should point to the folder containing:
- `Tools/QtCreator/` (Windows/Linux) or `Qt Creator.app` (macOS)
- Version folders like `6.10.1/`, `6.11.0/`, etc.
- `MaintenanceTool.exe` (Windows) or `MaintenanceTool.app` (macOS)

## Examples

**Windows:**
```
C:\Users\username\Developer\Qt
```

**macOS:**
```
/Users/username/Developer/Qt
```

**Linux:**
```
/opt/Qt
```

## Auto-Discovery

If the file is empty or contains only comments, the build script will attempt to auto-discover the Qt path from common locations. If discovery fails, it will prompt you to edit the appropriate file.
