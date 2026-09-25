extends SceneTree
## Headless asset regeneration entry point:
##   godot --headless --path . --script res://game/editor/rebuild_assets.gd
## Regenerates every procedural asset (block atlas, item atlas, UI, fonts, sounds...) deterministically.

func _init() -> void:
	var ok := AssetPipeline.rebuild_all(true)
	print("Asset rebuild ", "OK" if ok else "FAILED")
	quit(0 if ok else 1)
