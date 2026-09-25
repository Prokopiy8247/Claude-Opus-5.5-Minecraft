class_name HandView
extends Node3D
## First-person arm and held item: equip animation, idle bob, look sway, mining/attack swing,
## eating/drinking wiggle, bow draw, shield block, spear charge. Rendered with depth-compressed
## "viewmodel" shaders so it never clips into walls.

var player = null
var arm: MeshInstance3D = null
var item_root: Node3D = null
var item_node: Node3D = null
var offhand_root: Node3D = null
var offhand_node: Node3D = null
var _item_key := ""
var _off_key := ""
var _sway := Vector2.ZERO
var _last_yaw := 0.0
var _last_pitch := 0.0
var _equip := 0.0
var _arm_mat: ShaderMaterial = null


func _ready() -> void:
	name = "HandView"
	top_level = true
	_arm_mat = ShaderMaterial.new()
	_arm_mat.shader = load("res://game/world/shaders/hand_item.gdshader")
	_arm_mat.set_shader_parameter("albedo_tex", ImageTexture.create_from_image(PlayerSkin.arm_image()))
	arm = MeshInstance3D.new()
	arm.mesh = MobRenderer.box_mesh_uv(4, 12, 4)
	arm.material_override = _arm_mat
	arm.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(arm)
	item_root = Node3D.new()
	add_child(item_root)
	offhand_root = Node3D.new()
	add_child(offhand_root)


func _process(delta: float) -> void:
	if player == null or player.camera == null:
		return
	var cam: Camera3D = player.camera
	global_transform = cam.global_transform
	visible = player.third_person == 0 and not player.dead and bool(player.session.hud_visible if player.session != null else true) \
		and player.gamemode != Player.SPECTATOR
	if not visible:
		return
	var inter = player.interact
	var st: ItemStack = player.inventory.selected_stack()
	var key := "" if st == null else "%d|%s" % [st.id, str(st.has_glint())]
	if key != _item_key:
		_item_key = key
		_rebuild_item(st)
	var off: ItemStack = player.inventory.stack_at(Inventory.OFFHAND)
	var okey := "" if off == null else "%d" % off.id
	if okey != _off_key:
		_off_key = okey
		_rebuild_offhand(off)
	# look sway (hand lags behind camera rotation)
	var dy := wrapf(player.yaw - _last_yaw, -PI, PI)
	var dp: float = player.pitch - _last_pitch
	_last_yaw = player.yaw
	_last_pitch = player.pitch
	_sway = _sway.lerp(Vector2(clampf(dy * 2.0, -0.25, 0.25), clampf(dp * 2.0, -0.25, 0.25)), minf(1.0, delta * 8.0))
	_sway = _sway.lerp(Vector2.ZERO, minf(1.0, delta * 6.0))
	var equip: float = inter.equip if inter != null else 0.0
	_equip = lerpf(_equip, equip, minf(1.0, delta * 20.0))
	var swing: float = 1.0 - (inter.swing if inter != null else 0.0)
	if inter != null and inter.swing <= 0.0:
		swing = 0.0
	var sp := sqrt(swing)
	var sw_x := -0.4 * sin(sp * PI)
	var sw_y := 0.2 * sin(sp * TAU)
	var sw_z := -0.2 * sin(swing * PI)
	var sw_rot_y := deg_to_rad(-20.0) * sin(swing * swing * PI)
	var sw_rot_z := deg_to_rad(-20.0) * sin(sp * PI)
	var sw_rot_x := deg_to_rad(-80.0) * sin(sp * PI)
	# walking bob follows the camera bob already; add a small idle breathing motion
	var t := Time.get_ticks_msec() / 1000.0
	var breathe := sin(t * 1.6) * 0.004
	var light := _light()
	arm.set_instance_shader_parameter("ent_light", light)
	var using: bool = inter != null and inter.using_item
	var use_t: float = (inter.use_ticks if inter != null else 0) + 0.0
	var base := Vector3(0.56, -0.52 - _equip * 0.6 + breathe, -0.72) + Vector3(_sway.x * 0.2, -_sway.y * 0.2, 0)
	if st == null:
		# bare arm (right hand, Minecraft-style angle)
		arm.visible = true
		item_root.visible = false
		var ab := base + Vector3(0.2, 0.1, 0.1) + Vector3(sw_x * 0.8, sw_y, sw_z)
		arm.transform = Transform3D(Basis.from_euler(Vector3(deg_to_rad(10.0) + sw_rot_x * 0.6, deg_to_rad(45.0) + sw_rot_y,
			deg_to_rad(60.0) + sw_rot_z)).scaled(Vector3.ONE * 0.8), ab)
	else:
		arm.visible = false
		item_root.visible = true
		var it := st.item()
		var is_block := it.block != "" and it.icon.begins_with("block:")
		var pos := base + Vector3(sw_x, sw_y, sw_z)
		var rot := Vector3(sw_rot_x * 0.5, sw_rot_y, sw_rot_z)
		if using and it.kind == "food" or using and it.use == "drink":
			var wig := sin(use_t * 1.2) * 0.02
			pos = Vector3(0.25, -0.35 + wig - minf(1.0, use_t / 8.0) * 0.05, -0.55)
			rot = Vector3(deg_to_rad(-10.0), deg_to_rad(70.0), deg_to_rad(10.0))
		elif using and (it.name == "bow" or it.name == "crossbow"):
			var draw := clampf(use_t / 20.0, 0.0, 1.0)
			pos = Vector3(0.2 - draw * 0.05, -0.32, -0.55 + draw * 0.08) + Vector3(randf_range(-1, 1), randf_range(-1, 1), 0) * 0.003 * draw
			rot = Vector3(deg_to_rad(-5.0), deg_to_rad(-10.0), deg_to_rad(-30.0))
		elif using and it.name == "shield":
			pos = Vector3(0.3, -0.35, -0.5)
			rot = Vector3(0, deg_to_rad(10.0), 0)
		elif using and (it.name == "trident" or it.tool == "spear"):
			var ch := clampf(use_t / 10.0, 0.0, 1.0)
			pos = base + Vector3(0, 0.1 * ch, 0.2 * ch)
			rot = Vector3(deg_to_rad(-60.0 * ch), 0, 0)
		elif using and it.name == "spyglass":
			pos = Vector3(0.05, -0.2, -0.3)
			rot = Vector3.ZERO
		if is_block:
			item_root.transform = Transform3D(Basis.from_euler(Vector3(rot.x, deg_to_rad(45.0) + rot.y, rot.z)).scaled(Vector3.ONE * 0.4),
				pos + Vector3(0, 0.02, 0))
		else:
			var tool_like := it.tool != "" or it.name in ["stick", "bone", "blaze_rod", "breeze_rod", "fishing_rod", "carrot_on_a_stick",
				"warped_fungus_on_a_stick", "mace", "trident", "brush", "bow", "crossbow"]
			var b := Basis.from_euler(Vector3(0, deg_to_rad(-90.0), deg_to_rad(25.0))) if tool_like \
				else Basis.from_euler(Vector3(0, deg_to_rad(-80.0), deg_to_rad(20.0)))
			var rb := Basis.from_euler(rot) * b
			item_root.transform = Transform3D(rb.scaled(Vector3.ONE * (0.68 if tool_like else 0.55)), pos + Vector3(0, 0.12, 0))
		_apply_light(item_node, light)
	# offhand (left side, mirrored, no swing)
	if offhand_node != null:
		offhand_root.visible = true
		var ob := Vector3(-0.56, -0.52 + breathe, -0.72)
		if using and inter.blocking and off != null and off.item_name() == "shield":
			ob = Vector3(-0.3, -0.35, -0.5)
		offhand_root.transform = Transform3D(Basis.from_euler(Vector3(0, deg_to_rad(80.0), deg_to_rad(-20.0))).scaled(Vector3.ONE * 0.55), ob)
		_apply_light(offhand_node, light)
	else:
		offhand_root.visible = false


func _light() -> Vector2:
	var w: World = player.world
	if w == null:
		return Vector2(15, 0)
	var e: Vector3 = player.eye_position()
	var l := w.get_light(floori(e.x), floori(e.y), floori(e.z))
	return Vector2(float(l.x), float(l.y))


func _apply_light(n: Node, l: Vector2) -> void:
	if n == null:
		return
	if n is GeometryInstance3D:
		(n as GeometryInstance3D).set_instance_shader_parameter("ent_light", l)
	for c in n.get_children():
		_apply_light(c, l)


func _rebuild_item(st: ItemStack) -> void:
	if item_node != null:
		item_node.queue_free()
		item_node = null
	if st == null:
		return
	item_node = ItemIcons.held_model(st.item(), true)
	if item_node != null:
		item_root.add_child(item_node)
		if st.has_glint():
			_set_glint(item_node)


func _set_glint(n: Node) -> void:
	if n is MeshInstance3D:
		var mi: MeshInstance3D = n
		if mi.material_override is ShaderMaterial:
			var m := (mi.material_override as ShaderMaterial).duplicate() as ShaderMaterial
			m.set_shader_parameter("glint", 1.0)
			mi.material_override = m
	for c in n.get_children():
		_set_glint(c)


func _rebuild_offhand(st: ItemStack) -> void:
	if offhand_node != null:
		offhand_node.queue_free()
		offhand_node = null
	if st == null:
		return
	offhand_node = ItemIcons.held_model(st.item(), true)
	if offhand_node != null:
		offhand_root.add_child(offhand_node)
