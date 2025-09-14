# Qt MCP Plugin

A plugin for Qt Creator that implements Model Context Protocol (MCP), allowing AI to communicate with the IDE for debugging, building, and project management.

## Features

- **MCP Server Integration**: Provides a TCP-based MCP server for AI communication
- **Project Management**: Load sessions, switch build configurations, manage projects
- **Build & Debug Support**: Trigger builds, start debug sessions, run projects
- **Timeout Management**: Intelligent timeout handling with operation duration hints
- **File Operations**: Open files, list open files, manage editor state
- **Issue Tracking**: List build issues and project status

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

## Prerequisites

### Windows
- **Qt Creator** 6.9.2 or later with development headers
- **CMake** 3.16 or later
- **Visual Studio** 2019/2022 or **MinGW** compiler
- **Git** for cloning the repository

### macOS
- **Qt Creator** 6.9.2 or later with development headers
- **CMake** 3.16 or later
- **Xcode Command Line Tools** (`xcode-select --install`)
- **Git** for cloning the repository

### Linux
- **Qt Creator** 6.9.2 or later with development headers
- **CMake** 3.16 or later
- **GCC** or **Clang** compiler
- **Git** for cloning the repository

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

**Windows:**
```cmd
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

## How to Run

From the command line run

    cmake --build . --target RunQtCreator

`RunQtCreator` is a custom CMake target that will use the <path to qtcreator> referenced above to
start the Qt Creator executable with the following parameters

    -pluginpath <path_to_plugin>

where `<path_to_plugin>` is the path to the resulting plugin library in the build directory
(`<plugin_build>/lib/qtcreator/plugins` on Windows and Linux,
`<plugin_build>/Qt Creator.app/Contents/PlugIns` on macOS).

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

### Getting Help

If you encounter issues:
1. Check the [GitHub Issues](https://github.com/davecotter/Qt-Creator-MCP-Plugin/issues) page
2. Make sure you're using the latest version
3. Verify your Qt Creator installation has development headers
4. Test with a simple project first

## Version History

- **v1.19.0**: Enhanced timeout management, added `getMethodMetadata` method, improved debug command reliability, Windows compatibility
- **v1.18.0**: Initial MCP server implementation with basic Qt Creator integration
