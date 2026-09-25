class_name WeatherFX
extends Node3D
## Rain and snow around the camera, and lightning bolts. Drops are drawn with two MultiMeshes;
## every drop belongs to a column and is only drawn above that column's top block, so roofs,
## caves and overhangs stay dry. Cold biomes get snow, dry biomes (desert, savanna, badlands)
## nothing. Bolts are jagged emissive segments shown for a few frames with a sky flash; the
## session decides where they strike and applies fire/damage.

const DROPS := 1000
const RADIUS := 14.0

var session = null
var intensity := 0.0
var flash := 0.0                    # 0..1 sky flash from the last bolt (read by the session)

var _rain: MultiMesh
var _snow: MultiMesh
var _pos := PackedVector3Array()
var _speed := PackedFloat32Array()
var _kind := PackedByteArray()      # 0 none, 1 rain, 2 snow
var _floor := PackedFloat32Array()  # y where the drop hits the column top
var _rng := RandomNumberGenerator.new()
var _bolt: MeshInstance3D = null
var _bolt_life := 0.0


func _ready() -> void:
	_rng.randomize()
	_rain = _make_mm(Vector2(0.05, 0.75), Color(0.62, 0.72, 0.95, 0.5), BaseMaterial3D.BILLBOARD_FIXED_Y)
	_snow = _make_mm(Vector2(0.1, 0.1), Color(0.97, 0.98, 1.0, 0.9), BaseMaterial3D.BILLBOARD_ENABLED)
	_pos.resize(DROPS)
	_speed.resize(DROPS)
	_kind.resize(DROPS)
	_floor.resize(DROPS)
	for i in DROPS:
		_floor[i] = INF


func _make_mm(size: Vector2, col: Color, billboard: int) -> MultiMesh:
	var q := QuadMesh.new()
	q.size = size
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.albedo_color = col
	mat.billboard_mode = billboard
	mat.billboard_keep_scale = true
	mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	q.material = mat
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.mesh = q
	mm.instance_count = DROPS
	mm.visible_instance_count = 0
	var mi := MultiMeshInstance3D.new()
	mi.multimesh = mm
	mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	mi.extra_cull_margin = 64.0
	add_child(mi)
	return mm


func _respawn(i: int, center: Vector3) -> void:
	var w: World = session.world
	var x := center.x + _rng.randf_range(-RADIUS, RADIUS)
	var z := center.z + _rng.randf_range(-RADIUS, RADIUS)
	var top := float(w.top_y(floori(x), floori(z)) + 1)
	var y := center.y + _rng.randf_range(3.0, 12.0)
	_pos[i] = Vector3(x, y, z)
	_floor[i] = top
	var b: Dictionary = BiomeDB.defs[w.biome_at(floori(x), floori(y), floori(z))]
	if not bool(b.get("rain", true)):
		_kind[i] = 0
	elif bool(b.get("snowy", false)) or float(b.get("temp", 0.8)) < 0.15:
		_kind[i] = 2
		_speed[i] = _rng.randf_range(1.4, 2.4)
	else:
		_kind[i] = 1
		_speed[i] = _rng.randf_range(11.0, 14.0)


func _process(delta: float) -> void:
	_update_bolt(delta)
	if session == null or session.player == null or session.world == null:
		return
	var want := 1.0 if (session.world.dim == 0 and session.weather > 0) else 0.0
	intensity = move_toward(intensity, want, delta * 0.35)
	var active := int(float(DROPS) * intensity)
	if active <= 0:
		_rain.visible_instance_count = 0
		_snow.visible_instance_count = 0
		return
	var cam := get_viewport().get_camera_3d()
	var center: Vector3 = cam.global_position if cam != null else session.player.eye_position()
	var nr := 0
	var ns := 0
	var t := float(Time.get_ticks_msec()) * 0.001
	for i in active:
		var p := _pos[i]
		if _floor[i] == INF or p.y < center.y - 14.0 or absf(p.x - center.x) > RADIUS + 1.0 or absf(p.z - center.z) > RADIUS + 1.0:
			_respawn(i, center)
			p = _pos[i]
		p.y -= _speed[i] * delta
		if _kind[i] == 2:
			p.x += sin(t * 1.3 + float(i)) * delta * 0.4
		if p.y < _floor[i]:
			if _kind[i] == 1 and _rng.randf() < 0.05 and session.particles != null and p.distance_to(center) < 9.0:
				session.particles.splash(Vector3(p.x, _floor[i] + 0.05, p.z), 1)
			_respawn(i, center)
			continue
		_pos[i] = p
		if p.y < _floor[i] or _kind[i] == 0:
			continue
		var xf := Transform3D(Basis(), p)
		if _kind[i] == 1:
			_rain.set_instance_transform(nr, xf)
			nr += 1
		else:
			_snow.set_instance_transform(ns, xf)
			ns += 1
	_rain.visible_instance_count = nr
	_snow.visible_instance_count = ns


## A lightning bolt from the sky down to `ground`.
func show_bolt(ground: Vector3) -> void:
	if _bolt != null:
		_bolt.queue_free()
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var p := ground + Vector3(0, 90, 0)
	var rng := RandomNumberGenerator.new()
	rng.randomize()
	while p.y > ground.y:
		var q := p + Vector3(rng.randf_range(-1.6, 1.6), -rng.randf_range(4.0, 8.0), rng.randf_range(-1.6, 1.6))
		if q.y < ground.y:
			q = ground
		_segment(st, p, q, 0.18)
		if rng.randf() < 0.25:
			_segment(st, q, q + Vector3(rng.randf_range(-4, 4), -rng.randf_range(3, 6), rng.randf_range(-4, 4)), 0.1)
		p = q
	var mat := StandardMaterial3D.new()
	mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mat.albedo_color = Color(0.85, 0.9, 1.0)
	mat.emission_enabled = true
	mat.emission = Color(0.8, 0.85, 1.0)
	mat.emission_energy_multiplier = 4.0
	mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	_bolt = MeshInstance3D.new()
	_bolt.mesh = st.commit()
	_bolt.material_override = mat
	_bolt.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(_bolt)
	_bolt_life = 0.35
	flash = 1.0


func _segment(st: SurfaceTool, a: Vector3, b: Vector3, r: float) -> void:
	# two crossed quads so the bolt reads from every side
	for off in [Vector3(r, 0, 0), Vector3(0, 0, r)]:
		var o: Vector3 = off
		st.add_vertex(a - o)
		st.add_vertex(a + o)
		st.add_vertex(b + o)
		st.add_vertex(a - o)
		st.add_vertex(b + o)
		st.add_vertex(b - o)


func _update_bolt(delta: float) -> void:
	flash = maxf(0.0, flash - delta * 2.5)
	if _bolt == null:
		return
	_bolt_life -= delta
	# flicker like the original's multi-stroke bolts
	_bolt.visible = fmod(_bolt_life, 0.1) > 0.03
	if _bolt_life <= 0.0:
		_bolt.queue_free()
		_bolt = null
