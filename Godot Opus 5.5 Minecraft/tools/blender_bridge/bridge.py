"""Thin client for the blender_godot MCP connection (Blender addon socket on port 9877).

It speaks exactly the same JSON protocol as the `mcp-for-blender` MCP server
(`{"type": <command>, "params": {...}}`) and runs every script through the
MCP server's own Safe Mode validator before sending it, so the same sandbox
rules apply as when going through the MCP tools.

Usage:
  python bridge.py exec <script.py>           # execute_code
  python bridge.py scene                      # get_scene_info
  python bridge.py object <name>              # get_object_info
  python bridge.py screenshot <out.png> [max] # get_viewport_screenshot
  python bridge.py export <out.glb> [obj ...] # export_scene (glb)
"""

import json
import os
import socket
import sys
import glob

HOST = os.environ.get("BLENDER_HOST", "localhost")
PORT = int(os.environ.get("BLENDER_PORT", "9877"))  # blender_godot


def _load_validator():
    """Import validate_code from the installed mcp-for-blender package."""
    candidates = glob.glob(os.path.expandvars(
        r"%LOCALAPPDATA%\uv\cache\archive-v0\*\Lib\site-packages\blender_mcp\safe_mode.py"))
    for c in candidates:
        site = os.path.dirname(os.path.dirname(c))
        if site not in sys.path:
            sys.path.insert(0, site)
        try:
            import importlib.util
            spec = importlib.util.spec_from_file_location("bmcp_safe_mode", c)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod.validate_code
        except Exception as exc:  # pragma: no cover
            print(f"[bridge] validator load failed from {c}: {exc}", file=sys.stderr)
    return None


def send(command_type, params=None, timeout=600.0):
    payload = json.dumps({"type": command_type, "params": params or {}}).encode("utf-8")
    with socket.create_connection((HOST, PORT), timeout=timeout) as s:
        s.sendall(payload)
        s.settimeout(timeout)
        chunks = []
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            chunks.append(chunk)
            try:
                data = json.loads(b"".join(chunks).decode("utf-8"))
                return data
            except json.JSONDecodeError:
                continue
    raise RuntimeError("Connection closed before a complete response")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    cmd = sys.argv[1]
    if cmd == "exec":
        code = open(sys.argv[2], "r", encoding="utf-8").read()
        validate = _load_validator()
        if validate is None:
            print("[bridge] WARNING: safe-mode validator unavailable", file=sys.stderr)
        else:
            try:
                validate(code)
            except Exception as exc:
                print(f"[bridge] Rejected by safe mode - {exc}")
                return 3
        resp = send("execute_code", {"code": code})
    elif cmd == "scene":
        resp = send("get_scene_info")
    elif cmd == "object":
        resp = send("get_object_info", {"name": sys.argv[2]})
    elif cmd == "screenshot":
        out = os.path.abspath(sys.argv[2])
        mx = int(sys.argv[3]) if len(sys.argv) > 3 else 900
        resp = send("get_viewport_screenshot", {"max_size": mx, "filepath": out, "format": "png"})
    elif cmd == "export":
        out = os.path.abspath(sys.argv[2])
        names = sys.argv[3:] or None
        resp = send("export_scene", {"filepath": out, "format": "glb", "object_names": names,
                                      "selection_only": False, "apply_modifiers": True})
    elif cmd == "raw":
        resp = send(sys.argv[2], json.loads(sys.argv[3]) if len(sys.argv) > 3 else {})
    else:
        print(__doc__)
        return 2
    status = resp.get("status")
    if status == "error":
        msg = resp.get("message", "")
        try:
            detail = json.loads(msg)
            print("ERROR:", detail.get("exception_type"), detail.get("message"))
            print(detail.get("traceback", ""))
        except Exception:
            print("ERROR:", msg)
        return 1
    result = resp.get("result", resp)
    if isinstance(result, dict) and "result" in result and cmd == "exec":
        print(result["result"])
    else:
        print(json.dumps(result, indent=1)[:20000])
    return 0


if __name__ == "__main__":
    sys.exit(main())
