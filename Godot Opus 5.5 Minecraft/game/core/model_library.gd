class_name ModelLibrary
extends RefCounted
## Access to Blender-authored GLB models exported by the asset pipeline into
## res://game/generated/models/{items,mobs,blocks,vehicles}. Materials of imported scenes are
## replaced with the project's pixel shaders (nearest filtering + voxel light map).

const BASE := "res://game/generated/models"

static var _cache: Dictionary = {}


static func _load(path: String) -> PackedScene:
	if _cache.has(path):
		return _cache[path]
	var ps: PackedScene = null
	if ResourceLoader.exists(path):
		ps = load(path) as PackedScene
	_cache[path] = ps
	return ps


static func has_model(kind: String, name: String) -> bool:
	return _load("%s/%s/%s.glb" % [BASE, kind, name]) != null


static func instance(kind: String, name: String) -> Node3D:
	var ps := _load("%s/%s/%s.glb" % [BASE, kind, name])
	if ps == null:
		return null
	var n := ps.instantiate() as Node3D
	return n


static func item_model(name: String) -> Node3D:
	var n := instance("items", name)
	if n == null:
		# tool families share one model per shape
		for shape in ["sword", "pickaxe", "axe", "shovel", "hoe", "spear"]:
			if name.ends_with("_" + shape):
				n = instance("items", name)
	return n


static func block_model(name: String) -> Node3D:
	var n := instance("blocks", name)
	if n != null:
		apply_entity_materials(n)
	return n


## Props / special entities (end crystal, boats, minecarts, chests...). Keeps the exported materials
## but switches them to unshaded nearest-filtered textures when present.
static func entity_model(name: String) -> Node3D:
	var n := instance("entities", name)
	if n != null:
		apply_entity_materials(n)
	return n


static func mob_model(name: String) -> Node3D:
	var n := instance("mobs", name)
	if n != null:
		apply_entity_materials(n)
	return n


## Replace imported materials with the mob/item shader, keeping the albedo texture.
static func apply_entity_materials(n: Node) -> void:
	_replace(n, "res://game/world/shaders/mob.gdshader")


static func apply_hand_materials(n: Node, hand: bool) -> void:
	_replace(n, "res://game/world/shaders/hand_item.gdshader" if hand else "res://game/world/shaders/item.gdshader")


static func _replace(n: Node, shader_path: String) -> void:
	if n is MeshInstance3D:
		var mi: MeshInstance3D = n
		var mesh := mi.mesh
		if mesh != null:
			for i in mesh.get_surface_count():
				var src := mi.get_active_material(i)
				var tex: Texture2D = null
				if src is BaseMaterial3D:
					tex = (src as BaseMaterial3D).albedo_texture
				var m := ShaderMaterial.new()
				m.shader = load(shader_path)
				if tex != null:
					m.set_shader_parameter("albedo_tex", tex)
				mi.set_surface_override_material(i, m)
		mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	for c in n.get_children():
		_replace(c, shader_path)
