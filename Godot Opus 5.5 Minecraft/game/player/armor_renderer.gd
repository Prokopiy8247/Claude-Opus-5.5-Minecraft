class_name ArmorRenderer
extends RefCounted
## Worn-armor visuals: the Blender-authored armor pieces (game/generated/models/entities/armor_*.glb)
## are attached to the player / mob rig on top of the body parts, tinted with the material's palette.
## Falls back to a colour overlay on the body meshes when the GLB is missing.

const MATERIALS := ["leather", "chainmail", "copper", "iron", "golden", "diamond", "netherite", "turtle"]

## Palette per material: base colour, dark shade, light shade (original, not sampled from any game).
const PALETTE := {
	"leather": ["#a06840", "#6b452b", "#c58a5c"],
	"chainmail": ["#8f8f97", "#5c5c64", "#b8b8c0"],
	"copper": ["#c1714a", "#8a4e30", "#e09872"],
	"iron": ["#d8d8d8", "#9a9a9a", "#f2f2f2"],
	"golden": ["#f5d24a", "#b08a1c", "#fff0a0"],
	"diamond": ["#5ce0d8", "#2f9c98", "#a8f6f0"],
	"netherite": ["#4a4145", "#2b2528", "#6d6166"],
	"turtle": ["#6fbf6a", "#3f7a3c", "#9fe09a"],
}

static var _tiles := {}
static var _mats := {}
static var _piece_cache := {}


## 16x16 overlay tile for one armor material (rivets and edge shading, transparent centre).
static func tile(material: String) -> Image:
	if _tiles.has(material):
		return _tiles[material]
	var cols: Array = PALETTE.get(material, PALETTE["iron"])
	var base := Color.html(String(cols[0]))
	var dark := Color.html(String(cols[1]))
	var light := Color.html(String(cols[2]))
	var img := Image.create_empty(16, 16, false, Image.FORMAT_RGBA8)
	var rng := RandomNumberGenerator.new()
	rng.seed = hash(material)
	for y in 16:
		for x in 16:
			var c := base
			# plate seams and rivets
			if x % 8 == 0 or y % 8 == 0:
				c = dark
			elif (x % 8 == 4 and y % 8 == 4):
				c = light
			var n := 0.94 + rng.randf() * 0.12
			img.set_pixel(x, y, Color(c.r * n, c.g * n, c.b * n, 1.0))
	_tiles[material] = img
	return img


static func material_for(material: String) -> Material:
	if _mats.has(material):
		return _mats[material]
	var m := ShaderMaterial.new()
	m.shader = load("res://game/world/shaders/mob.gdshader")
	m.set_shader_parameter("albedo_tex", ImageTexture.create_from_image(tile(material)))
	_mats[material] = m
	return m


## The armor piece GLB for a slot (0 helmet, 1 chest, 2 legs, 3 boots). "armor" piece when unknown.
static func piece(slot: int) -> Node3D:
	var names := ["helmet", "chestplate", "leggings", "boots"]
	if slot < 0 or slot > 3:
		return null
	var n: String = names[slot]
	if _piece_cache.has(n):
		var cached = _piece_cache[n]
		return (cached as Node3D).duplicate() if cached != null else null
	var scene := ModelLibrary.instance("entities", "armor_" + n)
	_piece_cache[n] = scene
	return scene.duplicate() if scene != null else null


static func material_of(item_name: String) -> String:
	for m in MATERIALS:
		if item_name.begins_with(String(m) + "_"):
			return String(m)
	if item_name == "turtle_helmet":
		return "turtle"
	return "iron"


## Attaches the worn armor to a rig: one GLB piece per filled armor slot, parented to the matching
## body part so it follows the animation. Inventory armor slots: ARMOR+0 feet .. ARMOR+3 head.
static func apply(rig: Node3D, parts: Dictionary, inventory) -> void:
	_clear(rig)
	for i in 4:
		var st: ItemStack = inventory.stack_at(Inventory.ARMOR + i)
		if st == null:
			continue
		var slot := 3 - i       # 0 helmet, 1 chestplate, 2 leggings, 3 boots
		var mat := material_of(st.item_name())
		var mtl := material_for(mat)
		var anchors: Array = [["head"], ["torso"], ["leg_fl", "leg_fr"], ["leg_fl", "leg_fr"]][slot]
		var placed := false
		for an in anchors:
			var anchor: Node3D = parts.get(String(an), null)
			if anchor == null:
				continue
			var node := piece(slot)
			if node == null:
				break
			node.name = "Worn_%d_%s" % [slot, an]
			_apply_material(node, mtl)
			anchor.add_child(node)
			placed = true
		if not placed:
			_colour_body(parts, mat)


static func _clear(n: Node) -> void:
	for c in n.get_children():
		if String(c.name).begins_with("Worn_"):
			c.queue_free()
		else:
			_clear(c)


## Fallback: tint the body meshes of the covered parts.
static func _colour_body(parts: Dictionary, material: String) -> void:
	var cols: Array = PALETTE.get(material, PALETTE["iron"])
	var c := Color.html(String(cols[0]))
	for pn in ["torso", "head", "leg_fl", "leg_fr"]:
		var holder: Node3D = parts.get(pn, null)
		if holder == null:
			continue
		for ch in holder.get_children():
			if ch is GeometryInstance3D:
				(ch as GeometryInstance3D).set_instance_shader_parameter("ent_flash", Vector4(c.r, c.g, c.b, 0.35))


static func _apply_material(n: Node, mat: Material) -> void:
	for c in n.get_children():
		if c is MeshInstance3D:
			var mi: MeshInstance3D = c
			for i in mi.mesh.get_surface_count():
				mi.set_surface_override_material(i, mat)
		_apply_material(c, mat)
