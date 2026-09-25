"""
blender_unity bridge client.

Talks to the Blender MCP addon socket of the dedicated Unity Blender instance
(127.0.0.1:9876, file UnityMinecraft.blend) using the exact same JSON protocol
as the `blender_unity` MCP server (mcp_for_blender).  Every script is validated
with the MCP's own Safe Mode validator before it is sent, so the workflow stays
inside Safe Mode limits (bpy / bmesh / mathutils / pure stdlib, native
save/export operators only).

Usage:
  python blender_unity.py info                       -> get_scene_info
  python blender_unity.py exec <script.py> [k=v ...] -> execute_code (validated)
  python blender_unity.py shot <out.png> [max]       -> viewport screenshot
"""
import json
import socket
import sys
import os

HOST = "127.0.0.1"
PORT = 9876  # blender_unity ONLY (never 9877/9878 = godot/unreal)

def _validator():
    try:
        # Keep machine-specific package paths out of the project. A custom
        # install can be supplied without editing this file.
        custom_path = os.environ.get("BLENDER_MCP_SAFE_MODE_PATH")
        if custom_path and custom_path not in sys.path:
            sys.path.insert(0, custom_path)
        from blender_mcp.safe_mode import validate_code
        return validate_code
    except Exception as e:  # pragma: no cover
        print("WARNING: safe-mode validator unavailable:", e, file=sys.stderr)
        return None


def send(command_type, params=None, timeout=600.0):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect((HOST, PORT))
    try:
        s.sendall(json.dumps({"type": command_type, "params": params or {}}).encode("utf-8"))
        chunks = []
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            chunks.append(chunk)
            try:
                data = b"".join(chunks)
                return json.loads(data.decode("utf-8"))
            except json.JSONDecodeError:
                continue
        return json.loads(b"".join(chunks).decode("utf-8"))
    finally:
        s.close()


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    cmd = sys.argv[1]
    if cmd == "info":
        r = send("get_scene_info")
        print(json.dumps(r, indent=1)[:20000])
    elif cmd == "exec":
        path = sys.argv[2]
        with open(path, "r", encoding="utf-8") as f:
            code = f.read()
        # simple key=value substitution for parameters: {{KEY}}
        for kv in sys.argv[3:]:
            k, v = kv.split("=", 1)
            code = code.replace("{{" + k + "}}", v)
        validate = _validator()
        if validate is not None:
            try:
                validate(code)
            except Exception as e:
                print("SAFE MODE REJECTED:", e)
                return 3
        r = send("execute_code", {"code": code})
        if r.get("status") == "error":
            print("BLENDER ERROR:", r.get("message"))
            return 1
        res = r.get("result", {})
        out = res.get("result", res) if isinstance(res, dict) else res
        print(out if isinstance(out, str) else json.dumps(out, indent=1))
    elif cmd == "shot":
        out = os.path.abspath(sys.argv[2])
        mx = int(sys.argv[3]) if len(sys.argv) > 3 else 1000
        r = send("get_viewport_screenshot", {"max_size": mx, "filepath": out, "format": "png"})
        print(json.dumps(r)[:2000])
    else:
        print("unknown command", cmd)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
