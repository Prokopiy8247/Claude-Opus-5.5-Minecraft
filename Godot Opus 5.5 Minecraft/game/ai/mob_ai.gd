class_name MobAI
extends RefCounted
## Archetype-driven mob brain: target selection, wandering, path following, melee/ranged attacks,
## creeper fuse, slime hopping, ghast/blaze fireballs, enderman teleport, shulker bullets,
## bosses (Ender Dragon with crystals, Wither with phases), warden, creaking.

var m: Mob = null
var effects: Dictionary = {}
var wants_up := false
var state := "idle"
var state_timer := 0
var sight := false
var last_seen := Vector3.ZERO
var circle_angle := 0.0
var perch := false
var crystal_target = null
var breath_timer := 0
var charge_timer := 0
var summon_timer := 0
var shoot_cd := 0
var hop_cd := 0
var teleport_cd := 0
var sonic_cd := 0
var phase := 0
var home := Vector3.ZERO


func _init(mob: Mob) -> void:
	m = mob
	home = mob.body.pos
	circle_angle = randf() * TAU


func _rng() -> RandomNumberGenerator:
	var r := RandomNumberGenerator.new()
	r.randomize()
	return r


func tick() -> void:
	if m.world == null or m.dead:
		return
	for e in effects.keys():
		var d: Dictionary = effects[e]
		d.ticks = int(d.ticks) - 1
		if int(d.ticks) <= 0:
			effects.erase(e)
	_tick_effects()
	state_timer -= 1
	if home == Vector3.ZERO:
		home = m.body.pos
	var arch: String = m.def.get("arch", "passive")
	var pl = m.session.player if m.session != null else null
	wants_up = false
	if arch == "boss_dragon":
		_dragon(pl)
		return
	if arch == "boss_wither":
		_wither(pl)
		return
	if arch == "shulker":
		_shulker(pl)
		return
	if arch == "creaking":
		_creaking(pl)
		return
	if arch == "warden":
		_warden(pl)
		return
	if arch == "golem":
		_golem(pl)
		return
	if arch == "copper_golem":
		_copper_golem()
		return
	if arch == "enderman":
		_enderman_senses(pl)
	# hurt passive animals panic: run around fast for a few seconds
	var panic := int(m.data.get("panic", 0))
	if panic > 0 and (arch == "passive" or arch == "ambient" or arch == "fish"):
		m.data["panic"] = panic - 1
		if panic % 20 == 0 or m.data.get("panic_dir", null) == null:
			var from: Vector3 = m.data.get("panic_from", m.body.pos + Vector3(_rng().randf_range(-1, 1), 0, _rng().randf_range(-1, 1)))
			var away := (m.body.pos - from)
			away.y = 0.0
			if away.length() < 0.1:
				away = Vector3(1, 0, 0)
			m.data["panic_dir"] = away.normalized().rotated(Vector3.UP, _rng().randf_range(-0.8, 0.8))
		_walk_to(m.body.pos + (m.data["panic_dir"] as Vector3) * 4.0, 2.0)
		return
	if mid_target_valid(pl):
		_combat(arch, pl)
	else:
		_wander(arch)


func _tick_effects() -> void:
	if effect_amp("poison") >= 0 and m.age % 25 == 0:
		m.hurt(1.0, "magic")
	if effect_amp("wither") >= 0 and m.age % 40 == 0:
		m.hurt(1.0, "magic")
	if effect_amp("regeneration") >= 0 and m.age % 50 == 0:
		m.health = minf(m.max_health, m.health + 1.0)
	if effect_amp("instant_damage") >= 0:
		m.hurt(6.0, "magic")
		effects.erase("instant_damage")
	if effect_amp("instant_health") >= 0:
		m.health = minf(m.max_health, m.health + 4.0)
		effects.erase("instant_health")
	if effect_amp("hunger") >= 0 and m.age % 30 == 0:
		m.hurt(1.0, "magic")
	if effect_amp("slowness") >= 3 and m.age % 20 == 0:
		pass


func effect_amp(e: String) -> int:
	var d: Dictionary = effects.get(e, {})
	return int(d.get("amp", -1))


## Endermen: staring at their head (without a carved pumpkin on) makes them hostile; they
## teleport away from water/rain and randomly when hurt, and blink closer to a distant target.
func _enderman_senses(pl) -> void:
	if m.in_water or (m.session != null and m.session.weather != 0 and m.world.gen.has_sky
			and m.world.get_light(floori(m.body.pos.x), floori(m.body.pos.y + 2.5), floori(m.body.pos.z)).x >= 15):
		if m.age % 10 == 0:
			teleport_random(16.0)
		return
	if pl == null or pl.dead or pl.gamemode != Player.SURVIVAL:
		return
	var angry := int(m.data.get("angry", 0)) > 0
	if not angry and m.age % 5 == 0:
		var head: ItemStack = pl.inventory.stack_at(Inventory.ARMOR + 3)
		if head == null or head.item_name() != "carved_pumpkin":
			var eye := m.body.pos + Vector3(0, m.body.height * 0.9, 0)
			var to: Vector3 = eye - pl.eye_position()
			var d := to.length()
			if d < 64.0 and d > 0.5 and pl.look_dir().dot(to / d) > 1.0 - 0.025 / maxf(1.0, d * 0.1) and sees(pl):
				m.data["angry"] = 600
				m.target = pl
				Sfx.play_mob(m.mob, "scream", m.body.pos, 1.0)
	elif angry and m.target != null and m.age % 40 == 0:
		var dd := m.body.pos.distance_to(pl.body.pos)
		if dd > 12.0 and _rng().randf() < 0.5:
			teleport_toward(pl.body.pos)


## Random teleport to a free standing spot within r blocks (endermen, chorus fruit style).
func teleport_random(r: float) -> bool:
	for i in 16:
		var q := m.body.pos + Vector3(_rng().randf_range(-r, r), _rng().randf_range(-8, 8), _rng().randf_range(-r, r))
		if _teleport_to(q):
			return true
	return false


func teleport_toward(target: Vector3) -> bool:
	for i in 8:
		var dirv := (target - m.body.pos).normalized()
		var q := target - dirv * _rng().randf_range(2.0, 5.0) + Vector3(_rng().randf_range(-2, 2), 0, _rng().randf_range(-2, 2))
		if _teleport_to(q):
			return true
	return false


func _teleport_to(q: Vector3) -> bool:
	var x := floori(q.x)
	var z := floori(q.z)
	var y := floori(q.y)
	if not m.world.is_ready_at(x, z):
		return false
	for dy in range(0, 10):
		var yy := y - dy
		var below := m.world.get_id(x, yy - 1, z)
		if BlockDB.solid[below] == 1 and BlockDB.fluid[below] == 0:
			var clear := true
			for h in 3:
				var id := m.world.get_id(x, yy + h, z)
				if BlockDB.solid[id] == 1 or BlockDB.fluid[id] == 1:
					clear = false
			if clear:
				if m.session != null:
					m.session.particles.portal(m.body.pos + Vector3(0, 1, 0))
				m.body.pos = Vector3(x + 0.5, yy, z + 0.5)
				m.prev_pos = m.body.pos
				m.body.vel = Vector3.ZERO
				m.path = PackedVector3Array()
				Sfx.play_at("teleport", m.body.pos, 0.6)
				return true
	return false


func on_hurt(attacker) -> void:
	if m.def.get("arch", "") == "enderman" and _rng().randf() < 0.5:
		teleport_random(12.0)
	if m.def.get("arch", "passive") in ["passive", "ambient", "fish"]:
		m.data["panic"] = 100
		m.data.erase("panic_dir")
		if attacker != null and attacker is Node3D:
			m.data["panic_from"] = (attacker as Node3D).global_position
	if attacker != null and m.def.get("arch", "") == "neutral":
		m.target = attacker
		m.data["angry"] = 240
	if attacker != null and m.def.get("arch", "") in ["enderman", "creaking"]:
		m.target = attacker
		m.data["angry"] = 600


# ------------------------------------------------------------------------------------------------
func sees(pl) -> bool:
	if pl == null or pl.dead:
		return false
	if m.world == null:
		return false
	var dst := m.body.pos.distance_to(pl.body.pos)
	var rng := float(m.def.get("reach", 16.0)) if m.def.get("arch", "").begins_with("hostile_ranged") else 16.0
	if dst > maxf(rng, 16.0):
		return false
	var hit := m.world.raycast(m.eye_pos(), (pl.eye_position() - m.eye_pos()).normalized(), dst, false, true)
	return hit.is_empty() or float(hit.dist) > dst - 0.5


func mid_target_valid(pl) -> bool:
	return _choose_target(pl) != null


func _choose_target(pl):
	if pl == null or pl.dead or pl.gamemode == Player.SPECTATOR:
		return null
	if pl.gamemode == Player.CREATIVE and not m.def.get("boss", false) and not bool(m.data.get("force_hostile", false)):
		return null
	var arch: String = m.def.get("arch", "passive")
	var hostile := bool(m.def.get("hostile", false))
	if !hostile and arch != "neutral" and arch != "enderman":
		return null
	if arch == "neutral" or arch == "enderman":
		var angry := int(m.data.get("angry", 0)) > 0
		if int(m.data.get("angry", 0)) > 0:
			m.data["angry"] = int(m.data["angry"]) - 1
		if m.target == null and not angry:
			return null
		if arch == "enderman" and !angry:
			return null
	var d := m.body.pos.distance_to(pl.body.pos)
	var maxd := 32.0 if not m.def.get("boss", false) else 256.0
	if d > maxd:
		return null
	if pl.dead:
		return null
	# non-boss hostile mobs need line of sight to acquire a target
	if m.target == null and not m.def.get("boss", false):
		if not sees(pl):
			return null
		if d > 24.0:
			return null
	if m.target == null:
		m.target = pl
		Sfx.play_mob(m.mob, "notice", m.body.pos, 0.5)
	return pl


# ------------------------------------------------------------------------------------------------
func _walk_to(target_pos: Vector3, speed_mul := 1.0) -> void:
	var s := float(m.def.get("speed", 0.06)) * speed_mul * m.speed_mult()
	var to := target_pos - m.body.pos
	var flat := Vector3(to.x, 0, to.z)
	if flat.length() < 0.35:
		m.body.vel.x *= 0.5
		m.body.vel.z *= 0.5
		m.move_speed = 0.0
		return
	var dir := flat.normalized()
	if m.body.on_ground and (m.body.collided_h or (jump_needed(dir) and m.body.vel.y <= 0.001)):
		m.body.vel.y = 0.42
		if m.def.get("arch", "") == "slime" or m.def.get("arch", "") == "sulfur_cube":
			m.body.vel.y = 0.5
	# desired ground speed = 2 * speed blocks/tick after the 0.546 ground friction applied in physics
	var want := dir * s * 2.0 / 0.546
	var k := 0.6 if m.body.on_ground else 0.12
	m.body.vel.x = lerpf(m.body.vel.x, want.x, k)
	m.body.vel.z = lerpf(m.body.vel.z, want.z, k)
	var hv := Vector2(m.body.vel.x, m.body.vel.z) * 0.546
	m.move_speed = hv.length()
	m.facing = atan2(-dir.x, -dir.z)
	m.last_move_dir = dir


func jump_needed(dir: Vector3) -> bool:
	var p := m.body.pos + dir * 0.6
	var x := floori(p.x)
	var z := floori(p.z)
	var y := floori(m.body.pos.y)
	if m.world == null:
		return false
	var id := m.world.get_id(x, y, z)
	if id != 0 and BlockDB.solid[id] == 1 and BlockDB.full[id] == 1:
		return true
	var id2 := m.world.get_id(x, y, z)
	if id2 != 0 and (BlockDB.model[id2] == BlockDB.M_SLAB or BlockDB.model[id2] == BlockDB.M_STAIRS):
		return true
	return false


func _wander(arch: String) -> void:
	m.move_speed = 0.0
	state_timer -= 0
	if arch == "ambient":
		_ambient()
		return
	if arch == "fish":
		_swim_wander()
		return
	if m.def.get("fly", false):
		_fly_wander()
		return
	if arch == "shulker" or arch == "sulfur_cube":
		return
	if m.sitting:
		return
	if m.wander_cd > 0:
		m.wander_cd -= 1
		if m.path.size() > 0 and m.path_i < m.path.size():
			_follow_path(0.5)
		else:
			m.body.vel.x *= 0.7
			m.body.vel.z *= 0.7
		return
	m.wander_cd = _rng().randi_range(60, 200)
	if m.path.size() > 0 and m.path_i < m.path.size():
		_follow_path(0.5)
		return
	if randf() < 0.4:
		var dir := Vector3(_rng().randf_range(-1, 1), 0, _rng().randf_range(-1, 1)).normalized()
		var target := m.body.pos + dir * _rng().randf_range(4.0, 12.0)
		if m.world.is_ready_at(floori(target.x), floori(target.z)):
			m.path = _path(target, 220, 3)
			m.path_i = 0
	# look around
	if m.age % 80 == 0:
		m.look_target = m.body.pos + Vector3(_rng().randf_range(-6, 6), 0, _rng().randf_range(-6, 6))


func _ambient() -> void:
	m.move_speed = 0.0
	if state_timer > 0:
		return
	state_timer = _rng().randi_range(40, 160)
	var dir := Vector3(_rng().randf_range(-1, 1), _rng().randf_range(0.2, 1.0), _rng().randf_range(-1, 1)).normalized()
	if m.def.get("fly", false):
		m.body.vel = dir * 0.12
	else:
		m.body.vel.x = dir.x * 0.15
		m.body.vel.z = dir.z * 0.15
	m.move_speed = 0.15
	m.facing = atan2(-dir.x, -dir.z)


func _swim_wander() -> void:
	m.move_speed = 0.0
	if state_timer > 0:
		state_timer -= 1
		return
	state_timer = _rng().randi_range(30, 120)
	var in_water := BlockDB.fluid[m.world.get_id(floori(m.body.pos.x), floori(m.body.pos.y + 0.3), floori(m.body.pos.z))] == 1
	if not in_water:
		m.body.vel.y += 0.05
		return
	var dir := Vector3(_rng().randf_range(-1, 1), _rng().randf_range(-0.3, 0.3), _rng().randf_range(-1, 1)).normalized()
	m.body.vel = dir * 0.1
	m.move_speed = 0.1
	m.facing = atan2(-dir.x, -dir.z)


func _fly_wander() -> void:
	m.move_speed = 0.0
	if state_timer > 0:
		state_timer -= 1
		return
	state_timer = _rng().randi_range(40, 140)
	var dir := Vector3(_rng().randf_range(-1, 1), 0, _rng().randf_range(-1, 1)).normalized()
	var target := m.body.pos + dir * 8.0
	if not m.def.get("boss", false):
		var below := m.world.top_y(floori(target.x), floori(target.z))
		target.y = maxf(float(below) + 2.5, m.body.pos.y + _rng().randf_range(-2, 2))
	var d: Vector3 = (target - m.body.pos)
	if d.length() > 0.3:
		var v := d.normalized() * float(m.def.get("speed", 0.1)) * 2.2
		m.body.vel = m.body.vel.lerp(v, 0.3)
		m.move_speed = m.body.vel.length()
		m.facing = atan2(-d.x, -d.z)


## A* path sized for this mob: head clearance rounded up (a 1.95-tall zombie needs 2 free blocks,
## a chicken 1) and its footprint width for straightening the path.
func _path(to: Vector3, max_nodes: int, max_drop: int) -> PackedVector3Array:
	var h := clampi(ceili(m.body.height - 0.05), 1, 3)
	return Pathfinder.find(m.world, m.body.pos, to, max_nodes, max_drop, h, m.body.width)


func _follow_path(speed_mul: float) -> void:
	if m.path_i >= m.path.size():
		m.move_speed = 0.0
		return
	var wp: Vector3 = m.path[m.path_i]
	var flat := Vector2(wp.x - m.body.pos.x, wp.z - m.body.pos.z)
	if flat.length() < 0.6:
		m.path_i += 1
		return
	var target := Vector3(wp.x, m.body.pos.y, wp.z)
	if wp.y > m.body.pos.y + 0.6 and m.body.on_ground:
		m.body.vel.y = 0.42
	_walk_to(target, speed_mul)


# ------------------------------------------------------------------------------------------------
func _combat(arch: String, pl) -> void:
	if pl == null or pl.dead:
		m.target = null
		_wander(arch)
		return
	var dist := m.body.pos.distance_to(pl.body.pos)
	last_seen = pl.body.pos
	m.look_target = pl.eye_position()
	match arch:
		"hostile_melee", "neutral", "enderman":
			_melee(arch, pl, dist)
		"hostile_ranged":
			_ranged(arch, pl, dist)
		"creeper":
			_creeper(pl, dist)
		"spider":
			_spider(pl, dist)
		"slime":
			_slime(pl, dist)
		"flyer":
			_flyer(pl, dist)
		"ghast":
			_ghast(pl, dist)
		"blaze":
			_blaze(pl, dist)
		"passive":
			_flee(pl, dist)
		_:
			_melee(arch, pl, dist)


func _melee(arch: String, pl, dist: float) -> void:
	if dist > 1.6 or m.attack_cd > 0:
		if m.attack_cd > 0:
			m.attack_cd -= 1
		var approach := 1.0
		if arch == "enderman":
			approach = 1.2
		_steer_to(pl.body.pos, approach, 22)
		return
	m.attack_cd = int(m.def.get("attack_cd", 20))
	pl.stats.damage(m.attack_damage(), "mob", m)
	Sfx.play_mob(m.mob, "attack", m.body.pos, 0.7)
	m.move_speed = 0.0


## Movement toward a target using a path when the way is blocked.
func _steer_to(pos: Vector3, speed_mul: float, repath_range := 20) -> void:
	var dist := m.body.pos.distance_to(pos)
	# the re-plan cooldown runs every tick (it used to stop once a path was used up, so a chasing
	# mob never planned again and kept walking into walls)
	if m.repath > 0:
		m.repath -= 1
	var has_path := m.path.size() > 0 and m.path_i < m.path.size()
	if m.repath <= 0 and (m.body.collided_h or (not has_path and dist > 2.5)):
		var capped := m.body.pos + (pos - m.body.pos).normalized() * minf(dist, 18.0)
		m.path = _path(capped, 300, 3)
		m.path_i = 0
		m.repath = 12 + _rng().randi_range(0, 10)
		has_path = not m.path.is_empty()
	if has_path and (m.body.collided_h or dist > 2.5):
		_follow_path(speed_mul)
	elif dist > 1.2:
		m.path = PackedVector3Array()
		_walk_to(pos, speed_mul)
	else:
		m.move_speed = 0.0
		m.body.vel.x *= 0.7
		m.body.vel.z *= 0.7


func _ranged(arch: String, pl, dist: float) -> void:
	var reach := float(m.def.get("reach", 12.0))
	var like := 8.0
	if m.mob == "witch":
		like = 6.0
	if dist > like + 3.0:
		_steer_to(pl.body.pos, 1.0, 24)
	elif dist < like - 4.0:
		var away: Vector3 = m.body.pos + (m.body.pos - pl.body.pos).normalized() * 4.0
		_walk_to(away, 0.9)
	else:
		m.move_speed = 0.0
		m.body.vel.x *= 0.8
		m.body.vel.z *= 0.8
		if shoot_cd > 0:
			shoot_cd -= 1
			return
		if not sees(pl):
			return
		shoot_cd = 40 + _rng().randi_range(0, 20)
		var from := m.eye_pos()
		var to: Vector3 = pl.eye_position() + Vector3(pl.body.vel.x, pl.body.vel.y, pl.body.vel.z) * 10.0
		var dir: Vector3 = (to - from).normalized()
		match m.mob:
			"witch":
				m.session.entities.spawn_projectile(m.world, "splash_potion", from, dir * 0.5 * 20.0, m,
					{"potion": ["harming", "poison", "slowness", "weakness"][_rng().randi_range(0, 3)]})
			"guardian", "elder_guardian":
				m.session.entities.spawn_projectile(m.world, "guardian_beam", from, dir * 1.2 * 20.0, m, {"damage": m.attack_damage()})
			"breeze":
				m.session.entities.spawn_projectile(m.world, "wind_charge", from, dir * 1.0 * 20.0, m, {})
			"evoker":
				pass
			_:
				m.session.entities.spawn_projectile(m.world, "arrow", from,
					(dir + Vector3(0, dist * 0.02, 0)).normalized() * (dist * 0.09 + 1.0) * 20.0, m,
					{"damage": m.attack_damage() * 1.5})
		Sfx.play_mob(m.mob, "shoot", m.body.pos, 0.7)


func _creeper(pl, dist: float) -> void:
	if m.fuse >= 0:
		m.fuse -= 1
		# stop and swell
		m.body.vel.x *= 0.6
		m.body.vel.z *= 0.6
		m.move_speed = 0.0
		var flash_on := (m.fuse / 3) % 2 == 0
		m.flash = 1.0 if flash_on else 0.0
		if m.fuse <= 0:
			Explosions.explode(m.world, m.body.pos + Vector3(0, 0.6, 0), 3.0, false, m)
			m.queue_free()
			return
		if dist > 4.0:
			m.fuse = -1
		return
	if dist < 3.0 and sees(pl):
		m.fuse = 30
		Sfx.play_at("creeper_hiss", m.body.pos, 0.8)
		return
	_steer_to(pl.body.pos, 1.15, 20)


func _spider(pl, dist: float) -> void:
	if not hostile_now(pl):
		_wander("spider")
		return
	if dist > 1.4:
		_steer_to(pl.body.pos, 1.2, 20)
	elif m.attack_cd <= 0:
		m.attack_cd = 20
		pl.stats.damage(m.attack_damage(), "mob", m)


func hostile_now(pl) -> bool:
	if m.data.get("angry", 0) != 0 and int(m.data.get("angry", 0)) > 0:
		return true
	var light := m.world.light_level(floori(m.body.pos.x), floori(m.body.pos.y + 1), floori(m.body.pos.z), m.session.sky_darken())
	return light < 9 or not m.world.gen.has_sky


func _slime(pl, dist: float) -> void:
	if m.body.on_ground and hop_cd <= 0:
		var size := int(m.data.get("size", 1))
		m.body.vel.y = 0.42 - 0.06 * (size - 1)
		var dir: Vector3 = (pl.body.pos - m.body.pos)
		dir.y = 0
		dir = dir.normalized() * (0.3 + 0.08 * (size - 1))
		if size <= 1:
			dir = Vector3(_rng().randf_range(-1, 1), 0, _rng().randf_range(-1, 1)).normalized() * 0.2
		m.body.vel.x = dir.x
		m.body.vel.z = dir.z
		m.move_speed = 0.3
		hop_cd = maxi(10, 26 - size * 4)
	else:
		hop_cd -= 1
	if dist < 1.0 + 0.3 * int(m.data.get("size", 1)) and m.attack_cd <= 0:
		m.attack_cd = 20
		pl.stats.damage(m.attack_damage(), "mob", m)


func _flyer(pl, dist: float) -> void:
	var above: Vector3 = pl.body.pos + Vector3(0, 3.5, 0)
	if dist > 2.0:
		var d: Vector3 = (above - m.body.pos)
		var v: Vector3 = d.normalized() * 0.25
		m.body.vel = m.body.vel.lerp(v, 0.2)
		m.move_speed = m.body.vel.length()
		m.facing = atan2(-d.x, -d.z)
	elif m.attack_cd <= 0:
		m.attack_cd = int(m.def.get("attack_cd", 30))
		pl.stats.damage(m.attack_damage(), "mob", m)


func _ghast(pl, dist: float) -> void:
	var keep := 24.0
	var target: Vector3 = pl.body.pos + (m.body.pos - pl.body.pos).normalized() * keep + Vector3(0, 6, 0)
	var d: Vector3 = (target - m.body.pos)
	if d.length() > 1.0:
		m.body.vel = m.body.vel.lerp(d.normalized() * 0.22, 0.05)
	m.move_speed = m.body.vel.length()
	m.facing = atan2(-(pl.body.pos.x - m.body.pos.x), -(pl.body.pos.z - m.body.pos.z))
	if shoot_cd > 0:
		shoot_cd -= 1
		return
	if dist < 48.0 and sees(pl):
		shoot_cd = 60 + _rng().randi_range(0, 40)
		m.session.entities.spawn_projectile(m.world, "fireball", m.eye_pos(), (pl.eye_position() - m.eye_pos()).normalized() * 0.9 * 20.0, m, {})
		Sfx.play_mob("ghast", "shoot", m.body.pos, 1.0)


func _blaze(pl, dist: float) -> void:
	var hover: Vector3 = pl.body.pos + Vector3(0, 4.0, 0)
	if dist > 6.0 or dist < 2.0:
		var d: Vector3 = (hover - m.body.pos)
		m.body.vel = m.body.vel.lerp(d.normalized() * 0.2, 0.08)
		m.move_speed = m.body.vel.length()
	m.facing = atan2(-(pl.body.pos.x - m.body.pos.x), -(pl.body.pos.z - m.body.pos.z))
	if shoot_cd > 0:
		shoot_cd -= 1
		return
	shoot_cd = 30 + _rng().randi_range(0, 30)
	# burst of three fireballs like the original
	for i in 3:
		var spread := Vector3(_rng().randf_range(-0.08, 0.08), _rng().randf_range(-0.05, 0.05), _rng().randf_range(-0.08, 0.08))
		m.session.entities.spawn_projectile(m.world, "small_fireball",
			m.eye_pos() + Vector3(0, 0.2 * i, 0), ((pl.eye_position() - m.eye_pos()).normalized() + spread) * 1.0 * 20.0, m, {})
	Sfx.play_mob("blaze", "shoot", m.body.pos, 1.0)


func _flee(pl, dist: float) -> void:
	if dist < 6.0:
		var away: Vector3 = m.body.pos + (m.body.pos - pl.body.pos).normalized() * 8.0
		_walk_to(away, 1.2)
	else:
		_wander("passive")


# ------------------------------------------------------------------------------------------------
## Shulker: anchored to its block, opens and shoots homing bullets.
func _shulker(pl) -> void:
	if m.age % 20 == 0:
		var attached := m.world.get_block(floori(m.body.pos.x), floori(m.body.pos.y) - 1, floori(m.body.pos.z))
		if attached == 0 and m.data.get("peek", false):
			m.data["peek"] = false
	if pl == null or pl.dead:
		return
	var dist := m.body.pos.distance_to(pl.body.pos)
	if dist > 24.0:
		return
	m.data["peek"] = true
	if shoot_cd > 0:
		shoot_cd -= 1
		return
	shoot_cd = 60 + _rng().randi_range(0, 40)
	m.session.entities.spawn_projectile(m.world, "shulker_bullet", m.eye_pos(), Vector3(0, 0.1, 0) * 20.0, m, {"damage": m.attack_damage()})
	Sfx.play_mob("shulker", "shoot", m.body.pos, 0.8)


func _creaking(pl) -> void:
	if pl == null or pl.dead:
		_wander("creaking")
		return
	var being_watched := _is_watched(pl)
	if being_watched:
		m.move_speed = 0.0
		m.body.vel.x *= 0.2
		m.body.vel.z *= 0.2
		m.facing = atan2(-(pl.body.pos.x - m.body.pos.x), -(pl.body.pos.z - m.body.pos.z))
		return
	var dist := m.body.pos.distance_to(pl.body.pos)
	if dist > 1.6:
		_steer_to(pl.body.pos, 1.4, 18)
	elif m.attack_cd <= 0:
		m.attack_cd = 30
		pl.stats.damage(m.attack_damage(), "mob", m)


func _is_watched(pl) -> bool:
	var to_mob: Vector3 = (m.eye_pos() - pl.eye_position()).normalized()
	var look: Vector3 = pl.look_dir()
	if look.dot(to_mob) < 0.85:
		return false
	var hit := m.world.raycast(pl.eye_position(), to_mob, 40.0, false, true)
	return hit.is_empty() or float(hit.dist) > m.body.pos.distance_to(pl.body.pos) - 1.0


func _warden(pl) -> void:
	if pl == null or pl.dead:
		_wander("warden")
		return
	var dist := m.body.pos.distance_to(pl.body.pos)
	m.data["darkness"] = 1
	if sonic_cd > 0:
		sonic_cd -= 1
	if dist < 3.0 and m.attack_cd <= 0:
		m.attack_cd = 40
		pl.stats.damage(m.attack_damage(), "mob", m)
		# violent knockback
		var dir: Vector3 = (pl.body.pos - m.body.pos).normalized()
		pl.body.vel.x += dir.x * 3.0
		pl.body.vel.z += dir.z * 3.0
		pl.body.vel.y = maxf(pl.body.vel.y, 0.7)
		Sfx.play_at("warden_attack", m.body.pos, 1.0)
	elif dist < 20.0 and sonic_cd <= 0 and sees(pl):
		sonic_cd = 100
		var dir2: Vector3 = (pl.eye_position() - m.eye_pos()).normalized()
		pl.body.vel = dir2 * 2.5
		pl.body.vel.y = maxf(1.0, pl.body.vel.y)
		pl.stats.damage(6.0, "sonic_boom", m, true)
		m.world.session.particles.burst(pl.eye_position(), Color(0.35, 0.85, 0.9), 40, 6.0, "soul", 0.25)
		Sfx.play_at("sonic_boom", m.body.pos, 1.0)
	_steer_to(pl.body.pos, 1.0, 24)


func _golem(pl) -> void:
	# Iron golems attack hostiles nearby; snow golems throw snowballs at them.
	var enemy = _nearest_hostile(16.0)
	if enemy != null:
		var dist := m.body.pos.distance_to(enemy.body.pos)
		m.look_target = enemy.eye_pos()
		if m.mob == "iron_golem":
			if dist > 2.2:
				_steer_to(enemy.body.pos, 1.1, 20)
			elif m.attack_cd <= 0:
				m.attack_cd = 20
				var dir := Vector2(enemy.body.pos.x - m.body.pos.x, enemy.body.pos.z - m.body.pos.z).normalized()
				enemy.hurt(7.0 + 4.0, "mob", m, dir, 1.2)
				enemy.body.vel.y = 0.6
				m.world.session.particles.burst(enemy.body.pos + Vector3(0, 1, 0), Color(0.9, 0.9, 0.9), 10, 3.0, "crit", 0.15)
				Sfx.play_at("iron_golem_attack", m.body.pos, 1.0)
		else:
			if dist > 10.0:
				_steer_to(enemy.body.pos, 0.9, 18)
			elif shoot_cd <= 0:
				shoot_cd = 40
				m.session.entities.spawn_projectile(m.world, "snowball", m.eye_pos(),
					(enemy.eye_position() - m.eye_pos()).normalized() * 1.2 * 20.0, m, {"damage": 0.0})
		return
	if pl != null and pl.dead:
		return
	_wander("passive")


func _nearest_hostile(radius: float):
	if m.session == null:
		return null
	var best = null
	var bd := radius * radius
	for e in m.session.entities.all():
		if e == m or not (e is Mob):
			continue
		var om: Mob = e
		if not om.hostile() or om.dead:
			continue
		var d: float = om.body.pos.distance_squared_to(m.body.pos)
		if d < bd:
			bd = d
			best = om
	return best


func _copper_golem() -> void:
	# Picks up dropped items and carries them to a nearby chest, sorting into matching stacks.
	if m.session == null:
		return
	if m.carried != 0:
		var chest := _find_chest(8.0)
		if chest != Vector3.INF:
			if m.body.pos.distance_to(chest) < 1.8:
				var be := m.world.get_be(floori(chest.x), floori(chest.y), floori(chest.z), true)
				if be.has("items"):
					var inv := Inventory.new((be["items"] as Array).size())
					inv.from_array(be["items"])
					var rem := inv.add(ItemStack.new(m.carried, 1), 0, inv.size())
					if rem == null:
						be["items"] = inv.to_array()
						m.world.mark_modified(floori(chest.x), floori(chest.z))
						m.carried = 0
				return
			_steer_to(chest, 0.9, 16)
			return
		m.carried = 0
	var item = _nearest_item(8.0)
	if item != null and item is SimpleEntities.ItemEntity:
		var ie: SimpleEntities.ItemEntity = item
		if ie.stack != null and ie.body.pos.distance_to(m.body.pos) < 1.4:
			m.carried = ie.stack.id
			ie.queue_free()
			Sfx.play_at("pop", m.body.pos, 0.4)
			return
		_steer_to(ie.body.pos, 0.9, 16)
		return
	if m.age % 60 == 0:
		m.look_target = m.body.pos + Vector3(_rng().randf_range(-5, 5), 0, _rng().randf_range(-5, 5))
	if m.wander_cd <= 0:
		m.wander_cd = _rng().randi_range(60, 160)
		var dir := Vector3(_rng().randf_range(-1, 1), 0, _rng().randf_range(-1, 1)).normalized()
		m.path = _path(m.body.pos + dir * 6.0, 160, 2)
		m.path_i = 0
	else:
		m.wander_cd -= 1
	if m.path.size() > 0 and m.path_i < m.path.size():
		_follow_path(0.7)


func _nearest_item(radius: float):
	if m.session == null:
		return null
	var best = null
	var bd := radius * radius
	for e in m.session.entities.all():
		if not (e is SimpleEntities.ItemEntity):
			continue
		var ie: SimpleEntities.ItemEntity = e
		if ie.pickup_delay > 0:
			continue
		var d: float = ie.body.pos.distance_squared_to(m.body.pos)
		if d < bd:
			bd = d
			best = ie
	return best


func _find_chest(radius: float) -> Vector3:
	var p := Vector3i(floori(m.body.pos.x), floori(m.body.pos.y), floori(m.body.pos.z))
	for r in range(1, int(radius)):
		for dx in range(-r, r + 1):
			for dy in range(-2, 3):
				for dz in range(-r, r + 1):
					if maxi(absi(dx), absi(dz)) != r:
						continue
					var v := m.world.get_block(p.x + dx, p.y + dy, p.z + dz)
					if v == 0:
						continue
					var d: BlockDef = BlockDB.defs[v & 0xFFF]
					if d.name in ["chest", "barrel", "trapped_chest"] or d.name.ends_with("copper_chest"):
						return Vector3(p.x + dx, p.y + dy, p.z + dz)
	return Vector3.INF


# ================================================================================================
# Bosses
func _dragon(pl) -> void:
	var sess = m.session
	if sess == null or pl == null or pl.dead:
		_circle_dragon(0.0)
		return
	var crystals: Array = sess.dragon_crystals()
	# healing beam from any crystal within reach (the crystals draw the beam themselves)
	if m.age % 10 == 0 and not crystals.is_empty():
		var active := false
		for c in crystals:
			if is_instance_valid(c) and (c as Node3D).is_inside_tree() and (c as Node3D).global_position.distance_to(m.body.pos) < 48.0:
				active = true
				break
		if active:
			m.health = minf(m.max_health, m.health + 1.0)
	if charge_timer > 0:
		charge_timer -= 1
		var dir: Vector3 = (pl.body.pos - m.body.pos)
		if dir.length() > 1.0:
			m.body.vel = m.body.vel.lerp(dir.normalized() * 0.75, 0.15)
			m.facing = atan2(-dir.x, -dir.z)
		if m.body.pos.distance_to(pl.body.pos) < 4.5 and m.attack_cd <= 0:
			m.attack_cd = 20
			pl.stats.damage(10.0, "mob", m)
			var kb: Vector3 = (pl.body.pos - m.body.pos).normalized()
			pl.body.vel.x += kb.x * 3.0
			pl.body.vel.z += kb.z * 3.0
			pl.body.vel.y = maxf(pl.body.vel.y, 0.8)
		return
	if perch:
		var lair := Vector3(0.5, float(m.data.get("perch_y", 86.0)), 0.5)
		var dl := lair - m.body.pos
		m.body.vel = m.body.vel.lerp(dl.normalized() * minf(0.5, dl.length() * 0.05), 0.1)
		if dl.length_squared() > 1.0:
			m.facing = atan2(-dl.x, -dl.z)
		if m.body.pos.distance_to(lair) < 6.0:
			m.body.vel *= 0.85
			if breath_timer <= 0:
				breath_timer = 100
				_dragon_breath(pl)
		breath_timer -= 1
		if state_timer <= 0:
			perch = false
			state_timer = 300 + _rng().randi_range(0, 200)
		return
	_circle_dragon(0.0)
	if state_timer <= 0:
		var roll := _rng().randf()
		if roll < 0.35 and m.body.pos.distance_to(Vector3(0.5, 70, 0.5)) < 80.0:
			perch = true
			state_timer = 300 + _rng().randi_range(0, 200)
			Sfx.play_at("dragon_flap", m.body.pos, 1.0)
		elif roll < 0.6:
			charge_timer = 60
		else:
			if not crystals.is_empty():
				var c = crystals[_rng().randi_range(0, crystals.size() - 1)]
				if is_instance_valid(c):
					m.data["crystal"] = (c as Node3D).global_position
			if shoot_cd <= 0:
				shoot_cd = 60
				_dragon_fireball(pl)
	shoot_cd -= 1
	if m.age % 200 == 0 and m.body.pos.distance_to(Vector3(0.5, 66, 0.5)) > 60.0:
		pass


func _circle_dragon(_x: float) -> void:
	circle_angle += 0.012
	# circles outside the ring of obsidian spikes (radius 42, up to y 103)
	var lair := Vector3(0.5, 92.0, 0.5)
	var r := 62.0
	var target := lair + Vector3(cos(circle_angle) * r, sin(circle_angle * 0.7) * 8.0, sin(circle_angle) * r)
	var d: Vector3 = (target - m.body.pos)
	m.body.vel = m.body.vel.lerp(d.normalized() * 0.55, 0.1)
	m.move_speed = m.body.vel.length()
	var to := (m.body.pos + m.body.vel * 10.0) - m.body.pos
	if to.length_squared() > 0.001:
		m.facing = atan2(-to.x, -to.z)
	m.head_pitch = clampf(-m.body.vel.y * 1.5, -0.6, 0.6)


func _dragon_breath(pl) -> void:
	if m.session == null:
		return
	var origin := m.body.pos + Vector3(0, -1.5, 0) + Vector3(-sin(m.facing), 0, -cos(m.facing)) * 8.0
	var dir: Vector3 = (pl.body.pos - origin)
	if dir.length() < 0.1:
		return
	dir = dir.normalized()
	for i in 30:
		var p: Vector3 = origin + dir * (i * 0.8)
		m.session.particles.dragon_breath(p, 4)
		if pl.body.pos.distance_to(p) < 1.6:
			pl.stats.damage(2.0, "dragon_breath")
	Sfx.play_at("dragon_breath", origin, 1.0)


func _dragon_fireball(pl) -> void:
	var from := m.body.pos + Vector3(0, -1.0, 0)
	var dir: Vector3 = (pl.body.pos + Vector3(0, 1, 0) - from).normalized()
	m.session.entities.spawn_projectile(m.world, "fireball", from, dir * 0.75 * 20.0, m, {})
	Sfx.play_at("dragon_shoot", m.body.pos, 1.0)


func _wither(pl) -> void:
	var health_frac := m.health / maxf(1.0, m.max_health)
	var new_phase := 0
	if health_frac <= 0.33:
		new_phase = 2
	elif health_frac <= 0.66:
		new_phase = 1
	if new_phase != phase:
		phase = new_phase
		m.data["armor"] = 0.0 if phase == 0 else (4.0 if phase == 1 else 8.0)
		m.invuln_phase = phase
		# phase transition explosion
		Explosions.explode(m.world, m.body.pos, 1.5, false, m, false)
		Sfx.play_at("wither_spawn", m.body.pos, 1.0)
		m.health = minf(m.max_health, m.health + 20.0)
		var sess = m.session
		if sess != null:
			sess.particles.burst(m.body.pos + Vector3(0, 1, 0), Color(0.15, 0.15, 0.18), 60, 4.0, "large_smoke", 0.4)
	if m.age < 120:
		m.health = maxf(m.health, m.max_health * 0.55)
		m.body.vel = m.body.vel.lerp(Vector3(0, 0.15, 0), 0.1)
	if pl == null or pl.dead:
		_circle_dragon(0.0)
		return
	var dist := m.body.pos.distance_to(pl.body.pos)
	var hover: Vector3 = pl.body.pos + Vector3(0, 12.0, 0)
	if dist > 24.0:
		var d: Vector3 = (hover - m.body.pos)
		m.body.vel = m.body.vel.lerp(d.normalized() * 0.3, 0.06)
	else:
		m.body.vel = m.body.vel.lerp(Vector3(cos(float(m.age) * 0.03) * 0.1, 0, sin(float(m.age) * 0.03) * 0.1), 0.05)
	m.move_speed = m.body.vel.length()
	m.facing = atan2(-(pl.body.pos.x - m.body.pos.x), -(pl.body.pos.z - m.body.pos.z))
	if shoot_cd > 0:
		shoot_cd -= 1
		return
	shoot_cd = 40 + _rng().randi_range(0, 30)
	if phase == 0:
		for i in 3:
			var dir: Vector3 = (pl.body.pos + Vector3(0, 1, 0) - m.body.pos).normalized()
			var spread := Vector3(_rng().randf_range(-0.12, 0.12), _rng().randf_range(-0.05, 0.05), _rng().randf_range(-0.12, 0.12))
			m.session.entities.spawn_projectile(m.world, "wither_skull", m.eye_pos(), (dir + spread) * 0.7 * 20.0, m, {})
	else:
		m.session.entities.spawn_projectile(m.world, "wither_skull_dangerous", m.eye_pos(),
			(pl.body.pos + Vector3(0, 1, 0) - m.eye_pos()).normalized() * 0.8 * 20.0, m, {})
	if phase >= 1 and summon_timer <= 0:
		summon_timer = 600
		if m.session != null:
			for i in 2:
				m.session.entities.spawn_mob(m.world, "wither_skeleton",
					m.body.pos + Vector3(_rng().randf_range(-3, 3), 1.0, _rng().randf_range(-3, 3)))
	summon_timer -= 1
	Sfx.play_mob("wither", "shoot", m.body.pos, 1.0)
