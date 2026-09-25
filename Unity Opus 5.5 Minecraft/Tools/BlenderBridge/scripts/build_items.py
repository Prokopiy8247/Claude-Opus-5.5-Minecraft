# Blender side of the item / prop FBX pipeline. Sent by Tools/BlenderBridge/item_pipeline.py through blender_unity.py
# (Safe Mode: bpy / math / json only, so the sprite masks and texture lists are embedded below by the driver).
#
# Items: every chosen 16x16 item sprite becomes one "extruded pixel" mesh with exactly the faces ItemRender.ExtrudedMesh
# builds in Unity: the full unit quad in the XY plane centred on the origin at z = -1/32 (readable from -Z), its mirror
# at z = +1/32, and a 1x1 pixel side face on every edge of an opaque pixel (alpha > 127) that borders a transparent
# pixel or the sprite border, uv'd to the inner 80% of that pixel. One material with the item's own PNG, nearest filter.
# Props: torch and soul torch (a 2x10x2 px stick with the flame texels on its tip), lantern and soul lantern (6x7x6
# body, 4x2x4 cap, two crossed handle planes), campfire and soul campfire (four 4x4x16 logs crossed over a 6x1x6 ember
# bed, a flame cross), in block pixels with the uv rects of the placed blocks (MeshCtx.XBox, Box auto uvs, Cross),
# centred on the block centre so they sit where the item sprite was. One material per block texture, named
# "mcrtex_<texture>": MCR.EditorTools.ItemModelImporter reads the texture of every submesh from those names.
# Unity space -> Blender: bx = -x, by = -z, bz = y. Exported with axis_forward=-Z, axis_up=Y and bake_space_transform,
# Unity imports the vertices back exactly as written here (the convention of build_models.py, kept identical).
import bpy
import json
import math
import mathutils

PROJECT = r"{{PROJECT}}"
ITEMS = json.loads(r'''{{ITEMS}}''')          # [{"id": ..., "mask": 16 rows (top-down) of 16 "0"/"1"}]
PROPS = json.loads(r'''{{PROPS}}''')          # prop ids to build
TEXTURES = json.loads(r'''{{TEXTURES}}''')    # exported block textures; props that need another one are skipped
EXPORT = {{EXPORT}}
LAYOUT = {{LAYOUT}}
VIEW = json.loads(r'''{{VIEW}}''')
SHOW_MODELS = json.loads(r'''{{SHOW_MODELS}}''')   # null: leave MCR_Models alone, true / false: show / hide it
SAVE = {{SAVE}}
AXIS_FORWARD = '{{AXIS_FORWARD}}'

MODEL_DIR = PROJECT + "/Assets/_Game/Generated/Models"
ITEM_DIR = MODEL_DIR + "/Items"
PROP_DIR = MODEL_DIR + "/Props"
ITEM_COLLECTION = "MCR_Items"
PROP_COLLECTION = "MCR_Props"
MODEL_COLLECTION = "MCR_Models"


# ---------------------------------------------------------------------- geometry (Unity space)
def new_geo():
    """Quads collected for one object (Safe Mode allows no classes): vertices, faces, uvs, material per face."""
    return {"verts": [], "faces": [], "uvs": [], "mats": [], "flipped": 0, "degenerate": 0}


def to_blender(p):
    return (-p[0], -p[2], p[1])


def quad(g, corners, uvs, normal=None, mat=0):
    """One Unity-space quad: corners bottom-left, top-left, top-right, bottom-right seen from its front (clockwise, the
    order ItemRender and MeshCtx emit), a uv per corner, the outward normal (None: from that winding)."""
    if normal is None:
        a, b, c = (mathutils.Vector(corners[i]) for i in range(3))
        normal = tuple((b - a).cross(c - a).normalized())
    pts = [to_blender(p) for p in corners]
    n = mathutils.Vector(to_blender(normal))
    a, b, c = (mathutils.Vector(pts[i]) for i in range(3))
    geo = (b - a).cross(c - a)
    if geo.length < 1e-12:
        g["degenerate"] += 1
        return
    # Unity front faces wind clockwise, Blender's counter-clockwise: reverse every quad. "flipped" counts faces that
    # would still face inwards (always 0 unless the mapping is wrong).
    order = [3, 2, 1, 0]
    if geo.dot(n) > 0.0:
        order = [0, 1, 2, 3]
        g["flipped"] += 1
    base = len(g["verts"])
    for k in order:
        g["verts"].append(pts[k])
        g["uvs"].append(uvs[k])
    g["faces"].append((base, base + 1, base + 2, base + 3))
    g["mats"].append(mat)


T = 1.0 / 32.0   # half thickness of an extruded sprite (ItemRender.ExtrudedMesh)
P = 1.0 / 16.0   # one sprite pixel


def item_geometry(mask):
    """ItemRender.ExtrudedMesh, face for face: front, back, then per opaque pixel its open top, bottom, left, right edges."""
    g = new_geo()
    quad(g, [(-0.5, -0.5, -T), (-0.5, 0.5, -T), (0.5, 0.5, -T), (0.5, -0.5, -T)], [(0, 0), (0, 1), (1, 1), (1, 0)], (0.0, 0.0, -1.0))
    quad(g, [(0.5, -0.5, T), (0.5, 0.5, T), (-0.5, 0.5, T), (-0.5, -0.5, T)], [(1, 0), (1, 1), (0, 1), (0, 0)], (0.0, 0.0, 1.0))

    def opaque(x, y):
        return 0 <= x < 16 and 0 <= y < 16 and mask[y][x] == "1"

    for y in range(16):
        for x in range(16):
            if not opaque(x, y):
                continue
            # pixel (x, y) with y top-down spans x -0.5 + x/16 .. + 1/16 and y 0.5 - y/16 (top) .. - 1/16
            x0 = -0.5 + x * P
            x1 = x0 + P
            yt = 0.5 - y * P
            yb = yt - P
            u0, u1 = (x + 0.1) / 16.0, (x + 0.9) / 16.0
            vt, vb = 1.0 - (y + 0.1) / 16.0, 1.0 - (y + 0.9) / 16.0
            uv = [(u0, vb), (u0, vt), (u1, vt), (u1, vb)]
            if not opaque(x, y - 1):
                quad(g, [(x0, yt, -T), (x0, yt, T), (x1, yt, T), (x1, yt, -T)], uv, (0.0, 1.0, 0.0))
            if not opaque(x, y + 1):
                quad(g, [(x1, yb, -T), (x1, yb, T), (x0, yb, T), (x0, yb, -T)], uv, (0.0, -1.0, 0.0))
            if not opaque(x - 1, y):
                quad(g, [(x0, yb, T), (x0, yt, T), (x0, yt, -T), (x0, yb, -T)], uv, (-1.0, 0.0, 0.0))
            if not opaque(x + 1, y):
                quad(g, [(x1, yb, -T), (x1, yt, -T), (x1, yt, T), (x1, yb, T)], uv, (1.0, 0.0, 0.0))
    return g


# MeshCtx.FaceVerts: per face (0 down, 1 up, 2 north +Z, 3 south -Z, 4 west -X, 5 east +X) the block corners
# bottom-left, top-left, top-right, bottom-right seen from outside; Us / Vs pick the uv rect corner for each
FACE_VERTS = (((1, 0, 0), (1, 0, 1), (0, 0, 1), (0, 0, 0)),
              ((0, 1, 0), (0, 1, 1), (1, 1, 1), (1, 1, 0)),
              ((1, 0, 1), (1, 1, 1), (0, 1, 1), (0, 0, 1)),
              ((0, 0, 0), (0, 1, 0), (1, 1, 0), (1, 0, 0)),
              ((0, 0, 1), (0, 1, 1), (0, 1, 0), (0, 0, 0)),
              ((1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1)))
FACE_NORMALS = ((0.0, -1.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0), (0.0, 0.0, -1.0), (-1.0, 0.0, 0.0), (1.0, 0.0, 0.0))
US = (0, 0, 1, 1)
VS = (0, 1, 1, 0)


def auto_uv(f, px, py, pz):
    """MeshCtx.AutoUV: each face's own projection of the block-local position (Box without uv rects)."""
    if f == 0:
        return (1.0 - px, pz)
    if f == 1:
        return (px, pz)
    if f == 2:
        return (1.0 - px, py)
    if f == 3:
        return (px, py)
    if f == 4:
        return (1.0 - pz, py)
    return (pz, py)


def box(g, lo, hi, mat, rects=None):
    """Axis-aligned box from lo to hi in block pixels (0..16): MeshCtx.XBox with per-face uv rects (u0, v0, u1, v1),
    or MeshCtx.Box's auto uvs when rects is None; moved so the block centre is the origin."""
    x0, y0, z0 = (c / 16.0 for c in lo)
    x1, y1, z1 = (c / 16.0 for c in hi)
    for f in range(6):
        corners, uvs = [], []
        for v in range(4):
            fv = FACE_VERTS[f][v]
            px, py, pz = (x1 if fv[0] else x0), (y1 if fv[1] else y0), (z1 if fv[2] else z0)
            if rects is not None:
                r = rects[f]
                uvs.append((r[0] if US[v] == 0 else r[2], r[1] if VS[v] == 0 else r[3]))
            else:
                uvs.append(auto_uv(f, px, py, pz))
            corners.append((px - 0.5, py - 0.5, pz - 0.5))
        quad(g, corners, uvs, FACE_NORMALS[f], mat)


def plane(g, corners_px, uvs, mat, yaw=0.0):
    """A single quad (corners in block pixels, clockwise seen from its front) turned yaw degrees about the vertical axis
    through the block centre (Quaternion.Euler(0, yaw, 0)). Item materials draw both sides, so one winding is enough."""
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    pts = []
    for (px, py, pz) in corners_px:
        x, z = px / 16.0 - 0.5, pz / 16.0 - 0.5
        pts.append((x * c + z * s, py / 16.0 - 0.5, -x * s + z * c))
    quad(g, pts, uvs, None, mat)


def rect_uvs(r):
    return [(r[0], r[1]), (r[0], r[3]), (r[2], r[3]), (r[2], r[1])]


def torch(tex):
    """TorchBlock standing: 2x10x2 px with TorchUV; the top face and the top two rows show the flame texels."""
    side = (7 / 16.0, 0.0, 9 / 16.0, 10 / 16.0)
    rects = [(7 / 16.0, 0.0, 9 / 16.0, 2 / 16.0), (7 / 16.0, 8 / 16.0, 9 / 16.0, 10 / 16.0), side, side, side, side]
    g = new_geo()
    box(g, (7, 0, 7), (9, 10, 9), 0, rects)
    return g, [tex]


def lantern(tex):
    """LanternBlock standing: body 6x7x6, cap 4x2x4, handle of two crossed 3x2 px planes (like the original's), uv'd
    to the regions TextureGen.Lantern paints: body front x 0-5 / y 2-8, cap band x 0-3 / y 9-10, cap top x 0-3 / y
    11-14, handle x 11-13 / y 4-5 (texel rows top-down; the rects below are bottom-up uvs like MeshCtx's)."""
    body_side = (0.0, 7 / 16.0, 6 / 16.0, 14 / 16.0)
    body_end = (0.0, 7 / 16.0, 6 / 16.0, 8 / 16.0)       # the metal band under the window: a plain metal plate
    cap_side = (0.0, 5 / 16.0, 4 / 16.0, 7 / 16.0)
    cap_end = (0.0, 1 / 16.0, 4 / 16.0, 5 / 16.0)
    handle = (11 / 16.0, 10 / 16.0, 14 / 16.0, 12 / 16.0)
    g = new_geo()
    box(g, (5, 0, 5), (11, 7, 11), 0, [body_end, body_end, body_side, body_side, body_side, body_side])
    box(g, (6, 7, 6), (10, 9, 10), 0, [cap_end, cap_end, cap_side, cap_side, cap_side, cap_side])
    for yaw in (45.0, -45.0):
        plane(g, [(6.5, 9, 8), (6.5, 11, 8), (9.5, 11, 8), (9.5, 9, 8)], rect_uvs(handle), 0, yaw)
    return g, [tex]


def campfire(log, fire):
    """CampfireBlock lit (rotation 0): two logs along Z below, two along X on top, the ember bed, MeshCtx.Cross flame."""
    g = new_geo()
    box(g, (1, 0, 0), (5, 4, 16), 0)
    box(g, (11, 0, 0), (15, 4, 16), 0)
    box(g, (0, 3, 1), (16, 7, 5), 0)
    box(g, (0, 3, 11), (16, 7, 15), 0)
    box(g, (5, 0, 5), (11, 1, 11), 0)
    # Cross(fire, scale 1, yOffset 0.05, height 1): diagonal planes from 0.05 to 0.95 of the block, 0.05 .. 1.05 high
    a, b, y0, y1 = 0.8, 15.2, 0.8, 16.8
    uv = [(0.0, 0.0), (0.0, 1.0), (1.0, 1.0), (1.0, 0.0)]
    plane(g, [(a, y0, a), (a, y1, a), (b, y1, b), (b, y0, b)], uv, 1)
    plane(g, [(a, y0, b), (a, y1, b), (b, y1, a), (b, y0, a)], uv, 1)
    return g, [log, fire]


def prop_geometry(pid):
    """(geometry, block texture per material) of a prop id, or None for an unknown id."""
    if pid == "torch" or pid == "soul_torch":
        return torch(pid)
    if pid == "lantern" or pid == "soul_lantern":
        return lantern(pid)
    if pid == "campfire":
        return campfire("campfire_log_lit", "fire")
    if pid == "soul_campfire":
        return campfire("soul_campfire_log_lit", "soul_fire")
    return None


# ---------------------------------------------------------------------- blender objects
def texture_material(mname, png, cull):
    img = bpy.data.images.load(png, check_existing=True)
    img.reload()
    mat = bpy.data.materials.get(mname)
    if mat is None:
        mat = bpy.data.materials.new(mname)
    if hasattr(mat, "use_nodes") and not mat.use_nodes:
        mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    tex = nt.nodes.new("ShaderNodeTexImage")
    out.location = (300.0, 0.0)
    tex.location = (-400.0, 0.0)
    tex.image = img
    tex.interpolation = 'Closest'
    tex.extension = 'EXTEND'
    nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    nt.links.new(tex.outputs["Alpha"], bsdf.inputs["Alpha"])
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    bsdf.inputs["Roughness"].default_value = 1.0
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.0
    mat.use_backface_culling = cull
    if hasattr(mat, "surface_render_method"):
        mat.surface_render_method = 'DITHERED'
    return mat


def collection(name):
    coll = bpy.data.collections.get(name)
    if coll is None:
        coll = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(coll)
    return coll


def build_object(name, g, materials, coll):
    old = bpy.data.objects.get(name)
    if old is not None:
        bpy.data.objects.remove(old, do_unlink=True)
    old_mesh = bpy.data.meshes.get(name)
    if old_mesh is not None:
        bpy.data.meshes.remove(old_mesh)
    me = bpy.data.meshes.new(name)
    me.from_pydata(g["verts"], [], g["faces"])
    uv = me.uv_layers.new(name="UVMap")
    flat = []
    for t in g["uvs"]:
        flat.extend(t)
    uv.uv.foreach_set("vector", flat)
    uv.active_render = True
    me.uv_layers.active = uv
    for m in materials:
        me.materials.append(m)
    me.polygons.foreach_set("material_index", g["mats"])
    me.update()
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    return ob, {"quads": len(g["faces"]), "tris": len(g["faces"]) * 2, "verts": len(g["verts"]), "flipped": g["flipped"],
                "degenerate": g["degenerate"], "materials": [m.name for m in materials]}


def export_fbx(ob, path):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    ob.location = (0.0, 0.0, 0.0)
    ob.rotation_euler = (0.0, 0.0, 0.0)
    ob.scale = (1.0, 1.0, 1.0)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(
        filepath=path,
        check_existing=False,
        use_selection=True,
        object_types={'MESH'},
        global_scale=1.0,
        apply_unit_scale=True,
        apply_scale_options='FBX_SCALE_UNITS',   # vertex data stays in Blender units = blocks
        use_space_transform=True,
        bake_space_transform=True,
        axis_forward=AXIS_FORWARD,
        axis_up='Y',
        use_mesh_modifiers=True,
        mesh_smooth_type='OFF',
        use_tspace=False,
        use_triangles=False,
        use_custom_props=False,
        add_leaf_bones=False,
        bake_anim=False,
        path_mode='RELATIVE',
        embed_textures=False,
        batch_mode='OFF',
    )
    ob.select_set(False)


# ---------------------------------------------------------------------- gallery / viewport
def world_bounds(objs):
    lo, hi = [1e9, 1e9, 1e9], [-1e9, -1e9, -1e9]
    for ob in objs:
        for c in ob.bound_box:
            p = ob.matrix_world @ mathutils.Vector(c)
            for k in range(3):
                lo[k], hi[k] = min(lo[k], p[k]), max(hi[k], p[k])
    return lo, hi


def lay_out():
    """Items as a wall of sprites behind the MCR_Models gallery (readable from +Y, i.e. Blender's back view), in the
    exported order, 12 per row; the props in a row below them."""
    models = bpy.data.collections.get(MODEL_COLLECTION)
    y = 4.0
    if models is not None and len(models.objects) > 0:
        y = world_bounds(models.objects)[1][1] + 4.0
    pitch, per_row = 1.25, 12
    items = bpy.data.collections.get(ITEM_COLLECTION)
    names = [o.name for o in items.objects] if items is not None else []
    order = ["item_" + it for it in ITEM_ORDER if ("item_" + it) in names] + sorted(n for n in names if n[5:] not in ITEM_ORDER)
    for i, n in enumerate(order):
        ob = bpy.data.objects[n]
        ob.location = (-(i % per_row) * pitch, y, -(i // per_row) * pitch)
    rows = (len(order) + per_row - 1) // per_row
    props = bpy.data.collections.get(PROP_COLLECTION)
    if props is not None:
        pnames = [p for p in PROP_ORDER if bpy.data.objects.get("prop_" + p) is not None]
        for i, p in enumerate(pnames):
            bpy.data.objects["prop_" + p].location = (-i * 2.0 * pitch - 0.5, y, -(rows + 0.4) * pitch)


def set_view(view):
    bpy.context.view_layer.update()
    objs = []
    for cname in (ITEM_COLLECTION, PROP_COLLECTION):
        coll = bpy.data.collections.get(cname)
        if coll is not None:
            objs.extend(o for o in coll.objects if not o.hide_get())
    lo, hi = world_bounds(objs) if objs else ([-1.0, -1.0, -1.0], [1.0, 1.0, 1.0])
    center = mathutils.Vector([(lo[k] + hi[k]) * 0.5 for k in range(3)])
    size = max(hi[0] - lo[0], hi[2] - lo[2])
    cube = bpy.data.objects.get("Cube")
    if cube is not None:
        cube.hide_set(True)
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    rot = mathutils.Euler([math.radians(a) for a in view.get("euler", [84.0, 0.0, 196.0])], 'XYZ').to_quaternion()
    for win in bpy.context.window_manager.windows:
        for area in win.screen.areas:
            if area.type != 'VIEW_3D':
                continue
            for space in area.spaces:
                if space.type != 'VIEW_3D':
                    continue
                space.shading.type = view.get("shading", "MATERIAL")
                if space.shading.type == 'SOLID':
                    space.shading.color_type = 'TEXTURE'
                space.clip_end = 1000.0
                r3d = space.region_3d
                r3d.view_perspective = 'PERSP'
                r3d.view_rotation = rot
                r3d.view_location = center
                r3d.view_distance = size * float(view.get("zoom", 1.1))
                r3d.update()
            area.tag_redraw()


def show_models(show):
    lc = bpy.context.view_layer.layer_collection.children.get(MODEL_COLLECTION)
    if lc is not None:
        lc.hide_viewport = not show


# ---------------------------------------------------------------------- run
ITEM_ORDER = [it["id"] for it in ITEMS] if ITEMS else json.loads(r'''{{ITEM_ORDER}}''')
PROP_ORDER = ["torch", "soul_torch", "lantern", "soul_lantern", "campfire", "soul_campfire"]
results = {"items": {}, "props": {}, "skipped": []}
if ITEMS:
    coll = collection(ITEM_COLLECTION)
    for it in ITEMS:
        name = "item_" + it["id"]
        mat = texture_material("mcr_item_" + it["id"], ITEM_DIR + "/Textures/" + it["id"] + ".png", True)
        ob, info = build_object(name, item_geometry(it["mask"]), [mat], coll)
        if EXPORT:
            export_fbx(ob, ITEM_DIR + "/" + it["id"] + ".fbx")
            info["fbx"] = it["id"] + ".fbx"
        results["items"][it["id"]] = info
if PROPS:
    coll = collection(PROP_COLLECTION)
    for pid in PROPS:
        made = prop_geometry(pid)
        if made is None:
            results["skipped"].append(pid + " (no definition)")
            continue
        g, textures = made
        missing = [t for t in textures if t not in TEXTURES]
        if missing:
            results["skipped"].append(pid + " (missing " + ",".join(missing) + ")")
            continue
        mats = [texture_material("mcrtex_" + t, PROP_DIR + "/Textures/" + t + ".png", False) for t in textures]
        ob, info = build_object("prop_" + pid, g, mats, coll)
        if EXPORT:
            export_fbx(ob, PROP_DIR + "/" + pid + ".fbx")
            info["fbx"] = pid + ".fbx"
        results["props"][pid] = info
if LAYOUT:
    lay_out()
if SHOW_MODELS is not None:
    show_models(bool(SHOW_MODELS))
if VIEW:
    set_view(VIEW)
if SAVE:
    bpy.ops.wm.save_mainfile()
print("RESULT " + json.dumps(results))
