"""Builds every hero asset in Blender through the blender_unreal MCP server.

    python Tools/BlenderMCP/run_build.py [--only rig1,rig2] [--batch 14] [--no-entities]

For each batch of rigs it prepends the matching slice of rig_table.py to
build_assets.py and sends the script with blender_unreal_mcp.py
(uvx mcp-for-blender --port 9878, BLENDER_MCP_SAFE_MODE=1), so every script is
validated by the MCP server's Safe Mode before it reaches Blender.  Batches keep
each script inside Safe Mode's size limits and under the 180 s socket timeout.
"""
import argparse
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))


def load_table():
    ns = {}
    with open(os.path.join(HERE, "rig_table.py"), encoding="utf-8") as f:
        exec(compile(f.read(), "rig_table.py", "exec"), ns)
    return ns["RIG_TABLE"]


def send(code, label):
    with tempfile.NamedTemporaryFile("w", suffix=".py", delete=False, encoding="utf-8") as tmp:
        tmp.write(code)
        path = tmp.name
    cmd = ["uv", "run", "--quiet", "--with", "mcp", "python", os.path.join(HERE, "blender_unreal_mcp.py"),
           "execute_blender_code", "--code-file", path]
    proc = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=900)
    os.unlink(path)
    out = [l for l in (proc.stdout + proc.stderr).splitlines() if " - INFO - " not in l and " - WARNING - " not in l]
    text = "\n".join(out)
    ok = "OPUS55_BATCH" in text
    print("[%s] %s" % (label, "ok" if ok else "FAILED"))
    for line in out:
        if "OPUS55_BATCH" in line or "Error" in line or "Rejected" in line or "Traceback" in line:
            print("    " + line[:400])
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", default="")
    ap.add_argument("--batch", type=int, default=14)
    ap.add_argument("--no-entities", action="store_true")
    args = ap.parse_args()

    table = load_table()
    if args.only:
        wanted = set(args.only.split(","))
        indexed = [(i, e) for i, e in enumerate(table) if e[0] in wanted]
    else:
        indexed = list(enumerate(table))
    with open(os.path.join(HERE, "build_assets.py"), encoding="utf-8") as f:
        builder = f.read()

    # make sure every export folder exists (the FBX exporter does not create folders and
    # Safe Mode keeps os.makedirs out of the Blender-side script)
    for _, (rig, _) in indexed:
        os.makedirs(os.path.join(ROOT, "Saved", "Opus55Fbx", "Mobs", rig), exist_ok=True)
    os.makedirs(os.path.join(ROOT, "Saved", "Opus55Fbx", "Entities"), exist_ok=True)

    batches = [indexed[i:i + args.batch] for i in range(0, len(indexed), args.batch)]
    failures = 0
    for bi, batch in enumerate(batches):
        last = bi == len(batches) - 1
        header = "RIG_TABLE = %r\nBATCH_START = %d\nBATCH_LAST = %r\nENTITY_PASS = %r\n" % (
            [e for _, e in batch], batch[0][0], last, last and not args.no_entities)
        # the builder's docstring stays at the top so Blender's text shows provenance
        code = header + builder
        label = "batch %d/%d: %s" % (bi + 1, len(batches), ", ".join(e[0] for _, e in batch))
        if not send(code, label):
            failures += 1
    print("done, %d failed batch(es)" % failures)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
