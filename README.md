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

## How to Build

Create a build directory and run

    cmake -DCMAKE_PREFIX_PATH=<path_to_qtcreator> -DCMAKE_BUILD_TYPE=RelWithDebInfo <path_to_plugin_source>
    cmake --build .

### Platform-Specific Paths

**Windows:**
```bash
cmake -DCMAKE_PREFIX_PATH="C:/Qt/Qt Creator/6.9.2/msvc2022_64" -DCMAKE_BUILD_TYPE=RelWithDebInfo .
```

**macOS:**
```bash
cmake -DCMAKE_PREFIX_PATH="/Users/username/Qt/Qt Creator.app/Contents/Resources" -DCMAKE_BUILD_TYPE=RelWithDebInfo .
```

**Linux:**
```bash
cmake -DCMAKE_PREFIX_PATH="/opt/qtcreator" -DCMAKE_BUILD_TYPE=RelWithDebInfo .
```

where `<path_to_qtcreator>` is the relative or absolute path to a Qt Creator build directory, or to a
combined binary and development package (Windows / Linux), or to the `Qt Creator.app/Contents/Resources/`
directory of a combined binary and development package (macOS), and `<path_to_plugin_source>` is the
relative or absolute path to this plugin directory.

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

## Usage Example

Once Qt Creator is running with the plugin, the MCP server will be available on port 3001 (or the next available port). You can interact with it using JSON-RPC:

### Unix/macOS/Linux:
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

### Windows:
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

**Note:** On Windows, you may need to install `ncat` (part of Nmap) or use PowerShell with `Test-NetConnection` and `Invoke-WebRequest` for testing connectivity.

## Version History

- **v1.19.0**: Enhanced timeout management, added `getMethodMetadata` method, improved debug command reliability
- **v1.18.0**: Initial MCP server implementation with basic Qt Creator integration
