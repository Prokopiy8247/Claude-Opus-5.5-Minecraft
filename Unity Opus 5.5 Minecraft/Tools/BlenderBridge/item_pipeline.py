"""Drive the Blender-side item / prop builder (Tools/BlenderBridge/scripts/build_items.py) through blender_unity.py.

Inputs come from the Unity editor (MCR.EditorTools.BatchTools.ExportItemSprites, also run by every Windows build):
export/items/<id>.png + _index.json (the 16x16 item sprites exactly as the game draws them) and export/props/<texture>.png
+ _index.json (the block textures of the torch, lantern and campfire props). Blender's Safe Mode blocks file reads, so
the opacity mask of every sprite (alpha > 127, like ItemRender.ExtrudedMesh) is decoded here and embedded into the
script text before it is sent to 127.0.0.1:9876.

Usage (any working directory):
  python item_pipeline.py build [id ...]   copy the sprites, build + export the voxel item FBX (all exported when none named)
  python item_pipeline.py props [id ...]   copy the textures, build + export the prop FBX (all six when none named)
  python item_pipeline.py all              both, lay the MCR_Items / MCR_Props gallery out and save the .blend
  python item_pipeline.py view             frame the gallery with MCR_Models hidden (for `blender_unity.py shot`)
  python item_pipeline.py restore          show MCR_Models again and save the .blend
Output: Assets/_Game/Generated/Models/Items/<id>.fbx (+ Textures/<id>.png), Models/Props/<id>.fbx (+ Textures/<texture>.png),
report Tools/_out/blender_items.txt. Unity imports them with MCR.EditorTools.ItemModelImporter (the Windows build runs it).
"""
import filecmp
import json
import os
import shutil
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
BRIDGE = os.path.join(HERE, "blender_unity.py")
SCRIPT = os.path.join(HERE, "scripts", "build_items.py")
ITEM_SRC = os.path.join(HERE, "export", "items")
PROP_SRC = os.path.join(HERE, "export", "props")
OUT = os.path.join(ROOT, "Assets", "_Game", "Generated", "Models")
ITEM_OUT = os.path.join(OUT, "Items")
PROP_OUT = os.path.join(OUT, "Props")
PROPS = ["torch", "soul_torch", "lantern", "soul_lantern", "campfire", "soul_campfire"]
BATCH = 25
FORWARD = "-Z"   # same FBX axis convention as fbx_pipeline.py


def index(folder):
    path = os.path.join(folder, "_index.json")
    if not os.path.exists(path):
        raise SystemExit("missing %s: run MCR.EditorTools.BatchTools.ExportItemSprites in Unity first" % path)
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def png_alpha(path):
    """Alpha of every pixel (rows top-down) of an 8-bit non-interlaced PNG, as Unity's EncodeToPNG writes them."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("not a PNG: " + path)
    pos, idat, w, h, ctype = 8, b"", 0, 0, 0
    while pos < len(data):
        n = struct.unpack(">I", data[pos:pos + 4])[0]
        kind, body = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + n]
        pos += 12 + n
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or interlace != 0 or ctype not in (2, 6):
                raise SystemExit("unsupported PNG (depth %d, colour type %d, interlace %d): %s" % (depth, ctype, interlace, path))
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    bpp = 4 if ctype == 6 else 3
    raw, stride = zlib.decompress(idat), w * bpp
    rows, prev, i = [], bytearray(stride), 0
    for _ in range(h):
        filt, line = raw[i], bytearray(raw[i + 1:i + 1 + stride])
        i += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if filt == 1:
                line[x] = (line[x] + a) & 255
            elif filt == 2:
                line[x] = (line[x] + b) & 255
            elif filt == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif filt == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line)
        prev = line
    return [[(r[x * bpp + 3] if bpp == 4 else 255) for x in range(w)] for r in rows]


def mask(path):
    alpha = png_alpha(path)
    if len(alpha) != 16 or any(len(r) != 16 for r in alpha):
        raise SystemExit("item sprite is not 16x16: " + path)
    return ["".join("1" if a > 127 else "0" for a in row) for row in alpha]


def copy_pngs(src_dir, names, dst_dir):
    os.makedirs(dst_dir, exist_ok=True)
    for n in names:
        src, dst = os.path.join(src_dir, n + ".png"), os.path.join(dst_dir, n + ".png")
        if not os.path.exists(src):
            raise SystemExit("missing " + src)
        if not os.path.exists(dst) or not filecmp.cmp(src, dst, shallow=False):
            shutil.copyfile(src, dst)


def send(items=(), props=(), textures=(), export=False, layout=False, view=None, show_models=None, save=False, order=()):
    with open(SCRIPT, encoding="utf-8") as f:
        text = f.read()
    subst = {
        "PROJECT": ROOT.replace("\\", "/"),
        "ITEMS": json.dumps(list(items)),
        "PROPS": json.dumps(list(props)),
        "TEXTURES": json.dumps(list(textures)),
        "EXPORT": "True" if export else "False",
        "LAYOUT": "True" if layout else "False",
        "VIEW": json.dumps(view) if view else "null",
        "SHOW_MODELS": json.dumps(show_models),
        "SAVE": "True" if save else "False",
        "AXIS_FORWARD": FORWARD,
        "ITEM_ORDER": json.dumps(list(order)),
    }
    for k, v in subst.items():
        text = text.replace("{{" + k + "}}", v)
    tmp = os.path.join(ROOT, "Tools", "_out", "_blender_items_send.py")
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


def build_items(ids):
    known = index(ITEM_SRC)
    ids = ids or known
    unknown = [i for i in ids if i not in known]
    if unknown:
        raise SystemExit("not exported by ExportItemSprites: " + " ".join(unknown))
    copy_pngs(ITEM_SRC, ids, os.path.join(ITEM_OUT, "Textures"))
    total = {}
    for i in range(0, len(ids), BATCH):
        chunk = [{"id": n, "mask": mask(os.path.join(ITEM_SRC, n + ".png"))} for n in ids[i:i + BATCH]]
        total.update(send(items=chunk, export=True)["items"])
        print("items built %d/%d" % (min(i + BATCH, len(ids)), len(ids)))
    return report_rows(ids, total, ITEM_OUT)


def build_props(ids):
    textures = index(PROP_SRC)
    ids = ids or PROPS
    copy_pngs(PROP_SRC, textures, os.path.join(PROP_OUT, "Textures"))
    res = send(props=ids, textures=textures, export=True)
    for s in res["skipped"]:
        print("prop skipped: " + s)
    return report_rows([p for p in ids if p in res["props"]], res["props"], PROP_OUT)


def report_rows(names, results, out_dir):
    rows = []
    for n in names:
        r = results.get(n, {})
        fbx = os.path.join(out_dir, n + ".fbx")
        ok = os.path.exists(fbx) and os.path.getsize(fbx) > 0
        rows.append("%-20s fbx=%s quads=%s tris=%s verts=%s flipped=%s degenerate=%s materials=%s" % (
            n, "ok" if ok else "MISSING", r.get("quads"), r.get("tris"), r.get("verts"), r.get("flipped"),
            r.get("degenerate"), ",".join(r.get("materials", []))))
    return rows


def write_report(sections):
    lines = []
    for title, rows in sections:
        lines.append("%s %d" % (title, len(rows)))
        lines.extend(rows)
    path = os.path.join(ROOT, "Tools", "_out", "blender_items.txt")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("\n".join(lines))
    return 1 if any("MISSING" in l or "flipped=0" not in l for _, rows in sections for l in rows) else 0


VIEW = {"euler": [80.0, 0.0, 200.0], "zoom": 1.05, "shading": "MATERIAL"}


def main():
    args = sys.argv[1:]
    cmd = args[0] if args else "all"
    rest = [a for a in args[1:] if not a.startswith("--")]
    if cmd == "build":
        return write_report([("ITEMS", build_items(rest))])
    if cmd == "props":
        return write_report([("PROPS", build_props(rest))])
    if cmd == "all":
        code = write_report([("ITEMS", build_items([])), ("PROPS", build_props([]))])
        send(layout=True, order=index(ITEM_SRC), save=True)
        return code
    if cmd == "view":
        send(layout=True, order=index(ITEM_SRC), view=VIEW, show_models=False)
        return 0
    if cmd == "restore":
        send(show_models=True, save=True)
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
