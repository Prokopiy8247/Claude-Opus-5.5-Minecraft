class_name Player
extends Node3D
## First-person player: Minecraft movement at 20 TPS with render interpolation, mouse look,
## creative flight (double-tap space), game modes, camera bobbing and FOV effects.
## Survival stats, inventory and block interaction live in dedicated components.

signal gamemode_changed(mode: int)
signal died

const CREATIVE := 1
const SURVIVAL := 0
const SPECTATOR := 3
const EYE := 1.62
const EYE_SNEAK := 1.27

var session = null
var world: World = null
var body := VoxelBody.new()
var prev_pos := Vector3.ZERO
var yaw := 0.0
var pitch := 0.0
var gamemode := CREATIVE
var flying := false
var may_fly := true
var sneaking := false
var sprinting := false
var swimming := false
var in_water := false
var eyes_in_water := false
var in_lava := false
var eyes_in_lava := false
var on_ladder := false
var input_locked := false        # UI open
var dead := false
var head: Node3D
var camera: Camera3D
var eye_height := EYE
var _eye_render := EYE
var _last_space := -10.0
var _jump_held := false
var _sprint_toggle := false
var walk_dist := 0.0
var prev_walk_dist := 0.0
var bob := 0.0
var prev_bob := 0.0
var fov_mod := 1.0
var third_person := 0            # 0 first, 1 back, 2 front
var stats: PlayerStats
var inventory: Inventory
var interact: PlayerInteraction
var hand: HandView
var effects: Dictionary = {}     # effect name -> {amp, ticks}
var spawn_point := Vector3.ZERO
var spawn_dim := 0
var riding = null
var portal_ticks := 0
var portal_cooldown := 0
var tick_accum := 0.0
var last_tick_time := 0.0
var fly_speed_mul := 1.0
var elytra_flying := false
var _model: Node3D = null
var _model_parts: Dictionary = {}
var _armor_key := ""


func _ready() -> void:
	head = Node3D.new()
	head.name = "Head"
	add_child(head)
	camera = Camera3D.new()
	camera.name = "Camera"
	camera.near = 0.05
	camera.far = 1000.0
	camera.fov = float(Game.settings.fov)
	camera.current = true
	head.add_child(camera)
	stats = PlayerStats.new(self)
	inventory = Inventory.new(46)
	interact = PlayerInteraction.new(self)
	hand = HandView.new()
	hand.name = "HandView"
	hand.player = self
	add_child(hand)
	# third-person body (Blender-authored player model), hidden in first person
	_model = PlayerSkin.build_model()
	if _model != null:
		_model.name = "Body"
		_model.visible = false
		add_child(_model)
		_model_parts = MobRenderer.parts_of(_model)


func setup(p_session, p_world: World, spawn: Vector3) -> void:
	session = p_session
	world = p_world
	body.pos = spawn
	prev_pos = spawn
	spawn_point = spawn


func set_world(w: World) -> void:
	world = w


func set_gamemode(m: int) -> void:
	gamemode = m
	may_fly = m == CREATIVE or m == SPECTATOR
	if not may_fly:
		flying = false
	if m == SPECTATOR:
		flying = true
	body.no_clip = m == SPECTATOR
	gamemode_changed.emit(m)


func is_creative() -> bool:
	return gamemode == CREATIVE


func eye_position() -> Vector3:
	return body.pos + Vector3(0, eye_height, 0)


func look_dir() -> Vector3:
	return Vector3(-sin(yaw) * cos(pitch), sin(pitch), -cos(yaw) * cos(pitch))


func render_eye() -> Vector3:
	return camera.global_position


func teleport(p: Vector3) -> void:
	body.pos = p
	prev_pos = p
	body.vel = Vector3.ZERO
	body.fall_distance = 0.0


# ------------------------------------------------------------------------------------------------
func _unhandled_input(event: InputEvent) -> void:
	if input_locked or dead:
		return
	if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		var sens := 0.0022 * (0.2 + float(Game.settings.mouse_sensitivity) * 1.6)
		var inv := -1.0 if Game.settings.invert_mouse else 1.0
		yaw -= event.relative.x * sens
		pitch = clampf(pitch - event.relative.y * sens * inv, -PI * 0.5 + 0.001, PI * 0.5 - 0.001)
	elif event.is_action_pressed("jump"):
		var now := Time.get_ticks_msec() / 1000.0
		if may_fly and gamemode != SPECTATOR and now - _last_space < 0.3:
			flying = not flying
			if flying:
				body.vel.y = 0.0
			_last_space = -10.0
		else:
			_last_space = now
	elif event.is_action_pressed("perspective"):
		third_person = (third_person + 1) % 3
	elif event is InputEventMouseButton and event.pressed:
		var mb: InputEventMouseButton = event
		if mb.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			inventory.selected = (inventory.selected + 1) % 9
		elif mb.button_index == MOUSE_BUTTON_WHEEL_UP:
			inventory.selected = (inventory.selected + 8) % 9
	elif event is InputEventKey and event.pressed and not event.echo:
		var k: InputEventKey = event
		if k.keycode >= KEY_1 and k.keycode <= KEY_9 and not k.ctrl_pressed:
			inventory.selected = k.keycode - KEY_1


func _process(delta: float) -> void:
	if world == null:
		return
	# render interpolation between physics ticks
	var alpha := clampf(session.tick_alpha if session != null else 1.0, 0.0, 1.0)
	var rp := prev_pos.lerp(body.pos, alpha)
	global_position = rp
	var target_eye := EYE_SNEAK if (sneaking and not flying) else EYE
	if swimming:
		target_eye = 0.4
	_eye_render = lerpf(_eye_render, target_eye, minf(1.0, delta * 12.0))
	head.position = Vector3(0, _eye_render, 0)
	head.rotation = Vector3(pitch, yaw, 0)
	# view bobbing
	var cam_off := Vector3.ZERO
	var cam_rot := Vector3.ZERO
	if Game.settings.view_bobbing and not flying and third_person == 0:
		var wd := lerpf(prev_walk_dist, walk_dist, alpha)
		var bb := lerpf(prev_bob, bob, alpha)
		cam_off = Vector3(sin(wd * PI) * bb * 0.5, -absf(cos(wd * PI) * bb), 0)
		cam_rot = Vector3(deg_to_rad(absf(cos(wd * PI - 0.2) * bb) * 5.0), 0, deg_to_rad(sin(wd * PI) * bb * 3.0))
	if third_person == 0:
		camera.position = cam_off
		camera.rotation = cam_rot
	else:
		var back := 4.0
		var dir := -look_dir() if third_person == 1 else look_dir()
		var hit := world.raycast(eye_position(), dir, back, false, true)
		var dist: float = (hit.dist - 0.2) if not hit.is_empty() else back
		camera.position = Vector3(0, 0, dist if third_person == 1 else -dist)
		camera.rotation = Vector3(0, 0 if third_person == 1 else PI, 0)
	# fov: sprint / fly boost like Minecraft
	var fm := 1.0
	if sprinting:
		fm += 0.15
	if flying:
		fm += 0.1 if sprinting else 0.0
	if effects.has("speed"):
		fm += 0.05 * (int(effects["speed"].amp) + 1)
	if effects.has("slowness"):
		fm -= 0.05 * (int(effects["slowness"].amp) + 1)
	if interact != null and interact.using_bow:
		fm *= 1.0 - 0.15 * clampf(interact.use_ticks / 20.0, 0.0, 1.0)
	if interact != null and interact.using_item and inventory.selected_stack() != null \
			and inventory.selected_stack().item_name() == "spyglass":
		fm = 0.12
	fov_mod = lerpf(fov_mod, fm, minf(1.0, delta * 10.0))
	camera.fov = float(Game.settings.fov) * fov_mod
	if _model != null:
		_model.visible = third_person != 0 and gamemode != SPECTATOR
		if _model.visible:
			_animate_model(alpha)
	if interact != null:
		interact.frame(delta)


# ------------------------------------------------------------------------------------------------
func read_input() -> Dictionary:
	var inp := Vector2.ZERO
	var jump := false
	var sneak := false
	var sprint_key := false
	if not input_locked and not dead:
		inp.y = Input.get_action_strength("move_forward") - Input.get_action_strength("move_back")
		inp.x = Input.get_action_strength("move_right") - Input.get_action_strength("move_left")
		jump = Input.is_action_pressed("jump")
		sneak = Input.is_action_pressed("sneak")
		sprint_key = Input.is_action_pressed("sprint")
	return {"input": inp * 0.98, "jump": jump, "sneak": sneak, "sprint_key": sprint_key}


## 20 TPS simulation step (called by the session).
func tick() -> void:
	if world == null:
		return
	prev_pos = body.pos
	prev_walk_dist = walk_dist
	prev_bob = bob
	if dead:
		return
	if not world.is_ready_at(floori(body.pos.x), floori(body.pos.z)):
		# wait for terrain under the player before simulating
		body.vel = Vector3.ZERO
		return
	var ri := read_input()
	var inp: Vector2 = ri.input
	sneaking = ri.sneak and not flying
	if flying:
		sneaking = false
	# sprint rules: forward, enough food, not sneaking, not using item
	var can_sprint: bool = inp.y > 0.5 and (stats.food > 6 or not stats.hunger_enabled()) and not ri.sneak
	if ri.sprint_key and can_sprint:
		sprinting = true
	if sprinting and (not can_sprint or body.collided_h or inp.y <= 0.0):
		sprinting = false
	if interact.using_item and not flying:
		inp *= 0.2
		sprinting = false
	swimming = in_water and sprinting and eyes_in_water
	var st := {
		"input": inp, "jump": ri.jump, "sneak": ri.sneak, "sprint": sprinting, "flying": flying, "yaw": yaw,
		"speed_mul": _speed_mul(), "jump_mul": 1.0 + 0.25 * (int(effects["jump_boost"].amp) + 1 if effects.has("jump_boost") else 0),
		"swimming": swimming, "fly_mul": fly_speed_mul, "spectator": gamemode == SPECTATOR,
		"levitation": int(effects["levitation"].amp) + 1 if effects.has("levitation") else 0,
		"slow_fall": effects.has("slow_falling"),
	}
	var was_ground := body.on_ground
	var fall_before := body.fall_distance
	var start := body.pos
	var env: Dictionary
	# elytra: jumping again while falling opens the wings
	if ri.jump and not _jump_held and not elytra_flying and riding == null and not body.on_ground and not flying and body.vel.y < 0.0 and not in_water:
		if ElytraFlight.try_start(self):
			body.fall_distance = 0.0
	_jump_held = ri.jump
	if elytra_flying:
		env = ElytraFlight.step(world, body, self)
		if gamemode != CREATIVE and session.tick % 20 == 0:
			interact.damage_armor_slot(Inventory.ARMOR + 2, 1)
	elif riding != null:
		Riding.control(self, ri)
		env = body.sample_env(world, EYE)
	else:
		env = PlayerMotion.step(world, body, st)
	if env.water and not in_water and body.vel.y < -0.25:
		Sfx.play_at("splash", body.pos, clampf(-body.vel.y, 0.3, 1.0))
	in_water = env.water
	eyes_in_water = env.eyes_water
	in_lava = env.lava
	eyes_in_lava = env.eyes_lava
	on_ladder = env.climb
	if flying and body.on_ground and gamemode != SPECTATOR:
		flying = false
	# fall damage
	if body.on_ground and not was_ground:
		var fd := fall_before
		if not in_water and not flying:
			stats.on_land(fd, world.get_block(floori(body.pos.x), floori(body.pos.y - 0.2), floori(body.pos.z)))
		body.fall_distance = 0.0
	if in_water or flying or on_ladder or body.vel.y > 0.0:
		body.fall_distance = 0.0
	# walking animation distance
	var moved := Vector2(body.pos.x - start.x, body.pos.z - start.z).length()
	if body.on_ground and not flying and riding == null:
		walk_dist += moved * 0.6
		bob = lerpf(bob, minf(0.1, moved), 0.4)
		interact.on_walk(moved)
	else:
		bob = lerpf(bob, 0.0, 0.4)
		if in_water and not flying and riding == null:
			interact.on_walk(moved)
	stats.tick(sprinting, moved, ri.jump and body.on_ground)
	interact.tick()
	_tick_effects()
	if portal_cooldown > 0:
		portal_cooldown -= 1
	# void
	if body.pos.y < world.min_y - 64:
		stats.damage(4.0, "void", null, true)
	world.focus = body.pos


func _speed_mul() -> float:
	var m := 1.0
	if effects.has("speed"):
		m *= 1.0 + 0.2 * (int(effects["speed"].amp) + 1)
	if effects.has("slowness"):
		m *= maxf(0.0, 1.0 - 0.15 * (int(effects["slowness"].amp) + 1))
	return m


func add_effect(effect: String, ticks: int, amp: int = 0) -> void:
	var cur: Dictionary = effects.get(effect, {})
	if cur.is_empty() or int(cur.amp) <= amp or int(cur.ticks) < ticks:
		effects[effect] = {"amp": amp, "ticks": ticks}
	if effect == "absorption":
		stats.absorption = maxf(stats.absorption, 4.0 * float(amp + 1))
	elif effect == "health_boost":
		stats.max_health = 20.0 + 4.0 * float(amp + 1)
	if effect == "instant_health":
		stats.heal(4.0 * pow(2, amp))
		effects.erase(effect)
	elif effect == "instant_damage":
		stats.damage(6.0 * pow(2, amp), "magic")
		effects.erase(effect)
	elif effect == "saturation":
		stats.food = mini(20, stats.food + amp + 1)


func clear_effects() -> void:
	effects.clear()
	stats.absorption = 0.0
	stats.max_health = 20.0
	stats.health = minf(stats.health, stats.max_health)


## Periodic effects (Minecraft intervals halve per amplifier level).
func _tick_effects() -> void:
	var done := []
	var t: int = session.tick if session != null else 0
	for e in effects:
		var d: Dictionary = effects[e]
		d.ticks = int(d.ticks) - 1
		if int(d.ticks) <= 0:
			done.append(e)
		var amp := int(d.amp)
		match String(e):
			"regeneration":
				if t % maxi(1, 50 >> amp) == 0 and stats.health < stats.max_health:
					stats.heal(1.0)
			"poison":
				if t % maxi(1, 25 >> amp) == 0 and stats.health > 1.0:
					stats.damage(1.0, "magic", null, true)
			"wither":
				if t % maxi(1, 40 >> amp) == 0:
					stats.damage(1.0, "wither", null, true)
			"hunger":
				stats.add_exhaustion(0.005 * float(amp + 1))
	for e in done:
		effects.erase(e)
		if e == "absorption":
			stats.absorption = 0.0
		elif e == "health_boost":
			stats.max_health = 20.0
			stats.health = minf(stats.health, stats.max_health)
	RenderingServer.global_shader_parameter_set("night_vision", 0.85 if effects.has("night_vision") else 0.0)


func die(cause: String) -> void:
	if dead:
		return
	dead = true
	flying = false
	died.emit()


## Third-person body: faces the look yaw, legs/arms swing with walking, head follows pitch,
## worn armour is attached as Blender-authored pieces.
func _animate_model(alpha: float) -> void:
	_model.rotation = Vector3(0, yaw, 0)
	var wd := lerpf(prev_walk_dist, walk_dist, alpha)
	var amt := clampf(bob * 12.0, 0.0, 1.0)
	var swing := sin(wd * PI * 1.3) * 0.8 * amt
	for pn in ["leg_fl", "leg_fr", "arm_left", "arm_right"]:
		var n: Node3D = _model_parts.get(pn, null)
		if n == null:
			continue
		var base: Vector3 = n.get_meta("base_rot", Vector3.ZERO)
		var sgn := 1.0 if pn in ["leg_fl", "arm_right"] else -1.0
		n.rotation = base + Vector3(swing * sgn, 0, 0)
	var head: Node3D = _model_parts.get("head", null)
	if head != null:
		head.rotation = Vector3(pitch, 0, 0)
	if sneaking:
		_model.position = Vector3(0, -0.15, 0)
	else:
		_model.position = Vector3.ZERO
	var key := ""
	for i in 4:
		var st: ItemStack = inventory.stack_at(Inventory.ARMOR + i)
		key += (st.item_name() if st != null else "-") + ","
	if key != _armor_key:
		_armor_key = key
		ArmorRenderer.apply(_model, _model_parts, inventory)


## Serialised player state (position, mode, inventory, stats, spawn, effects).
func to_dict() -> Dictionary:
	return {
		"pos": [body.pos.x, body.pos.y, body.pos.z],
		"gamemode": gamemode,
		"yaw": yaw, "pitch": pitch,
		"inv": inventory.to_array(),
		"sel": inventory.selected,
		"stats": stats.to_dict(),
		"spawn": [spawn_point.x, spawn_point.y, spawn_point.z],
		"spawn_dim": spawn_dim,
		"effects": effects.duplicate(true),
		"flying": flying,
	}


func from_dict(d: Dictionary) -> void:
	var pa: Array = d.get("pos", [0.0, 100.0, 0.0])
	teleport(Vector3(float(pa[0]), float(pa[1]), float(pa[2])))
	set_gamemode(int(d.get("gamemode", gamemode)))
	yaw = float(d.get("yaw", yaw))
	pitch = float(d.get("pitch", pitch))
	if inventory != null:
		inventory.from_array(d.get("inv", []))
		inventory.selected = clampi(int(d.get("sel", 0)), 0, 8)
	if stats != null:
		stats.from_dict(d.get("stats", {}))
	var sp: Array = d.get("spawn", [])
	if sp.size() >= 3:
		spawn_point = Vector3(float(sp[0]), float(sp[1]), float(sp[2]))
	spawn_dim = int(d.get("spawn_dim", 0))
	effects = (d.get("effects", {}) as Dictionary).duplicate(true)
	flying = bool(d.get("flying", false)) and may_fly


func respawn() -> void:
	dead = false
	stats.reset()
	effects.clear()
	body.vel = Vector3.ZERO
	body.fall_distance = 0.0
