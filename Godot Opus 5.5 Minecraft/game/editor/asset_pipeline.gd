class_name AssetPipeline
extends RefCounted
## Reproducible generation of all project-owned procedural assets. Used by the headless script
## (game/editor/rebuild_assets.gd) and by the editor plugin menu "Opus 5.5 Minecraft > Rebuild Generated Assets".

static func rebuild_all(verbose := false) -> bool:
	BlockDB.init_registry()
	var ok := true
	var t0 := Time.get_ticks_msec()
	var res := BlockTextureGen.build_atlas(BlockDB.texture_names)
	if not (res.missing as PackedStringArray).is_empty():
		push_warning("Missing block textures: %s" % str(res.missing))
	if BlockTextureGen.save_atlas(res) != OK:
		push_error("Failed to save block atlas")
		ok = false
	if verbose:
		print("block atlas: %d layers (%d ms)" % [res.count, Time.get_ticks_msec() - t0])
	for step in _extra_steps():
		var c: Callable = step
		var r = c.call(verbose)
		if r is bool and not r:
			ok = false
	return ok


## Additional generator steps registered by later subsystems (items, UI, fonts, audio...).
static func _extra_steps() -> Array:
	var out := []
	for cls in ["ItemIconGen", "UiAssetGen", "FontGen", "SoundGen", "MobTextureGen"]:
		if ClassDB.class_exists(cls):
			pass
	var script_steps := [
		"res://game/editor/steps/items_step.gd",
		"res://game/editor/steps/ui_step.gd",
		"res://game/editor/steps/font_step.gd",
		"res://game/editor/steps/mob_textures_step.gd",
	]
	for p in script_steps:
		if ResourceLoader.exists(p):
			var s = load(p)
			if s != null and s.has_method("run"):
				out.append(Callable(s, "run"))
	return out
