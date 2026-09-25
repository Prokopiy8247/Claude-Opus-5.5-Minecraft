import bpy
print("FILE:", bpy.data.filepath)
print("VERSION:", bpy.app.version_string)
print("SCENES:", [s.name for s in bpy.data.scenes])
print("COLLECTIONS:", [c.name for c in bpy.data.collections])
print("OBJECTS:", [(o.name, o.type) for o in bpy.data.objects])
print("ADDONS FBX:", hasattr(bpy.ops.export_scene, "fbx"), "GLTF:", hasattr(bpy.ops.export_scene, "gltf"))
