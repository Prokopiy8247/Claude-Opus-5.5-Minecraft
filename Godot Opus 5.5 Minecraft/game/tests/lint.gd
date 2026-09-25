extends SceneTree
## Loads every GDScript under res://game to surface parse/compile errors in one pass.
##   godot --headless --path . --script res://game/tests/lint.gd

var count := 0
var failed := 0


func _init() -> void:
	_scan("res://game")
	print("lint: %d scripts checked, %d failed" % [count, failed])
	quit(1 if failed > 0 else 0)


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
			var s = ResourceLoader.load(p, "", ResourceLoader.CACHE_MODE_REUSE)
			if s == null or not (s is Script) or not (s as Script).can_instantiate():
				if not (s is Script and (s as Script).is_abstract()):
					failed += 1
					print("FAILED: ", p)
		f = d.get_next()
