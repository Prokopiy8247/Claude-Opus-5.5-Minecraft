class_name ParticleSystem
extends Node3D
## CPU-simulated particle pool rendered with one MultiMeshInstance3D (billboards in the shader).

const MAX := 3000

var world: World = null
var mm: MultiMesh
var mmi: MultiMeshInstance3D
var n := 0
var pos := PackedVector3Array()
var vel := PackedVector3Array()
var life := PackedFloat32Array()
var max_life := PackedFloat32Array()
var size := PackedFloat32Array()
var grav := PackedFloat32Array()
var drag := PackedFloat32Array()
var custom := PackedColorArray()     # layer, u, v, uv scale
var color := PackedColorArray()
var anim := PackedInt32Array()       # frames for animated sprites (base layer in custom.r)
var collide := PackedByteArray()
var _rng := RandomNumberGenerator.new()
var L := {}                          # particle sprite layers
var daylight := 1.0                  # sky light multiplier pushed by the session every frame


func _ready() -> void:
	mm = MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.use_colors = true
	mm.use_custom_data = true
	var q := QuadMesh.new()
	q.size = Vector2(1, 1)
	mm.mesh = q
	mm.instance_count = MAX
	mm.visible_instance_count = 0
	mmi = MultiMeshInstance3D.new()
	mmi.multimesh = mm
	mmi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var mat := ShaderMaterial.new()
	mat.shader = load("res://game/world/shaders/particle.gdshader")
	mat.set_shader_parameter("blocks", TextureLibrary.block_array)
	mmi.material_override = mat
	mmi.custom_aabb = AABB(Vector3(-100000, -1000, -100000), Vector3(200000, 3000, 200000))
	add_child(mmi)
	for arr in [pos, vel]:
		arr.resize(MAX)
	for arr2 in [life, max_life, size, grav, drag]:
		arr2.resize(MAX)
	custom.resize(MAX)
	color.resize(MAX)
	anim.resize(MAX)
	collide.resize(MAX)
	for k in ["smoke", "flame", "soul", "portal", "crit", "heart", "angry", "happy", "bubble", "splash", "drip", "note", "dust",
			"spark", "poof", "glint", "sculk", "large_smoke", "explosion", "end_rod", "snow", "rain", "lava", "totem"]:
		L[k] = BlockDB.layer_of("particle_" + k)
	_rng.randomize()


func spawn(p: Vector3, v: Vector3, layer: int, lifetime: float, sz: float, c: Color = Color.WHITE, g := 0.0, uvs := Vector3(0, 0, 1),
		frames := 1, dr := 0.98, coll := true) -> void:
	if n >= MAX:
		return
	var i := n
	n += 1
	pos[i] = p
	vel[i] = v
	life[i] = lifetime
	max_life[i] = lifetime
	size[i] = sz
	grav[i] = g
	drag[i] = dr
	custom[i] = Color(float(layer), uvs.x, uvs.y, uvs.z)
	color[i] = c
	anim[i] = frames
	collide[i] = 1 if coll else 0


func _light_mul(p: Vector3) -> float:
	if world == null:
		return 1.0
	var l := world.get_light(floori(p.x), floori(p.y), floori(p.z))
	var sky: float = daylight
	var lv := maxf(l.x * sky, l.y) / 15.0
	return clampf(lv / (4.0 - 3.0 * lv) * 0.9 + 0.12, 0.1, 1.0)


func _block_layer(v: int, face := 2) -> int:
	var id := v & 0xFFF
	var ft := BlockDB.face_tex[((id << 4) | ((v >> 12) & 15)) * 6 + face]
	return ft & 0xFFFF


func _block_tint(v: int, p: Vector3) -> Color:
	var d: BlockDef = BlockDB.defs[v & 0xFFF]
	if d.tint == BlockDB.T_NONE:
		return Color.WHITE
	if world != null:
		var c := world.chunk_at(floori(p.x), floori(p.z))
		if c != null:
			var ci := ((floori(p.z) & 15) << 4 | (floori(p.x) & 15)) * 3
			if d.tint == BlockDB.T_FOLIAGE:
				return c.colors[ci + 1]
			if d.tint == BlockDB.T_WATER:
				return c.colors[ci + 2]
			if d.tint == BlockDB.T_GRASS:
				return c.colors[ci]
	return Color(0.5, 0.75, 0.35)


func block_break(v: int, bp: Vector3) -> void:
	var face := 2 if BlockDB.model[v & 0xFFF] != BlockDB.M_CUBE else 4
	var layer := _block_layer(v, face)
	var tint := _block_tint(v, bp) * _light_mul(bp + Vector3(0.5, 0.5, 0.5))
	tint.a = 1.0
	for ix in 4:
		for iy in 4:
			for iz in 4:
				if _rng.randf() < 0.55:
					continue
				var o := Vector3((ix + 0.5) / 4.0, (iy + 0.5) / 4.0, (iz + 0.5) / 4.0)
				var v2 := (o - Vector3(0.5, 0.5, 0.5)) * 0.25 + Vector3(0, 0.05, 0)
				v2 += Vector3(_rng.randf_range(-0.05, 0.05), _rng.randf_range(0, 0.1), _rng.randf_range(-0.05, 0.05))
				var uv := Vector3(_rng.randi_range(0, 3) / 4.0, _rng.randi_range(0, 3) / 4.0, 0.25)
				spawn(bp + o, v2 * 20.0, layer, _rng.randf_range(0.5, 1.2), _rng.randf_range(0.08, 0.16), tint, 20.0, uv)


func block_hit(v: int, bp: Vector3, face: int) -> void:
	var layer := _block_layer(v, face)
	var tint := _block_tint(v, bp) * _light_mul(bp + Vector3(0.5, 1.0, 0.5))
	tint.a = 1.0
	var nrm: Vector3 = Vector3(Vox.DIR_VEC[face])
	for k in 2:
		var o := Vector3(_rng.randf(), _rng.randf(), _rng.randf())
		o = o * (Vector3.ONE - nrm.abs()) + (nrm * 0.55 + Vector3(0.5, 0.5, 0.5)) * nrm.abs()
		var uv := Vector3(_rng.randi_range(0, 3) / 4.0, _rng.randi_range(0, 3) / 4.0, 0.25)
		spawn(bp + o, nrm * 1.5 + Vector3(_rng.randf_range(-1, 1), 1.0, _rng.randf_range(-1, 1)), layer, 0.5, 0.08, tint, 20.0, uv)


func item_break(st: ItemStack, p: Vector3) -> void:
	for k in 10:
		spawn(p, Vector3(_rng.randf_range(-2, 2), _rng.randf_range(1, 3), _rng.randf_range(-2, 2)), L["dust"], 0.6, 0.07,
			Color(0.6, 0.6, 0.6), 20.0)


func crit(p: Vector3) -> void:
	for k in 14:
		var d := Vector3(_rng.randf_range(-1, 1), _rng.randf_range(-0.5, 1), _rng.randf_range(-1, 1)).normalized()
		spawn(p + d * 0.3, d * 5.0, L["crit"], 0.6, 0.15, Color(1, 1, 1), 6.0, Vector3(0, 0, 1), 1, 0.85)


func burst(p: Vector3, c: Color, count := 20, speed := 3.0, layer_key := "dust", sz := 0.12) -> void:
	for k in count:
		var d := Vector3(_rng.randf_range(-1, 1), _rng.randf_range(-1, 1), _rng.randf_range(-1, 1)).normalized()
		spawn(p, d * speed * _rng.randf_range(0.4, 1.0), L[layer_key], _rng.randf_range(0.4, 1.0), sz, c, 2.0)


func poof(p: Vector3, count := 12, scale := 1.0) -> void:
	for k in count:
		var d := Vector3(_rng.randf_range(-1, 1), _rng.randf_range(0, 1), _rng.randf_range(-1, 1))
		spawn(p + d * 0.4 * scale, d * 1.2, L["poof"], _rng.randf_range(0.5, 0.9), 0.35 * scale, Color(0.95, 0.95, 0.95), -0.5,
			Vector3(0, 0, 1), 4, 0.9, false)


func smoke(p: Vector3, count := 1, big := false) -> void:
	for k in count:
		spawn(p + Vector3(_rng.randf_range(-0.2, 0.2), 0, _rng.randf_range(-0.2, 0.2)), Vector3(_rng.randf_range(-0.2, 0.2), 1.2, _rng.randf_range(-0.2, 0.2)),
			L["large_smoke" if big else "smoke"], _rng.randf_range(0.8, 1.6), 0.25 if not big else 0.6,
			Color(0.35, 0.35, 0.35) if big else Color(0.55, 0.55, 0.55), -0.6, Vector3(0, 0, 1), 8, 0.96, false)


func flame(p: Vector3, soul := false) -> void:
	spawn(p, Vector3(0, 0.3, 0), L["soul" if soul else "flame"], 0.5, 0.12, Color(1, 1, 1), -0.2, Vector3(0, 0, 1), 1, 0.96, false)


func portal(p: Vector3, target: Vector3 = Vector3.INF) -> void:
	var v := Vector3(_rng.randf_range(-1, 1), _rng.randf_range(-0.5, 1), _rng.randf_range(-1, 1)) * 1.5
	if target != Vector3.INF:
		v = (target - p) * 1.5
	spawn(p, v, L["portal"], _rng.randf_range(0.6, 1.2), 0.1, Color(0.7 + _rng.randf() * 0.3, 0.3, 1.0), 0.0, Vector3(0, 0, 1), 1, 0.95, false)


func explosion(p: Vector3, power: float) -> void:
	var count := int(clampf(power * 12.0, 12.0, 90.0))
	for k in count:
		var d := Vector3(_rng.randf_range(-1, 1), _rng.randf_range(-1, 1), _rng.randf_range(-1, 1))
		spawn(p + d * power * 0.5, d * 2.0, L["explosion"], _rng.randf_range(0.4, 0.8), _rng.randf_range(0.5, 1.2) * (0.6 + power * 0.15),
			Color(1, 1, 1), 0.0, Vector3(0, 0, 1), 4, 0.9, false)
	for k in count:
		var d2 := Vector3(_rng.randf_range(-1, 1), _rng.randf_range(0, 1), _rng.randf_range(-1, 1))
		spawn(p + d2 * power * 0.6, d2 * 3.0 + Vector3(0, 1, 0), L["large_smoke"], _rng.randf_range(1.0, 2.2), _rng.randf_range(0.6, 1.4),
			Color(0.4, 0.4, 0.4), -0.8, Vector3(0, 0, 1), 8, 0.93, false)


func hearts(p: Vector3, count := 5, angry := false) -> void:
	for k in count:
		spawn(p + Vector3(_rng.randf_range(-0.5, 0.5), _rng.randf_range(0, 0.5), _rng.randf_range(-0.5, 0.5)), Vector3(0, 0.6, 0),
			L["angry" if angry else "heart"], 1.0, 0.18, Color(1, 1, 1), 0.0, Vector3(0, 0, 1), 1, 0.9, false)


func happy(p: Vector3, count := 8) -> void:
	for k in count:
		spawn(p + Vector3(_rng.randf_range(-0.6, 0.6), _rng.randf_range(0, 1.0), _rng.randf_range(-0.6, 0.6)), Vector3(0, 0.2, 0),
			L["happy"], 1.0, 0.12, Color(1, 1, 1), 0.0, Vector3(0, 0, 1), 1, 0.9, false)


func bubbles(p: Vector3, count := 4) -> void:
	for k in count:
		spawn(p + Vector3(_rng.randf_range(-0.3, 0.3), 0, _rng.randf_range(-0.3, 0.3)), Vector3(0, 1.5, 0), L["bubble"], 1.0, 0.1,
			Color(1, 1, 1), -2.0, Vector3(0, 0, 1), 1, 0.9, true)


func splash(p: Vector3, count := 20) -> void:
	for k in count:
		spawn(p + Vector3(_rng.randf_range(-0.5, 0.5), 0, _rng.randf_range(-0.5, 0.5)), Vector3(_rng.randf_range(-1.5, 1.5), _rng.randf_range(2, 5), _rng.randf_range(-1.5, 1.5)),
			L["splash"], 0.6, 0.1, Color(0.6, 0.75, 1.0), 20.0)


func dust(p: Vector3, c: Color, count := 1) -> void:
	for k in count:
		spawn(p + Vector3(_rng.randf_range(-0.2, 0.2), _rng.randf_range(-0.2, 0.2), _rng.randf_range(-0.2, 0.2)), Vector3(0, 0.3, 0),
			L["dust"], 0.8, 0.08, c, 0.0, Vector3(0, 0, 1), 1, 0.9, false)


func note(p: Vector3, hue: float) -> void:
	spawn(p, Vector3(0, 1.0, 0), L["note"], 1.0, 0.2, Color.from_hsv(hue, 0.9, 1.0), 0.0, Vector3(0, 0, 1), 1, 0.9, false)


func spark(p: Vector3, c: Color, count := 8, speed := 4.0) -> void:
	for k in count:
		var d := Vector3(_rng.randf_range(-1, 1), _rng.randf_range(-1, 1), _rng.randf_range(-1, 1)).normalized()
		spawn(p, d * speed, L["spark"], _rng.randf_range(0.5, 1.2), 0.12, c, 0.5, Vector3(0, 0, 1), 1, 0.95, false)


func dragon_breath(p: Vector3, count := 6) -> void:
	for k in count:
		spawn(p + Vector3(_rng.randf_range(-1, 1), _rng.randf_range(0, 0.5), _rng.randf_range(-1, 1)), Vector3(_rng.randf_range(-0.3, 0.3), 0.3, _rng.randf_range(-0.3, 0.3)),
			L["portal"], _rng.randf_range(1.0, 2.0), 0.25, Color(0.8, 0.2, 1.0), 0.0, Vector3(0, 0, 1), 1, 0.97, false)


func end_rod(p: Vector3, v := Vector3.ZERO) -> void:
	spawn(p, v, L["end_rod"], _rng.randf_range(1.0, 2.0), 0.1, Color(1, 1, 1), -0.1, Vector3(0, 0, 1), 1, 0.97, false)


func totem(p: Vector3) -> void:
	for k in 80:
		var d := Vector3(_rng.randf_range(-1, 1), _rng.randf_range(0, 1.5), _rng.randf_range(-1, 1))
		var c := Color(0.6 + _rng.randf() * 0.4, 0.8 + _rng.randf() * 0.2, 0.2) if _rng.randf() < 0.5 else Color(1.0, 0.8, 0.2)
		spawn(p, d * 5.0, L["spark"], _rng.randf_range(1.0, 2.0), 0.14, c, 3.0, Vector3(0, 0, 1), 1, 0.93, false)


func _process(delta: float) -> void:
	var dt := minf(delta, 0.05)
	var i := 0
	var solid := BlockDB.solid
	while i < n:
		life[i] -= dt
		if life[i] <= 0.0:
			n -= 1
			if i != n:
				pos[i] = pos[n]
				vel[i] = vel[n]
				life[i] = life[n]
				max_life[i] = max_life[n]
				size[i] = size[n]
				grav[i] = grav[n]
				drag[i] = drag[n]
				custom[i] = custom[n]
				color[i] = color[n]
				anim[i] = anim[n]
				collide[i] = collide[n]
			continue
		var v := vel[i]
		v.y -= grav[i] * dt
		v *= pow(drag[i], dt * 20.0)
		var np := pos[i] + v * dt
		if collide[i] == 1 and world != null:
			var bv := world.get_block(floori(np.x), floori(np.y), floori(np.z))
			if bv != 0 and solid[bv & 0xFFF] == 1:
				np = pos[i]
				v = Vector3(v.x * 0.3, 0.0, v.z * 0.3)
		pos[i] = np
		vel[i] = v
		i += 1
	mm.visible_instance_count = n
	for k in n:
		var s := size[k]
		var frames := anim[k]
		var cst := custom[k]
		if frames > 1:
			var fr := clampi(int((1.0 - life[k] / max_life[k]) * frames), 0, frames - 1)
			cst = Color(cst.r + fr, cst.g, cst.b, cst.a)
		mm.set_instance_transform(k, Transform3D(Basis().scaled(Vector3(s, s, s)), pos[k]))
		mm.set_instance_custom_data(k, cst)
		var c := color[k]
		mm.set_instance_color(k, c)
