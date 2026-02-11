#!/usr/bin/env python3
"""
getBuildDiagnostics test: verify the MCP plugin returns Qt Creator build
diagnostics for the current project. Does NOT create or modify any files.
Your project already has an error; this builds via MCP, waits, then calls
getBuildDiagnostics and validates/prints the diagnostics.
Usage: python3 scripts/test/test_build_diagnostics.py [--no-build] [-v]
"""
import json
import sys
import socket
import argparse

def send_tcp_request(request_data, timeout=5):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    sock.connect(("localhost", 3001))
    sock.send((request_data + "\n").encode("utf-8"))
    response_data = b""
    while True:
        chunk = sock.recv(4096)
        if not chunk: break
        response_data += chunk
        try:
            json.loads(response_data.decode("utf-8").strip())
            break
        except Exception: continue
    sock.close()
    return response_data.decode("utf-8").strip()

def mcp_tools_call(tool_name, arguments=None, timeout=5):
    params = {"name": tool_name}
    if arguments: params["arguments"] = arguments
    req = {"jsonrpc": "2.0", "method": "tools/call", "params": params, "id": 1}
    raw = send_tcp_request(json.dumps(req), timeout=timeout)
    data = json.loads(raw)
    if "error" in data: raise RuntimeError("MCP error: " + str(data["error"]))
    return data.get("result", {})

def get_diagnostics_from_result(result):
    content = result.get("content") or []
    if not content: return None
    text = content[0].get("text")
    if not text: return None
    try: return json.loads(text).get("diagnostics")
    except Exception: return None

def run_test(do_build=True, verbose=False):
    if do_build:
        if verbose: print("Calling MCP: build ...")
        mcp_tools_call("build", timeout=10)
        if verbose: print("Calling MCP: waitForBuildCompletion(120) ...")
        mcp_tools_call("waitForBuildCompletion", arguments={"timeoutSeconds": 120}, timeout=130)
    if verbose: print("Calling MCP: getBuildDiagnostics(all) ...")
    diag_result = mcp_tools_call("getBuildDiagnostics", arguments={"filter": "all"}, timeout=10)
    diagnostics = get_diagnostics_from_result(diag_result)
    if diagnostics is None:
        print("FAIL: getBuildDiagnostics did not return diagnostics array")
        if verbose: print("  result:", diag_result)
        return False
    for i, d in enumerate(diagnostics):
        if not isinstance(d, dict):
            print("FAIL: diagnostic", i, "not object"); return False
        for key in ("file", "line", "message", "severity"):
            if key not in d:
                print("FAIL: diagnostic", i, "missing", key); return False
    errors = [d for d in diagnostics if d.get("severity") == "error"]
    warnings = [d for d in diagnostics if d.get("severity") == "warning"]
    print("PASS:", len(diagnostics), "diagnostics (", len(errors), "errors,", len(warnings), "warnings)")
    if diagnostics:
        print("\nDiagnostics from Qt Creator:")
        for d in diagnostics:
            sev, path, line = d.get("severity", "?"), d.get("file", "?"), d.get("line", "?")
            msg = (d.get("message") or "").strip()[:80]
            print("  [" + sev + "]", path + ":" + str(line), msg)
    return True

def main():
    ap = argparse.ArgumentParser(description="Get build diagnostics from Qt Creator via MCP (no file injection).")
    ap.add_argument("--no-build", action="store_true", help="Skip build; get current diagnostics only")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()
    print("getBuildDiagnostics test (port 3001), build=", not args.no_build)
    try:
        ok = run_test(do_build=not args.no_build, verbose=args.verbose)
        sys.exit(0 if ok else 1)
    except Exception as e:
        print("FAIL:", e)
        if args.verbose: import traceback; traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__": main()
