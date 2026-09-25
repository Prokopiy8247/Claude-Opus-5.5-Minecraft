"""Props authored in Blender: boats (every wood, with and without chest), minecart variants,
the chest (base + hinged lid + latch) and the End crystal (bedrock base + three nested cubes).
Each part is a box textured with a block tile (1 texel per pixel); pivot nodes are named so the
game can animate lids, paddles and crystal cubes.

Called from build_assets.py:  python tools/blender_bridge/build_assets.py props
"""

import json
import os

from build_assets import OUT, SRC, fwd, g2b

BUILD_PROP = r'''
def build_prop(name, entry, out_dir, tex_dir):
    c = coll("Props/prop_" + name)
    clear_coll(c)
    root = empty(name + "__root", c)
    objs = {}
    for p in entry["parts"]:
        h = empty(name + "__" + p["name"], c)
        h.parent = objs.get(p["parent"], root) if p["parent"] else root
        h.location = Vector(p["t"])
        mat = material("tile__" + p["tex"], tex_dir + "/" + p["tex"] + ".png", "tile_" + p["tex"])
        box = mesh_object(name + "__" + p["name"] + "__box", p["v"], p["f"], p["uv"], mat, c)
        box.parent = h
        box.location = Vector(p["bp"])
        objs[p["name"]] = h
    export_tree(root, out_dir + "/" + name + ".glb")
    return len(entry["parts"])
'''

RUNNER = """
DATA = json.loads(%r)
res = []
for k in sorted(DATA.keys()):
    res.append('%%s:%%d' %% (k, build_prop(k, DATA[k], %r, %r)))
print(' '.join(res))
"""


def prop_box(size_px):
    """Box of size (w,h,d) pixels, centred; each face samples the tile at 1 texel per pixel."""
    w, h, d = size_px
    hw, hh, hd = w / 32.0, h / 32.0, d / 32.0
    faces = [
        ((hw, -hh, -hd), (-hw, -hh, -hd), (-hw, hh, -hd), (hw, hh, -hd), w, h),
        ((-hw, -hh, hd), (hw, -hh, hd), (hw, hh, hd), (-hw, hh, hd), w, h),
        ((hw, -hh, hd), (hw, -hh, -hd), (hw, hh, -hd), (hw, hh, hd), d, h),
        ((-hw, -hh, -hd), (-hw, -hh, hd), (-hw, hh, hd), (-hw, hh, -hd), d, h),
        ((hw, hh, -hd), (-hw, hh, -hd), (-hw, hh, hd), (hw, hh, hd), w, d),
        ((-hw, -hh, -hd), (hw, -hh, -hd), (hw, -hh, hd), (-hw, -hh, hd), w, d),
    ]
    verts, fs, uvs = [], [], []
    for f in faces:
        base = len(verts)
        for c in f[0:4]:
            verts.append([round(x, 6) for x in g2b(c)])
        fs.append([base, base + 1, base + 2, base + 3])
        uw = min(1.0, f[4] / 16.0)
        vh = min(1.0, f[5] / 16.0)
        for (u, v) in ((0.0, 0.0), (uw, 0.0), (uw, vh), (0.0, vh)):
            uvs.append([round(u, 6), round(v, 6)])
    return verts, fs, uvs


def part(name, size, center_px, tex, parent="", pivot_px=None):
    """Box of `size` px centred at `center_px` (Godot space, pixels); optional own pivot."""
    piv = pivot_px if pivot_px is not None else center_px
    v, f, uv = prop_box(size)
    off = [(center_px[i] - piv[i]) / 16.0 for i in range(3)]
    return {
        "name": name, "parent": parent, "tex": tex,
        "t": [round(x, 6) for x in g2b([piv[0] / 16.0, piv[1] / 16.0, piv[2] / 16.0])],
        "bp": [round(x, 6) for x in g2b(off)], "v": v, "f": f, "uv": uv,
    }


def defs():
    woods = ["oak", "spruce", "birch", "jungle", "acacia", "dark_oak", "mangrove", "cherry", "pale_oak", "bamboo"]
    props = {}
    for wd in woods:
        tex = wd + "_planks"
        for chest in (False, True):
            parts = [
                part("hull", [20, 3, 28], [0, 1.5, 0], tex),
                part("side_l", [2, 6, 28], [-9, 5, 0], tex),
                part("side_r", [2, 6, 28], [9, 5, 0], tex),
                part("bow", [16, 6, 2], [0, 5, -13], tex),
                part("stern", [16, 6, 2], [0, 5, 13], tex),
                part("seat", [16, 2, 4], [0, 6, 2], tex),
                part("paddle_l", [1, 1, 18], [-11, 8, 0], tex, "", [-10, 8, 0]),
                part("paddle_r", [1, 1, 18], [11, 8, 0], tex, "", [10, 8, 0]),
            ]
            if chest:
                parts.append(part("chest", [12, 11, 12], [0, 8.5, 7], "oak_planks"))
            props[("chest_boat_" if chest else "boat_") + wd] = parts
    cart = [
        part("floor", [16, 2, 20], [0, 3, 0], "iron_block"),
        part("wall_n", [16, 8, 2], [0, 8, -9], "iron_block"),
        part("wall_s", [16, 8, 2], [0, 8, 9], "iron_block"),
        part("wall_w", [2, 8, 16], [-7, 8, 0], "iron_block"),
        part("wall_e", [2, 8, 16], [7, 8, 0], "iron_block"),
        part("axle_f", [18, 2, 2], [0, 1, -6], "stone"),
        part("axle_b", [18, 2, 2], [0, 1, 6], "stone"),
    ]
    props["minecart"] = cart
    props["chest_minecart"] = cart + [part("cargo", [12, 12, 12], [0, 10, 0], "oak_planks")]
    props["furnace_minecart"] = cart + [part("cargo", [12, 12, 12], [0, 10, 0], "furnace_front")]
    props["tnt_minecart"] = cart + [part("cargo", [12, 12, 12], [0, 10, 0], "tnt_side")]
    props["hopper_minecart"] = cart + [part("cargo", [12, 10, 12], [0, 9, 0], "hopper_outside")]
    props["chest"] = [
        part("base", [14, 10, 14], [0, 5, 0], "oak_planks"),
        part("lid", [14, 5, 14], [0, 11.5, 0], "oak_planks", "", [0, 9, 7]),
        part("latch", [2, 4, 1], [0, 9, -7.5], "iron_block", "lid"),
    ]
    # worn armor pieces, authored around the biped part pivots (the game swaps the material tile)
    a = "armor_iron"
    props["armor_helmet"] = [
        part("top", [10, 2, 10], [0, 4.5, 0], a),
        part("back", [10, 8, 1], [0, 0.5, 4.5], a),
        part("side_l", [1, 8, 10], [-4.5, 0.5, 0], a),
        part("side_r", [1, 8, 10], [4.5, 0.5, 0], a),
        part("brow", [10, 2, 1], [0, 3, -4.5], a),
    ]
    props["armor_chestplate"] = [
        part("body", [10, 14, 6], [0, 0, 0], a),
        part("shoulder_l", [6, 4, 6], [-7, 5, 0], a),
        part("shoulder_r", [6, 4, 6], [7, 5, 0], a),
    ]
    props["armor_leggings"] = [
        part("leg", [5, 10, 5], [0, -5, 0], a),
    ]
    props["armor_boots"] = [
        part("boot", [6, 5, 6], [0, -9.5, 0], a),
    ]
    props["end_crystal"] = [
        part("base", [12, 4, 12], [0, 2, 0], "bedrock"),
        part("outer", [16, 16, 16], [0, 20, 0], "glass", "", [0, 20, 0]),
        part("middle", [12, 12, 12], [0, 20, 0], "purpur_block", "", [0, 20, 0]),
        part("core", [8, 8, 8], [0, 20, 0], "amethyst_block", "", [0, 20, 0]),
    ]
    return props


def build(run, lib, names=None, batch=6):
    out_dir = fwd(os.path.join(OUT, "entities"))
    os.makedirs(out_dir, exist_ok=True)
    tex_dir = fwd(os.path.join(SRC, "props"))
    table = defs()
    have = set(n[:-4] for n in os.listdir(os.path.join(SRC, "props")) if n.endswith(".png"))
    todo = [n for n in sorted(table.keys()) if not names or n in names]
    for i in range(0, len(todo), batch):
        chunk = {}
        for n in todo[i:i + batch]:
            parts = [dict(p) for p in table[n]]
            for p in parts:
                if p["tex"] not in have:
                    p["tex"] = "oak_planks"
            chunk[n] = {"parts": parts}
        code = lib + BUILD_PROP + RUNNER % (json.dumps(chunk, separators=(",", ":")), out_dir, tex_dir)
        print(run(code).strip())
    return len(todo)
