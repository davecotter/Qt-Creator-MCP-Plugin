# Qt MCP Plugin

A plugin for Qt Creator that implements Model Context Protocol (MCP), allowing AI to communicate with the IDE for debugging, building, and project management.

## Features

- **MCP Server Integration**: Provides a TCP-based MCP server for AI communication
- **Project Management**: Load sessions, switch build configurations, manage projects
- **Build & Debug Support**: Trigger builds, start debug sessions, run projects
- **Timeout Management**: Intelligent timeout handling with operation duration hints
- **File Operations**: Open files, list open files, manage editor state
- **Issue Tracking**: List build issues and project status
- **Tools Menu Integration**: Complete MCP command menu accessible via Tools → MCP Plugin
- **Console Output**: All command results displayed in Qt Creator's General Messages panel

## MCP Protocol Support

### Available Methods

- `getVersion` - Get plugin version and timeout information
- `listMethods` - List all available MCP methods
- `getMethodMetadata` - Get expected operation durations for long-running tasks
- `listSessions` - List available Qt Creator sessions
- `loadSession` - Load a specific session
- `listProjects` - List loaded projects
- `listBuildConfigs` - List available build configurations
- `switchToBuildConfig` - Switch to a specific build configuration
- `build` - Start a project build
- `debug` - Start a debug session
- `runProject` - Run the current project
- `cleanProject` - Clean the current project
- `openFile` - Open a file in the editor
- `listOpenFiles` - List currently open files
- `listIssues` - List build issues and project status
- `quit` - Quit Qt Creator

### Timeout Management

The plugin provides intelligent timeout handling for long-running operations:

- **debug**: Up to 60 seconds
- **build**: Up to 20 minutes (1200 seconds)
- **runProject**: Up to 60 seconds
- **loadSession**: Up to 30 seconds
- **cleanProject**: Up to 5 minutes (300 seconds)

All long-running operations include timeout hints in their responses, and you can call `getMethodMetadata()` to get detailed timeout information.

## Using the Tools Menu

The plugin adds a comprehensive menu under **Tools → MCP Plugin** with the following options:

### Menu Structure
- **About MCP Plugin** - Shows plugin status and server information
- **Get Version** - Display plugin version information
- **List Sessions** - Show available Qt Creator sessions
- **List Projects** - Display loaded projects
- **List Build Configs** - Show available build configurations
- **Get Current Project** - Display current project name
- **Get Current Build Config** - Show current build configuration
- **Get Current Session** - Display current session name
- **List Open Files** - Show currently open files
- **List Issues** - Display build issues and warnings
- **Build Project** - Start a project build
- **Run Project** - Execute the current project
- **Debug Project** - Start debug session
- **Clean Project** - Clean build artifacts
- **Save Session** - Save current session
- **Quit Qt Creator** - Exit Qt Creator

### Console Output

All command results are displayed in Qt Creator's **General Messages** panel (View → Views → General Messages). The messages flash briefly to draw attention, then remain visible in the panel. This provides a convenient way to see command results without needing to use the MCP server directly.

## Prerequisites

### Required Qt Components

**⚠️ IMPORTANT: You must install Qt with the following components:**

1. **Qt Creator** 6.9.2 or later
2. **Qt Libraries** 6.9.2 or later (exact version match required)
3. **Qt Plugin Development** (includes Qt Creator plugin development headers)
4. **Qt Sources** (required for plugin compilation)

### Installation Instructions

**Windows:**
- Download Qt from [qt.io](https://www.qt.io/download)
- During installation, ensure you select:
  - ✅ Qt Creator 6.9.2
  - ✅ Qt 6.9.2 (MSVC 2022 64-bit or MinGW 64-bit)
  - ✅ Qt Plugin Development
  - ✅ Qt Sources
- **CMake** 3.16 or later
- **Visual Studio** 2019/2022 or **MinGW** compiler
- **Git** for cloning the repository

**macOS:**
- Download Qt from [qt.io](https://www.qt.io/download)
- During installation, ensure you select:
  - ✅ Qt Creator 6.9.2
  - ✅ Qt 6.9.2 (macOS)
  - ✅ Qt Plugin Development
  - ✅ Qt Sources
- **CMake** 3.16 or later
- **Xcode Command Line Tools** (`xcode-select --install`)
- **Git** for cloning the repository

**Linux:**
- Download Qt from [qt.io](https://www.qt.io/download)
- During installation, ensure you select:
  - ✅ Qt Creator 6.9.2
  - ✅ Qt 6.9.2 (GCC 64-bit)
  - ✅ Qt Plugin Development
  - ✅ Qt Sources
- **CMake** 3.16 or later
- **GCC** or **Clang** compiler
- **Git** for cloning the repository

### Verifying Your Installation

**Check Qt Creator Version:**
- Open Qt Creator
- Go to Help → About Qt Creator
- Verify version is 6.9.2 or later

**Check Qt Libraries Version:**
- In Qt Creator, go to Help → About Qt
- Verify Qt version is 6.9.2 or later

**Verify Plugin Development Headers:**
- Look for these directories in your Qt installation:
  - `Qt Creator.app/Contents/Resources/Headers/qtcreator/src/plugins/` (macOS)
  - `Qt Creator/6.9.2/msvc2022_64/include/qtcreator/src/plugins/` (Windows)
  - `/opt/qtcreator/include/qtcreator/src/plugins/` (Linux)

## How to Build

### Step 1: Clone the Repository
```bash
git clone https://github.com/davecotter/Qt-Creator-MCP-Plugin.git
cd Qt-Creator-MCP-Plugin
```

### Step 2: Create Build Directory
```bash
mkdir build
cd build
```

### Step 3: Configure and Build

**Windows (Command Prompt):**
```cmd
cmake -DCMAKE_PREFIX_PATH="C:/Qt/Qt Creator/6.9.2/msvc2022_64" -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build .
```

**Windows (PowerShell):**
```powershell
cmake -DCMAKE_PREFIX_PATH="C:/Qt/Qt Creator/6.9.2/msvc2022_64" -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build .
```

**macOS:**
```bash
cmake -DCMAKE_PREFIX_PATH="/Users/$(whoami)/Qt/Qt Creator.app/Contents/Resources" -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build .
```

**Linux:**
```bash
cmake -DCMAKE_PREFIX_PATH="/opt/qtcreator" -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
cmake --build .
```

### Finding Your Qt Creator Path

**Windows:** Look for Qt Creator installation in:
- `C:/Qt/Qt Creator/6.9.2/msvc2022_64/`
- `C:/Program Files/Qt/Qt Creator/6.9.2/msvc2022_64/`

**macOS:** Look for Qt Creator in:
- `/Users/$(whoami)/Qt/Qt Creator.app/Contents/Resources/`
- `/Applications/Qt Creator.app/Contents/Resources/`

**Linux:** Look for Qt Creator in:
- `/opt/qtcreator/`
- `/usr/local/qtcreator/`
- `~/Qt/Qt Creator/6.9.2/gcc_64/`

## How to Install the Plugin

### ⚠️ IMPORTANT: Plugin Installation Requirements

**The plugin MUST be installed in the correct location to work automatically when users double-click Qt Creator.**

### Step 4: Install the Plugin

After building, install the plugin using CMake:

```bash
cmake --build . --target InstallPlugin
```

This will automatically:
1. Clean up any previous plugin versions
2. Install the plugin to the correct user-specific directory
3. Copy the plugin configuration file

### Plugin Installation Locations

The plugin is installed to user-specific directories that persist across Qt Creator updates:

**Windows:**
- `%LOCALAPPDATA%\QtProject\qtcreator\plugins\`

**macOS:**
- `~/Library/Application Support/QtProject/qtcreator/plugins/`

**Linux:**
- `~/.local/share/data/QtProject/qtcreator/plugins/`

### ⚠️ CRITICAL: macOS Plugin Loading Limitations

**IMPORTANT: Qt Creator on macOS does NOT automatically load plugins from external user directories.**

After extensive testing, the following approaches **DO NOT WORK** on macOS:
- ❌ Installing plugins in user directories (`~/Library/Application Support/QtProject/qtcreator/plugins/`)
- ❌ Modifying `qt.conf` with `AdditionalPlugins=` 
- ❌ Using `QT_PLUGIN_PATH` environment variable
- ❌ Symbolic links from external directories

**Qt Creator only loads plugins from its internal app bundle by default.**

### macOS Working Solutions

Since external plugin loading doesn't work on macOS, you have these options:

#### Option 1: Install in App Bundle (Temporary)

**⚠️ WARNING: This will be lost when Qt Creator is updated!**

```bash
# Install plugin directly in Qt Creator app bundle
cp ~/Library/Application\ Support/QtProject/qtcreator/plugins/* "/Applications/Qt Creator.app/Contents/PlugIns/qtcreator/"
```

**Pros:** Works immediately, no configuration needed
**Cons:** Plugin is lost when Qt Creator updates

#### Option 2: Use Command-Line Plugin Loading

Create a script or alias to launch Qt Creator with the plugin path:

```bash
# Create a launch script
cat > ~/launch_qtcreator_with_mcp.sh << 'EOF'
#!/bin/bash
"/Applications/Qt Creator.app/Contents/MacOS/Qt Creator" -pluginpath ~/Library/Application\ Support/QtProject/qtcreator/plugins "$@"
EOF

chmod +x ~/launch_qtcreator_with_mcp.sh
```

Then use: `~/launch_qtcreator_with_mcp.sh` instead of launching Qt Creator normally.

**Pros:** Plugin persists across updates
**Cons:** Must use script instead of double-clicking the app

#### Option 3: Create Application Alias (Recommended)

Create a new application bundle that launches Qt Creator with the plugin:

```bash
# Create a wrapper app (advanced users)
# This creates a new app that automatically loads the plugin
```

### macOS Plugin Installation Commands

```bash
# Build and install the plugin
cmake --build . --target InstallPlugin

# Then choose one of the working solutions above
```

#### Windows Configuration

1. **Modify Qt Creator's configuration file:**
   ```cmd
   # Backup the original configuration
   copy "C:\Qt\Qt Creator\6.9.2\msvc2022_64\bin\qt.conf" "C:\Qt\Qt Creator\6.9.2\msvc2022_64\bin\qt.conf.backup"
   
   # Edit the configuration file
   notepad "C:\Qt\Qt Creator\6.9.2\msvc2022_64\bin\qt.conf"
   ```

2. **Add the user plugin directory to the configuration:**
   ```
   [Paths]
   Prefix=.
   Binaries=.
   Libraries=lib
   Plugins=plugins
   Qml2Imports=qml
   AdditionalPlugins=%LOCALAPPDATA%\QtProject\qtcreator\plugins
   ```

#### Linux Configuration

1. **Modify Qt Creator's configuration file:**
   ```bash
   # Backup the original configuration
   cp "/opt/qtcreator/bin/qt.conf" "/opt/qtcreator/bin/qt.conf.backup"
   
   # Edit the configuration file
   nano "/opt/qtcreator/bin/qt.conf"
   ```

2. **Add the user plugin directory to the configuration:**
   ```
   [Paths]
   Prefix=.
   Binaries=bin
   Libraries=lib
   Plugins=lib/qtcreator/plugins
   Qml2Imports=qml
   AdditionalPlugins=/home/$(whoami)/.local/share/data/QtProject/qtcreator/plugins
   ```

### Alternative: Command-Line Plugin Loading

If you prefer not to modify Qt Creator's configuration, you can start Qt Creator with the plugin path:

```bash
# macOS
"/Applications/Qt Creator.app/Contents/MacOS/Qt Creator" -pluginpath ~/Library/Application\ Support/QtProject/qtcreator/plugins

# Windows
"C:\Qt\Qt Creator\6.9.2\msvc2022_64\bin\qtcreator.exe" -pluginpath "%LOCALAPPDATA%\QtProject\qtcreator\plugins"

# Linux
/opt/qtcreator/bin/qtcreator -pluginpath ~/.local/share/data/QtProject/qtcreator/plugins
```

**Note:** Command-line loading only works when explicitly specified. For automatic loading when users double-click the app, the qt.conf configuration is required.

### Verify Plugin Installation

#### macOS Verification

1. **Start Qt Creator** using one of the working solutions above
2. **Check Help → About Plugins** to verify "Qt MCP Plugin" is listed
3. **Test MCP connectivity:**
   ```bash
   # Test if MCP server is running
   nc -z localhost 3001 && echo "MCP server is running" || echo "MCP server is not running"
   
   # Get plugin version
   echo '{"jsonrpc": "2.0", "method": "getVersion", "id": 1}' | nc localhost 3001
   ```

#### Windows/Linux Verification

1. **Start Qt Creator** (normally or with -pluginpath)
2. **Check Help → About Plugins** to verify "Qt MCP Plugin" is listed
3. **Test MCP connectivity:**
   ```bash
   # Test if MCP server is running
   nc -z localhost 3001 && echo "MCP server is running" || echo "MCP server is not running"
   
   # Get plugin version
   echo '{"jsonrpc": "2.0", "method": "getVersion", "id": 1}' | nc localhost 3001
   ```

## How to Run

### Option 1: Using CMake Target (Cross-Platform)

From the command line run

    cmake --build . --target RunQtCreator

`RunQtCreator` is a custom CMake target that will use the <path to qtcreator> referenced above to
start the Qt Creator executable with the following parameters

    -pluginpath <path_to_plugin>

where `<path_to_plugin>` is the path to the resulting plugin library in the build directory
(`<plugin_build>/lib/qtcreator/plugins` on Windows and Linux,
`<plugin_build>/Qt Creator.app/Contents/PlugIns` on macOS).

### Option 2: Using Platform-Specific Scripts

**Windows:**
```cmd
launch_qtcreator_with_plugin.bat
```

**macOS/Linux:**
```bash
./launch_qtcreator_with_plugin.sh
```

You might want to add `-temporarycleansettings` (or `-tcs`) to ensure that the opened Qt Creator
instance cannot mess with your user-global Qt Creator settings.

## Usage Examples

Once Qt Creator is running with the plugin, the MCP server will be available on port 3001 (or the next available port). You can interact with it using JSON-RPC:

### macOS/Linux:
```bash
# Get plugin version and timeout information
echo '{"jsonrpc": "2.0", "method": "getVersion", "id": 1}' | nc localhost 3001

# List available methods
echo '{"jsonrpc": "2.0", "method": "listMethods", "id": 2}' | nc localhost 3001

# Get timeout metadata for long-running operations
echo '{"jsonrpc": "2.0", "method": "getMethodMetadata", "id": 3}' | nc localhost 3001

# Load a session
echo '{"jsonrpc": "2.0", "method": "loadSession", "params": {"sessionName": "MyProject"}, "id": 4}' | nc localhost 3001

# Start a debug session (may take up to 60 seconds)
echo '{"jsonrpc": "2.0", "method": "debug", "id": 5}' | nc localhost 3001
```

### Windows (Command Prompt):
```cmd
# Get plugin version and timeout information
echo {"jsonrpc": "2.0", "method": "getVersion", "id": 1} | ncat localhost 3001

# List available methods  
echo {"jsonrpc": "2.0", "method": "listMethods", "id": 2} | ncat localhost 3001

# Get timeout metadata for long-running operations
echo {"jsonrpc": "2.0", "method": "getMethodMetadata", "id": 3} | ncat localhost 3001

# Load a session
echo {"jsonrpc": "2.0", "method": "loadSession", "params": {"sessionName": "MyProject"}, "id": 4} | ncat localhost 3001

# Start a debug session (may take up to 60 seconds)
echo {"jsonrpc": "2.0", "method": "debug", "id": 5} | ncat localhost 3001
```

### Windows (PowerShell):
```powershell
# Get plugin version and timeout information
'{"jsonrpc": "2.0", "method": "getVersion", "id": 1}' | ncat localhost 3001

# List available methods
'{"jsonrpc": "2.0", "method": "listMethods", "id": 2}' | ncat localhost 3001

# Get timeout metadata for long-running operations
'{"jsonrpc": "2.0", "method": "getMethodMetadata", "id": 3}' | ncat localhost 3001

# Load a session
'{"jsonrpc": "2.0", "method": "loadSession", "params": {"sessionName": "MyProject"}, "id": 4}' | ncat localhost 3001

# Start a debug session (may take up to 60 seconds)
'{"jsonrpc": "2.0", "method": "debug", "id": 5}' | ncat localhost 3001
```

### Testing Connectivity

**macOS/Linux:**
```bash
# Test if the MCP server is running
nc -z localhost 3001 && echo "MCP server is running" || echo "MCP server is not running"
```

**Windows:**
```cmd
# Test if the MCP server is running (requires ncat)
ncat -z localhost 3001 && echo MCP server is running || echo MCP server is not running
```

**Windows (PowerShell alternative):**
```powershell
# Test if the MCP server is running
Test-NetConnection -ComputerName localhost -Port 3001
```

### Installing Required Tools

**Windows - Install ncat:**
1. Download and install [Nmap](https://nmap.org/download.html)
2. Add Nmap's bin directory to your PATH
3. Or use PowerShell alternatives as shown above

**macOS - Install netcat:**
```bash
# Usually pre-installed, but if not:
brew install netcat
```

**Linux - Install netcat:**
```bash
# Ubuntu/Debian:
sudo apt-get install netcat

# CentOS/RHEL:
sudo yum install nc
```

## Troubleshooting

### Common Issues

**Build Errors:**
- **"Could not find QtCreator"**: Make sure you're using the correct `CMAKE_PREFIX_PATH` for your platform
- **"CMake version too old"**: Update CMake to version 3.16 or later
- **"Compiler not found"**: Install Visual Studio (Windows), Xcode Command Line Tools (macOS), or GCC/Clang (Linux)
- **"Qt Plugin Development headers not found"**: Reinstall Qt and ensure "Qt Plugin Development" component is selected
- **"Qt Sources not found"**: Reinstall Qt and ensure "Qt Sources" component is selected
- **"Version mismatch"**: Ensure Qt Creator and Qt Libraries are both version 6.9.2 or later

**Runtime Issues:**
- **"MCP server not starting"**: Check that Qt Creator is running and the plugin is loaded
- **"Connection refused"**: The MCP server might be on a different port (3002, 3003, etc.)
- **"Process detection failed"**: This is normal - the plugin will still work for debugging

**Platform-Specific Issues:**

**Windows:**
- If `ncat` is not found, install Nmap or use PowerShell alternatives
- Make sure Visual Studio or MinGW is properly installed
- Use forward slashes in CMake paths: `"C:/Qt/Qt Creator/6.9.2/msvc2022_64"`

**macOS:**
- If Xcode Command Line Tools are missing, run: `xcode-select --install`
- Make sure Qt Creator is in your Applications folder or specify the full path
- Use the `.app/Contents/Resources` path for Qt Creator

**Linux:**
- Install development packages: `sudo apt-get install build-essential cmake` (Ubuntu/Debian)
- Make sure Qt Creator development headers are installed
- Check that your Qt installation includes the development files

### Qt Installation Requirements

**If you get build errors related to missing Qt components:**

1. **Uninstall current Qt installation**
2. **Reinstall Qt with ALL required components:**
   - Qt Creator 6.9.2
   - Qt 6.9.2 (matching your platform)
   - ✅ **Qt Plugin Development** (CRITICAL - includes plugin headers)
   - ✅ **Qt Sources** (CRITICAL - required for compilation)
   - ✅ **Qt Debug Information Files** (recommended)

**Common Qt Installation Mistakes:**
- ❌ Installing only Qt Creator without Qt Libraries
- ❌ Installing Qt Libraries without Plugin Development component
- ❌ Installing different versions of Qt Creator and Qt Libraries
- ❌ Missing Qt Sources component

**Verification Commands:**
```bash
# Check if plugin development headers exist
ls -la "Qt Creator.app/Contents/Resources/Headers/qtcreator/src/plugins/"  # macOS
ls -la "C:/Qt/Qt Creator/6.9.2/msvc2022_64/include/qtcreator/src/plugins/"  # Windows
ls -la "/opt/qtcreator/include/qtcreator/src/plugins/"  # Linux
```

### Getting Help

If you encounter issues:
1. Check the [GitHub Issues](https://github.com/davecotter/Qt-Creator-MCP-Plugin/issues) page
2. Make sure you're using the latest version
3. Verify your Qt Creator installation has development headers
4. Test with a simple project first

## Version History

- **v1.22.0**: Fixed console output to use Qt Creator's General Messages panel via `Core::MessageManager::writeFlashing()`, removed all dialog popups, improved user experience
- **v1.21.0**: Updated version numbering in menu items and About dialog, implemented centralized output function
- **v1.20.0**: Added comprehensive Tools menu with all MCP commands, console output integration, improved user experience
- **v1.19.0**: Enhanced timeout management, added `getMethodMetadata` method, improved debug command reliability, Windows compatibility
- **v1.18.0**: Initial MCP server implementation with basic Qt Creator integration
