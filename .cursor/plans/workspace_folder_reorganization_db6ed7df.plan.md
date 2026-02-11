---
name: Workspace folder reorganization
overview: "Reorganize the Qt_MCP_Plugin repo: move C++/headers into a src subfolder with separate source/ and header/ dirs, move resources under src/resources/, consolidate all scripts under scripts/ with subfolders (build, test, installer), convert the only non-Python script (git-credential-github) to Python, and update CMake, imports, and documentation accordingly."
todos: []
isProject: false
---

# Workspace folder reorganization plan

## Current layout (summary)

- **Root:** [CMakeLists.txt](CMakeLists.txt), [version.cmake](version.cmake), version.h.in, Qt_MCP_Plugin.json.in, Qt_MCP_Plugin_discovery.json.in; all plugin `.cpp`, `.h`, `.mm`; [mcp.qrc](mcp.qrc), [mcp.png](mcp.png); [build_main.py](build_main.py), [build.py](build.py), [qt_config.py](qt_config.py), [check_version.py](check_version.py), [test_discovery.py](test_discovery.py), [test_suite.py](test_suite.py); README, LICENSE, mcp.json, etc.
- **scripts/:** [scripts/git-credential-github](scripts/git-credential-github) (shell only).
- **installer/:** [installer/build_installer.py](installer/build_installer.py), [installer/qt_mcp_installer.py](installer/qt_mcp_installer.py), [installer/setup.py](installer/setup.py), Qt MCP Plugin Installer.spec, QtMCPInstaller.swift, installer_icon.icns, README.md.

## Target layout

```
Qt_MCP_Plugin/
  CMakeLists.txt
  version.cmake
  .cursorrules  .gitignore  .qt/  .github/
  README.md  LICENSE  mcp.json  mcp-build-diagnostics.md  TESTING.md
  build/              # build artifacts; ensure in .gitignore

  src/
    source/          # .cpp, .mm
    header/          # .h
    resources/       # mcp.qrc, mcp.png, Qt_MCP_Plugin_en_US.ts
    version.h.in
    Qt_MCP_Plugin.json.in
    Qt_MCP_Plugin_discovery.json.in

  scripts/
    build/
      build_main.py
      build.py
      qt_config.py
    test/
      test_discovery.py
      test_suite.py
    installer/
      build_installer.py
      qt_mcp_installer.py
      setup.py
      Qt MCP Plugin Installer.spec
      QtMCPInstaller.swift
      installer_icon.icns
      README.md
    git_credential_github.py   # converted from shell
    check_version.py          # at scripts root or under build/, as you prefer)
```

Root keeps only CMake, version.cmake, and top-level docs/config; plugin source and config templates live under `src/`; all scripts and installer assets live under `scripts/` with a simple structure (build, test, installer, plus one credential script and check_version).

---

## 1. Create `src/` and move plugin source/headers/resources

- Add directories: `src/source/`, `src/header/`, `src/resources/`.
- Move implementation files into `src/source/`:
  - `qt_mcp_plugin.cpp`, `mcpserver.cpp`, `mcpcommands.cpp`, `issuesmanager.cpp`, `httpparser.cpp`, `httpresponse.cpp`, `platform_menu_icon.cpp`, `macos_menu_icon.mm`.
- Move headers into `src/header/`:
  - `qt_mcp_pluginconstants.h`, `qt_mcp_plugintr.h`, `mcpserver.h`, `mcpcommands.h`, `issuesmanager.h`, `httpparser.h`, `httpresponse.h`, `macos_menu_icon.h`.
- Move into `src/resources/`: `mcp.qrc`, `mcp.png`, and (optional but consistent) `Qt_MCP_Plugin_en_US.ts`.
- Move into `src/` (templates used by CMake): `version.h.in`, `Qt_MCP_Plugin.json.in`, `Qt_MCP_Plugin_discovery.json.in`.

No C++ `#include` paths need to change: all current includes are either local (e.g. `"mcpserver.h"`) or Qt/Creator. CMake will get include dirs and source lists updated (see below). The resource path in code (`":/icons/mcp.png"`) stays valid as long as the .qrc stays with the same prefix and the file is listed relative to the .qrc (e.g. `mcp.png` next to or under the same resource root).

---

## 2. Update CMakeLists.txt

- **Source list:** In `add_qtc_plugin(... SOURCES ...)`, replace every current plugin file path with paths under `src/source/`, `src/header/`, and `src/resources/` (e.g. `src/source/qt_mcp_plugin.cpp`, `src/header/qt_mcp_pluginconstants.h`, …, `src/resources/mcp.qrc`, `src/resources/mcp.png`). Add the platform-specific source (`macos_menu_icon.mm` or `platform_menu_icon.cpp`) from `src/source/`.
- **Include directories:** Add `target_include_directories(Qt_MCP_Plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/header ${CMAKE_CURRENT_BINARY_DIR})` so that `#include "version.h"` and all project headers still resolve. Keep `version.h` generated into the build dir (see next).
- **Generated files:** Point all references from `CMAKE_CURRENT_SOURCE_DIR` to the new locations:
  - `file(READ .../Qt_MCP_Plugin.json.in ...)` → `.../src/Qt_MCP_Plugin.json.in`
  - `configure_file(version.h.in ...)` → `src/version.h.in` → `${CMAKE_CURRENT_BINARY_DIR}/version.h`
  - `configure_file(Qt_MCP_Plugin_discovery.json.in ...)` → `src/Qt_MCP_Plugin_discovery.json.in`
  - Custom command that copies/sed’s the .in files: use `src/` paths and same output paths in the binary dir.
- **Install target:** The `InstallPlugin` target runs `build.py`; change to run the script under `scripts/build/` and set `PYTHONPATH` so that `qt_config` is importable (e.g. set `PYTHONPATH` to the repo root or to `scripts/build` and run `python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build/build.py`). Use `CMAKE_CURRENT_SOURCE_DIR` so the workflow (which runs from repo root) does not need changes.

Remove or adjust the `SOURCES` entries that currently list `.github/workflows/...` and `README.md` if they are not required by the plugin build; otherwise keep them with correct paths.

---

## 3. mcp.qrc and resource paths

- [mcp.qrc](mcp.qrc) currently has `<file>mcp.png</file>` (same directory). After moving, keep the same logical resource: put `mcp.qrc` and `mcp.png` both in `src/resources/` so the relative path inside the .qrc can remain `<file>mcp.png</file>`. No code change for `":/icons/mcp.png"`.

---

## 4. Scripts: move and fix repo-root resolution

- **Move into `scripts/build/`:** `build_main.py`, `build.py`, `qt_config.py`.
- **Move into `scripts/test/`:** `test_discovery.py`, `test_suite.py`.
- **Move entire `installer/**` (all contents) into `**scripts/installer/**` so installer scripts and assets (spec, Swift, icns, README) live under scripts.

**Repo root and .qt:** [qt_config.py](qt_config.py) uses `os.path.dirname(os.path.abspath(__file__))` and then `os.path.join(script_dir, ".qt", filename)`, so it assumes the script lives at repo root. After moving to `scripts/build/qt_config.py`, `script_dir` would point to `scripts/build/` and `.qt` would not be found. Fix by resolving repo root once (e.g. walk up from `__file__` until a directory contains both `.qt` and `CMakeLists.txt`, or a single known marker like `.qt`), and use that for `.qt` and for any paths that must point at the project root (e.g. build dir, installer path). Apply the same idea anywhere else that assumes “script dir = repo root” (e.g. [build_main.py](build_main.py) references `installer/build_installer.py` and `current_dir` for build outputs). Use a shared convention: e.g. `get_repo_root()` in `qt_config` returning the directory that contains `.qt`, and have `build_main`/`build` use that for `current_dir`, installer script path, and PYTHONPATH when invoking subprocesses if needed.

- **CMake InstallPlugin:** Invoke `python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build/build.py` and set `PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}` (or `${CMAKE_CURRENT_SOURCE_DIR}/scripts/build`) so that `import qt_config` works; with repo-root resolution inside the script, the script can still find `.qt` at repo root.

---

## 5. Convert `scripts/git-credential-github` to Python

- Replace the shell script with `scripts/git_credential_github.py` (or keep name `git-credential-github` with a `.py` extension if desired; Git credential helpers are invoked by path).
- Behavior: handle `get` (read `~/.github/credentials.txt`, output `username=...` and `password=...` for the token line); other actions exit 0. Use a shebang `#!/usr/bin/env python3` and make the file executable so `git config credential.helper "path/to/scripts/git_credential_github.py"` (or the script name) still works. Remove the old `scripts/git-credential-github` after the Python version is in place and documented.

---

## 6. Scripts folder organization (summary)

- **scripts/build/** – build and Qt/config: `build_main.py`, `build.py`, `qt_config.py`; optionally `check_version.py` if it’s build-related.
- **scripts/test/** – `test_discovery.py`, `test_suite.py`.
- **scripts/installer/** – everything from current `installer/` (Python, spec, Swift, icns, README).
- **scripts/** (root) – `git_credential_github.py`, and `check_version.py` if not under `build/`.

No other obvious split is required unless you add more categories later.

---

## 7. Canonical paths and build script path specification

All scripts and CMake must use a single, documented set of path conventions so build artifacts and sources are predictable and discoverable.

- **REPO_ROOT:** The directory containing `CMakeLists.txt` and `.qt/`. Resolved at runtime by walking up from the script file until a directory contains both (e.g. in `qt_config.get_repo_root()`).
- **BUILD_DIR:** Always `REPO_ROOT/build`. CMake must be invoked with `-B build` (or equivalent) so all build output (generated headers, plugin binary, Qt_MCP_Plugin.json, Qt_MCP_Plugin_discovery.json) lives under `build/`. No `build_windows` / `build_darwin` variants; use a single `build/` and let CMake handle platform-specific subdirs.
- **SRC_DIR:** `REPO_ROOT/src`. Plugin source: `src/source/`, headers: `src/header/`, resources: `src/resources/`. Templates: `src/*.in`.
- **SCRIPTS_DIR:** `REPO_ROOT/scripts`. Build entrypoint: `scripts/build/build_main.py`. Lifecycle (used by CMake InstallPlugin): `scripts/build/build.py`. Config helper: `scripts/build/qt_config.py`. Installer: `scripts/installer/`. Tests: `scripts/test/`.
- **Qt config:** `REPO_ROOT/.qt/qt_path_{platform}.txt` (unchanged).

**Build script updates:** In `build_main.py` and `build.py`: derive `current_dir` / working directory from `get_repo_root()` (not `os.getcwd()` for repo-root–relative paths). Use `build_dir = os.path.join(get_repo_root(), "build")` for CMake `-B`, plugin binary path, and generated JSON paths. Use `os.path.join(get_repo_root(), "scripts", "installer", "build_installer.py")` for the installer script. In `installer/build_installer.py`, resolve the build dir as `os.path.join(repo_root, "build")` (repo_root from env or by walking up from script). In CMake, `CMAKE_CURRENT_SOURCE_DIR` is the repo root when configuring from root; use `CMAKE_CURRENT_SOURCE_DIR/src/...` for sources and templates, and use a single build tree (e.g. `-B build` from repo root) so `CMAKE_CURRENT_BINARY_DIR` is `REPO_ROOT/build`.

---

## 8. AI/agent discoverability

Add a single, top-level document so an AI agent (or human) can immediately understand the project and how to build and use it. Prefer **AGENTS.md** at repo root (or a dedicated "For AI" section in README.md) containing at least:

1. **Project type:** Qt Creator plugin implementing the Model Context Protocol (MCP); enables AI/IDE control of Qt Creator (build, debug, open file, list issues, etc.).
2. **Directory map:**
   - `src/source/` – C++ implementation (`.cpp`, `.mm`)
   - `src/header/` – C++ headers
   - `src/resources/` – mcp.qrc, mcp.png, translations
   - `src/*.in` – version and plugin JSON templates
   - `scripts/build/` – main build and config scripts
   - `scripts/test/` – test scripts
   - `scripts/installer/` – installer assets and scripts
   - `build/` – build artifacts (CMake output; in .gitignore)
   - `.qt/` – Qt path config per platform
3. **Build:** From repo root run: `python3 scripts/build/build_main.py`. This quits Qt Creator (if running), cleans old plugin, builds, installs to Qt Creator, launches Qt Creator, and verifies the MCP server. Do not run raw `cmake` or `make`; use the build script.
4. **MCP server:** When Qt Creator is running with the plugin loaded, the MCP server listens at **http://localhost:3001**. Cursor (or another MCP client) can connect to this URL to call tools.
5. **Tools available (after plugin is installed and Qt Creator is running):** build, getBuildStatus, debug, openFile(path), listProjects, listBuildConfigs, switchBuildConfig(name), runProject, cleanProject, listOpenFiles, listSessions, loadSession(sessionName), listIssues, getBuildDiagnostics, getCurrentProject, getCurrentBuildConfig, getCurrentSession, saveSession, quit. (Authoritative list is in `src/Qt_MCP_Plugin_discovery.json.in` or the generated `build/Qt_MCP_Plugin_discovery.json`.)

Ensure `.cursorrules` and README point to this document (or repeat the same facts) so the build command and tool list are easy to find.

---

## 9. Documentation and config updates

- **Build folder:** Use a top-level `build/` directory for CMake/build artifacts. Ensure `build/` is in [.gitignore](.gitignore) (repo already has build/ and build_*/).

- **AGENTS.md (or equivalent):** Add or update a discoverability document as in section 8 above so AI agents and developers immediately see project type, directory map, build command, MCP endpoint, and tool list.

- **[.cursorrules](.cursorrules):** Update the “how to build” instruction from `python3 build_main.py` to `python3 scripts/build/build_main.py` (or the final path you choose). Update any other paths that reference root-level scripts or the installer.
- **README.md / TESTING.md:** Replace references to `build_main.py`, `build.py`, `test_*.py`, and `installer/` with the new paths under `scripts/`.
- **Git credential helper:** If README or docs mention `git-credential-github`, document the new Python path and `git config` usage.

---

## 10. Order of operations and validation

1. Create `src/source`, `src/header`, `src/resources` and move C++/headers/resources and template files.
2. Update `CMakeLists.txt` (sources, include dirs, all `CMAKE_CURRENT_SOURCE_DIR` paths, InstallPlugin command and PYTHONPATH).
3. Add repo-root resolution in `qt_config.py` and move `build_main.py`, `build.py`, `qt_config.py` to `scripts/build/`; update all build scripts to use get_repo_root() and canonical paths: build_dir = repo_root/build, installer at scripts/installer/build_installer.py.
4. Move test_discovery.py, test_suite.py to scripts/test/; fix any path assumptions (e.g. repo root) if they have them.
5. Move installer/ contents to scripts/installer/; update build_main.py reference to scripts/installer/build_installer.py; update installer script to resolve repo root and build/ for plugin binary.
6. Implement scripts/git_credential_github.py, remove scripts/git-credential-github, and document.
7. Move check_version.py to scripts/ or scripts/build/ and fix its paths if it assumes repo root.
8. Update .cursorrules, README, TESTING.md; add or update AGENTS.md (or equivalent) with project type, directory map, build command, MCP endpoint (localhost:3001), and tool list per section 8.
9. Run a full build and install (e.g. python3 scripts/build/build_main.py from repo root), run tests, and confirm the plugin loads and MCP works.

---

## Dependency / risk notes

- **add_qtc_plugin SOURCES:** The Qt Creator plugin API expects all source paths relative to the project; using `src/source/...` and `src/header/...` is standard and only requires correct include dirs and listing every file.
- **version.h:** It is generated into the build dir; as long as that dir is in the target’s include path (as above), `#include "version.h"` in sources under `src/source/` is fine.
- **CI:** `.github/workflows/build_cmake.yml` runs CMake from repo root and then the install target; no workflow change is needed if InstallPlugin just runs `python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build/build.py` with appropriate PYTHONPATH.

This plan keeps a single, consistent layout: source under `src/` with clear source/header/resources separation, and all scripts (including installer and credential helper) under `scripts/` with minimal, clear subfolders.