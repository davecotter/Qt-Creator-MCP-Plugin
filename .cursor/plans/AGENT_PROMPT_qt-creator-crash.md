# Agent prompt: Qt Creator MCP crash during kJams build

Copy this entire document into a new Cursor chat in the **Qt_MCP_Plugin** project.

---

## Goal

Find and fix why the **Qt Creator MCP plugin** (or its host) **crashes or exits** when driven from Cursor (`user-qt-creator` MCP server) during a kJams Windows build workflow.

## Environment

| Item | Value |
|------|-------|
| Host OS | Windows 11 (machine: **pandora-pc**) |
| Qt Creator | `C:\Users\davec\Developer\Qt\Tools\QtCreator\bin\qtcreator.exe` |
| Qt kit | **qt6** / MSVC 2022 64-bit, Qt **6.11.0** |
| Cursor MCP server name | `user-qt-creator` |
| Consumer project | kJams — `kJams_pandora_win.pro`, session **kJams** |
| Build config in use | **kJams NightClub Debug** |
| Plugin discovery (Windows) | `~\Developer\Qt\Tools\QtCreator\lib\qtcreator\plugins\Qt_MCP_Plugin_discovery.json` |

## What happened (repro context)

A Cursor agent was fixing kJams build issues and driving Qt Creator exclusively via MCP (per kJams project rules). Qt Creator was running (PID 9268 at one point). The agent **did not** kill Qt Creator — only `taskkill` on kJams app executables (`kJams NightClub Debug.exe`, etc.).

Eventually:

- MCP calls started returning `fetch failed` or `Not connected`
- `qtcreator.exe` was **no longer running**
- No crash dump was found under `%LOCALAPPDATA%\CrashDumps` (may have exited without WER dump)

### MCP command timeline (chronological — treat as suspect list)

All via `user-qt-creator` unless noted.

1. **Ping / status (OK)** — `getBuildStatus`, `getCurrentSession`, `getCurrentBuildConfig`, `listIssues`
2. **Build attempt 1 (failed, Creator survived)** — `build` → `waitForBuildCompletion` (returned `completed: true`)
   - jom error: `dependent '..\..\..\..\_source\Qt\main\KJ_MainThreadProcPump.h' does not exist`
   - qmake step was **skipped** (“Configuration unchanged”)
3. **Config nudge (Creator survived)** — `switchBuildConfig` → `kJams Lite` → `kJams NightClub Debug` (did **not** force qmake)
4. **Build attempt 2 (failed, Creator survived)** — `build` → `waitForBuildCompletion`
5. **`cleanProject`** — returned `success: true`
6. **Shell (not MCP)** — manual `qmake` in `qt/kJams/win_qt6/kJams NightClub Debug` after touching `kJams.pri` (regenerated Makefile; removed stale header dep)
7. **Build attempt 3 (large rebuild started)** — `build` → returned `success: true`
8. **`waitForBuildCompletion`** — first call returned `fetch failed` (no payload)
9. **`getBuildStatus`** — still responded: **Building 50%**, Compiling, 0 errors
10. **Long compile** — status stuck at 50% for many minutes
11. **`getCompileOutput`** — returned **~148 KB** JSON payload (full compile log)
12. **Subsequent MCP calls** — `fetch failed` / `Not connected`
13. **Process check** — `qtcreator.exe` **not running**

### Leading hypotheses (verify in plugin code)

1. **`waitForBuildCompletion`** — blocks/polls build state; may wedge or crash Creator on long full rebuilds after `cleanProject`, or when client disconnects mid-wait (`fetch failed`).
2. **`getCompileOutput`** — reads entire Compile Output pane; **~148 KB** response may overflow buffers, block GUI thread, or destabilize plugin.
3. **`cleanProject` + immediate full `build`** — heavy clean + massive parallel compile (jom) may stress Creator; plugin may not survive concurrent build monitoring.
4. **Client disconnect during active build** — Cursor MCP `fetch failed` may correlate with plugin teardown that takes down Creator (see also existing plan: Issues panel / “Canceled build” on disconnect).

## What to investigate in this repo

1. **Implementations** of: `cleanProject`, `build`, `waitForBuildCompletion`, `getBuildStatus`, `getCompileOutput`, `getBuildDiagnostics`
2. **Threading** — any MCP handler running on GUI thread with blocking waits, unbounded loops, or synchronous reads of large panes
3. **Build lifecycle** — does disconnect/cancel during `waitForBuildCompletion` call `ProjectExplorer::BuildManager::cancel()` or similar in a way that crashes?
4. **Output size limits** — cap/truncate `getCompileOutput` and stream or paginate; ensure large logs cannot crash host
5. **Logging** — add crash-safe logging around each tool entry/exit so the last completed vs in-flight tool is known

## Reproduction plan (suggested)

1. Launch Qt Creator, load session **kJams**, config **kJams NightClub Debug**, open `kJams_pandora_win.pro`
2. Connect Cursor `user-qt-creator` MCP
3. **Ping** — `getBuildStatus` (must succeed before each step)
4. **Baseline** — `getCurrentProject`, `getCurrentBuildConfig`
5. **`cleanProject`** — ping; confirm Creator still running
6. **`build`** — ping; confirm Creator still running
7. **`waitForBuildCompletion`** with `timeoutSeconds: 900` — ping after; if silent, check `qtcreator.exe`
8. During compile, call **`getCompileOutput`** — ping after; check if this is the crash trigger
9. Bisect: repeat without `cleanProject`, without `getCompileOutput`, with shorter waits

## Success criteria

- Creator **stays running** through full clean + rebuild + wait + compile-output fetch
- MCP returns structured errors instead of host crash when operations fail or time out
- `waitForBuildCompletion` survives client `fetch failed` / reconnect without killing Creator
- Document root cause and minimal fix in plugin code

## Related existing notes

See `.cursor/plans/MCP_PLUGIN_FIXES_NEEDED.plan.md` in this repo (Issues panel cleared, “Canceled build/deployment”, getBuildDiagnostics parity) — disconnect during failed builds may share root cause with this crash.

## kJams-side context (not your fix, but explains the workload)

- Stale Makefile referenced deleted `KJ_MainThreadProcPump.h` until manual `qmake` was run
- After fix, a **full rebuild** after `cleanProject` was in progress at ~50% compile when Creator died
- kJams rule added: `.cursor/rules/qt-creator-mcp-crash-debug.mdc` — ping before commands; if MCP goes silent, check Creator process; treat last command as crash suspect

## Constraints

- Fix in **Qt MCP plugin**, not in kJams
- Prefer minimal, correct fixes over timeouts/workarounds
- Preserve existing tool contracts where possible; document any breaking changes

---

**Start by:** locating tool handlers for the suspect commands, adding defensive logging, then reproducing steps 5–8 above on Windows with kJams NightClub Debug.
