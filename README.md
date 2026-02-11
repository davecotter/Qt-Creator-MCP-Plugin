# Qt MCP Plugin

A Qt Creator plugin that implements the Model Context Protocol (MCP), allowing AI assistants to control Qt Creator for debugging, building, and project management.

## AI-First Development

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

The plugin adds a **Tools -> MCP Plugin** menu (with icon) to Qt Creator and runs an MCP server on port 3001. AI assistants can:

- **Build & Debug**: Start builds, debug sessions, run projects
- **Project Management**: Load sessions, switch build configs, list projects
- **File Operations**: Open files, list open files, manage editor state
- **Issue Tracking**: List build errors and warnings; get structured diagnostics (JSON) for Cursor Problems panel
- **Discovery**: Use standard MCP protocol to discover available tools

## Quick Start

**Prerequisites:** Qt Creator with Qt Plugin Development components, CMake 3.16+, C++20 compiler

**Platform Support:** Windows, macOS, Linux

**Build & Install:**
```bash
python3 scripts/build/build_main.py
```

The build script handles everything:
- Quits Qt Creator and cleans old versions
- Auto-detects Qt version from Qt Creator binary (no manual configuration)
- Verifies matching Qt SDK is installed
- Builds, installs, and verifies the plugin
- Registers with Cursor IDE automatically

After a successful build, restart Cursor to enable AI control of Qt Creator.

**For AI Assistants:** See `.cursorrules` for detailed build instructions and troubleshooting.

## Repository structure

| Path | Contents |
|------|----------|
| `src/source/` | C++ implementation (.cpp, .mm) |
| `src/header/` | C++ headers |
| `src/resources/` | mcp.qrc, mcp.png, translations |
| `src/*.in` | Plugin JSON and version templates |
| `scripts/build/` | Build and Qt config (build_main.py, qt_config.py) |
| `scripts/test/` | Test suite and build-diagnostics tests |
| `scripts/installer/` | macOS installer app and scripts |
| `scripts/git/` | Git credential helper for pushes |
| `documentation/` | AGENTS.md, TESTING.md, and other docs |
| `.qt/` | Qt path config per platform (see .qt/README.md) |

Build output and logs go to `build/` and `logs/` (gitignored).

## Build diagnostics (Cursor Problems panel)

The plugin exposes **structured build diagnostics** so Cursor (or other MCP clients) can show Qt Creator build errors and warnings in the Problems panel:

- **MCP tool:** `getBuildDiagnostics` - Returns a JSON array of `{ file, line, column, message, severity, code }` per issue (pretty-printed). Optional `filter`: "all", "errors", or "warnings".
- **Menu:** **Tools -> MCP Plugin -> Get Build Diagnostics (JSON)** - Runs the same and shows the JSON in the plugin output pane so you can try it manually.

Use this from a preLaunch script on build failure, or from an extension, to fill the Problems panel with the same issues Qt Creator reports. See documentation/ for build diagnostics and other docs (e.g. documentation/TESTING.md, documentation/AGENTS.md).

**Self-test:** Run `python3 scripts/test/test_build_diagnostics.py --inject` (or `python3 scripts/test/test_suite.py --build-diagnostics`) to inject a syntax error, build via MCP, then assert `getBuildDiagnostics` returns the error. Requires Qt Creator running with a project open.

## Architecture

### Cross-Platform Menu Icons

The plugin displays an icon in the **Tools -> MCP Plugin** menu. Menu icon handling is platform-specific:

- **macOS**: Uses native AppKit APIs (`macos_menu_icon.mm`) to force icon display, as Qt may hide menu icons per Apple HIG
- **Windows/Linux**: Uses Qt native icon support via stub implementation (`platform_menu_icon.cpp`)

### Source Files

All plugin source lives under `src/`: headers in `src/header/`, implementation in `src/source/`, resources in `src/resources/`.

| File | Purpose |
|------|---------|
| `src/source/qt_mcp_plugin.cpp` | Main plugin entry point, menu setup |
| `src/source/mcpserver.cpp`, `src/header/mcpserver.h` | HTTP server on port 3001 |
| `src/source/mcpcommands.cpp`, `src/header/mcpcommands.h` | MCP tool implementations |
| `src/source/macos_menu_icon.mm`, `src/header/macos_menu_icon.h` | macOS native menu icon support |
| `src/source/platform_menu_icon.cpp` | Windows/Linux menu icon stub |
| `src/source/issuesmanager.cpp`, `src/header/issuesmanager.h` | Build issues tracking (list + structured JSON) |
| `src/source/httpparser.cpp`, `src/header/httpparser.h` | HTTP request parsing |
| `src/source/httpresponse.cpp`, `src/header/httpresponse.h` | HTTP response generation |

## Extending the Plugin

**Let AI do the work:** The plugin is designed for AI-assisted development. Describe what you want to add and let your AI assistant:

1. **Add new MCP commands** in `src/source/mcpcommands.cpp` and `src/header/mcpcommands.h`
2. **Register tools** in `src/source/mcpserver.cpp` (both tool-list blocks if present)
3. **Add menu actions** in `src/source/qt_mcp_plugin.cpp` and constants in `src/header/qt_mcp_pluginconstants.h`
4. **Test** using the MCP server or Tools -> MCP Plugin menu

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
- `tools/call` - Execute tools (build, debug, load sessions, get build diagnostics, etc.)

**Server runs on:** `localhost:3001`

## Cursor IDE Integration

The build script automatically registers the Qt MCP server with Cursor IDE by adding it to `~/.cursor/mcp.json`. After building:

1. **Restart Cursor** to activate the MCP connection
2. **Start Qt Creator** (plugin loads automatically)
3. **Use AI commands** like:
   - "Build the current Qt project"
   - "List build errors and warnings"
   - "Get structured build diagnostics for the Problems panel"
   - "Open file main.cpp in Qt Creator"
   - "Switch to Debug configuration"

**Manual registration** (if needed):
```json
{
  "mcpServers": {
    "qt-creator": {
      "url": "http://localhost:3001",
      "description": "Qt Creator MCP Plugin - AI control of Qt Creator IDE"
    }
  }
}
```

## Troubleshooting

**Plugin not loading?** Check Help -> About Plugins in Qt Creator

**MCP server not responding?** Ensure Qt Creator is running with plugin loaded

**Need help?** Ask your AI assistant - the build system is fully automated and designed for AI-assisted development.

## License

This project is licensed under the MIT License - see the included LICENSE file for details.
