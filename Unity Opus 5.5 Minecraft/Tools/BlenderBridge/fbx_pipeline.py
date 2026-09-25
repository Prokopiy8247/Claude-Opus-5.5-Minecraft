"""Drive the Blender-side FBX builder (Tools/BlenderBridge/scripts/build_models.py) through blender_unity.py.

Blender's Safe Mode blocks file reads inside Blender, so the model JSON (Assets/_Game/Data/Models) is embedded into
the script text here before it is sent to 127.0.0.1:9876.

Usage (any working directory):
  python fbx_pipeline.py build [model ...]           copy skins, build + export FBX (all models when none are named)
  python fbx_pipeline.py layout [--save] [model ...]  show only these models (default all) in a grid, frame the view
Output: Assets/_Game/Generated/Models/<name>.fbx and Textures/<name>.png; then run
"Tools/Opus 5.5 Minecraft/Import Blender Models" in Unity (the Windows build runs it too).
"""
import filecmp
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
BRIDGE = os.path.join(HERE, "blender_unity.py")
SCRIPT = os.path.join(HERE, "scripts", "build_models.py")
DATA_DIR = os.path.join(ROOT, "Assets", "_Game", "Data", "Models")
SKIN_DIR = os.path.join(HERE, "export", "skins")
OUT_DIR = os.path.join(ROOT, "Assets", "_Game", "Generated", "Models")
TEX_DIR = os.path.join(OUT_DIR, "Textures")
BATCH = 25
FORWARD = "-Z"  # FBX forward axis, overridable with --forward= (Unity also reads it back from the file metadata)


def all_names():
    with open(os.path.join(DATA_DIR, "_index.json"), encoding="utf-8") as f:
        return sorted(json.load(f))


def model_json(names):
    parts = []
    for n in names:
        with open(os.path.join(DATA_DIR, n + ".json"), encoding="utf-8") as f:
            parts.append(f.read().strip())
    return "[" + ",".join(parts) + "]"


def send(names=(), export=False, layout=None, view=None, save=False):
    text = open(SCRIPT, encoding="utf-8").read()
    subst = {
        "PROJECT": ROOT.replace("\\", "/"),
        "MODELS": model_json(names) if names else "[]",
        "EXPORT": "True" if export else "False",
        "LAYOUT": json.dumps(layout) if layout else "null",
        "VIEW": json.dumps(view) if view else "null",
        "SAVE": "True" if save else "False",
        "AXIS_FORWARD": FORWARD,
    }
    for k, v in subst.items():
        text = text.replace("{{" + k + "}}", v)
    tmp = os.path.join(ROOT, "Tools", "_out", "_blender_send.py")
    os.makedirs(os.path.dirname(tmp), exist_ok=True)
    with open(tmp, "w", encoding="utf-8") as f:
        f.write(text)
    r = subprocess.run([sys.executable, BRIDGE, "exec", tmp], cwd=HERE, capture_output=True, text=True)
    result = None
    for line in r.stdout.splitlines():
        if line.startswith("RESULT "):
            result = json.loads(line[7:])
        elif line.strip():
            print(line)
    if r.stderr.strip():
        print(r.stderr.strip(), file=sys.stderr)
    if r.returncode != 0 or result is None:
        raise SystemExit("Blender call failed (exit %d)" % r.returncode)
    return result


def copy_skins(names):
    os.makedirs(TEX_DIR, exist_ok=True)
    for n in names:
        src, dst = os.path.join(SKIN_DIR, n + ".png"), os.path.join(TEX_DIR, n + ".png")
        if not os.path.exists(src):
            raise SystemExit("missing skin " + src)
        if not os.path.exists(dst) or not filecmp.cmp(src, dst, shallow=False):
            shutil.copyfile(src, dst)


def build(names):
    copy_skins(names)
    total = {}
    for i in range(0, len(names), BATCH):
        chunk = names[i:i + BATCH]
        total.update(send(names=chunk, export=True))
        print("built %d/%d" % (min(i + BATCH, len(names)), len(names)))
    lines = []
    for n in names:
        r = total.get(n, {})
        fbx = os.path.join(OUT_DIR, n + ".fbx")
        ok = os.path.exists(fbx) and os.path.getsize(fbx) > 0
        lines.append("%-20s fbx=%s tris=%s verts=%s flipped=%s degenerate=%s" % (
            n, "ok" if ok else "MISSING", r.get("tris"), r.get("verts"), r.get("flipped"), r.get("degenerate")))
    report = os.path.join(ROOT, "Tools", "_out", "blender_models.txt")
    with open(report, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines))
    bad = [l for l in lines if "MISSING" in l]
    print("exported %d/%d FBX -> %s" % (len(names) - len(bad), len(names), OUT_DIR))
    return 1 if bad else 0


def main():
    global FORWARD
    args = sys.argv[1:]
    for a in args:
        if a.startswith("--forward="):
            FORWARD = a.split("=", 1)[1]
    cmd = args[0] if args else "build"
    rest = [a for a in args[1:] if not a.startswith("--")]
    if cmd == "build":
        return build(rest or all_names())
    if cmd == "layout":
        names = rest or all_names()
        send(layout={"names": names, "gap": 1.0, "row_width": 40.0},
             view={"names": names, "euler": [72.0, 0.0, 24.0], "zoom": 1.15, "shading": "MATERIAL"},
             save="--save" in args)
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
