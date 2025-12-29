# Qt MCP Plugin

A Qt Creator plugin that implements the Model Context Protocol (MCP), allowing AI assistants to control Qt Creator for debugging, building, and project management.

## 🤖 AI-First Development

**This plugin is designed to be extended by AI assistants.** Simply ask your AI to:

```
Build and install the Qt MCP Plugin
```

```
Add new MCP commands for [your specific needs]
```

```
Test the plugin and verify MCP server is working
```

## What It Does

The plugin adds a **Tools → MCP Plugin** menu to Qt Creator and runs an MCP server on port 3001. AI assistants can:

- 🔧 **Build & Debug**: Start builds, debug sessions, run projects
- 📁 **Project Management**: Load sessions, switch build configs, list projects  
- 📄 **File Operations**: Open files, list open files, manage editor state
- ⚠️ **Issue Tracking**: List build errors and warnings
- 🔍 **Discovery**: Use standard MCP protocol to discover available tools

## Quick Start

**Prerequisites:** Qt Creator with Qt Plugin Development components, CMake 3.16+, C++20 compiler

**Platform Support:** Windows ✅, macOS ✅, Linux 🔄

**Build & Install:**
```bash
python3 build_main.py
```

The build script handles everything:
- ✅ Quits Qt Creator and cleans old versions
- ✅ Auto-detects Qt version from Qt Creator
- ✅ Builds, installs, and verifies the plugin
- ✅ **Registers with Cursor IDE automatically**

After a successful build, restart Cursor to enable AI control of Qt Creator.

## Extending the Plugin

**Let AI do the work:** The plugin is designed for AI-assisted development. Simply describe what you want to add and let your AI assistant:

1. **Add new MCP commands** in `mcpcommands.cpp/h`
2. **Update the JSON schema** in `Qt_MCP_Plugin.json.in`
3. **Test the new functionality** using the built-in MCP server

**Example AI prompts:**
```
Add an MCP command to open the current file in an external editor
```

```
Create a command to export the current project as a standalone executable
```

```
Add debugging commands to set breakpoints and inspect variables
```

## MCP Protocol

Standard MCP methods:
- `initialize` - Server handshake and capabilities
- `tools/list` - Discover available tools with schemas  
- `tools/call` - Execute tools (build, debug, load sessions, etc.)

**Server runs on:** `localhost:3001`

## Cursor IDE Integration

The build script automatically registers the Qt MCP server with Cursor IDE by adding it to `~/.cursor/mcp.json`. After building:

1. **Restart Cursor** to activate the MCP connection
2. **Start Qt Creator** (plugin loads automatically)
3. **Use AI commands** like:
   - "Build the current Qt project"
   - "List build errors and warnings"
   - "Open file main.cpp in Qt Creator"
   - "Switch to Debug configuration"

**Manual registration** (if needed):
```json
// Add to ~/.cursor/mcp.json
{
  "mcpServers": {
    "qt-creator": {
      "url": "http://localhost:3001"
    }
  }
}
```

## Troubleshooting

**Plugin not loading?** Check Help → About Plugins in Qt Creator

**MCP server not responding?** Ensure Qt Creator is running with plugin loaded

**Need help?** Ask your AI assistant - the build system is fully automated and designed for AI-assisted development.

## License

This project is licensed under the MIT License — see the included LICENSE file for details.