"""
blender_unreal MCP client bridge.

Launches the *same* MCP server that is configured as `blender_unreal`
(`uvx mcp-for-blender --port 9878`, BLENDER_MCP_SAFE_MODE=1, telemetry off)
over stdio and invokes one of its tools. Safe Mode validation therefore
happens inside the official MCP server exactly as for any other MCP client.

Usage:
  python blender_unreal_mcp.py <tool> [--code-file script.py] [--args-json '{...}']
                               [--out screenshot.png]
Examples:
  python blender_unreal_mcp.py get_scene_info
  python blender_unreal_mcp.py execute_blender_code --code-file build_pig.py
  python blender_unreal_mcp.py get_viewport_screenshot --out shot.png
  python blender_unreal_mcp.py export_scene --args-json "{\"filepath\": \"C:/x.fbx\", \"format\": \"fbx\"}"
"""
import argparse
import asyncio
import base64
import json
import os
import shutil
import sys

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

USER_PROMPT = ("Для важных 3D-ассетов обязательно используй Blender MCP blender_unreal, "
               "связанный с портом 9878 и файлом UnrealMinecraft.blend.")


def server_params():
    env = dict(os.environ)
    env.update({"BLENDER_HOST": "localhost", "BLENDER_MCP_SAFE_MODE": "1", "DISABLE_TELEMETRY": "true"})
    uvx = shutil.which("uvx")
    if uvx:
        return StdioServerParameters(command=uvx, args=["mcp-for-blender", "--port", "9878"], env=env)
    here = os.path.dirname(sys.executable)
    exe = os.path.join(here, "mcp-for-blender.exe")
    return StdioServerParameters(command=exe, args=["--port", "9878"], env=env)


async def run(tool, tool_args, out_path):
    async with stdio_client(server_params()) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            result = await session.call_tool(tool, tool_args)
            rc = 0
            for item in result.content:
                t = getattr(item, "type", "")
                if t == "text":
                    print(item.text)
                elif t == "image":
                    data = base64.b64decode(item.data)
                    path = out_path or "blender_screenshot.png"
                    with open(path, "wb") as f:
                        f.write(data)
                    print(f"[image saved] {path} ({len(data)} bytes)")
                else:
                    print(f"[{t}] {item}")
            if getattr(result, "isError", False):
                rc = 2
            return rc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tool")
    ap.add_argument("--code-file")
    ap.add_argument("--args-json")
    ap.add_argument("--out")
    ns = ap.parse_args()
    tool_args = json.loads(ns.args_json) if ns.args_json else {}
    if ns.code_file:
        with open(ns.code_file, "r", encoding="utf-8") as f:
            tool_args["code"] = f.read()
    if ns.tool in ("execute_blender_code", "get_scene_info", "get_viewport_screenshot", "export_scene",
                   "get_object_info"):
        tool_args.setdefault("user_prompt", USER_PROMPT)
    sys.exit(asyncio.run(run(ns.tool, tool_args, ns.out)))


if __name__ == "__main__":
    main()
