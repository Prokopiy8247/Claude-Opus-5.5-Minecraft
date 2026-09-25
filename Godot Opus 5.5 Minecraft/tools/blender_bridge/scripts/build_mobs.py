"""Builds every mob rig as a real Blender mesh hierarchy and exports one GLB per model.

Input : specs.json dumped from the game (user://specs.json) - part name, parent, pivot position,
        box size, texture region, inflate and base rotation in Minecraft units (1/16 block).
Output: game/generated/models/mobs/<name>.glb

Each part becomes one box mesh whose UVs use Minecraft's box unwrap for an image of size
(2*d + 2*w) x (d + h):  top (d,0,w,d)  bottom (d+w,0,w,d)  right (0,d,d,h)  front (d,d,w,h)
                       left (d+w,d,d,h)  back (2d+w,d,w,h)
so the game's procedural textures map onto the geometry unchanged. Part pivots are preserved as
empty parents named after the part, keeping the Godot animator's absolute-pivot maths valid.

Run through the blender_godot MCP bridge (port 9877):
    python tools/blender_bridge/bridge.py exec tools/blender_bridge/scripts/build_mobs.py
"""

import json
import math
import os
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

_default_project = Path(bpy.data.filepath).resolve().parent if bpy.data.filepath else Path.cwd()
PROJECT_DIR = Path(os.environ.get("GODOT_MINECRAFT_PROJECT", _default_project))
_appdata = Path(os.environ.get("APPDATA", Path.home() / "AppData" / "Roaming"))
SPECS_PATH = str(Path(os.environ.get(
    "GODOT_MINECRAFT_SPECS",
    _appdata / "Godot" / "app_userdata" / "Godot Minecraft" / "specs.json",
)))
OUT_DIR = str(PROJECT_DIR / "game" / "generated" / "models" / "mobs")
BLEND_PATH = str(PROJECT_DIR / "GodotMinecraft.blend")

UNIT = 1.0 / 16.0


def read_specs():
    with open(SPECS_PATH, "r", encoding="utf-8") as fh:
        return json.load(fh)["specs"]


def clear_scene():
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    for me in list(bpy.data.meshes):
        bpy.data.meshes.remove(me, do_unlink=True)


def box_uv(bm, region_rect, w, h, d):
    """Assigns the Minecraft box unwrap to the six faces of a box of size (w,h,d) in blocks."""
    # faces of a bmesh cube: +X, -X, +Y, -Y, +Z, -Z (Blender axis order)
    # Minecraft ordering: right(+X), left(-X), top(+Y), bottom(-Y), front(-Z), back(+Z)
    tw = 2.0 * d + 2.0 * w
    th = d + h
    rects = {
        "right": (0.0, d, d, h),
        "left": (d + w, d, d, h),
        "top": (d, 0.0, w, d),
        "bottom": (d + w, 0.0, w, d),
        "front": (d, d, w, h),
        "back": (2.0 * d + w, d, w, h),
    }
    uv_layer = bm.loops.layers.uv.verify()
    # match faces by their normal
    for face in bm.faces:
        n = face.normal
        key = "front"
        if n.x > 0.5:
            key = "right"
        elif n.x < -0.5:
            key = "left"
        elif n.y > 0.5:
            key = "top"
        elif n.y < -0.5:
            key = "bottom"
        elif n.z > 0.5:
            key = "back"
        u0, v0, bw, bh = rects[key]
        # map the loop UVs by their position inside the face rectangle
        lo = Vector(face.verts[0].co)
        for loop in face.loops:
            co = loop.vert.co
            # local 0..1 coordinates across the face
            if key in ("right", "left"):
                u = (co.z - lo.z) / max(1e-6, d * UNIT if d else 1.0)
                v = (co.y - lo.y) / max(1e-6, h * UNIT if h else 1.0)
            elif key in ("top", "bottom"):
                u = (co.x - lo.x) / max(1e-6, w * UNIT if w else 1.0)
                v = (co.z - lo.z) / max(1e-6, d * UNIT if d else 1.0)
            else:
                u = (co.x - lo.x) / max(1e-6, w * UNIT if w else 1.0)
                v = (co.y - lo.y) / max(1e-6, h * UNIT if h else 1.0)
            u = min(max(abs(u), 0.0), 1.0)
            v = min(max(abs(v), 0.0), 1.0)
            loop[uv_layer].uv = (
                (u0 + u * bw) / tw,
                1.0 - (v0 + v * bh) / th,
            )
            _ = region_rect


def make_part(name, pos, size, inflate, pivot_top, rot, top_only=False):
    """One box mesh (Minecraft units) parented to an empty that carries the pivot."""
    w = size[0] + inflate * 2.0
    h = size[1] + inflate * 2.0
    d = size[2] + inflate * 2.0
    mesh = bpy.data.meshes.new(name + "_mesh")
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co.x *= w * UNIT
        v.co.y *= h * UNIT
        v.co.z *= d * UNIT
    if pivot_top:
        # the box hangs below its pivot
        for v in bm.verts:
            v.co.y -= h * UNIT * 0.5
    box_uv(bm, None, w, h, d)
    bm.to_mesh(mesh)
    bm.free()
    ob = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(ob)
    holder = bpy.data.objects.new(name + "_pivot", None)
    bpy.context.collection.objects.link(holder)
    holder.empty_display_type = "PLAIN_AXES"
    holder.empty_display_size = 0.15
    holder.location = Vector((pos[0] * UNIT, pos[1] * UNIT, -pos[2] * UNIT))
    # Minecraft Z+ (south) is Blender -Z; the mob faces -Z in Minecraft (-Y here is forward)
    if rot and (abs(rot[0]) + abs(rot[1]) + abs(rot[2])) > 0.001:
        holder.rotation_euler = (
            math.radians(-rot[0]),
            math.radians(-rot[1]),
            math.radians(rot[2]),
        )
    ob.parent = holder
    return holder


def swap_yz(ob):
    """Converts the authoring space (Y up, -Z forward) into Blender's Z-up convention."""
    _ = ob


def build_model(name, spec):
    clear_scene()
    holders = {}
    order = []
    parts = spec.get("parts", [])
    for p in parts:
        hide = bool(p.get("pivot_top", False))
        holder = make_part(
            p["name"],
            p["pos"],
            p["size"],
            float(p.get("inflate", 0.0)),
            hide,
            p.get("rot", []),
        )
        holders[p["name"]] = holder
        order.append((p, holder))
    # parent pivots (positions are absolute in the spec)
    for p, holder in order:
        idx = int(p.get("parent", -1))
        if idx < 0 or idx >= len(parts):
            continue
        parent_name = parts[idx]["name"]
        holder.parent = holders[parent_name]
        holder.matrix_parent_inverse = holders[parent_name].matrix_world.inverted()
    rig = bpy.data.objects.new(name + "_rig", None)
    bpy.context.collection.objects.link(rig)
    for p, holder in order:
        if int(p.get("parent", -1)) < 0:
            holder.parent = rig
            holder.matrix_parent_inverse = rig.matrix_world.inverted()
    # orient the whole rig: Minecraft -Z (forward) becomes Blender -Y, Minecraft +Y becomes +Z
    rig.rotation_euler = (math.radians(-90.0), 0.0, 0.0)
    return rig


def export(name, rig):
    for ob in bpy.data.objects:
        ob.select_set(False)
    def sel(o):
        o.select_set(True)
        for c in o.children:
            sel(c)
    sel(rig)
    bpy.context.view_layer.objects.active = rig
    path = OUT_DIR + "/" + name + ".glb"
    bpy.ops.export_scene.gltf(
        filepath=path,
        export_format="GLB",
        use_selection=True,
        export_apply=False,
        export_yup=True,
        export_texcoords=True,
        export_normals=True,
        export_materials="NONE",
    )
    return path


def main():
    specs = read_specs()
    done = []
    for name in sorted(specs.keys()):
        rig = build_model(name, specs[name])
        path = export(name, rig)
        done.append(name)
    bpy.ops.wm.save_as_mainfile(filepath=BLEND_PATH)
    print("exported %d mob models: %s" % (len(done), ", ".join(done)))


main()
