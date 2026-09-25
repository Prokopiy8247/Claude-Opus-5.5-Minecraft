class_name WorldScene
extends Node3D
## The playable scene: creates the WorldSession from Game.pending_world, builds the WorldEnvironment
## (blocky sky shader, fog per dimension, ambient), applies the saved level, and tears everything
## down cleanly when leaving to the title screen.

var session: WorldSession = null
var env: WorldEnvironment = null
var sky_mat: ShaderMaterial = null
var sun: DirectionalLight3D = null
var rain_sound_timer := 0.0
var _apply_level := false


func _ready() -> void:
	var params := Game.pending_world
	if params.is_empty():
		params = {"name": "World", "seed": randi()}
	_build_environment()
	session = WorldSession.new()
	session.name = "Session"
	add_child(session)
	session.setup(params)
	Game.session = session
	var fx := WeatherFX.new()
	fx.name = "WeatherFX"
	fx.session = session
	add_child(fx)
	session.weather_fx = fx
	# wait one frame so the world's scenario and the chunk manager are inside the tree
	await get_tree().process_frame
	session.start()
	_autostart()


func _build_environment() -> void:
	env = WorldEnvironment.new()
	env.name = "Environment"
	var e := Environment.new()
	e.background_mode = Environment.BG_SKY
	e.sky = Sky.new()
	sky_mat = ShaderMaterial.new()
	sky_mat.shader = load("res://game/world/shaders/sky.gdshader")
	(e.sky as Sky).sky_material = sky_mat
	(e.sky as Sky).process_mode = Sky.PROCESS_MODE_REALTIME
	e.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	e.ambient_light_color = Color(1, 1, 1)
	e.ambient_light_energy = 0.0
	e.tonemap_mode = Environment.TONE_MAPPER_LINEAR
	e.ssao_enabled = false
	e.glow_enabled = true
	e.glow_intensity = 0.35
	e.glow_bloom = 0.05
	e.fog_enabled = true
	e.fog_mode = Environment.FOG_MODE_EXPONENTIAL
	e.fog_density = 0.0028
	e.fog_sky_affect = 0.0
	e.fog_light_color = Color(0.75, 0.85, 1.0)
	env.environment = e
	add_child(env)
	sun = DirectionalLight3D.new()
	sun.name = "Sun"
	sun.light_energy = 0.0     # the voxel light map does the work; this only shapes mobs/entities
	sun.shadow_enabled = false
	sun.rotation_degrees = Vector3(-50, 35, 0)
	add_child(sun)


func _autostart() -> void:
	var start: String = String(Game.cmdline.get("start", ""))
	if start != "":
		var run := load("res://game/tests/autotest.gd")
		if run != null:
			run.run(session, start)


func _process(delta: float) -> void:
	if session == null or session.player == null:
		return
	_update_environment()
	_update_weather_audio(delta)


const NETHER_FOG := {
	"nether_wastes": Color(0.2, 0.03, 0.03),
	"crimson_forest": Color(0.2, 0.01, 0.01),
	"warped_forest": Color(0.1, 0.02, 0.1),
	"soul_sand_valley": Color(0.1, 0.2, 0.2),
	"basalt_deltas": Color(0.4, 0.37, 0.42),
}
var _nether_fog := Color(0.2, 0.03, 0.03)


func _update_environment() -> void:
	var e := env.environment
	var d := session.dim
	var def := DimensionDB.get_def(d)
	# fog and sky follow the dimension
	if d == 1:
		# each Nether biome has its own haze; blend towards it so walking across borders is smooth
		var p: Vector3 = session.player.body.pos
		var bname := BiomeDB.name_of(session.world.biome_at(floori(p.x), floori(p.y), floori(p.z)))
		var want: Color = NETHER_FOG.get(bname, NETHER_FOG["nether_wastes"])
		_nether_fog = _nether_fog.lerp(want, 0.03)
		e.fog_light_color = _nether_fog
		e.fog_density = 0.034 if bname != "basalt_deltas" else 0.05
		e.ambient_light_energy = 0.22
		e.ambient_light_color = Color(0.55, 0.3, 0.3)
	elif d == 2:
		e.fog_light_color = Color(0.09, 0.07, 0.12)
		e.fog_density = 0.005
		e.ambient_light_energy = 0.3
		e.ambient_light_color = Color(0.6, 0.55, 0.75)
	else:
		var f := session.sun_angle_factor()
		var rain := session.weather
		var fog_col := Color(0.75, 0.85, 1.0).lerp(Color(0.95, 0.6, 0.45), clampf(1.0 - absf(f - 0.25) * 5.0, 0.0, 1.0) * 0.6)
		e.fog_light_color = fog_col.lerp(Color(0.55, 0.58, 0.6), 0.55 if rain > 0 else 0.0)
		e.fog_density = 0.0028 + (0.006 if rain == 1 else (0.012 if rain == 2 else 0.0))
		e.ambient_light_energy = 0.06 + 0.16 * f
		e.ambient_light_color = Color(0.55, 0.62, 0.78)
	sky_mat.set_shader_parameter("day_time", float(session.day_time))
	sky_mat.set_shader_parameter("dim", d)
	sky_mat.set_shader_parameter("rain", 0.75 if session.weather == 2 else (0.4 if session.weather == 1 else 0.0))
	if sun != null:
		sun.rotation_degrees = Vector3(-20.0 - 60.0 * sin(session.day_time / 24000.0 * TAU), 35, 0)


func _update_weather_audio(delta: float) -> void:
	if session.weather == 0 or session.dim != 0:
		rain_sound_timer = 0.0
		return
	rain_sound_timer -= delta
	if rain_sound_timer <= 0.0:
		rain_sound_timer = 2.6 if session.weather == 1 else 1.4
		var p := session.player.body.pos
		var above := session.world.top_solid_y(floori(p.x), floori(p.z))
		if p.y > float(above):
			Sfx.play_at("rain", p + Vector3(0, 6, 0), 0.5)
		if session.weather == 2 and randf() < 0.25:
			Sfx.play_ui("thunder", 0.5)


# ------------------------------------------------------------------------------------------------
func _unhandled_input(event: InputEvent) -> void:
	if session == null:
		return
	if event is InputEventKey and event.pressed and not event.echo:
		var k: InputEventKey = event
		if k.keycode == KEY_F3 and k.shift_pressed:
			session.chat("seed %d  dim %d  pos %s" % [session.seed_value, session.dim, str(session.player.body.pos.round())],
				Color(0.8, 0.9, 1.0))
		elif k.keycode == KEY_F1:
			session.hud_visible = not session.hud_visible
			if session.hud != null:
				session.hud.visible_hud = session.hud_visible


func _exit_tree() -> void:
	if session != null:
		session.save_all()
	Game.session = null
