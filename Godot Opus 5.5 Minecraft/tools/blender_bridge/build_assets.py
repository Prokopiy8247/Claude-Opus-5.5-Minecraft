"""Asset pipeline step 2/3: authors the project's 3D assets in Blender through the blender_godot
MCP connection (addon socket on port 9877) and exports them as GLB files for Godot.

Inputs  (written by Godot, step 1: res://game/editor/blender_export.tscn)
  game/generated/blender_src/mobs.json + mobs/<mob>.png    mob rigs + box-UV texture atlases
  game/generated/blender_src/items.json + items/<item>.png held item sprites
  game/generated/blender_src/props/<texture>.png           block textures for props
Outputs
  GodotMinecraft.blend  (collections Mobs/*, Items/*, Props/*)
  game/generated/models/mobs/<mob>.glb
  game/generated/models/items/<item>.glb
  game/generated/models/entities/<prop>.glb

Every script sent to Blender passes the MCP server's Safe Mode validator first (bridge.validate);
data is embedded as JSON literals because Safe Mode forbids file I/O other than bpy loaders.

Usage:  python tools/blender_bridge/build_assets.py [mobs|items|props|all] [name ...]
"""

import json
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import bridge  # noqa: E402

SRC = os.path.join(ROOT, "game", "generated", "blender_src")
OUT = os.path.join(ROOT, "game", "generated", "models")
BLEND = os.path.join(ROOT, "GodotMinecraft.blend")


def fwd(p):
    return p.replace("\\", "/")


# ------------------------------------------------------------------------------------------------
# Coordinate conversion: Godot (x, y, z) with Y up  ->  Blender (x, -z, y) with Z up.
# The glTF exporter (+Y up) applies the inverse, so the GLB arrives in Godot unchanged.
def g2b(v):
    return [v[0], -v[2], v[1]]


def basis_g2b(cols):
    """cols = Godot basis columns x, y, z (9 floats). Returns a Blender 3x3 row-major matrix C*B*C^T."""
    bx = cols[0:3]
    by = cols[3:6]
    bz = cols[6:9]
    # Godot matrix M (row-major) from its columns
    m = [[bx[0], by[0], bz[0]], [bx[1], by[1], bz[1]], [bx[2], by[2], bz[2]]]
    c = [[1, 0, 0], [0, 0, -1], [0, 1, 0]]
    ct = [[1, 0, 0], [0, 0, 1], [0, -1, 0]]

    def mul(a, b):
        return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]

    return mul(mul(c, m), ct)


def box_faces(w, h, d):
    """Godot MobRenderer.box_mesh_uv, as quads [corners BL,BR,TR,TL (CCW from outside), uvs]."""
    tw = 2.0 * d + 2.0 * w
    th = d + h
    hw, hh, hd = w / 32.0, h / 32.0, d / 32.0
    rects = {
        "top": (d / tw, 0.0, w / tw, d / th),
        "bottom": ((d + w) / tw, 0.0, w / tw, d / th),
        "right": (0.0, d / th, d / tw, h / th),
        "front": (d / tw, d / th, w / tw, h / th),
        "left": ((d + w) / tw, d / th, d / tw, h / th),
        "back": ((2.0 * d + w) / tw, d / th, w / tw, h / th),
    }
    faces = [
        ("front", (hw, -hh, -hd), (-hw, -hh, -hd), (-hw, hh, -hd), (hw, hh, -hd)),
        ("back", (-hw, -hh, hd), (hw, -hh, hd), (hw, hh, hd), (-hw, hh, hd)),
        ("right", (hw, -hh, hd), (hw, -hh, -hd), (hw, hh, -hd), (hw, hh, hd)),
        ("left", (-hw, -hh, -hd), (-hw, -hh, hd), (-hw, hh, hd), (-hw, hh, -hd)),
        ("top", (hw, hh, -hd), (-hw, hh, -hd), (-hw, hh, hd), (hw, hh, hd)),
        ("bottom", (-hw, -hh, -hd), (hw, -hh, -hd), (hw, -hh, hd), (-hw, -hh, hd)),
    ]
    out = []
    for f in faces:
        rx, ry, rw, rh = rects[f[0]]
        uvs = [(rx, ry + rh), (rx + rw, ry + rh), (rx + rw, ry), (rx, ry)]
        out.append((f[1:], uvs))
    return out


def mob_mesh(part, atlas_w, atlas_h):
    """Vertex/face/uv lists (Blender space) for one part box, UVs mapped into the mob atlas."""
    w, h, d = part["size"]
    rx, ry, rw, rh = part["rect"]
    verts, faces, uvs = [], [], []
    for corners, fuv in box_faces(w, h, d):
        base = len(verts)
        for c in corners:
            verts.append([round(x, 6) for x in g2b(c)])
        faces.append([base, base + 1, base + 2, base + 3])
        for (u, v) in fuv:
            au = (rx + u * rw) / atlas_w
            av = (ry + v * rh) / atlas_h
            uvs.append([round(au, 6), round(1.0 - av, 6)])
    return verts, faces, uvs


# ------------------------------------------------------------------------------------------------
# Minimal PNG reader (RGBA8 / RGB8, non-interlaced) so the driver has no third-party dependency.
def read_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", path
    pos = 8
    width = height = 0
    ctype = 6
    idat = b""
    while pos < len(data):
        ln = struct.unpack(">I", data[pos:pos + 4])[0]
        tag = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + ln]
        pos += 12 + ln
        if tag == b"IHDR":
            width, height, depth, ctype, _, _, inter = struct.unpack(">IIBBBBB", body)
            assert depth == 8 and inter == 0, "unsupported png " + path
        elif tag == b"IDAT":
            idat += body
        elif tag == b"IEND":
            break
    ch = {6: 4, 2: 3, 0: 1, 4: 2}[ctype]
    raw = zlib.decompress(idat)
    stride = width * ch
    rows = []
    prev = bytearray(stride)
    i = 0
    for _ in range(height):
        ft = raw[i]
        i += 1
        line = bytearray(raw[i:i + stride])
        i += stride
        for x in range(stride):
            a = line[x - ch] if x >= ch else 0
            b = prev[x]
            c = prev[x - ch] if x >= ch else 0
            if ft == 1:
                line[x] = (line[x] + a) & 255
            elif ft == 2:
                line[x] = (line[x] + b) & 255
            elif ft == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif ft == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[x] = (line[x] + pr) & 255
        rows.append(bytes(line))
        prev = line

    def px(x, y):
        r = rows[y]
        if ch == 4:
            return r[x * 4 + 3]
        return 255

    return width, height, px


def item_mesh(path):
    """Extruded pixel mesh identical to ItemIcons.extruded_mesh (1 unit wide, 1/16 thick)."""
    w, h, alpha = read_png(path)
    p = 1.0 / 16.0
    th = p * 0.5
    verts, faces, uvs = [], [], []

    def quad(a, b, c, d, ua, ub, uc, ud):
        base = len(verts)
        for v in (a, b, c, d):
            verts.append([round(x, 6) for x in g2b(v)])
        faces.append([base, base + 1, base + 2, base + 3])
        for u in (ua, ub, uc, ud):
            uvs.append([round(u[0], 6), round(1.0 - u[1], 6)])

    quad((-0.5, -0.5, th), (0.5, -0.5, th), (0.5, 0.5, th), (-0.5, 0.5, th), (0, 1), (1, 1), (1, 0), (0, 0))
    quad((0.5, -0.5, -th), (-0.5, -0.5, -th), (-0.5, 0.5, -th), (0.5, 0.5, -th), (1, 1), (0, 1), (0, 0), (1, 0))
    for y in range(h):
        for x in range(w):
            if alpha(x, y) < 128:
                continue
            x0 = -0.5 + x * p
            x1 = x0 + p
            y1 = 0.5 - y * p
            y0 = y1 - p
            uv = ((x + 0.5) / w, (y + 0.5) / h)
            if x == 0 or alpha(x - 1, y) < 128:
                quad((x0, y0, -th), (x0, y0, th), (x0, y1, th), (x0, y1, -th), uv, uv, uv, uv)
            if x == w - 1 or alpha(x + 1, y) < 128:
                quad((x1, y0, th), (x1, y0, -th), (x1, y1, -th), (x1, y1, th), uv, uv, uv, uv)
            if y == 0 or alpha(x, y - 1) < 128:
                quad((x0, y1, th), (x1, y1, th), (x1, y1, -th), (x0, y1, -th), uv, uv, uv, uv)
            if y == h - 1 or alpha(x, y + 1) < 128:
                quad((x0, y0, -th), (x1, y0, -th), (x1, y0, th), (x0, y0, th), uv, uv, uv, uv)
    return verts, faces, uvs


# ------------------------------------------------------------------------------------------------
# Blender-side code (Safe Mode compliant: bpy/bmesh/mathutils/json/math only, no file access)
BLENDER_LIB = r'''
import bpy
import json
from mathutils import Matrix, Vector


def coll(path):
    """Nested collection by 'A/B' path, linked under the scene collection."""
    parent = bpy.context.scene.collection
    cur = None
    for name in path.split("/"):
        cur = bpy.data.collections.get(name)
        if cur is None:
            cur = bpy.data.collections.new(name)
        if cur.name not in [c.name for c in parent.children]:
            parent.children.link(cur)
        parent = cur
    return cur


def clear_coll(c):
    for ob in list(c.objects):
        bpy.data.objects.remove(ob, do_unlink=True)


def material(name, image_path, image_name):
    mat = bpy.data.materials.get(name)
    if mat is None:
        mat = bpy.data.materials.new(name)
    try:
        mat.use_nodes = True
    except Exception:
        pass
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    tex = nt.nodes.new("ShaderNodeTexImage")
    img = bpy.data.images.get(image_name)
    if img is None:
        img = bpy.data.images.load(image_path, check_existing=True)
        img.name = image_name
    else:
        img.filepath = image_path
        img.reload()
    tex.image = img
    tex.interpolation = "Closest"
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    bsdf.inputs["Roughness"].default_value = 1.0
    try:
        mat.blend_method = "CLIP"
    except Exception:
        pass
    return mat


def mesh_object(name, verts, faces, uvs, mat, c):
    me = bpy.data.meshes.get(name + "_mesh")
    if me is not None:
        bpy.data.meshes.remove(me)
    me = bpy.data.meshes.new(name + "_mesh")
    me.from_pydata(verts, [], faces)
    uvl = me.uv_layers.new(name="UVMap")
    for i, loop in enumerate(me.loops):
        uvl.data[i].uv = uvs[loop.vertex_index]
    me.materials.append(mat)
    me.update()
    ob = bpy.data.objects.new(name, me)
    c.objects.link(ob)
    return ob


def empty(name, c):
    ob = bpy.data.objects.new(name, None)
    ob.empty_display_type = "PLAIN_AXES"
    ob.empty_display_size = 0.1
    c.objects.link(ob)
    return ob


def export_tree(root, path):
    for ob in bpy.context.view_layer.objects:
        ob.select_set(False)
    stack = [root]
    while stack:
        o = stack.pop()
        o.select_set(True)
        stack.extend(o.children)
    bpy.context.view_layer.objects.active = root
    bpy.ops.export_scene.gltf(filepath=path, export_format="GLB", use_selection=True, export_yup=True,
                              export_apply=False, export_animations=False)
'''

BUILD_MOB = r'''
def build_mob(mob, entry, out_dir, tex_dir):
    c = coll("Mobs/mob_" + mob)
    clear_coll(c)
    mat = material(mob + "__mat", tex_dir + "/" + mob + ".png", mob + "_tex")
    root = empty(mob + "__root", c)
    objs = {}
    for p in entry["parts"]:
        h = empty(mob + "__" + p["name"], c)
        parent = objs.get(p["parent"], root) if p["parent"] else root
        h.parent = parent
        m = p["m"]
        h.matrix_basis = Matrix(((m[0][0], m[0][1], m[0][2], p["o"][0]),
                                 (m[1][0], m[1][1], m[1][2], p["o"][1]),
                                 (m[2][0], m[2][1], m[2][2], p["o"][2]),
                                 (0.0, 0.0, 0.0, 1.0)))
        box = mesh_object(mob + "__" + p["name"] + "__box", p["v"], p["f"], p["uv"], mat, c)
        box.parent = h
        box.location = Vector(p["bp"])
        objs[p["name"]] = h
    export_tree(root, out_dir + "/" + mob + ".glb")
    return len(entry["parts"])
'''

BUILD_ITEM = r'''
def build_item(name, entry, out_dir, tex_dir):
    c = coll("Items/item_" + name)
    clear_coll(c)
    mat = material(name + "__mat", tex_dir + "/" + name + ".png", name + "_tex")
    root = empty(name + "__root", c)
    ob = mesh_object(name + "__mesh", entry["v"], entry["f"], entry["uv"], mat, c)
    ob.parent = root
    export_tree(root, out_dir + "/" + name + ".glb")
    return len(entry["f"])
'''


def run(code):
    validate = bridge._load_validator()
    if validate is not None:
        validate(code)
    resp = bridge.send("execute_code", {"code": code}, timeout=900.0)
    if resp.get("status") == "error":
        raise RuntimeError(resp.get("message", "blender error")[:2000])
    res = resp.get("result", {})
    return res.get("result", res) if isinstance(res, dict) else res


def build_mobs(names=None, batch=6):
    data = json.load(open(os.path.join(SRC, "mobs.json"), encoding="utf-8"))
    out_dir = fwd(os.path.join(OUT, "mobs"))
    os.makedirs(out_dir, exist_ok=True)
    tex_dir = fwd(os.path.join(SRC, "mobs"))
    todo = [n for n in sorted(data.keys()) if not names or n in names]
    total = 0
    for i in range(0, len(todo), batch):
        chunk = {}
        for mob in todo[i:i + batch]:
            e = data[mob]
            aw, ah = e["atlas"]
            parts = []
            for p in e["parts"]:
                v, f, uv = mob_mesh(p, aw, ah)
                parts.append({
                    "name": p["name"], "parent": p["parent"],
                    "m": [[round(x, 6) for x in row] for row in basis_g2b(p["basis"])],
                    "o": [round(x, 6) for x in g2b(p["origin"])],
                    "bp": [round(x, 6) for x in g2b(p["box_pos"])],
                    "v": v, "f": f, "uv": uv,
                })
            chunk[mob] = {"parts": parts}
        code = BLENDER_LIB + BUILD_MOB + (
            "\nDATA = json.loads(%r)\nres = []\nfor k in sorted(DATA.keys()):\n"
            "    res.append('%%s:%%d' %% (k, build_mob(k, DATA[k], %r, %r)))\nprint(' '.join(res))\n"
        ) % (json.dumps(chunk, separators=(",", ":")), out_dir, tex_dir)
        print(run(code).strip())
        total += len(chunk)
    return total


def build_items(names=None, batch=12):
    data = json.load(open(os.path.join(SRC, "items.json"), encoding="utf-8"))
    out_dir = fwd(os.path.join(OUT, "items"))
    os.makedirs(out_dir, exist_ok=True)
    tex_dir = fwd(os.path.join(SRC, "items"))
    todo = [n for n in sorted(data.keys()) if not names or n in names]
    for i in range(0, len(todo), batch):
        chunk = {}
        for n in todo[i:i + batch]:
            v, f, uv = item_mesh(os.path.join(SRC, "items", n + ".png"))
            chunk[n] = {"v": v, "f": f, "uv": uv}
        code = BLENDER_LIB + BUILD_ITEM + (
            "\nDATA = json.loads(%r)\nres = []\nfor k in sorted(DATA.keys()):\n"
            "    res.append('%%s:%%d' %% (k, build_item(k, DATA[k], %r, %r)))\nprint(' '.join(res))\n"
        ) % (json.dumps(chunk, separators=(",", ":")), out_dir, tex_dir)
        print(run(code).strip())
    return len(todo)


def save_blend():
    code = "import bpy\nbpy.ops.wm.save_as_mainfile(filepath=%r)\nprint('saved', bpy.data.filepath)\n" % fwd(BLEND)
    print(run(code).strip())


def main():
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    names = sys.argv[2:] or None
    if what in ("mobs", "all"):
        print("mobs built:", build_mobs(names))
    if what in ("items", "all"):
        print("items built:", build_items(names))
    if what in ("props", "all"):
        import build_props
        print("props built:", build_props.build(run, BLENDER_LIB))
    save_blend()


if __name__ == "__main__":
    main()
