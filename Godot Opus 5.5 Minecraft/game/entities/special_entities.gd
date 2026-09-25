class_name SpecialEntities
extends RefCounted
## Entities that are neither mobs nor simple physics objects: End crystals (heal the dragon,
## explode when hit) and thrown eyes of ender (fly towards the nearest stronghold).

static var _mats := {}


static func _glow_mat(c: Color, emission: float, alpha := 1.0) -> StandardMaterial3D:
	var key := "%s|%.2f|%.2f" % [c.to_html(), emission, alpha]
	if _mats.has(key):
		return _mats[key]
	var m := StandardMaterial3D.new()
	m.albedo_color = Color(c.r, c.g, c.b, alpha)
	m.emission_enabled = true
	m.emission = c
	m.emission_energy_multiplier = emission
	m.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	if alpha < 1.0:
		m.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		m.cull_mode = BaseMaterial3D.CULL_DISABLED
	_mats[key] = m
	return m


static func _cube(size: float, mat: Material) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = Vector3.ONE * size
	mi.mesh = bm
	mi.material_override = mat
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return mi


# ================================================================================================
class EndCrystal extends Entity:
	var outer: MeshInstance3D
	var middle: MeshInstance3D
	var core: MeshInstance3D
	var beam: MeshInstance3D
	var pillar := -1
	var show_base := true

	func _init() -> void:
		type_name = "end_crystal"
		body.width = 2.0
		body.height = 2.0
		gravity = 0.0

	func setup_crystal(w: World, pos: Vector3, p_pillar := -1) -> void:
		setup(w, pos)
		pillar = p_pillar
		no_physics = true
		health = 1.0
		max_health = 1.0
		outer = SpecialEntities._cube(1.25, SpecialEntities._glow_mat(Color(0.85, 0.75, 1.0), 0.6, 0.35))
		middle = SpecialEntities._cube(0.9, SpecialEntities._glow_mat(Color(0.9, 0.6, 1.0), 0.9, 0.5))
		core = SpecialEntities._cube(0.5, SpecialEntities._glow_mat(Color(1.0, 0.35, 0.8), 2.5))
		for n in [outer, middle, core]:
			(n as Node3D).position = Vector3(0, 1.2, 0)
			visual.add_child(n)
		# the bedrock base comes from the Blender model (the cubes stay procedural so they can glow)
		var model := ModelLibrary.entity_model("end_crystal")
		if model != null:
			for nm in ["end_crystal__outer", "end_crystal__middle", "end_crystal__core"]:
				var extra := model.find_child(nm, true, false)
				if extra != null:
					extra.queue_free()
			visual.add_child(model)
		beam = MeshInstance3D.new()
		var cyl := CylinderMesh.new()
		cyl.top_radius = 0.06
		cyl.bottom_radius = 0.06
		cyl.height = 1.0
		beam.mesh = cyl
		beam.material_override = SpecialEntities._glow_mat(Color(1.0, 0.6, 1.0), 2.0, 0.8)
		beam.visible = false
		add_child(beam)

	func tick() -> void:
		age += 1
		prev_pos = body.pos

	func frame(_alpha: float) -> void:
		global_position = body.pos
		var t := float(Time.get_ticks_msec()) / 1000.0
		var bob := sin(t * 2.0) * 0.25
		if outer != null:
			outer.rotation = Vector3(t * 1.3, t * 1.7, 0.0)
			outer.position.y = 1.2 + bob
		if middle != null:
			middle.rotation = Vector3(-t * 1.1, t * 0.9, t * 0.6)
			middle.position.y = 1.2 + bob
		if core != null:
			core.rotation = Vector3(t * 2.1, 0.0, t * 1.5)
			core.position.y = 1.2 + bob
		# healing beam to the dragon
		var dragon = DragonFight.dragon_entity(session)
		if dragon != null and (dragon as Node3D).global_position.distance_to(global_position) < 48.0:
			var a := global_position + Vector3(0, 1.2 + bob, 0)
			var b: Vector3 = (dragon as Node3D).global_position + Vector3(0, 1.5, 0)
			var mid := (a + b) * 0.5
			var len := a.distance_to(b)
			beam.visible = true
			beam.global_position = mid
			beam.scale = Vector3(1, len, 1)
			var up := (b - a).normalized()
			if absf(up.dot(Vector3.UP)) < 0.999:
				var x := up.cross(Vector3.UP).normalized()
				var z := x.cross(up).normalized()
				beam.global_basis = Basis(x, up, z).scaled(Vector3(1, len, 1))
		else:
			beam.visible = false

	func hurt(amount: float, cause: String, attacker = null, _dir := Vector2.ZERO, _kb := 0.0) -> float:
		if dead:
			return 0.0
		dead = true
		DragonFight.crystal_destroyed(self, attacker)
		Explosions.explode(world, body.pos + Vector3(0, 1.0, 0), 6.0, false, self, true)
		queue_free()
		return amount

	func can_be_pushed() -> bool:
		return false


# ================================================================================================
class EyeOfEnder extends Entity:
	var goal := Vector3.ZERO
	var core: MeshInstance3D
	var life := 80

	func _init() -> void:
		type_name = "eye_of_ender"
		body.width = 0.25
		body.height = 0.25
		gravity = 0.0

	func setup_eye(w: World, pos: Vector3, p_target: Vector3) -> void:
		setup(w, pos)
		no_physics = true
		goal = p_target
		core = SpecialEntities._cube(0.3, SpecialEntities._glow_mat(Color(0.2, 0.75, 0.5), 1.2))
		visual.add_child(core)
		var pupil := SpecialEntities._cube(0.14, SpecialEntities._glow_mat(Color(0.05, 0.1, 0.05), 0.0))
		pupil.position = Vector3(0, 0, -0.12)
		core.add_child(pupil)

	func tick() -> void:
		age += 1
		prev_pos = body.pos
		var to := Vector2(goal.x - body.pos.x, goal.z - body.pos.z)
		var d := to.length()
		var dir := to.normalized() if d > 0.01 else Vector2.ZERO
		# rise, then glide towards the target; hover when close
		var speed := 0.5 if d > 12.0 else d * 0.03
		var vy := 0.08 if age < 40 else (-0.02 if d < 12.0 else 0.0)
		body.pos += Vector3(dir.x * speed, vy, dir.y * speed)
		facing = atan2(-dir.x, -dir.y)
		if session != null and age % 2 == 0:
			session.particles.portal(body.pos)
		if age >= life:
			if session != null:
				var rng := RandomNumberGenerator.new()
				rng.randomize()
				if rng.randf() < 0.8:
					session.entities.spawn_item(world, body.pos, ItemStack.of("ender_eye", 1), Vector3.ZERO)
				else:
					Sfx.play_at("glass_break", body.pos, 0.6)
					session.particles.burst(body.pos, Color(0.2, 0.7, 0.5), 16, 2.0, "spark", 0.1)
			queue_free()

	func frame(alpha: float) -> void:
		global_position = prev_pos.lerp(body.pos, alpha)
		if core != null:
			core.rotation.y = facing
