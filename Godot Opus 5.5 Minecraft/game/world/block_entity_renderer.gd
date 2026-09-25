class_name BlockEntityRenderer
extends RefCounted
## Node-based renderers for block entities that need animation or non-chunk geometry:
## chests (opening lid, double chests), enchanting table floating book, beacon beam.

static var _mat: ShaderMaterial = null
static var _beam_mat: ShaderMaterial = null


static func _material() -> ShaderMaterial:
	if _mat == null:
		_mat = ShaderMaterial.new()
		_mat.shader = load("res://game/world/shaders/voxel_entity.gdshader")
		_mat.set_shader_parameter("blocks", TextureLibrary.block_array)
		_mat.set_shader_parameter("alpha_cut", 0.5)
	return _mat


static func make(world: World, pos: Vector3i, v: int) -> Node3D:
	var id := v & 0xFFF
	var d: BlockDef = BlockDB.defs[id]
	match d.model:
		BlockDB.M_CHEST:
			return _chest(world, pos, v, d)
		BlockDB.M_ENCHANT:
			return _enchant_book()
		BlockDB.M_BEACON:
			return _beacon_beam(world, pos)
	return null


## Chest: base + lid + latch; the lid pivots at the back edge. meta: facing (2 bits), half (2 bits).
static func _chest(world: World, pos: Vector3i, v: int, d: BlockDef) -> Node3D:
	var meta := (v >> 12) & 15
	var facing := meta & 3
	var half := (meta >> 2) & 3
	if half == 2:
		return Node3D.new()   # the right half is drawn by the left half
	var width := 2.0 if half == 1 else 1.0
	var glb := ModelLibrary.block_model("chest")
	var root: Node3D = load("res://game/world/chest_node.gd").new()
	root.name = "Chest"
	var layer := BlockDB.layer_of(d.tex.get("all", "chest"))
	var px := 1.0 / 16.0
	var w := (14.0 + (16.0 if width > 1.0 else 0.0)) * px
	var base_mi := MeshInstance3D.new()
	base_mi.mesh = _box_mesh(Vector3(w, 10 * px, 14 * px), layer)
	base_mi.position = Vector3(0, 5 * px, 0)
	base_mi.material_override = _material()
	root.add_child(base_mi)
	var lid_pivot := Node3D.new()
	lid_pivot.name = "Lid"
	lid_pivot.position = Vector3(0, 9 * px, 7 * px)
	root.add_child(lid_pivot)
	var lid := MeshInstance3D.new()
	lid.mesh = _box_mesh(Vector3(w, 5 * px, 14 * px), layer)
	lid.position = Vector3(0, 2.5 * px, -7 * px)
	lid.material_override = _material()
	lid_pivot.add_child(lid)
	var latch := MeshInstance3D.new()
	latch.mesh = _box_mesh(Vector3(2 * px, 4 * px, 1 * px), BlockDB.layer_of("iron_block"))
	latch.position = Vector3(0, 1 * px, -14.5 * px)
	latch.material_override = _material()
	lid_pivot.add_child(latch)
	if glb != null and width <= 1.0:
		# Blender chest: base + hinged lid ("chest__lid" pivot) replace the procedural boxes
		var glb_lid := glb.find_child("chest__lid", true, false) as Node3D
		if glb_lid != null:
			for c in root.get_children():
				root.remove_child(c)
				c.queue_free()
			root.add_child(glb)
			lid_pivot = glb_lid
		else:
			glb.queue_free()
	elif glb != null:
		glb.queue_free()
	# orientation: chest front faces its facing direction; double chests extend to the right
	var yaw: float = [0.0, PI * 0.5, PI, -PI * 0.5][facing]
	root.rotation.y = yaw + PI
	if width > 1.0:
		var fvec: Vector3i = Vox.H_FACING_VEC[facing]
		var right := Vector3(-fvec.z, 0, fvec.x)
		root.set_meta("offset", right * 0.5)
	var l := world.get_light(pos.x, pos.y, pos.z)
	_set_light(root, Vector2(l.x, l.y))
	root.set_meta("lid", lid_pivot)
	return root


static func _set_light(n: Node, l: Vector2) -> void:
	for c in n.get_children():
		if c is GeometryInstance3D:
			(c as GeometryInstance3D).set_instance_shader_parameter("ent_light", l)
		_set_light(c, l)


## Box mesh in the voxel vertex format with one texture layer on every face.
static func _box_mesh(size: Vector3, layer: int) -> ArrayMesh:
	var sf := MeshSurface.new()
	var a := -size * 0.5
	var b := size * 0.5
	for f in 6:
		var cs: Array = MeshModels._face_corners(f, a, b)
		var uv0 := Vector2(0, 1)
		var uv1 := Vector2(0, 0)
		var uv2 := Vector2(1, 0)
		var uv3 := Vector2(1, 1)
		sf.quad(cs[0], cs[1], cs[2], cs[3], uv0, uv1, uv2, uv3, float(layer), 1.0, Color(1, 1, 1), Mesher.SHADE[f], 15.0, 0.0)
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, sf.to_arrays(), [], {}, MeshSurface.format_flags())
	return mesh


static func _enchant_book() -> Node3D:
	var root: Node3D = load("res://game/world/spin_node.gd").new()
	var it := ItemDB.get_by_name("enchanted_book")
	if it != null:
		var book := ItemIcons.sprite_mesh(it)
		book.position = Vector3(0, 1.05, 0)
		book.scale = Vector3(0.6, 0.6, 0.6)
		root.add_child(book)
		root.set_meta("book", book)
	return root


static func _beacon_beam(world: World, pos: Vector3i) -> Node3D:
	var root := Node3D.new()
	if _beam_mat == null:
		_beam_mat = ShaderMaterial.new()
		_beam_mat.shader = load("res://game/world/shaders/beacon_beam.gdshader")
	var mi := MeshInstance3D.new()
	var box := BoxMesh.new()
	var h := float(world.max_y - pos.y)
	box.size = Vector3(0.3, h, 0.3)
	mi.mesh = box
	mi.material_override = _beam_mat
	mi.position = Vector3(0, 1.0 + h * 0.5, 0)
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	root.add_child(mi)
	return root
