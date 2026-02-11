# Qt MCP Plugin Fixes Needed

This document describes issues that must be addressed in the Qt MCP Plugin. Use this as context when working on the MCP Plugin project.

## Context

A preLaunch script (in the kJams project) runs when the user presses F5 to build and debug. It connects to the Qt Creator MCP server, loads the kJams session, builds, and on success launches the app under LLDB. When the build has compile errors, the script fetches diagnostics via getBuildDiagnostics and exits without launching.

## Issues to Fix

### 1. Qt Issues panel should remain filled after build fails

**Problem:** When the build script detects errors and halts, the Qt Creator Issues panel becomes empty. The user expects the Issues panel to remain populated with the build errors so they can inspect them in Qt Creator.

**Expected behavior:** When a build fails with compile errors, the Issues panel in Qt Creator should remain filled with those errors. The panel should not be cleared when:
- The MCP client disconnects or closes the connection
- The build script exits
- Any MCP-related cleanup occurs

**Likely cause:** Something in the plugin or Qt Creator is clearing the Issues panel when the build is "canceled" or when the MCP client disconnects. Need to ensure the Issues panel is not cleared in these scenarios.

### 2. Compile Output should not show "Canceled build/deployment"

**Problem:** When the build fails with compile errors, the Compile Output panel in Qt Creator shows "Canceled build/deployment". The user expects the build to fail naturally (errors are self-cancelling) rather than appear as "canceled".

**Expected behavior:** When the build hits compile errors, it should stop naturally and the Compile Output should reflect that the build failed (e.g. "Build failed" or similar), not "Canceled build/deployment".

**Likely cause:** Either:
- The configureIssues tool with stopBuildOnLimit causes Qt Creator to treat the stop as a "cancel" rather than a natural failure. The client script has been updated to not use this, but if other clients use it, or if there is another code path that cancels the build, it needs to be fixed.
- When the MCP client disconnects (e.g. script exits), the plugin or Qt Creator may be canceling the build and/or clearing state. The build should be allowed to complete (or fail) naturally before any cleanup; the Issues and Compile Output should reflect the actual build outcome, not "Canceled".

### 3. getBuildDiagnostics must match Qt Issues panel exactly

**Problem:** The client uses getBuildDiagnostics to populate the Cursor Problems panel. It must be an exact duplicate of what Qt Creator shows in its Issues panel.

**Expected behavior:** The getBuildDiagnostics tool should return diagnostics that exactly match what Qt Creator shows in its Issues panel—same entries, same file paths, line numbers, columns, and messages. No more, no less.

**Verify:** Ensure the JSON structure returned by getBuildDiagnostics (or the underlying Qt Creator API) is the same data source that populates the Issues panel. If there are any transformations or filters, they should not introduce discrepancies.

## Summary

1. **Issues panel persistence:** Do not clear the Issues panel when the build fails or when the MCP client disconnects.
2. **No "Canceled":** The build should fail naturally on errors; Compile Output should not show "Canceled build/deployment".
3. **getBuildDiagnostics accuracy:** Return exactly what the Qt Issues panel shows.
