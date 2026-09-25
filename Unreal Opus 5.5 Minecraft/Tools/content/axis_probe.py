# Imports the Blender axis-test meshes and logs their bounds, to pin down the
# Blender -> FBX -> Unreal axis mapping used by Tools/BlenderMCP/build_assets.py.
import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()) + "Opus55Fbx/Test/"
tasks = []
for name in ["SM_AxisTestA.001", "SM_AxisTestB.001"]:
    t = unreal.AssetImportTask()
    t.filename = ROOT + name + ".fbx"
    t.destination_path = "/Game/Opus55Minecraft/Test"
    t.destination_name = name.replace(".001", "")
    t.automated = True
    t.replace_existing = True
    t.save = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    for p in t.imported_object_paths:
        a = unreal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            b = a.get_bounding_box()
            unreal.log_warning("OPUS55_AXIS %s min=(%.1f %.1f %.1f) max=(%.1f %.1f %.1f)" % (
                p, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z))
        else:
            unreal.log_warning("OPUS55_AXIS imported %s (%s)" % (p, type(a).__name__))
