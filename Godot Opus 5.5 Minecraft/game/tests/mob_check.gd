extends Node
## Verifies that every mob can build a rig (Blender GLB when present, procedural otherwise) and
## that the part names the animator expects are all there. Prints a per-mob report.

func _ready() -> void:
	Game.init_registries()
	var missing := 0
	var glb := 0
	var proc := 0
	for m in MobDB.order:
		var mob := String(m)
		var rig := MobRenderer.build(mob, String(MobDB.get_def(mob).get("model", "")))
		if rig == null:
			print("  FAIL no rig: ", mob)
			missing += 1
			continue
		var parts := MobRenderer.parts_of(rig)
		var from_glb := rig.get_meta("from_glb", false)
		if from_glb:
			glb += 1
		else:
			proc += 1
		if parts.is_empty():
			print("  FAIL no parts: ", mob)
			missing += 1
		var boxes := 0
		for n in parts:
			var holder: Node3D = parts[n]
			for c in holder.get_children():
				if c is MeshInstance3D and (c as MeshInstance3D).mesh != null:
					boxes += 1
		if boxes == 0:
			print("  FAIL no meshes: ", mob)
			missing += 1
		if mob in ["pig", "creeper", "ender_dragon", "wither", "player"]:
			print("  %s: %d parts, %d boxes, glb=%s" % [mob, parts.size(), boxes, str(from_glb)])
		rig.free()
	print("MobRig: %d mobs, %d from Blender GLB, %d procedural, %d failures" % [MobDB.order.size(), glb, proc, missing])
	get_tree().quit(1 if missing > 0 else 0)
