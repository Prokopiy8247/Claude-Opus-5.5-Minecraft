extends Node
## Compile check that runs with the real autoloads loaded (Game + Sfx are visible here).
##   godot --headless --path . --main-pack ... / or via game/main.tscn's dev entry

var count := 0
var failed := 0


func _ready() -> void:
	_scan("res://game")
	print("lint2: %d scripts checked, %d failed" % [count, failed])
	Game.init_registries()
	print("registries ok: %d blocks, %d items, %d mobs, recipes %d skipped" % [
		BlockDB.count, ItemDB.count, MobDB.order.size(), RecipeDB.skipped])
	get_tree().quit(1 if failed > 0 else 0)


func _scan(dir: String) -> void:
	var d := DirAccess.open(dir)
	if d == null:
		return
	d.list_dir_begin()
	var f := d.get_next()
	while f != "":
		var p := dir + "/" + f
		if d.current_is_dir():
			if not f.begins_with("."):
				_scan(p)
		elif f.ends_with(".gd"):
			count += 1
			var s = load(p)
			if s == null or not (s as Script).can_instantiate():
				failed += 1
				print("FAILED: ", p)
		f = d.get_next()
