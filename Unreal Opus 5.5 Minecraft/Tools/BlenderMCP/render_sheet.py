"""Renders the model sheet (the Opus55_Rigs collections) with Workbench vertex colours.

Sent through the blender_unreal MCP like the builder, so it obeys Safe Mode.  The
camera frames whatever the Opus55_Rigs collections currently hold, which makes the
render independent of where the layout placed each rig.
"""
import bpy
import mathutils
import math

PAD = 1.12          # extra room around the model sheet
ELEV = 0.55         # camera elevation (radians above the horizon)
AZIM = -0.55        # camera azimuth around Z (front-right three-quarter view)

scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'VERTEX'
scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True
scene.display.shading.background_type = 'VIEWPORT'
scene.display.shading.background_color = (0.13, 0.15, 0.18)
scene.render.resolution_x = SHEET_W
scene.render.resolution_y = SHEET_H
scene.render.film_transparent = False
scene.render.image_settings.file_format = 'PNG'

# ---- optional lineup: park the selected rigs side by side for the shot, restore after
moved = {}
if len(SHEET_ONLY) > 0 and len(SHEET_COLL) == 0:
    cursor = 0.0
    row_x = 0.0
    for rig in SHEET_ONLY:
        coll = bpy.data.collections.get("Rig_" + rig)
        if coll is None:
            continue
        mins = [1e9, 1e9, 1e9]
        maxs = [-1e9, -1e9, -1e9]
        for obj in coll.objects:
            for corner in obj.bound_box:
                p = obj.matrix_world @ mathutils.Vector(corner)
                for k in range(3):
                    mins[k] = min(mins[k], p[k])
                    maxs[k] = max(maxs[k], p[k])
        if mins[0] > maxs[0]:
            continue
        width = maxs[1] - mins[1]
        shift = mathutils.Vector((-(mins[0] + maxs[0]) * 0.5, cursor - mins[1], 0.0))
        for obj in coll.objects:
            moved[obj.name] = obj.location.copy()
            obj.location = obj.location + shift + mathutils.Vector((0.0, 0.0, 0.0))
        cursor = cursor + width + 0.6
    bpy.context.view_layer.update()

lo = [1e9, 1e9, 1e9]
hi = [-1e9, -1e9, -1e9]
found = 0
for obj in bpy.data.objects:
    if obj.type != 'MESH':
        continue
    in_sheet = False
    for coll in obj.users_collection:
        if len(SHEET_COLL) > 0:
            if coll.name == SHEET_COLL:
                in_sheet = True
        elif coll.name == "Opus55_Rigs" or coll.name.startswith("Rig_"):
            if len(SHEET_ONLY) == 0 or coll.name[4:] in SHEET_ONLY:
                in_sheet = True
    if not in_sheet:
        continue
    found += 1
    for corner in obj.bound_box:
        p = obj.matrix_world @ mathutils.Vector(corner)
        for k in range(3):
            lo[k] = min(lo[k], p[k])
            hi[k] = max(hi[k], p[k])

if found == 0:
    lo = [0.0, -1.0, 0.0]
    hi = [1.0, 1.0, 1.0]

center = mathutils.Vector(((lo[0] + hi[0]) * 0.5, (lo[1] + hi[1]) * 0.5, (lo[2] + hi[2]) * 0.5))
direction = mathutils.Vector((math.cos(AZIM) * math.cos(ELEV), math.sin(AZIM) * math.cos(ELEV), math.sin(ELEV)))
forward = -direction
right = forward.cross(mathutils.Vector((0.0, 0.0, 1.0))).normalized()
up = right.cross(forward).normalized()
# projected extent of the bounding box on the camera plane
w = 0.0
h = 0.0
for cx in (lo[0], hi[0]):
    for cy in (lo[1], hi[1]):
        for cz in (lo[2], hi[2]):
            d = mathutils.Vector((cx, cy, cz)) - center
            w = max(w, abs(d.dot(right)) * 2.0)
            h = max(h, abs(d.dot(up)) * 2.0)
aspect = float(SHEET_W) / float(SHEET_H)
span = max(w, h * aspect, 0.5) * PAD

cam = bpy.data.objects.get("Opus55SheetCam")
if cam is None:
    cam = bpy.data.objects.new("Opus55SheetCam", bpy.data.cameras.new("Opus55SheetCam"))
    scene.collection.objects.link(cam)
cam.data.type = 'ORTHO'
cam.data.ortho_scale = span
cam.data.clip_end = 1000.0
cam.location = center + direction * max(span * 4.0, 20.0)
cam.rotation_euler = forward.to_track_quat('-Z', 'Y').to_euler()
scene.camera = cam
scene.render.filepath = SHEET_PATH
bpy.ops.render.render(write_still=True)
for name in moved:
    obj = bpy.data.objects.get(name)
    if obj is not None:
        obj.location = moved[name]
print("OPUS55_SHEET meshes=%d span=%.2f -> %s" % (found, span, bpy.path.abspath(SHEET_PATH)))
