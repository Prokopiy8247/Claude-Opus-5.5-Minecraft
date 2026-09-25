extends Node
## Asset pipeline step 1/3 (Godot -> Blender): writes everything Blender needs to author the
## 3D assets into res://game/generated/blender_src/ (ignored by the Godot importer):
##   mobs.json + mobs/<mob>.png  - every mob rig (part pivots, boxes, box-UV atlas of the
##                                 procedural pixel textures) as built by MobRenderer
##   items.json + items/<item>.png - held-item sprites (tools, weapons, ...) to voxel-extrude
##   props.json + props/*.png     - textures for chests, boats, minecarts, end crystal
## Step 2 is tools/blender_bridge/build_assets.py (Blender through the blender_godot MCP socket),
## step 3 is the normal Godot import of res://game/generated/models/**.glb.
##   godot --headless --path . res://game/editor/blender_export.tscn

const OUT := "res://game/generated/blender_src"

const HELD_ITEMS := [
	"wooden_sword", "stone_sword", "iron_sword", "golden_sword", "diamond_sword", "netherite_sword",
	"wooden_pickaxe", "stone_pickaxe", "iron_pickaxe", "golden_pickaxe", "diamond_pickaxe", "netherite_pickaxe",
	"wooden_axe", "stone_axe", "iron_axe", "golden_axe", "diamond_axe", "netherite_axe",
	"wooden_shovel", "stone_shovel", "iron_shovel", "golden_shovel", "diamond_shovel", "netherite_shovel",
	"wooden_hoe", "stone_hoe", "iron_hoe", "golden_hoe", "diamond_hoe", "netherite_hoe",
	"bow", "crossbow", "trident", "mace", "shield", "fishing_rod", "flint_and_steel", "shears", "spyglass",
	"brush", "stick", "torch", "carrot_on_a_stick", "blaze_rod", "bone", "compass", "clock", "totem_of_undying",
	"elytra", "ender_eye", "ender_pearl", "wind_charge", "breeze_rod", "copper_spear", "iron_spear",
]


func _ready() -> void:
	Game.init_registries()
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(OUT + "/mobs"))
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(OUT + "/items"))
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(OUT + "/props"))
	var f := FileAccess.open(OUT + "/.gdignore", FileAccess.WRITE)
	f.store_string("")
	f.close()
	var n_mobs := _export_mobs()
	var n_items := _export_items()
	var n_props := _export_props()
	print("blender_export: %d mobs, %d items, %d props -> %s" % [n_mobs, n_items, n_props, ProjectSettings.globalize_path(OUT)])
	get_tree().quit()


# ------------------------------------------------------------------------------------------------
func _export_mobs() -> int:
	var list := []
	for m in MobDB.order:
		list.append([String(m), String(MobDB.get_def(String(m)).get("model", ""))])
	list.append(["player", "player_model"])
	var out := {}
	for e in list:
		var mob: String = e[0]
		var model: String = e[1]
		var rig := MobRenderer.build_procedural(mob, model)
		var parts := []
		var images := []
		_collect(rig, rig, parts, images)
		var packed := _pack(images)
		var atlas: Image = packed[0]
		var rects: Array = packed[1]
		atlas.save_png(OUT + "/mobs/%s.png" % mob)
		for i in parts.size():
			var r: Rect2i = rects[i]
			(parts[i] as Dictionary)["rect"] = [r.position.x, r.position.y, r.size.x, r.size.y]
		out[mob] = {"model": model if model != "" else mob, "atlas": [atlas.get_width(), atlas.get_height()], "parts": parts}
		rig.free()
	var fj := FileAccess.open(OUT + "/mobs.json", FileAccess.WRITE)
	fj.store_string(JSON.stringify(out))
	fj.close()
	return out.size()


## Walks the rig: every holder is a Node3D that owns a "Box" MeshInstance3D.
func _collect(rig: Node3D, node: Node, parts: Array, images: Array) -> void:
	for c in node.get_children():
		if not (c is Node3D) or c.name == "Box":
			continue
		var h: Node3D = c
		var box := h.get_node_or_null("Box") as MeshInstance3D
		if box == null:
			_collect(rig, h, parts, images)
			continue
		var aabb := box.mesh.get_aabb()
		var size := aabb.size * 16.0
		var t := h.transform
		var parent_name := "" if h.get_parent() == rig else String(h.get_parent().name)
		# the same procedural texture MobRenderer used for this box (read from the painter's cache so
		# no GPU read-back is needed in headless mode)
		var mob: String = String(rig.get_meta("mob", ""))
		var region: String = String(h.get_meta("region", "body"))
		var img: Image = MobRenderer.texture_image(mob, region, size.x, size.y, size.z, String(h.name))
		parts.append({
			"name": String(h.name), "parent": parent_name,
			"basis": [t.basis.x.x, t.basis.x.y, t.basis.x.z, t.basis.y.x, t.basis.y.y, t.basis.y.z, t.basis.z.x, t.basis.z.y, t.basis.z.z],
			"origin": [t.origin.x, t.origin.y, t.origin.z],
			"box_pos": [box.position.x, box.position.y, box.position.z],
			"size": [roundf(size.x * 100.0) / 100.0, roundf(size.y * 100.0) / 100.0, roundf(size.z * 100.0) / 100.0],
			"pivot_top": bool(h.get_meta("pivot_top", false)),
			"tex": [img.get_width(), img.get_height()],
		})
		images.append(img)
		_collect(rig, h, parts, images)


## Shelf packer: returns [atlas Image, Array[Rect2i]] (power-of-two width, 1px padding).
func _pack(images: Array) -> Array:
	var width := 64
	var total := 0
	for im in images:
		total += (im as Image).get_width() * (im as Image).get_height()
		width = maxi(width, (im as Image).get_width() + 2)
	while width * width < total * 2:
		width *= 2
	width = mini(nearest_po2(width), 1024)
	var rects := []
	var x := 0
	var y := 0
	var row_h := 0
	for im in images:
		var img: Image = im
		if x + img.get_width() > width:
			x = 0
			y += row_h + 1
			row_h = 0
		rects.append(Rect2i(x, y, img.get_width(), img.get_height()))
		x += img.get_width() + 1
		row_h = maxi(row_h, img.get_height())
	var height := nearest_po2(maxi(y + row_h, 1))
	var atlas := Image.create_empty(width, height, false, Image.FORMAT_RGBA8)
	atlas.fill(Color(0, 0, 0, 0))
	for i in images.size():
		var img2: Image = images[i]
		if img2.get_format() != Image.FORMAT_RGBA8:
			img2 = img2.duplicate()
			img2.convert(Image.FORMAT_RGBA8)
		atlas.blit_rect(img2, Rect2i(0, 0, img2.get_width(), img2.get_height()), (rects[i] as Rect2i).position)
	return [atlas, rects]


# ------------------------------------------------------------------------------------------------
func _export_items() -> int:
	var out := {}
	for n in HELD_ITEMS:
		if not ItemDB.has(n):
			continue
		var it := ItemDB.get_by_name(n)
		var img := ItemIcons.item_sprite(n)
		if img == null:
			continue
		img.save_png(OUT + "/items/%s.png" % n)
		out[n] = {"size": [img.get_width(), img.get_height()], "handheld": it.kind == "tool" or it.kind == "weapon"}
	var fj := FileAccess.open(OUT + "/items.json", FileAccess.WRITE)
	fj.store_string(JSON.stringify(out))
	fj.close()
	return out.size()


# ------------------------------------------------------------------------------------------------
func _export_props() -> int:
	var names := ["oak_planks", "spruce_planks", "birch_planks", "jungle_planks", "acacia_planks", "dark_oak_planks",
		"mangrove_planks", "cherry_planks", "pale_oak_planks", "bamboo_planks", "iron_block", "furnace_front",
		"tnt_side", "hopper_outside", "obsidian", "bedrock", "stone", "chest", "gold_block", "copper_block"]
	var out := {}
	for n in names:
		var img := TextureLibrary.tile(n)
		if img == null:
			continue
		img.save_png(OUT + "/props/%s.png" % n)
		out[n] = true
	# armor overlay tiles (one per material) for the Blender-authored armor pieces
	for mat in ArmorRenderer.MATERIALS:
		var img2 := ArmorRenderer.tile(String(mat))
		if img2 != null:
			img2.save_png(OUT + "/props/armor_%s.png" % mat)
			out["armor_" + String(mat)] = true
	# chest textures from the item/block painter
	var fj := FileAccess.open(OUT + "/props.json", FileAccess.WRITE)
	fj.store_string(JSON.stringify(out))
	fj.close()
	return out.size()
