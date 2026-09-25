# Blender side of the FBX model pipeline. Sent by Tools/BlenderBridge/fbx_pipeline.py through blender_unity.py
# (Safe Mode: bpy / math / json only, the model data is embedded below by the driver).
#
# For every embedded ModelDef JSON (see ModelDef.ToJson) this builds one merged box mesh in the model's rest pose,
# exactly like ModelMesher.AddCube + ModelRenderer.Build lay it out in Unity:
#   * model pixels -> blocks (/16), bone pivots and default rotations (Unity Quaternion.Euler order: Z, X, Y),
#   * Minecraft box UVs from ModelDef.BoxFaces (0 top, 1 bottom, 2 right +X, 3 front +Z, 4 left -X, 5 back -Z),
#   * a second UV map "BoneCube" holding (bone index, cube index) per corner, so Unity can cut the mesh back into
#     per-bone meshes for the rig,
#   * one material with the painted skin PNG (Assets/_Game/Generated/Models/Textures/<name>.png).
# Unity model space -> Blender: bx = -x, by = -z, bz = y (models face -Y, Blender's front view). Exported with
# axis_forward=-Z, axis_up=Y, bake_space_transform and FBX_SCALE_UNITS, Unity imports the vertices back as exactly
# the procedural coordinates, with identity transforms, as long as the model importer keeps its default
# bakeAxisConversion = false (baking turns the models 180 degrees about Y). ModelImporter.ValidateModels checks it.
import bpy
import json
import math
import mathutils

PROJECT = r"{{PROJECT}}"
MODELS = json.loads(r'''{{MODELS}}''')
EXPORT = {{EXPORT}}
LAYOUT = json.loads(r'''{{LAYOUT}}''')
VIEW = json.loads(r'''{{VIEW}}''')
SAVE = {{SAVE}}

MODEL_DIR = PROJECT + "/Assets/_Game/Generated/Models"
TEX_DIR = MODEL_DIR + "/Textures"
COLLECTION = "MCR_Models"
AXIS_FORWARD = '{{AXIS_FORWARD}}'
IDENTITY = [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]


# ---------------------------------------------------------------------- math (Unity conventions)
def mat_mul(a, b):
    return [[a[r][0] * b[0][c] + a[r][1] * b[1][c] + a[r][2] * b[2][c] for c in range(3)] for r in range(3)]


def mat_vec(m, v):
    return [m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2],
            m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2],
            m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2]]


def unity_euler(e):
    """Quaternion.Euler(x, y, z) as a matrix: z first, then x, then y (R = Ry * Rx * Rz)."""
    x, y, z = math.radians(e[0]), math.radians(e[1]), math.radians(e[2])
    cx, sx, cy, sy, cz, sz = math.cos(x), math.sin(x), math.cos(y), math.sin(y), math.cos(z), math.sin(z)
    rx = [[1.0, 0.0, 0.0], [0.0, cx, -sx], [0.0, sx, cx]]
    ry = [[cy, 0.0, sy], [0.0, 1.0, 0.0], [-sy, 0.0, cy]]
    rz = [[cz, -sz, 0.0], [sz, cz, 0.0], [0.0, 0.0, 1.0]]
    return mat_mul(ry, mat_mul(rx, rz))


def rest_pose(model):
    """Model-space (rotation, position) of every bone, as ModelRenderer.Build parents them."""
    bones = model["bones"]
    index = {}
    for i, b in enumerate(bones):
        index[b["name"]] = i
    pose = []
    for i, b in enumerate(bones):
        prot, ppos, ppiv = IDENTITY, [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]
        p = index.get(b["parent"], -1) if b["parent"] is not None else -1
        if p >= 0:
            ppiv = bones[p]["pivot"]
            if p < i:
                prot, ppos = pose[p]
        local = [(b["pivot"][k] - ppiv[k]) / 16.0 for k in range(3)]
        lp = mat_vec(prot, local)
        pose.append((mat_mul(prot, unity_euler(b["rot"])), [ppos[k] + lp[k] for k in range(3)]))
    return pose


def box_faces(c):
    """ModelDef.BoxFaces: (u, v, w, h) texel rects, 0 top, 1 bottom, 2 right(+X), 3 front(+Z), 4 left(-X), 5 back(-Z)."""
    w = math.ceil(c["s"][0] - 0.001)
    h = math.ceil(c["s"][1] - 0.001)
    d = math.ceil(c["s"][2] - 0.001)
    u, v = c["uv"][0], c["uv"][1]
    return [(u + d, v, w, d), (u + d + w, v, w, d), (u, v + d, d, h),
            (u + d, v + d, w, h), (u + d + w, v + d, d, h), (u + d + w + d, v + d, w, h)]


def cube_quads(model, bone, c):
    """The six faces of a cube in bone space (blocks): corners bl, tl, tr, br as seen from outside, uvs, normal."""
    inf = c["inf"]
    o, s, piv = c["o"], c["s"], bone["pivot"]
    x0, y0, z0 = [(o[k] - inf - piv[k]) / 16.0 for k in range(3)]
    x1, y1, z1 = [(o[k] + s[k] + inf - piv[k]) / 16.0 for k in range(3)]
    r = box_faces(c)
    tw, th = float(model["texW"]), float(model["texH"])
    quads = [
        ([(x1, y0, z1), (x1, y1, z1), (x0, y1, z1), (x0, y0, z1)], r[3], (0.0, 0.0, 1.0)),    # front +Z
        ([(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)], r[5], (0.0, 0.0, -1.0)),   # back -Z
        ([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], r[2], (1.0, 0.0, 0.0)),    # right +X
        ([(x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x0, y0, z0)], r[4], (-1.0, 0.0, 0.0)),   # left -X
        ([(x1, y1, z1), (x1, y1, z0), (x0, y1, z0), (x0, y1, z1)], r[0], (0.0, 1.0, 0.0)),    # top
        ([(x1, y0, z0), (x1, y0, z1), (x0, y0, z1), (x0, y0, z0)], r[1], (0.0, -1.0, 0.0)),   # bottom
    ]
    out = []
    for corners, rect, normal in quads:
        u0, u1 = rect[0] / tw, (rect[0] + rect[2]) / tw
        vt, vb = 1.0 - rect[1] / th, 1.0 - (rect[1] + rect[3]) / th
        if c["mir"]:
            u0, u1 = u1, u0
        out.append((corners, [(u0, vb), (u0, vt), (u1, vt), (u1, vb)], normal))
    return out


def to_blender(p):
    return (-p[0], -p[2], p[1])


# ---------------------------------------------------------------------- building
def build_geometry(model):
    pose = rest_pose(model)
    verts, faces, uvs, ids = [], [], [], []
    flipped, degenerate = 0, 0
    for bi, bone in enumerate(model["bones"]):
        rot, pos = pose[bi]
        for ci, c in enumerate(bone["cubes"]):
            for corners, quv, normal in cube_quads(model, bone, c):
                pts = []
                for q in corners:
                    w = mat_vec(rot, q)
                    pts.append(to_blender([w[0] + pos[0], w[1] + pos[1], w[2] + pos[2]]))
                n = mathutils.Vector(to_blender(mat_vec(rot, normal)))
                a, b, cc = mathutils.Vector(pts[0]), mathutils.Vector(pts[1]), mathutils.Vector(pts[2])
                geo = (b - a).cross(cc - a)
                if geo.length < 1e-12:
                    degenerate += 1
                    continue
                # Unity front faces wind clockwise, Blender's counter-clockwise: reverse every quad. "flipped"
                # counts faces where that would still face inwards (always 0 unless the mapping is wrong).
                order = [3, 2, 1, 0]
                if geo.dot(n) > 0.0:
                    order = [0, 1, 2, 3]
                    flipped += 1
                base = len(verts)
                for k in order:
                    verts.append(pts[k])
                    uvs.append(quv[k])
                    ids.append((float(bi), float(ci)))
                faces.append((base, base + 1, base + 2, base + 3))
    return verts, faces, uvs, ids, flipped, degenerate


def skin_material(name):
    img = bpy.data.images.load(TEX_DIR + "/" + name + ".png", check_existing=True)
    img.reload()
    mname = "mcr_" + name
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
    mat.use_backface_culling = True
    if hasattr(mat, "surface_render_method"):
        mat.surface_render_method = 'DITHERED'
    return mat


def model_collection():
    coll = bpy.data.collections.get(COLLECTION)
    if coll is None:
        coll = bpy.data.collections.new(COLLECTION)
        bpy.context.scene.collection.children.link(coll)
    return coll


def build_object(model, coll):
    name = model["name"]
    verts, faces, uvs, ids, flipped, degenerate = build_geometry(model)
    old = bpy.data.objects.get(name)
    if old is not None:
        bpy.data.objects.remove(old, do_unlink=True)
    old_mesh = bpy.data.meshes.get(name)
    if old_mesh is not None:
        bpy.data.meshes.remove(old_mesh)
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    uv_main = me.uv_layers.new(name="UVMap")
    uv_ids = me.uv_layers.new(name="BoneCube")
    flat_uv, flat_ids = [], []
    for t in uvs:
        flat_uv.extend(t)
    for t in ids:
        flat_ids.extend(t)
    uv_main.uv.foreach_set("vector", flat_uv)
    uv_ids.uv.foreach_set("vector", flat_ids)
    uv_main.active_render = True
    me.uv_layers.active = uv_main
    me.materials.append(skin_material(name))
    me.update()
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    return ob, {"faces": len(faces), "tris": len(faces) * 2, "verts": len(verts), "flipped": flipped, "degenerate": degenerate}


def export_fbx(ob):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    ob.location = (0.0, 0.0, 0.0)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(
        filepath=MODEL_DIR + "/" + ob.name + ".fbx",
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
def lay_out(layout):
    coll = bpy.data.collections.get(COLLECTION)
    if coll is None:
        return
    names = layout["names"]
    wanted = set(names)
    for ob in coll.objects:
        ob.hide_set(ob.name not in wanted)
    gap, row_width = float(layout.get("gap", 1.0)), float(layout.get("row_width", 40.0))
    x, y, row_depth = 0.0, 0.0, 0.0
    for name in names:
        ob = bpy.data.objects.get(name)
        if ob is None:
            continue
        xs = [c[0] for c in ob.bound_box]
        ys = [c[1] for c in ob.bound_box]
        w, d = max(xs) - min(xs), max(ys) - min(ys)
        if x > 0.0 and x + w > row_width:
            x, y, row_depth = 0.0, y + row_depth + gap * 2.0, 0.0
        ob.location = (x - min(xs), y - min(ys), 0.0)
        x += w + gap
        row_depth = max(row_depth, d)


def set_view(view):
    bpy.context.view_layer.update()
    coll = bpy.data.collections.get(COLLECTION)
    lo, hi = [1e9, 1e9, 1e9], [-1e9, -1e9, -1e9]
    focus = set(view.get("names", []))
    if coll is not None:
        for ob in coll.objects:
            if ob.hide_get() or (focus and ob.name not in focus):
                continue
            for c in ob.bound_box:
                p = ob.matrix_world @ mathutils.Vector(c)
                for k in range(3):
                    lo[k], hi[k] = min(lo[k], p[k]), max(hi[k], p[k])
    if lo[0] > hi[0]:
        lo, hi = [-1.0, -1.0, 0.0], [1.0, 1.0, 2.0]
    center = mathutils.Vector([(lo[k] + hi[k]) * 0.5 for k in range(3)])
    size = max(hi[0] - lo[0], hi[2] - lo[2], (hi[1] - lo[1]) * 0.6)
    cube = bpy.data.objects.get("Cube")
    if cube is not None:
        cube.hide_set(True)
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    euler = view.get("euler", [72.0, 0.0, 24.0])
    rot = mathutils.Euler([math.radians(a) for a in euler], 'XYZ').to_quaternion()
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
                r3d.view_distance = size * float(view.get("zoom", 1.15))
                r3d.update()
            area.tag_redraw()


# ---------------------------------------------------------------------- run
results = {}
if MODELS:
    coll = model_collection()
    for model in MODELS:
        ob, info = build_object(model, coll)
        if EXPORT:
            export_fbx(ob)
            info["fbx"] = ob.name + ".fbx"
        results[model["name"]] = info
if LAYOUT:
    lay_out(LAYOUT)
if VIEW:
    set_view(VIEW)
if SAVE:
    bpy.ops.wm.save_mainfile()
print("RESULT " + json.dumps(results))
