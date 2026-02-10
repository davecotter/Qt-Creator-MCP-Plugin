# MCP: Structured build diagnostics for Cursor

## Problem

When building via F5 in Cursor, the preLaunch task uses the Qt Creator MCP to build and, on failure, calls `listIssues` and `getCompileOutput`. That output is printed to the terminal only. Cursor's **Problems** panel is not filled from that output because:

1. The task uses `"problemMatcher": []`, so VS Code does not parse the task output into diagnostics.
2. `listIssues` and `getCompileOutput` return **human-readable text**, not a machine-parseable list of file/line/message that a problem matcher or extension can consume reliably.

So the only "real" build errors are in the terminal; the Problems panel shows unrelated clangd diagnostics. To make build failures show up in the Problems panel and match what Qt Creator reports, the Qt Creator MCP plugin needs to expose **structured** diagnostics.

---

## What to add to the Qt Creator MCP plugin

### 1. Structured issues tool (recommended)

Add a new MCP tool (or extend `listIssues`) that returns **structured diagnostics** instead of (or in addition to) plain text.

**Suggested tool name:** `getBuildDiagnostics` (or `listIssuesStructured`).

**Return format:** JSON array of diagnostic objects, one per issue. Example:

```json
[
  {
    "file": "/absolute/path/to/Project/Sources/Foo.cpp",
    "line": 42,
    "column": 10,
    "message": "use of undeclared identifier 'Bar'",
    "severity": "error",
    "code": "unknown"
  }
]
```

**Fields:**

- `file` (string, required): Absolute path so Cursor can match workspace files.
- `line` (number, required): 1-based line number.
- `column` (number, optional): 1-based column; 0 or omit if unknown.
- `message` (string, required): Full compiler message (one line preferred).
- `severity` (string, optional): `"error"` or `"warning"`. Default `"error"`.
- `code` (string, optional): Compiler error code if available (e.g. `C2065`).

**Optional parameter:** `filter` (e.g. `"errors"` | `"warnings"` | `"all"`) to mirror existing `listIssues` behavior.

This allows Cursor to:

- Have the preLaunch script call `getBuildDiagnostics` on build failure and write the JSON to a well-known file; a Cursor extension or task reads that file and publishes diagnostics to the Problems panel, **or**
- Use the same JSON to generate a fake "compiler" output stream (e.g. `file:line:col: message`) so the existing `$gcc` problem matcher can parse it and fill the Problems panel.

### 2. LSP-style diagnostics (optional)

If the MCP client in Cursor is (or will be) an extension that can publish diagnostics:

- Add a tool that returns diagnostics in **LSP Diagnostic shape** (range with start/end line/character, message, severity, source, code).
- The extension then maps file + range to workspace URIs and publishes them so the Problems panel shows exactly what Qt Creator's build reported.

This requires extension changes; the structured JSON above is enough for a problem matcher or a simple script.

### 3. Keep existing tools

- **`listIssues`** and **`getCompileOutput`** should remain for human-readable output in the terminal and for backward compatibility.
- **`getBuildDiagnostics`** is the structured counterpart for tooling.

---

## Cursor side (after MCP has structured diagnostics)

- **Option A – Problem matcher:** PreLaunch script on build failure: call `getBuildDiagnostics`, write each entry as `file:line:column: message` to stdout/stderr, and add a `$gcc` (or custom) problemMatcher to the `mcp-build` task so VS Code parses and shows them in Problems.
- **Option B – Diagnostics file:** PreLaunch script writes the JSON to e.g. `.vscode/build-diagnostics.json`; a small Cursor extension or a task reads the file and uses the VS Code API to set diagnostics for the given file paths.
- **Option C – Extension calls MCP:** A Cursor extension that talks to the Qt Creator MCP after a build and calls `getBuildDiagnostics`, then publishes those diagnostics to the Problems panel.

---

## Summary

**Add to Qt Creator MCP:** A **structured diagnostics tool** (e.g. `getBuildDiagnostics`) that returns a JSON array of `{ file, line, column?, message, severity? }` per issue. That lets Cursor (task or extension) show the same errors as Qt Creator in the Problems panel instead of relying on clangd or regex over raw compiler output.

Keep `listIssues` and `getCompileOutput` as-is; the new tool is what makes build errors in Cursor match Qt Creator.
