class_name MobRenderer
extends RefCounted
## Turns Mojang-style cuboid rigs (ModelSpecs) into Godot node hierarchies using procedurally
## generated original pixel textures, and exposes the named parts for procedural animation.

static var _tex_cache: Dictionary = {}
static var _mat_cache: Dictionary = {}

## Style families used by the texture painter.
const STYLES := {
	"pig": "animal_pink", "cow": "animal_cow", "mooshroom": "animal_cow", "sheep": "animal_sheep", "chicken": "animal_chicken",
	"rabbit": "animal_rabbit", "horse": "animal_horse", "skeleton_horse": "skeleton", "camel": "animal_camel",
	"wolf": "animal_wolf", "cat": "animal_cat", "ocelot": "animal_cat", "parrot": "animal_parrot", "fox": "animal_fox",
	"panda": "animal_panda", "polar_bear": "animal_polar", "goat": "animal_goat", "llama": "animal_llama",
	"villager": "villager", "villager_zombie": "zombie_villager", "iron_golem": "metal_iron", "snow_golem": "snow",
	"copper_golem": "metal_copper", "armadillo": "animal_armadillo", "sniffer": "animal_sniffer", "frog": "animal_frog",
	"turtle": "animal_turtle", "axolotl": "animal_axolotl", "bat": "animal_bat", "bee": "animal_bee", "allay": "allay",
	"squid": "squid", "glow_squid": "glow_squid", "dolphin": "animal_dolphin", "fish": "fish",
	"nautilus": "nautilus", "nautilus_zombie": "squid",
	"zombie": "zombie", "husk": "husk", "drowned": "drowned", "skeleton": "skeleton", "wither_skeleton": "wither_skeleton",
	"creeper": "creeper", "spider": "spider", "enderman": "enderman", "slime": "slime", "witch": "witch", "phantom": "phantom",
	"silverfish": "silver", "guardian": "guardian", "pillager": "pillager", "vindicator": "villager_dark", "evoker": "villager_dark",
	"vex": "vex", "ravager": "ravager", "warden": "warden", "creaking": "creaking", "breeze": "breeze",
	"ghast": "ghast", "blaze": "blaze", "piglin": "piglin", "piglin_brute": "piglin_brute", "zombified_piglin": "zombified_piglin",
	"hoglin": "hoglin", "magma_cube": "magma_cube", "strider": "strider", "happy_ghast": "happy_ghast", "shulker": "shulker",
	"sulfur_cube": "sulfur", "ender_dragon": "dragon", "wither": "wither", "player": "player",
}


static func style_of(mob: String) -> String:
	return String(STYLES.get(mob, "skin"))


## Builds the rig. Prefers the Blender-authored GLB (res://game/generated/models/mobs/<mob>.glb)
## and falls back to the procedural cuboid rig when the asset is missing. Returns a Node3D whose
## children are named parts; use parts_of() to fetch them.
static func build(mob: String, model_name := "") -> Node3D:
	var glb := _from_glb(mob)
	if glb != null:
		return glb
	return build_procedural(mob, model_name)


## Wraps an imported Blender model: every part keeps the pivot and box-UV mesh authored in Blender
## and the atlas texture embedded in the GLB; nodes are renamed to the part names the animator uses.
static func _from_glb(mob: String) -> Node3D:
	if not use_blender_models:
		return null
	var scene := ModelLibrary.instance("mobs", mob)
	if scene == null:
		return null
	var prefix := mob + "__"
	var root := Node3D.new()
	root.name = "Rig"
	root.set_meta("mob", mob)
	# collect the part pivots (Node3D whose first child is the part's mesh), parents first
	var order: Array = []
	var walk: Array = [scene]
	while not walk.is_empty():
		var n: Node = walk.pop_front()
		for c in n.get_children():
			walk.append(c)
			if c is Node3D and not (c is MeshInstance3D) and String(c.name).begins_with(prefix) and _glb_box(c) != null:
				order.append(c)
	var holders := {}         # source node -> holder
	var names := PackedStringArray()
	var mat := _glb_material(mob, order)
	for src in order:
		var sn: Node3D = src
		var pname := String(sn.name).trim_prefix(prefix)
		var holder := Node3D.new()
		holder.name = pname
		holder.transform = sn.transform
		holder.set_meta("pivot_top", pname.begins_with("leg") or pname.begins_with("arm") or pname.begins_with("tentacle")
			or pname.begins_with("rod") or pname.begins_with("wing"))
		var mesh_src := _glb_box(sn)
		var box := MeshInstance3D.new()
		box.name = "Box"
		box.mesh = mesh_src.mesh
		box.transform = mesh_src.transform
		box.material_override = mat
		box.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
		holder.add_child(box)
		holders[sn] = holder
		names.append(pname)
		# parent: the nearest ancestor that is also a part pivot
		var p := sn.get_parent()
		var parent_holder: Node3D = null
		while p != null and p != scene:
			if holders.has(p):
				parent_holder = holders[p]
				break
			p = p.get_parent()
		if parent_holder == null:
			root.add_child(holder)
		else:
			parent_holder.add_child(holder)
		holder.set_meta("base_rot", holder.rotation)
		holder.set_meta("base_pos", holder.position)
	scene.free()
	root.set_meta("part_names", names)
	root.set_meta("from_glb", true)
	return root


## The part's box mesh: the child named "<part node>__box" (child order is not guaranteed).
static func _glb_box(n: Node) -> MeshInstance3D:
	var want := String(n.name) + "__box"
	for c in n.get_children():
		if c is MeshInstance3D and String(c.name) == want:
			return c
	return null


static var _glb_mats := {}

## One material per mob using the atlas texture Blender embedded in the GLB.
static func _glb_material(mob: String, parts: Array) -> Material:
	if _glb_mats.has(mob):
		return _glb_mats[mob]
	var tex: Texture2D = null
	for src in parts:
		var mi := _glb_box(src)
		if mi == null or mi.mesh == null:
			continue
		var m := mi.get_active_material(0)
		if m is BaseMaterial3D and (m as BaseMaterial3D).albedo_texture != null:
			tex = (m as BaseMaterial3D).albedo_texture
			break
	var sm := ShaderMaterial.new()
	sm.shader = load("res://game/world/shaders/mob.gdshader")
	if tex != null:
		sm.set_shader_parameter("albedo_tex", tex)
	_glb_mats[mob] = sm
	return sm


## True when the Blender-authored models should be used (option toggle in the options screen).
static var use_blender_models := true


static func build_procedural(mob: String, model_name := "") -> Node3D:
	var spec_name := model_name if model_name != "" else mob
	var spec := ModelSpecs.get_spec(spec_name)
	var root := Node3D.new()
	root.name = "Rig"
	root.set_meta("mob", mob)
	var parts: Array = spec.get("parts", [])
	var nodes: Array = []
	for i in parts.size():
		var p: Array = parts[i]
		var pname: String = p[0]
		var parent_idx: int = p[1]
		if parent_idx >= i:
			push_warning("model %s: part %s has a forward/self parent (%d) - attaching to the root" % [spec_name, pname, parent_idx])
			parent_idx = -1
		var px: float = p[2]
		var py: float = p[3]
		var pz: float = p[4]
		var sx: float = p[5]
		var sy: float = p[6]
		var sz: float = p[7]
		var region: String = p[8]
		var inflate: float = p[9]
		var pivot_top: bool = p[10]
		var holder := Node3D.new()
		holder.name = pname
		holder.set_meta("region", region)
		holder.position = Vector3(px, py, pz) / 16.0
		var rot: Vector3 = p[11] if p.size() > 11 else Vector3.ZERO
		var up: bool = String(pname).begins_with("leg") or String(pname).begins_with("arm") or String(pname).begins_with("tentacle") \
				or String(pname).begins_with("rod") or String(pname).begins_with("wing") or pivot_top
		if pivot_top or up:
			holder.set_meta("pivot_top", true)
		var mi := MeshInstance3D.new()
		mi.name = "Box"
		var w := sx + inflate * 2.0
		var h := sy + inflate * 2.0
		var d := sz + inflate * 2.0
		mi.mesh = cached_box(w, h, d)
		mi.material_override = material(mob, region, w, h, d, pname)
		mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
		if pivot_top or up:
			mi.position = Vector3(0, -h / 16.0 * 0.5, 0)
		holder.add_child(mi)
		var abs_pos := holder.position
		if parent_idx < 0:
			root.add_child(holder)
		else:
			var pn: Node3D = nodes[parent_idx]
			# parent pivots are absolute: convert into the parent's (possibly rotated) local frame
			var pabs: Vector3 = pn.get_meta("abs_pos", pn.position)
			var pbasis: Basis = pn.get_meta("abs_basis", Basis())
			holder.position = pbasis.inverse() * (abs_pos - pabs)
			pn.add_child(holder)
			rot = rot
		holder.rotation_degrees = rot
		var parent_basis: Basis = Basis() if parent_idx < 0 else (nodes[parent_idx] as Node3D).get_meta("abs_basis", Basis())
		holder.set_meta("abs_pos", abs_pos)
		holder.set_meta("abs_basis", parent_basis * Basis.from_euler(holder.rotation))
		holder.set_meta("base_rot", holder.rotation)
		holder.set_meta("base_pos", holder.position)
		nodes.append(holder)
	root.set_meta("part_names", PackedStringArray(parts.map(func(p): return String(p[0]))))
	return root


static func parts_of(rig: Node3D) -> Dictionary:
	var out := {}
	if rig == null:
		return out
	var names: PackedStringArray = rig.get_meta("part_names", PackedStringArray())
	for n in names:
		var node := _find(rig, String(n))
		if node != null:
			out[String(n)] = node
	return out


static func _find(node: Node, n: String) -> Node3D:
	if node.name == n:
		return node as Node3D
	for c in node.get_children():
		var r := _find(c, n)
		if r != null:
			return r
	return null


# ------------------------------------------------------------------------------------------------
## Box mesh with Minecraft's box-UV unwrap for an image of size (2*d + 2*w) x (d + h):
##   top (d,0,w,d)  bottom (d+w,0,w,d)  right (0,d,d,h)  front (d,d,w,h)  left (d+w,d,d,h)  back (2d+w,d,w,h)
## The mob faces -Z; its right side is +X.
static func box_mesh_uv(w: float, h: float, d: float) -> ArrayMesh:
	var tw := 2.0 * d + 2.0 * w
	var th := d + h
	var hw := w / 32.0
	var hh := h / 32.0
	var hd := d / 32.0
	var rects := {
		"top": Rect2(d / tw, 0.0, w / tw, d / th),
		"bottom": Rect2((d + w) / tw, 0.0, w / tw, d / th),
		"right": Rect2(0.0, d / th, d / tw, h / th),
		"front": Rect2(d / tw, d / th, w / tw, h / th),
		"left": Rect2((d + w) / tw, d / th, d / tw, h / th),
		"back": Rect2((2.0 * d + w) / tw, d / th, w / tw, h / th),
	}
	# corners listed BL, BR, TR, TL as seen from outside the face
	var faces := [
		["front", Vector3(hw, -hh, -hd), Vector3(-hw, -hh, -hd), Vector3(-hw, hh, -hd), Vector3(hw, hh, -hd), Vector3(0, 0, -1)],
		["back", Vector3(-hw, -hh, hd), Vector3(hw, -hh, hd), Vector3(hw, hh, hd), Vector3(-hw, hh, hd), Vector3(0, 0, 1)],
		["right", Vector3(hw, -hh, hd), Vector3(hw, -hh, -hd), Vector3(hw, hh, -hd), Vector3(hw, hh, hd), Vector3(1, 0, 0)],
		["left", Vector3(-hw, -hh, -hd), Vector3(-hw, -hh, hd), Vector3(-hw, hh, hd), Vector3(-hw, hh, -hd), Vector3(-1, 0, 0)],
		["top", Vector3(hw, hh, -hd), Vector3(-hw, hh, -hd), Vector3(-hw, hh, hd), Vector3(hw, hh, hd), Vector3(0, 1, 0)],
		["bottom", Vector3(-hw, -hh, -hd), Vector3(hw, -hh, -hd), Vector3(hw, -hh, hd), Vector3(-hw, -hh, hd), Vector3(0, -1, 0)],
	]
	var verts := PackedVector3Array()
	var uvs := PackedVector2Array()
	var norms := PackedVector3Array()
	var idx := PackedInt32Array()
	for f in faces:
		var r: Rect2 = rects[f[0]]
		var base := verts.size()
		var uvc := [Vector2(r.position.x, r.end.y), Vector2(r.end.x, r.end.y), Vector2(r.end.x, r.position.y), Vector2(r.position.x, r.position.y)]
		for i in 4:
			verts.append(f[1 + i])
			uvs.append(uvc[i])
			norms.append(f[5])
		idx.append_array([base, base + 2, base + 1, base, base + 3, base + 2])
	var arr := []
	arr.resize(Mesh.ARRAY_MAX)
	arr[Mesh.ARRAY_VERTEX] = verts
	arr[Mesh.ARRAY_TEX_UV] = uvs
	arr[Mesh.ARRAY_NORMAL] = norms
	arr[Mesh.ARRAY_INDEX] = idx
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arr)
	return mesh


static var _box_cache := {}

static func cached_box(w: float, h: float, d: float) -> ArrayMesh:
	var key := Vector3(w, h, d)
	if _box_cache.has(key):
		return _box_cache[key]
	var m := box_mesh_uv(w, h, d)
	_box_cache[key] = m
	return m


## Material for one region of a mob (its generated texture, unshaded pixel look with voxel lighting).
static func material(mob: String, region: String, w: float, h: float, d: float, part_name := "") -> Material:
	var key := "%s|%s|%d|%d|%d|%s" % [mob, region, int(w), int(h), int(d), String(part_name).split("_")[0]]
	if _mat_cache.has(key):
		return _mat_cache[key]
	var img := texture_image(mob, region, w, h, d, part_name)
	var tex := ImageTexture.create_from_image(img)
	var m := ShaderMaterial.new()
	m.shader = load("res://game/world/shaders/mob.gdshader")
	m.set_shader_parameter("albedo_tex", tex)
	m.set_shader_parameter("cutout", float(region == "extra"))
	_mat_cache[key] = m
	return m


static func clear_cache() -> void:
	_tex_cache.clear()
	_mat_cache.clear()


# ------------------------------------------------------------------------------------------------
## Generates (and caches) the pixel texture for one part region of a mob.
static func texture_image(mob: String, region: String, w: float, h: float, d: float, part_name := "") -> Image:
	var key := "%s|%s|%d|%d|%d|%s" % [mob, region, int(w), int(h), int(d), String(part_name)]
	if _tex_cache.has(key):
		return _tex_cache[key]
	var tw := int(round(2.0 * d + 2.0 * w))
	var th := int(round(d + h))
	tw = maxi(tw, 2)
	th = maxi(th, 2)
	var img := Image.create_empty(tw, th, false, Image.FORMAT_RGBA8)
	MobTextures.paint(img, mob, style_of(mob), region, int(w), int(h), int(d), part_name)
	_tex_cache[key] = img
	return img


# ------------------------------------------------------------------------------------------------
## Vehicles (boats, minecarts) as small cuboid assemblies with wood/metal textures.
static func vehicle_mesh(kind: String, wood: String, chest: bool) -> Node3D:
	# Blender-authored models first (boat_<wood>, chest_boat_<wood>, minecart variants)
	var model_name := kind
	if kind == "boat" or kind == "chest_boat" or kind.ends_with("_boat"):
		model_name = ("chest_boat_" if chest or kind.begins_with("chest") else "boat_") + wood
	var glb := ModelLibrary.entity_model(model_name)
	if glb != null:
		return glb
	var root := Node3D.new()
	if kind.ends_with("minecart"):
		var iron := _simple_material(Color(0.62, 0.64, 0.66), "iron")
		_add_box(root, Vector3(0.98, 0.16, 0.98), Vector3(0, 0.12, 0), iron)
		_add_box(root, Vector3(0.98, 0.5, 0.14), Vector3(0, 0.42, -0.42), iron)
		_add_box(root, Vector3(0.98, 0.5, 0.14), Vector3(0, 0.42, 0.42), iron)
		_add_box(root, Vector3(0.14, 0.5, 0.98), Vector3(-0.42, 0.42, 0), iron)
		_add_box(root, Vector3(0.14, 0.5, 0.98), Vector3(0.42, 0.42, 0), iron)
		if chest:
			_add_box(root, Vector3(0.6, 0.5, 0.6), Vector3(0, 0.5, 0), _simple_material(Color(0.55, 0.36, 0.16), "wood"))
		return root
	var plank := _simple_material(MobTextures.wood_color(wood), "wood_" + wood)
	var dark := _simple_material(MobTextures.wood_color(wood).darkened(0.25), "wood_" + wood)
	# hull: bottom + 4 sides, sloped look approximated with thin boxes
	_add_box(root, Vector3(1.25, 0.1, 1.375), Vector3(0, 0.1, 0), plank)
	_add_box(root, Vector3(1.25, 0.35, 0.16), Vector3(0, 0.28, -0.6), dark)
	_add_box(root, Vector3(1.25, 0.35, 0.16), Vector3(0, 0.28, 0.6), dark)
	_add_box(root, Vector3(0.16, 0.35, 1.375), Vector3(-0.55, 0.28, 0), dark)
	_add_box(root, Vector3(0.16, 0.35, 1.375), Vector3(0.55, 0.28, 0), dark)
	_add_box(root, Vector3(0.9, 0.12, 0.3), Vector3(0, 0.36, 0), plank)
	if chest:
		_add_box(root, Vector3(0.6, 0.45, 0.55), Vector3(0, 0.45, 0), _simple_material(Color(0.55, 0.36, 0.16), "wood"))
	return root


static func _add_box(parent: Node3D, size: Vector3, pos: Vector3, mat: Material) -> void:
	var mi := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = size
	mi.mesh = box
	mi.material_override = mat
	mi.position = pos
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
	parent.add_child(mi)


static var _simple_cache := {}

static func _simple_material(c: Color, key: String) -> Material:
	if _simple_cache.has(key):
		return _simple_cache[key]
	var img := Image.create_empty(16, 16, false, Image.FORMAT_RGBA8)
	var rng := RandomNumberGenerator.new()
	rng.seed = hash(key)
	for y in 16:
		for x in 16:
			var f := 0.9 + rng.randf() * 0.2
			if key.begins_with("wood") and (y % 4 == 0 or x % 8 == 0):
				f *= 0.85
			img.set_pixel(x, y, Color(c.r * f, c.g * f, c.b * f))
	var m := ShaderMaterial.new()
	m.shader = load("res://game/world/shaders/mob.gdshader")
	m.set_shader_parameter("albedo_tex", ImageTexture.create_from_image(img))
	_simple_cache[key] = m
	return m
