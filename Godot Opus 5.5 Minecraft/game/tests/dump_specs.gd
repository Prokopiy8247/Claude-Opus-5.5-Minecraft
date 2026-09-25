extends Node
## Dumps every mob model spec to user://specs.json so the Blender pipeline can build the same rigs.
##   godot --headless --path . res://game/tests/dump_specs.tscn

func _ready() -> void:
	Game.init_registries()
	ModelSpecs.get_spec("pig")   # force the table to build
	var out := {"specs": {}}
	for name in ModelSpecs.SPECS:
		var spec: Dictionary = ModelSpecs.SPECS[name]
		var parts := []
		for p in (spec.get("parts", []) as Array):
			var e: Array = p
			var rot: Array = []
			if e.size() > 11:
				var r: Vector3 = e[11]
				rot = [r.x, r.y, r.z]
			parts.append({
				"name": String(e[0]), "parent": int(e[1]),
				"pos": [float(e[2]), float(e[3]), float(e[4])],
				"size": [float(e[5]), float(e[6]), float(e[7])],
				"region": String(e[8]), "inflate": float(e[9]), "pivot_top": bool(e[10]),
				"rot": rot,
			})
		var sz: Array = spec.get("size", [0, 0])
		out["specs"][name] = {"size": [float(sz[0]), float(sz[1])], "parts": parts}
	var f := FileAccess.open("user://specs.json", FileAccess.WRITE)
	f.store_string(JSON.stringify(out))
	f.close()
	print("specs: %d models -> %s" % [out["specs"].size(), ProjectSettings.globalize_path("user://specs.json")])
	print("mob models: ", ", ".join(PackedStringArray(MobDB.order)))
	get_tree().quit()
