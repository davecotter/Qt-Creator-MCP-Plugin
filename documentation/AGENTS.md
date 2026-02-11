# Qt MCP Plugin - Agent discoverability

## Project type
Qt Creator plugin implementing the **Model Context Protocol (MCP)**. Enables AI/IDE control of Qt Creator (build, debug, open file, list issues, etc.).

## Directory map
- src/source/ – C++ implementation (.cpp, .mm)
- src/header/ – C++ headers
- src/resources/ – mcp.qrc, mcp.png, translations
- src/*.in – version and plugin JSON templates
- scripts/build/ – main build and config scripts
- scripts/test/ – test scripts
- scripts/installer/ – installer assets and scripts
- build/ – build artifacts (CMake output, installer .app; in .gitignore)
- logs/ – build and other log files (in .gitignore)
- documentation/ – AGENTS.md, TESTING.md, and other docs
- .qt/ – Qt path config per platform

## Build
From repo root run:
  python3 scripts/build/build_main.py
This quits Qt Creator (if running), cleans old plugin, builds, installs to Qt Creator, launches Qt Creator, and verifies the MCP server. Do not run raw cmake or make; use the build script. Logs are written to logs/build.log.

## MCP server
When Qt Creator is running with the plugin loaded, the MCP server listens at http://localhost:3001. Cursor (or another MCP client) can connect to this URL to call tools.

## Tools available (after plugin is installed and Qt Creator is running)
build, getBuildStatus, debug, openFile(path), listProjects, listBuildConfigs, switchBuildConfig(name), runProject, cleanProject, listOpenFiles, listSessions, loadSession(sessionName), listIssues, getBuildDiagnostics, getCurrentProject, getCurrentBuildConfig, getCurrentSession, saveSession, quit.

Authoritative list: src/Qt_MCP_Plugin_discovery.json.in or generated build/Qt_MCP_Plugin_discovery.json.

## Documentation
- documentation/AGENTS.md – this file (agent discoverability)
- documentation/TESTING.md – test suite and how to run tests
