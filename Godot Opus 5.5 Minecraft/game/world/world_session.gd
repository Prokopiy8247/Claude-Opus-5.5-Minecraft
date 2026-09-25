class_name WorldSession
extends Node3D
## One loaded game session: the 20 TPS simulation loop, dimensions (Overworld / Nether / End) and
## travel between them, time of day, weather, entities, block entities, boss bars, break overlay,
## chat + commands, saving, and the HUD/UI wiring.

signal chat_message(text: String, color: Color)
signal action_bar_message(text: String)
signal dimension_changed(dim: int)
signal gamemode_message(text: String)

const TPS := 20.0
const TICK_MS := 1000.0 / TPS
const DAY_TICKS := 24000
const MAX_STEPS := 5

var world_name := "World"
var folder := "World"
var seed_value := 0
var difficulty := 2              # 0 peaceful, 1 easy, 2 normal, 3 hard
var hardcore := false
var tick := 0                    # ticks since the session started (simulation)
var tick_count := 0              # alias kept for block entity / behaviour code
var day_time := 1000             # 0..23999 (0 sunrise, 6000 noon, 18000 dusk)
var tick_accum := 0.0
var tick_alpha := 1.0
var running := true
var paused := false
var player: Player = null
var world: World = null
var dim := 0
var worlds := {}                 # dim -> World
var entities: EntityManager = null
var particles = null             # ParticleSystem
var ui = null                    # UIRoot
var hud = null                   # Hud
var hud_visible := true
var debug_visible := false

var gamerules := {
	"doMobSpawning": true, "doTileDrops": true, "doFireTick": true, "mobGriefing": true, "keepInventory": false,
	"doDaylightCycle": true, "doWeatherCycle": true, "naturalRegeneration": true, "showDeathMessages": true,
	"doMobLoot": true, "randomTickSpeed": 3, "doImmediateRespawn": false, "doInsomnia": true,
}

var weather := 0                 # 0 clear, 1 rain, 2 thunder
var weather_timer := 12000

# Block entities that need per-tick work, keyed per dimension: dim -> {Vector3i: true}
var ticking_bes := {}

var ender_inventory: Inventory = null
var weather_fx = null            # WeatherFX (rain/snow drops, lightning bolts), set by WorldScene
var last_death_pos := Vector3.INF
var enchant_seed := 0
var boss_bars := {}              # key -> {name, hp, max}

var break_pos := Vector3i.ZERO
var break_stage := -1

var geysers := []                # [{pos: Vector3i, ticks: int}]
var portals_ow := []
var portals_nether := []
var dragon_crystals_list: Array = []

var sleeping := false
var sleep_timer := 0
var bed_position := Vector3i.ZERO

var save_timer := 2400
var autosave := true
var camera_shake_amount := 0.0
var camera_shake_decay := 0.0
var traveling := false
var travel_stage := 0.0          # 0..1 progress, non-zero means the travelling overlay is up
var travel_label := ""
var stats := {"chunks": 0, "entities": 0, "fps": 0}
var autotest := ""
var autotest_timer := 0
var autotest_stage := 0
var autotest_dir := "res://shots"
var placing_scene := false
var recent_break: Dictionary = {}

var _sky_light := 1.0
var _last_died_tick := -1000
var start_dim := 0


func _ready() -> void:
	entities = EntityManager.new()
	add_child(entities)
	particles = ParticleSystem.new()
	add_child(particles)
	tick_accum = 0.0


# ------------------------------------------------------------------------------------------------
# Setup
func setup(params: Dictionary) -> void:
	SaveManager.prepare_palette(params.get("block_palette", []))
	world_name = String(params.get("name", "World"))
	folder = String(params.get("folder", SaveManager.unique_folder(world_name)))
	seed_value = int(params.get("seed", randi()))
	difficulty = int(params.get("difficulty", 2))
	hardcore = bool(params.get("hardcore", false))
	gamerules = (params.get("gamerules", gamerules) as Dictionary).duplicate()
	autotest = Game.autotest
	autotest_dir = String(Game.cmdline.get("shots", "user://autotest"))
	ender_inventory = Inventory.new(27)
	enchant_seed = randi()

	var gm := int(params.get("gamemode", Player.CREATIVE))
	var save_dir := SaveManager.dim_dir(folder, 0)
	DirAccess.make_dir_recursive_absolute(save_dir)

	world = World.new()
	add_child(world)
	world.setup(self, 0, seed_value, save_dir)
	worlds[0] = world
	if params.has("dragon_defeated"):
		DragonFight.defeated = bool(params["dragon_defeated"])
	entities.world = world
	if particles != null:
		particles.world = world

	var saved_player: Dictionary = params.get("player", {})
	var spawn := Vector3(0.5, 100.0, 0.5)
	if saved_player.is_empty() or not params.has("player"):
		spawn = world.gen.find_spawn()
		spawn.y += 0.2

	player = Player.new()
	add_child(player)
	player.setup(self, world, spawn)
	# however the player dies (damage, /kill, void), the session drops the inventory and shows
	# the death screen exactly once
	player.died.connect(_on_player_death)
	player.set_gamemode(gm)
	if player.spawn_point == Vector3.ZERO:
		player.spawn_point = spawn
		player.spawn_dim = 0

	if not saved_player.is_empty():
		apply_level(params)

	if not saved_player.is_empty():
		player.from_dict(saved_player)

	start_dim = int(params.get("dim", 0))
	print("[Session] world '%s' seed %d spawn %s dim %d" % [world_name, seed_value, spawn, start_dim])


func start() -> void:
	if hud == null:
		hud = Hud.new()
		hud.session = self
		add_child(hud)
	if ui == null:
		ui = UIRoot.new()
		ui.session = self
		add_child(ui)
	if ui != null and ui.has_method("refresh"):
		ui.refresh()
	dimension_changed.emit(dim)
	if autotest == "":
		chat("Welcome to %s" % world_name, Color(1, 1, 0.6))
		chat("E inventory | T chat | / commands | F7 admin panel | F3 debug", Color(0.8, 0.8, 0.85))
	if start_dim != 0:
		# the save was made in another dimension: bring the player back there
		var saved_pos: Vector3 = player.body.pos
		player.teleport(world.gen.find_spawn())
		await travel_to(start_dim, saved_pos, false)


# ------------------------------------------------------------------------------------------------
# Main loop
func _process(delta: float) -> void:
	if paused or player == null:
		tick_alpha = 1.0
		return
	delta = minf(delta, 0.25)
	tick_accum += delta * TPS
	var steps := 0
	while tick_accum >= 1.0 and steps < MAX_STEPS:
		tick_accum -= 1.0
		simulate()
		steps += 1
	if steps == MAX_STEPS:
		tick_accum = minf(tick_accum, 1.0)
	tick_alpha = clampf(tick_accum, 0.0, 1.0)
	_update_sky(delta)
	_update_camera_shake(delta)
	stats.fps = Engine.get_frames_per_second()
	if autotest != "":
		_step_autotest(delta)


func simulate() -> void:
	tick += 1
	tick_count = tick
	if bool(gamerules.get("doDaylightCycle", true)):
		day_time = (day_time + 1) % DAY_TICKS
	_tick_weather()
	_tick_lightning()

	# only the active dimension simulates; the others are frozen (hidden, not streaming) until the
	# player comes back, like unloaded dimensions in the original
	world.focus = player.body.pos
	world.tick()

	entities.tick()
	BlockEntityTicker.tick_all(world, self)
	_tick_portals()
	_tick_plates()
	_tick_geysers()
	_tick_sleep()
	if player != null:
		player.tick()
		Sfx.listener_pos = player.eye_position()
	if tick % 20 == 0:
		_tick_second()
	if tick % 10 == 0 and player != null and not player.dead:
		var readout := ItemExtras.held_readout(player)
		if readout != "":
			action_bar(readout)
	if autosave:
		save_timer -= 1
		if save_timer <= 0:
			save_timer = 2400
			if terrain_ready():
				save_all()


func terrain_ready() -> bool:
	return world.is_ready_at(floori(player.body.pos.x), floori(player.body.pos.z))


func _tick_second() -> void:
	stats.chunks = world.cm.chunks.size()
	var n := 0
	for e in entities.all():
		if e is Mob:
			n += 1
	stats.entities = n
	if player != null and not player.dead and player.stats.health <= 0.0:
		player.die(player.stats.last_cause)
	# hotbar food-poisoning style ambient checks
	if hud != null and hud.has_method("update_stats"):
		hud.update_stats()


# ------------------------------------------------------------------------------------------------
# Weather
func _tick_weather() -> void:
	if not bool(gamerules.get("doWeatherCycle", true)):
		return
	weather_timer -= 1
	if weather_timer > 0:
		return
	if weather == 0:
		if randf() < 0.28:
			weather = 2 if randf() < 0.25 else 1
			weather_timer = randi_range(2400, 12000)
		else:
			weather_timer = randi_range(12000, 60000)
	elif randf() < 0.5:
		weather = 2 if (weather == 1 and randf() < 0.3) else 0
		weather_timer = randi_range(12000, 60000)
	else:
		weather_timer = randi_range(1200, 6000)


## Thunderstorms strike near the player every ~15 s on average (only where it rains).
func _tick_lightning() -> void:
	if weather != 2 or dim != 0 or player == null or randf() > 1.0 / 300.0:
		return
	var ang := randf() * TAU
	var r := randf_range(6.0, 64.0)
	var x := floori(player.body.pos.x + cos(ang) * r)
	var z := floori(player.body.pos.z + sin(ang) * r)
	if not world.is_ready_at(x, z):
		return
	var b: Dictionary = BiomeDB.defs[world.biome_at(x, 64, z)]
	if not bool(b.get("rain", true)) or bool(b.get("snowy", false)):
		return
	strike_lightning(Vector3(x + 0.5, world.top_y(x, z) + 1, z + 0.5))


## A lightning strike: bolt + flash + thunder, fire at the impact, 5 damage and burning within
## 3.5 blocks, pigs turn into zombified piglins and villagers into witches, creepers get charged.
func strike_lightning(p: Vector3) -> void:
	if weather_fx != null:
		weather_fx.show_bolt(p)
	Sfx.play_at("thunder", p, 1.0)
	camera_shake(p, 1.5)
	var bp := Vector3i(floori(p.x), floori(p.y), floori(p.z))
	var ground_ok := BlockDB.solid[world.get_id(bp.x, bp.y - 1, bp.z)] == 1
	if bool(gamerules.get("doFireTick", true)) and difficulty > 0 and world.get_block(bp.x, bp.y, bp.z) == 0 and ground_ok:
		world.set_block(bp.x, bp.y, bp.z, BlockDB.id("fire"))
	for e in entities.all().duplicate():
		if not is_instance_valid(e):
			continue
		var ent: Entity = e
		if ent.world != world or ent.body.pos.distance_to(p) > 3.5:
			continue
		if ent is Mob:
			var m: Mob = ent
			var into := {"pig": "zombified_piglin", "villager": "witch"}
			if into.has(m.mob):
				entities.spawn_mob(world, String(into[m.mob]), m.body.pos, {"persistent": true})
				m.queue_free()
				continue
			if m.mob == "creeper":
				m.data["charged"] = true
			m.hurt(5.0, "lightning")
			m.set_on_fire(160)
		elif ent is SimpleEntities.ItemEntity:
			ent.queue_free()
	if player != null and player.world == world and player.body.pos.distance_to(p) < 3.5:
		player.stats.damage(5.0, "lightning")
		player.stats.fire_ticks = maxi(player.stats.fire_ticks, 160)


# ------------------------------------------------------------------------------------------------
# Sky / light
## Daylight factor 0..1 (Minecraft curve): full from ~1000 to ~11000, 0.5 at sunrise (0) and
## sunset (12000), dark from ~13000 to ~23000. day_time 6000 is noon.
func sun_angle_factor() -> float:
	var t := float(day_time) / float(DAY_TICKS)
	return clampf(cos((t - 0.25) * TAU) * 2.0 + 0.5, 0.0, 1.0)


func sky_darken() -> int:
	return int(round((1.0 - sun_angle_factor()) * 11.0))


func is_night() -> bool:
	return day_time > 13000 and day_time < 23000


func thundering() -> bool:
	return weather == 2


func is_raining_at(w: World, x: int, _y: int, z: int) -> bool:
	if weather == 0:
		return false
	if w == null or w.dim != 0:
		return weather != 0
	if not w.is_loaded(x, z):
		return false
	return true


func _update_sky(delta: float) -> void:
	var f := sun_angle_factor()
	var twilight := clampf(1.0 - absf(f - 0.28) * 3.5, 0.0, 1.0)
	var day_light := lerpf(0.05, 1.0, f)
	var rain := 1.0 if weather == 0 else (0.55 if weather == 1 else 0.35)
	var target := day_light * rain
	if dim == 1:
		target = 0.42
	elif dim == 2:
		target = 0.6
	_sky_light = lerpf(_sky_light, target, minf(1.0, delta * 0.6))
	var flash: float = weather_fx.flash if weather_fx != null else 0.0
	RenderingServer.global_shader_parameter_set("daylight", maxf(_sky_light, flash * 0.9))
	if particles != null:
		particles.daylight = _sky_light
	RenderingServer.global_shader_parameter_set("block_flicker", 0.94 + sin(float(tick) * 0.3) * 0.03 + randf() * 0.02)
	RenderingServer.global_shader_parameter_set("world_time", float(day_time))
	# no sky light below the Nether roof or in the End: a dimension ambient keeps them readable
	# (the End uses a bright, flat light map like the original)
	var ambient := 0.0
	if dim == 1:
		ambient = 0.24
	elif dim == 2:
		ambient = 0.5
	RenderingServer.global_shader_parameter_set("ambient_light", ambient)
	var tint := Color(1, 1, 1).lerp(Color(1.0, 0.72, 0.55), twilight * 0.55)
	if dim == 1:
		tint = Color(0.72, 0.5, 0.5)
	elif dim == 2:
		tint = Color(0.78, 0.72, 0.85)
	RenderingServer.global_shader_parameter_set("sky_tint", Vector3(tint.r, tint.g, tint.b))
	if hud != null and hud.has_method("update_sky"):
		hud.update_sky(dim, f, weather, day_time)


func _update_camera_shake(delta: float) -> void:
	if camera_shake_amount <= 0.0 or player == null or player.camera == null:
		return
	camera_shake_amount = maxf(0.0, camera_shake_amount - delta * camera_shake_decay)
	var s := camera_shake_amount
	player.camera.position += Vector3(randf_range(-s, s), randf_range(-s, s), 0) * 0.3
	player.camera.rotation.z += randf_range(-s, s) * 0.06


func camera_shake(center: Vector3, power: float) -> void:
	if player == null:
		return
	var d := player.body.pos.distance_to(center)
	if d > power * 4.0 + 8.0:
		return
	var amt := clampf(power / 6.0 * (1.0 - d / (power * 4.0 + 8.0)), 0.0, 0.5)
	camera_shake_amount = maxf(camera_shake_amount, amt)
	camera_shake_decay = 1.2


# ------------------------------------------------------------------------------------------------
# Dimensions
func dimension_world(d: int) -> World:
	if worlds.has(d):
		var existing = worlds[d]
		if existing != null and is_instance_valid(existing):
			return existing
	var w := World.new()
	add_child(w)
	w.setup(self, d, seed_value, SaveManager.dim_dir(folder, d))
	worlds[d] = w
	if world != null and w != world:
		# stays hidden and idle until the player actually travels there (travel_to streams the
		# destination explicitly); /locate-style queries only need the generator
		w.visible = false
		w.cm.set_visible(false)
		w.paused_streaming = true
	return w


## Moves the player to another dimension, creating it on demand. With make_portal a Nether portal
## is found or built at the destination. Returns false when blocked.
func travel_to(d: int, target := Vector3.INF, make_portal := true) -> bool:
	if traveling or player == null:
		return false
	if d == dim:
		return false
	traveling = true
	travel_stage = 0.01
	travel_label = "Entering %s" % String(DimensionDB.get_def(d).get("display", "the world"))
	if hud != null and hud.has_method("set_travel_overlay"):
		hud.set_travel_overlay(travel_stage, travel_label)

	if player.riding != null:
		Riding.dismount(player)

	var from_dim := dim
	var dest := dimension_world(d)
	if target == Vector3.INF:
		target = DimensionDB.map_position(player.body.pos, from_dim, d)
	var px := floori(target.x)
	var pz := floori(target.z)

	# stream terrain around the destination until it is ready (generation runs on worker threads)
	var t0 := Time.get_ticks_msec()
	var limit := 20000
	while not dest.is_ready_at(px, pz) and Time.get_ticks_msec() - t0 < limit:
		dest.focus = target
		dest.cm.update(target)
		travel_stage = clampf(0.05 + 0.9 * float(Time.get_ticks_msec() - t0) / float(limit), 0.05, 0.95)
		if hud != null and hud.has_method("set_travel_overlay"):
			hud.set_travel_overlay(maxf(travel_stage, 0.9), travel_label)
		await get_tree().process_frame
	if not dest.is_ready_at(px, pz):
		chat("Destination terrain is still loading - try again in a moment", Color(1, 0.6, 0.4))
		traveling = false
		if hud != null and hud.has_method("set_travel_overlay"):
			hud.set_travel_overlay(0.0, "")
		return false

	# find / build a portal at the destination
	var arrival := target
	if d == 2:
		arrival = end_platform(dest)
	elif make_portal and from_dim != 2:
		var site := Portals.find_portal_site(dest, Vector3i(px, clampi(int(target.y), dest.min_y + 2, dest.max_y - 4), pz))
		if not site.is_empty():
			arrival = Portals.build_portal(dest, site.pos, int(site.axis), bool(site.platform))
			register_portal(d, site.pos)
		else:
			arrival = _safe_surface(dest, target)
	else:
		arrival = _safe_surface(dest, target)

	_switch_dimension(dest, d, arrival)
	traveling = false
	travel_stage = 0.0
	if hud != null and hud.has_method("set_travel_overlay"):
		hud.set_travel_overlay(0.0, "")
	Sfx.play_ui("portal_travel", 0.8)
	if particles != null:
		particles.portal(arrival + Vector3(0, 1, 0))
	return true


## Moves every live entity and the player into `dest` and switches the active world.
func _switch_dimension(dest: World, d: int, arrival: Vector3) -> void:
	var src := world
	# stash entities belonging to the dimension we are leaving
	if src != null and src != dest:
		for k in src.cm.chunks.keys():
			var c: Chunk = src.cm.chunks[k]
			if c.state >= Chunk.S_GENERATED:
				entities.collect_chunk(c)
	if src != null and src != dest:
		src.cm.set_visible(false)
		src.visible = false
		src.paused_streaming = true
	dest.visible = true
	dest.paused_streaming = false
	dest.cm.set_visible(true)
	world = dest
	dim = d
	entities.world = dest
	if particles != null:
		particles.world = dest
	if player != null:
		var saved_inv := player.inventory
		var saved_stats := player.stats
		var saved_effects := player.effects
		player.set_world(dest)
		player.inventory = saved_inv
		player.stats = saved_stats
		player.effects = saved_effects
		player.teleport(arrival)
		player.portal_cooldown = 80
		player.portal_ticks = 0
		player.body.vel = Vector3.ZERO
		player.flying = false
	# bring in entities saved for the chunks that are already loaded here
	for k in dest.cm.chunks.keys():
		var c2: Chunk = dest.cm.chunks[k]
		if c2.state >= Chunk.S_LIT and not c2.pending_entities.is_empty():
			entities.spawn_saved(c2)
	dimension_changed.emit(d)
	if ui != null and ui.has_method("on_dimension_changed"):
		ui.on_dimension_changed()
	chat("Entered the %s" % String(DimensionDB.get_def(d).get("display", "world")), Color(0.8, 0.6, 1.0))
	if d == 2 and (not DragonFight.started or (not DragonFight.defeated and DragonFight.dragon_entity(self) == null)):
		# first visit, or the fight was interrupted by leaving the End: the dragon and crystals return
		DragonFight.prepare(world)


## The End arrival platform: 5x5 obsidian at (100, 48, 0) with air above, like the original.
func end_platform(w: World) -> Vector3:
	var obs := BlockDB.id("obsidian")
	for dx in range(-2, 3):
		for dz in range(-2, 3):
			w.set_block(100 + dx, 48, dz, obs, World.F_URGENT)
			for dy in range(1, 4):
				w.set_block(100 + dx, 48 + dy, dz, 0, World.F_URGENT)
	return Vector3(100.5, 49.0, 0.5)


## Highest safe standing spot at target's column (used when arriving without a portal).
func _safe_surface(w: World, target: Vector3) -> Vector3:
	var x := floori(target.x)
	var z := floori(target.z)
	var y := floori(target.y)
	if w.dim == 1:
		for dy in range(0, 60):
			for yy in [y - dy, y + dy]:
				if yy < w.min_y + 2 or yy > w.max_y - 3:
					continue
				if BlockDB.solid[w.get_id(x, yy - 1, z)] == 1 and w.get_block(x, yy, z) == 0 and w.get_block(x, yy + 1, z) == 0:
					return Vector3(x + 0.5, float(yy), z + 0.5)
		return Vector3(x + 0.5, float(y), z + 0.5)
	var top := w.top_solid_y(x, z)
	if absi(top - y) < 3 and w.get_block(x, y, z) == 0 and w.get_block(x, y + 1, z) == 0:
		return target
	return Vector3(x + 0.5, float(top) + 1.0, z + 0.5)


func register_portal(d: int, pos: Vector3i) -> void:
	var lst: Array = portals_nether if d == 1 else portals_ow
	if not lst.has(pos):
		lst.append(pos)


func _tick_portals() -> void:
	if player == null or player.dead or traveling:
		return
	var p := Vector3i(floori(player.body.pos.x), floori(player.body.pos.y + 0.5), floori(player.body.pos.z))
	var n := BlockDB.name_of(world.get_blockv(p))
	if n == "nether_portal" and player.portal_cooldown <= 0:
		player.portal_ticks += 1
		if player.portal_ticks % 4 == 0 and particles != null:
			particles.portal(player.body.pos + Vector3(randf_range(-0.5, 0.5), 0.5, randf_range(-0.5, 0.5)))
		if player.portal_ticks > 40:
			player.portal_ticks = 0
			var to := 1 if dim == 0 else 0
			var tgt := DimensionDB.map_position(player.body.pos, dim, to)
			var known: Array = portals_nether if to == 1 else portals_ow
			var found = Portals.find_existing(dimension_world(to), Vector3i(floori(tgt.x), floori(tgt.y), floori(tgt.z)), 128, known)
			if found == null:
				await travel_to(to, tgt)
			else:
				await travel_to(to, Vector3(found) + Vector3(0.5, 0.0, 0.5))
		return
	player.portal_ticks = 0
	if n == "end_portal":
		if dim != 2:
			await travel_to(2, Vector3(100.5, 49.0, 0.5), false)
		else:
			chat("You have beaten the End. Returning to the Overworld...", Color(1.0, 0.9, 0.5))
			var sp := player.spawn_point if player.spawn_dim == 0 else Vector3.ZERO
			if sp == Vector3.ZERO:
				sp = dimension_world(0).gen.find_spawn()
			await travel_to(0, sp, false)
		return
	if n == "end_gateway" and dim == 2:
		var tgt2 := EndGen.gateway_destination(world, p)
		player.teleport(tgt2)
		player.portal_cooldown = 200
		if particles != null:
			particles.portal(tgt2)
		Sfx.play_at("teleport", tgt2)
		_land_after_gateway(tgt2)


## After a gateway jump: the player hovers (Player.tick waits for terrain) until the destination
## has streamed in, then lands on the surface. On the outer islands a return gateway is built next
## to the landing spot (and a small end stone island if the jump ended over the void).
func _land_after_gateway(tgt: Vector3) -> void:
	var x := floori(tgt.x)
	var z := floori(tgt.z)
	var t0 := Time.get_ticks_msec()
	while not world.is_ready_at(x, z) and Time.get_ticks_msec() - t0 < 20000:
		await get_tree().process_frame
	if world.dim != 2 or not world.is_ready_at(x, z) or player == null:
		return
	var top := world.top_solid_y(x, z)
	var outer := Vector2(tgt.x, tgt.z).length() > 500.0
	if top < 1:
		top = 64
		for dx in range(-3, 4):
			for dz in range(-3, 4):
				if absi(dx) + absi(dz) <= 4:
					world.set_block(x + dx, top, z + dz, BlockDB.id("end_stone"), World.F_URGENT)
	player.teleport(Vector3(x + 0.5, float(top) + 1.0, z + 0.5))
	player.body.fall_distance = 0.0
	if outer:
		var gx := x + (3 if (x & 15) < 12 else -3)      # same chunk as the landing spot
		var gy := maxi(world.top_solid_y(gx, z), top) + 1
		if world.get_block(gx, gy, z) == 0 or BlockDB.name_of(world.get_block(gx, gy, z)) != "end_gateway":
			world.set_block(gx, gy - 1, z, BlockDB.id("bedrock"), World.F_URGENT)
			world.set_block(gx, gy, z, BlockDB.id("end_gateway"), World.F_URGENT)
			world.set_block(gx, gy + 1, z, BlockDB.id("bedrock"), World.F_URGENT)


# ------------------------------------------------------------------------------------------------
# Chunk hooks (called by World)
func on_chunk_ready(w: World, c: Chunk) -> void:
	if w != world:
		return
	entities.world = w
	if not c.pending_entities.is_empty():
		entities.spawn_saved(c)
	for i in c.block_entities.keys():
		var be: Dictionary = c.block_entities[i]
		if String(be.get("type", "")) in BlockEntityTicker.TICKING:
			var lx := int(i) & 15
			var lz := (int(i) >> 4) & 15
			var y := (int(i) >> 8) + w.min_y
			register_ticking_be(w, Vector3i(c.cx * 16 + lx, y, c.cz * 16 + lz))


func on_chunk_unloading(w: World, c: Chunk) -> void:
	entities.collect_chunk(c)


# ------------------------------------------------------------------------------------------------
# Sleep
func _tick_sleep() -> void:
	if not sleeping:
		return
	sleep_timer += 1
	if player == null:
		return
	var d := player.body.pos.distance_to(Vector3(bed_position))
	if d > 3.0 and sleep_timer > 20:
		sleeping = false
		chat("You are not sleeping in the bed")
		if hud != null and hud.has_method("set_sleep_overlay"):
			hud.set_sleep_overlay(false)
		return
	if sleep_timer > 60:
		day_time = 0
		if weather != 0:
			weather = 0
			weather_timer = 12000
		sleeping = false
		sleep_timer = 0
		if hud != null and hud.has_method("set_sleep_overlay"):
			hud.set_sleep_overlay(false)
		Sfx.play_ui("levelup", 0.4)
		chat("Good morning", Color(1, 1, 0.6))


func start_sleep(bed: Vector3i) -> void:
	sleeping = true
	sleep_timer = 0
	bed_position = bed
	if hud != null and hud.has_method("set_sleep_overlay"):
		hud.set_sleep_overlay(true)


func is_sleeping() -> bool:
	return sleeping


func hostiles_near(pos: Vector3, radius: float) -> bool:
	for e in entities.all():
		if e is Mob and (e as Mob).hostile() and (e as Mob).body.pos.distance_to(pos) < radius:
			return true
	return false


# ------------------------------------------------------------------------------------------------
# Sulfur geysers
func _tick_geysers() -> void:
	var i := geysers.size() - 1
	while i >= 0:
		var g: Dictionary = geysers[i]
		g["ticks"] = int(g["ticks"]) - 1
		var p: Vector3i = g["pos"]
		var base := Vector3(p) + Vector3(0.5, 1.0, 0.5)
		if int(g["ticks"]) % 2 == 0 and particles != null:
			var col := 0.7 + float(int(g["ticks"]) % 20) / 20.0
			particles.spawn(base + Vector3(randf_range(-0.3, 0.3), 0, randf_range(-0.3, 0.3)),
				Vector3(randf_range(-0.02, 0.02), randf_range(0.25, 0.5), randf_range(-0.02, 0.02)),
				particles.L.get("large_smoke", 0), 1.2, 0.35 * col, Color(1, 1, 1, 0.55))
		for e in entities.all():
			var ent: Entity = e
			var dd := Vector2(ent.body.pos.x - base.x, ent.body.pos.z - base.z).length()
			if dd < 0.9 and ent.body.pos.y > base.y - 0.5 and ent.body.pos.y < base.y + 14.0:
				ent.body.vel.y = maxf(ent.body.vel.y, 0.55)
		if player != null and player.body.pos.distance_to(base) < 0.9 and player.body.pos.y > base.y - 0.5:
			player.body.vel.y = maxf(player.body.vel.y, 0.5)
		if int(g["ticks"]) <= 0:
			geysers.remove_at(i)
		i -= 1


func start_geyser(pos: Vector3i, ticks: int) -> void:
	for g in geysers:
		if (g["pos"] as Vector3i) == pos:
			return
	geysers.append({"pos": pos, "ticks": ticks})
	Sfx.play_at("geyser", Vector3(pos) + Vector3(0.5, 1.0, 0.5), 0.8)


# ------------------------------------------------------------------------------------------------
# Gamerules
func gamerule(name: String, default_value: bool) -> bool:
	return bool(gamerules.get(name, default_value))


func set_gamerule(name: String, value: bool) -> void:
	gamerules[name] = value


# ------------------------------------------------------------------------------------------------
# Player-facing helpers used by interaction / entity code
func set_break_overlay(pos: Vector3i, stage: int) -> void:
	break_pos = pos
	break_stage = stage
	if hud != null and hud.has_method("set_breaking"):
		hud.set_breaking(pos, stage)


func clear_break_overlay() -> void:
	break_stage = -1
	if hud != null and hud.has_method("set_breaking"):
		hud.set_breaking(Vector3i.ZERO, -1)


func drop_item_from_player(st: ItemStack) -> void:
	if st == null or st.is_empty():
		return
	var into := player.world
	var dir := player.look_dir()
	var pos := player.eye_position() + dir * 0.4 - Vector3(0, 0.2, 0)
	var e = entities.spawn_item(into, pos, st, dir * 6.0 + Vector3(0, 1.0, 0))
	if e != null and e is SimpleEntities.ItemEntity:
		(e as SimpleEntities.ItemEntity).pickup_delay = 40
	Sfx.play_ui("pop", 0.4, 1.2)


func entities_on_plate(w: World, box: AABB, kind: String) -> int:
	var n := 0
	for e in entities.all():
		var ent: Entity = e
		if ent.world == w and ent.body.aabb().intersects(box):
			if kind == "stone" and not (ent is Mob):
				continue
			n += 1
	if player != null and player.world == w and player.body.aabb().intersects(box) and player.gamemode != Player.SPECTATOR:
		n += 1
	return n


## Blocks moved by a piston carry entities (and the player) standing in their way along.
func piston_moved(w: World, moved: Array, dir: Vector3i) -> void:
	var dv := Vector3(dir)
	var cells := []
	for e in moved:
		var to: Vector3i = e[1]
		cells.append(AABB(Vector3(to), Vector3.ONE))
	for e in entities.all():
		var ent: Entity = e
		if ent.world != w:
			continue
		for c in cells:
			var box: AABB = c
			if ent.body.aabb().intersects(box) or ent.body.aabb().intersects(AABB(box.position + Vector3(0, 1, 0), Vector3(1, 0.05, 1))):
				ent.body.pos += dv * 1.01
				break
	if player != null and player.world == w:
		for c in cells:
			var box2: AABB = c
			if player.body.aabb().intersects(box2):
				player.body.pos += dv * 1.01
				break


## Entities and the player standing on pressure plates (checked every other tick).
func _tick_plates() -> void:
	if tick % 2 != 0:
		return
	var feet := []
	if player != null and not player.dead and player.gamemode != Player.SPECTATOR:
		feet.append(player.body.pos)
	for e in entities.all():
		var ent: Entity = e
		if ent.world == world and not (ent is SimpleEntities.XpOrb):
			feet.append(ent.body.pos)
	for fp in feet:
		var pos: Vector3 = fp
		var bp := Vector3i(floori(pos.x), floori(pos.y + 0.05), floori(pos.z))
		if BlockDB.model[world.get_id(bp.x, bp.y, bp.z)] == BlockDB.M_PLATE:
			RedstoneSystem.plate_step(world, bp)


func chest_opened(w: World, positions: Array, open: bool) -> void:
	for p in positions:
		var pv: Vector3i = p
		var node = w.be_node(pv)
		if node != null and node.has_method("set_open"):
			node.set_open(open)
		var be := w.get_be(pv.x, pv.y, pv.z, true)
		be["viewers"] = maxi(0, int(be.get("viewers", 0)) + (1 if open else -1))
		if BlockDB.name_of(w.get_blockv(pv)) == "trapped_chest":
			RedstoneSystem.consumer_update(w, pv, w.get_blockv(pv))


func register_ticking_be(w: World, pos: Vector3i) -> void:
	if w == null or pos.y < w.min_y or pos.y > w.max_y:
		return
	var d := w.dim
	if not ticking_bes.has(d):
		ticking_bes[d] = {}
	(ticking_bes[d] as Dictionary)[pos] = true
	var be := w.get_be(pos.x, pos.y, pos.z, true)
	be["ticking"] = true


func unregister_ticking_be(w: World, pos: Vector3i) -> void:
	if ticking_bes.has(w.dim):
		(ticking_bes[w.dim] as Dictionary).erase(pos)


func stop_tnt_at(w: World, pos: Vector3i) -> void:
	for e in entities.all():
		if e is SimpleEntities.TntEntity:
			var t := e as SimpleEntities.TntEntity
			if t.world == w and (t.body.pos - Vector3(pos) - Vector3(0.5, 0.5, 0.5)).length() < 0.8:
				t.fuse = 1


## Decorative items (item frames, paintings) drop as items for now.
func decor_item(_p, name: String, target: Dictionary, st: ItemStack, _slot: int) -> void:
	var pos: Vector3 = target.get("pos", player.body.pos)
	var dir: Vector3 = target.get("dir", Vector3(0, 0, 1))
	var e = entities.spawn_item(world, pos + dir * 0.5, st)
	if e != null:
		(e as Entity).data["decor"] = name
	action_bar("Placed %s" % name.replace("_", " "))


func explosion_player_impact(w: World, center: Vector3, power: float, source) -> void:
	var pl = player
	if pl == null or pl.dead or pl.gamemode == Player.SPECTATOR or pl.world != w:
		return
	var pos: Vector3 = pl.body.pos + Vector3(0, 0.9, 0)
	var dist: float = pos.distance_to(center)
	if dist > power * 2.0:
		return
	var expo := Explosions.exposure(w, center, pl.body.aabb())
	var dmg := Explosions.damage_for(center, pos, power, expo)
	var kb := Explosions.knockback_for(center, pos, power, expo)
	if dmg > 0.0:
		pl.stats.damage(dmg, "explosion", source)
	pl.body.vel += kb * 2.0
	pl.sprinting = false


func spawn_end_crystal(pos: Vector3) -> void:
	var e = DragonFight.spawn_crystal(world, pos)
	if e != null:
		dragon_crystals_list.append(e)


func dragon_crystals() -> Array:
	var out := []
	for c in dragon_crystals_list:
		if is_instance_valid(c) and not (c as Node3D).is_queued_for_deletion():
			out.append(c)
	dragon_crystals_list = out
	if out.is_empty() and world != null and world.dim == 2:
		out = DragonFight.crystals()
	return out


func dragon_alive_near(pos: Vector3, radius: float) -> bool:
	for e in entities.all():
		if e is Mob and (e as Mob).mob == "ender_dragon":
			return (e as Mob).body.pos.distance_to(pos) < radius
	return false


func structures_at(w: World, p: Vector3i) -> String:
	if w == null or w.gen == null or w.gen.structures == null:
		return ""
	return w.gen.structures.structure_name_at(w, p)


## Nearest structure of a type, searched in the dimension that has it (the current one first;
## strongholds for eyes of ender live in the Overworld).
func locate_structure(name: String, from: Vector3) -> Vector3:
	var d := dim
	if not StructureRegistry.names_for(d).has(name):
		for k in [0, 1, 2]:
			if StructureRegistry.names_for(k).has(name):
				d = k
				break
	var w := world if d == dim else dimension_world(d)
	if w == null or w.gen == null or w.gen.structures == null:
		return Vector3.INF
	return w.gen.structures.locate(name, from)


# ------------------------------------------------------------------------------------------------
# Boss bars
func show_boss_bar(key: String, name: String, hp: float, max_hp: float) -> void:
	boss_bars[key] = {"name": name, "hp": hp, "max": max_hp}
	if hud != null and hud.has_method("set_boss_bars"):
		hud.set_boss_bars(boss_bars)


func update_boss_bar(key: String, hp: float, max_hp: float) -> void:
	if not boss_bars.has(key):
		return
	var b: Dictionary = boss_bars[key]
	b["hp"] = hp
	b["max"] = max_hp
	if hud != null and hud.has_method("set_boss_bars"):
		hud.set_boss_bars(boss_bars)


func remove_boss_bar(key: String) -> void:
	boss_bars.erase(key)
	if hud != null and hud.has_method("set_boss_bars"):
		hud.set_boss_bars(boss_bars)


# ------------------------------------------------------------------------------------------------
# Chat / messages
func chat(text: String, color := Color(1, 1, 1)) -> void:
	chat_message.emit(text, color)
	if hud != null and hud.has_method("add_chat"):
		hud.add_chat(text, color)


func action_bar(text: String) -> void:
	action_bar_message.emit(text)
	if hud != null and hud.has_method("set_action_bar"):
		hud.set_action_bar(text, 2.0)


func _on_player_death() -> void:
	_last_died_tick = tick
	last_death_pos = player.body.pos
	if player.riding != null:
		Riding.dismount(player)
	player.elytra_flying = false
	var cause := player.stats.last_cause
	chat("You died! (%s)" % _cause_name(cause), Color(1, 0.4, 0.4))
	var keep := bool(gamerules.get("keepInventory", false)) or player.gamemode == Player.CREATIVE
	if not keep:
		for i in 46:
			var st: ItemStack = player.inventory.stack_at(i)
			if st == null:
				continue
			player.inventory.set_stack(i, null)
			if st.enchant_level("vanishing_curse") > 0:
				continue
			drop_item_from_player(st)
		player.stats.xp_total = 0
		player.stats.xp_level = 0
		player.stats.xp_progress = 0.0
		player.stats.score = 0
	if ui != null and ui.has_method("show_death_screen"):
		if bool(gamerules.get("doImmediateRespawn", false)):
			respawn_player()
		else:
			ui.show_death_screen(cause)


func _cause_name(cause: String) -> String:
	return cause.capitalize() if cause != "" else "unknown"


func respawn_player() -> void:
	if player == null:
		return
	player.respawn()
	var target_dim: int = player.spawn_dim
	var pos: Vector3 = player.spawn_point
	if pos == Vector3.ZERO:
		pos = world.gen.find_spawn()
		target_dim = 0
	if target_dim != dim:
		await travel_to(target_dim, pos)
	if player != null and is_instance_valid(player):
		player.teleport(pos + Vector3(0, 0.2, 0))
		player.dead = false
		player.body.vel = Vector3.ZERO
		player.portal_cooldown = 40
	if ui != null and ui.has_method("hide_death_screen"):
		ui.hide_death_screen()


func on_totem() -> void:
	if particles != null:
		particles.totem(player.body.pos + Vector3(0, 1, 0))
	Sfx.play_at("totem", player.body.pos, 1.0)


# ------------------------------------------------------------------------------------------------
# Saving
func save_all() -> void:
	for k in worlds.keys():
		var w = worlds[k]
		if w == null or not is_instance_valid(w):
			continue
		(w as World).save_all()
	var data := {
		"name": world_name, "folder": folder, "seed": seed_value, "difficulty": difficulty,
		"hardcore": hardcore, "gamerules": gamerules, "time": day_time, "weather": weather,
		"weather_timer": weather_timer, "player": _player_dict(), "dim": dim, "tick": tick,
		"ender": ender_inventory.to_array(), "portals_ow": _vec_array(portals_ow),
		"portals_nether": _vec_array(portals_nether), "dragon_defeated": DragonFight.defeated,
	}
	SaveManager.write_level(folder, data)


func _vec_array(a: Array) -> Array:
	var out := []
	for p in a:
		var pv: Vector3i = p
		out.append([pv.x, pv.y, pv.z])
	return out


static func _vec_from(a: Array) -> Vector3i:
	return Vector3i(int(a[0]), int(a[1]), int(a[2]))


func _player_dict() -> Dictionary:
	return {
		"pos": [player.body.pos.x, player.body.pos.y, player.body.pos.z],
		"gamemode": player.gamemode,
		"yaw": player.yaw, "pitch": player.pitch,
		"inv": player.inventory.to_array(),
		"sel": player.inventory.selected,
		"stats": player.stats.to_dict(),
		"spawn": [player.spawn_point.x, player.spawn_point.y, player.spawn_point.z],
		"spawn_dim": player.spawn_dim,
		"effects": player.effects.duplicate(true),
		"dim": dim,
	}


func apply_level(data: Dictionary) -> void:
	if data.is_empty():
		return
	day_time = int(data.get("time", day_time))
	weather = int(data.get("weather", weather))
	weather_timer = int(data.get("weather_timer", weather_timer))
	difficulty = int(data.get("difficulty", difficulty))
	hardcore = bool(data.get("hardcore", hardcore))
	var gr: Dictionary = data.get("gamerules", {})
	for k in gr:
		gamerules[k] = gr[k]
	SaveManager.prepare_palette(data.get("block_palette", []))
	if ender_inventory != null:
		ender_inventory.from_array(data.get("ender", []))
	for p in data.get("portals_ow", []):
		portals_ow.append(_vec_from(p))
	for p in data.get("portals_nether", []):
		portals_nether.append(_vec_from(p))
	DragonFight.defeated = bool(data.get("dragon_defeated", false))


# ------------------------------------------------------------------------------------------------
# Headless screenshot automation: --autotest=<scenario> (see DEVELOPMENT.md). Scenarios:
#   "walk"  - land the player, look around, take screenshots of the world
#   "menu"  - open the creative inventory and screenshot the UI
#   "fly"   - fly up for an aerial view
func _step_autotest(delta: float) -> void:
	autotest_timer += 1
	match autotest:
		"mobs", "blocks":
			# set up the scene once the terrain around the player exists, then take a few shots
			if autotest_timer == 60:
				var sc = load("res://game/tests/autotest.gd")
				if sc != null:
					sc.run(self, autotest)
			if has_meta("showcase") and autotest_timer > 60 and autotest_timer < 400:
				# hold the camera still while the mobs settle
				var sp: Vector3 = get_meta("showcase")
				if autotest_timer >= 250:
					sp += Vector3(-6.0, 1.0, -6.0)
				player.teleport(sp)
				player.flying = true
				if autotest_timer >= 250:
					player.yaw = -PI * 0.5
					player.pitch = -0.3
			if autotest_timer == 160:
				_shot("01_" + autotest)
			if autotest_timer == 300:
				_shot("02_" + autotest)
			if autotest_timer == 340:
				if player.camera != null:
					player.camera.fov = 45.0
			if autotest_timer == 360:
				_shot("03_" + autotest)
			if autotest_timer > 400:
				_finish_autotest()
		"walk":
			if autotest_timer == 200:
				_shot("01_walk")
			if autotest_timer == 300:
				player.yaw += 1.0
			if autotest_timer == 360:
				_shot("02_walk")
			if autotest_timer > 420:
				_finish_autotest()
		"menu":
			if autotest_timer == 120:
				_shot("01_world")
				if ui != null:
					ui.open_screen("creative", {})
			if autotest_timer == 160:
				if ui != null and ui.screen != null:
					print("[Autotest] screen size %s origin %s scale %d viewport %s" % [str(ui.screen.size), str(ui.screen.origin),
						ui.scale(), str(get_viewport().get_visible_rect().size)])
				_shot("02_creative")
			if autotest_timer == 200:
				if ui != null:
					ui.close_screen()
					ui.open_screen("inventory", {})
			if autotest_timer == 240:
				_shot("03_inventory")
				if ui != null and ui.screen != null and ui.screen.get("preview") != null:
					var pv: SubViewportContainer = ui.screen.get("preview")
					var vp := pv.get_child(0) as SubViewport
					var rig: Node3D = pv.get_meta("rig", null)
					print("[Autotest] preview size %s vp %s rig children %d" % [str(pv.size), str(vp.size), rig.get_child_count() if rig != null else -1])
					for c in (rig.get_children() if rig != null else []):
						print("   part %s pos %s rot %s" % [c.name, str((c as Node3D).position), str((c as Node3D).rotation_degrees)])
					vp.get_texture().get_image().save_png(autotest_dir + "/preview.png")
			if autotest_timer == 280:
				if ui != null:
					ui.close_screen()
					ui.open_console(true)
					Commands.run(self, "help")
			if autotest_timer == 320:
				_shot("04_console")
			if autotest_timer > 380:
				_finish_autotest()
		"armor":
			if autotest_timer == 60:
				var sc2 = load("res://game/tests/autotest.gd")
				if sc2 != null:
					sc2.run(self, "mobs")
				player.inventory.set_stack(Inventory.ARMOR + 3, ItemStack.of("diamond_helmet", 1))
				player.inventory.set_stack(Inventory.ARMOR + 2, ItemStack.of("iron_chestplate", 1))
				player.inventory.set_stack(Inventory.ARMOR + 1, ItemStack.of("golden_leggings", 1))
				player.inventory.set_stack(Inventory.ARMOR + 0, ItemStack.of("netherite_boots", 1))
				player.inventory.set_stack(0, ItemStack.of("diamond_sword", 1))
				player.third_person = 2
			if has_meta("showcase") and autotest_timer > 60:
				var sp2: Vector3 = get_meta("showcase")
				player.teleport(sp2 + Vector3(0, -1.0, 0))
				player.flying = false
				player.pitch = -0.1
			if autotest_timer == 200:
				_shot("01_armor_front")
			if autotest_timer == 220:
				player.third_person = 0
			if autotest_timer == 260:
				_shot("02_first_person_sword")
			if autotest_timer > 300:
				_finish_autotest()
		"nether":
			if autotest_timer == 60:
				Commands.run(self, "dimension nether")
			if autotest_timer == 400:
				_shot("01_nether")
			if autotest_timer == 440:
				player.yaw += 2.0
			if autotest_timer == 480:
				_shot("02_nether")
			if autotest_timer == 520:
				Commands.run(self, "dimension end")
			if autotest_timer == 900:
				_shot("03_end")
			if autotest_timer == 940:
				player.yaw += PI
				player.pitch = 0.25
			if autotest_timer == 980:
				_shot("04_end_dragon")
			if autotest_timer > 1000 and autotest_timer <= 1060:
				# follow the dragon with the camera for a close look
				var dr = DragonFight.dragon_entity(self)
				if dr != null:
					var to: Vector3 = (dr as Node3D).global_position + Vector3(0, 2, 0) - player.eye_position()
					player.yaw = atan2(-to.x, -to.z)
					player.pitch = clampf(atan2(to.y, Vector2(to.x, to.z).length()), -1.4, 1.4)
			if autotest_timer == 1060:
				_shot("05_dragon")
			if autotest_timer > 1080:
				print("[Autotest] dimension=%d dragon=%s crystals=%d" % [dim, str(DragonFight.dragon_entity(self) != null), DragonFight.crystals().size()])
				_finish_autotest()
		"flight":
			if autotest_timer == 40:
				player.set_gamemode(Player.SPECTATOR)
				player.flying = true
				player.teleport(Vector3(floori(player.body.pos.x) + 0.5, 150.0, floori(player.body.pos.z) + 0.5))
			if autotest_timer == 160:
				_shot("01_aerial")
			if autotest_timer == 220:
				player.yaw += 1.2
			if autotest_timer == 280:
				_shot("02_aerial")
			if autotest_timer > 340:
				_finish_autotest()
		_:
			if autotest_timer > 60:
				_finish_autotest()


func _shot(name: String) -> void:
	if DisplayServer.get_name() == "headless":
		autotest_stage += 1
		return
	DirAccess.make_dir_recursive_absolute(autotest_dir)
	await RenderingServer.frame_post_draw
	var tex := get_viewport().get_texture()
	if tex == null:
		return
	var img := tex.get_image()
	if img == null:
		return
	img.save_png("%s/%s.png" % [autotest_dir, name])
	print("[Autotest] shot %s -> %s" % [name, ProjectSettings.globalize_path("%s/%s.png" % [autotest_dir, name])])
	autotest_stage += 1


func _finish_autotest() -> void:
	print("[Autotest] done (%d shots)" % autotest_stage)
	autotest = ""
	if autotest_stage > 0:
		get_tree().quit()
