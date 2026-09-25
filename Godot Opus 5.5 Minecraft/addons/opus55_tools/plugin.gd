@tool
extends EditorPlugin
## Editor menu "Project > Tools > Opus 5.5 Minecraft: ..." for the reproducible asset pipeline.


func _enter_tree() -> void:
	add_tool_menu_item("Opus 5.5 Minecraft: Rebuild Generated Assets", _rebuild)
	add_tool_menu_item("Opus 5.5 Minecraft: Validate Registries", _validate)


func _exit_tree() -> void:
	remove_tool_menu_item("Opus 5.5 Minecraft: Rebuild Generated Assets")
	remove_tool_menu_item("Opus 5.5 Minecraft: Validate Registries")


func _rebuild() -> void:
	var s = load("res://game/editor/asset_pipeline.gd")
	var ok: bool = s.rebuild_all(true)
	print("Opus 5.5 tools: asset rebuild ", "OK" if ok else "FAILED")
	EditorInterface.get_resource_filesystem().scan()


func _validate() -> void:
	var s = load("res://game/tests/validate.gd")
	print("Opus 5.5 tools: run  godot --headless --path . --script res://game/tests/validate.gd  for the full report (", s != null, ")")
