class_name EntityManager
extends Node3D
## Owns every entity of one World: spawning helpers, 20 TPS ticking, render interpolation,
## pickups, entity/entity pushing, ray queries, explosion impact, natural mob spawning with caps,
## and chunk-based persistence of entities.

const HOSTILE_CAP := 45
const PASSIVE_CAP := 14
const WATER_CAP := 10
const AMBIENT_CAP := 6

var world: World = null
var list: Array = []
var _spawn_timer := 0
var _light_i := 0
var _rng := RandomNumberGenerator.new()
var paused := false


func _init() -> void:
	name = "Entities"
	_rng.randomize()


func all() -> Array:
	return list


func count_mobs() -> Dictionary:
	var c := {"hostile": 0, "passive": 0, "water": 0, "ambient": 0}
	for e in list:
		if e is Mob:
			var m: Mob = e
			var cat := _category(m.mob)
			c[cat] = int(c[cat]) + 1
	return c


func _category(n: String) -> String:
	var d: Dictionary = MobDB.get_def(n)
	if d.is_empty():
		return "passive"
	if bool(d.get("hostile", false)):
		return "hostile"
	var arch: String = d.get("arch", "passive")
	if arch == "fish":
		return "water"
	if arch == "ambient":
		return "ambient"
	return "passive"


## Registers an externally created entity (crystals, eyes of ender...).
func add_entity(e: Entity) -> Entity:
	return _add(e)


func _add(e: Entity) -> Entity:
	add_child(e)
	list.append(e)
	e.tree_exiting.connect(_on_exit.bind(e))
	return e


func _on_exit(e) -> void:
	list.erase(e)


# ------------------------------------------------------------------------------------------------
# Spawning helpers
func spawn_item(w: World, pos: Vector3, st: ItemStack, vel = null) -> Entity:
	if st == null or st.is_empty():
		return null
	var e := SimpleEntities.ItemEntity.new()
	var v: Vector3 = vel if vel is Vector3 else Vector3(_rng.randf_range(-0.1, 0.1), 0.2, _rng.randf_range(-0.1, 0.1))
	if vel is Vector3:
		v = (vel as Vector3) / 20.0
	_add(e)
	e.setup_item(w, pos, st, v)
	return e


func spawn_xp(w: World, pos: Vector3, amount: int) -> void:
	var left := amount
	while left > 0:
		var n := 1
		for s in [2477, 1237, 617, 307, 149, 73, 37, 17, 7, 3, 1]:
			if left >= int(s):
				n = int(s)
				break
		left -= n
		var e := SimpleEntities.XpOrb.new()
		_add(e)
		e.setup_xp(w, pos + Vector3(_rng.randf_range(-0.3, 0.3), 0.2, _rng.randf_range(-0.3, 0.3)), n)


func spawn_tnt(w: World, pos: Vector3, fuse: int, igniter = null) -> Entity:
	var e := SimpleEntities.TntEntity.new()
	_add(e)
	e.setup_tnt(w, pos, fuse, igniter)
	e.body.vel = Vector3(_rng.randf_range(-0.02, 0.02), 0.2, _rng.randf_range(-0.02, 0.02))
	return e


func spawn_falling_block(w: World, pos: Vector3, v: int) -> Entity:
	var e := SimpleEntities.FallingBlockEntity.new()
	_add(e)
	e.setup_falling(w, pos, v)
	return e


func spawn_projectile(w: World, kind: String, pos: Vector3, vel: Vector3, shooter, extra := {}) -> Entity:
	var e := SimpleEntities.Projectile.new()
	_add(e)
	var k := kind
	if kind == "wither_skull" or kind == "wither_skull_dangerous":
		k = "small_fireball"
		extra["wither"] = true
	if kind == "shulker_bullet" or kind == "guardian_beam":
		k = "small_fireball"
		extra["special"] = kind
	e.setup_proj(w, k, pos, vel / 20.0, shooter, extra)
	if kind == "wither_skull_dangerous":
		e.kind = "fireball"
	return e


func spawn_vehicle(w: World, kind: String, pos: Vector3, props := {}) -> Entity:
	var e := SimpleEntities.Vehicle.new()
	_add(e)
	e.setup_vehicle(w, kind, pos, props)
	return e


func spawn_mob(w: World, mob_name: String, pos: Vector3, extra := {}) -> Mob:
	if not MobDB.has(mob_name):
		push_warning("unknown mob " + mob_name)
		return null
	var m := Mob.new()
	_add(m)
	if extra.has("size"):
		m.data["size"] = int(extra["size"])
	m.setup_mob(w, mob_name, pos, extra)
	if m.data.has("size"):
		var sz: int = int(m.data["size"])
		m.body.width = 0.52 * sz
		m.body.height = 0.52 * sz
		m.health = float(sz * sz)
		m.max_health = m.health
		if m.rig != null:
			m.rig.scale = Vector3.ONE * (0.5 * sz)
	if extra.get("persistent", false) or extra.get("summoned", false):
		m.data.erase("despawn")
		m.def = m.def.duplicate()
		m.def["despawn"] = false
	m.update_light()
	return m


## Spawn with slime sizes etc. picked like the original.
func spawn_natural(w: World, mob_name: String, pos: Vector3) -> Mob:
	var extra := {}
	if mob_name == "slime" or mob_name == "magma_cube" or mob_name == "sulfur_cube":
		extra["size"] = [1, 2, 4][_rng.randi_range(0, 2)]
	if _rng.randf() < 0.05 and mob_name in ["zombie", "husk", "drowned", "zombie_villager", "piglin", "hoglin", "pig", "cow",
			"sheep", "chicken", "wolf", "rabbit", "fox", "cat", "villager"]:
		extra["baby"] = true
	return spawn_mob(w, mob_name, pos, extra)


# ------------------------------------------------------------------------------------------------
func tick() -> void:
	if paused:
		return
	var session = world.session
	var pl = session.player if session != null else null
	for e in list.duplicate():
		var ent: Entity = e
		if not is_instance_valid(ent) or ent.is_queued_for_deletion():
			continue
		# freeze entities in chunks that are not ready (no ground); drawn standing still
		if not world.is_ready_at(floori(ent.body.pos.x), floori(ent.body.pos.z)):
			ent.prev_pos = ent.body.pos
			ent.prev_facing = ent.facing
			if ent is Mob:
				(ent as Mob).prev_body_yaw = (ent as Mob).body_yaw
			continue
		ent.tick()
		if pl != null and not pl.dead and ent.has_method("try_pickup"):
			ent.try_pickup(pl)
	_push()
	# light updates spread over ticks
	if not list.is_empty():
		for k in mini(8, list.size()):
			_light_i = (_light_i + 1) % list.size()
			var e2 = list[_light_i]
			if is_instance_valid(e2) and e2.has_method("update_light"):
				e2.update_light()
			elif is_instance_valid(e2):
				_light_simple(e2)
	_spawn_timer -= 1
	if _spawn_timer <= 0 and session != null:
		_spawn_timer = 20
		natural_spawn()


func _light_simple(e: Entity) -> void:
	if e.visual == null:
		return
	var l := world.get_light(floori(e.body.pos.x), floori(e.body.pos.y + 0.3), floori(e.body.pos.z))
	_set_light_rec(e.visual, Vector2(l.x, l.y))


func _set_light_rec(n: Node, l: Vector2) -> void:
	for c in n.get_children():
		if c is GeometryInstance3D:
			(c as GeometryInstance3D).set_instance_shader_parameter("ent_light", l)
		_set_light_rec(c, l)


func _process(_delta: float) -> void:
	var session = world.session if world != null else null
	var alpha: float = session.tick_alpha if session != null else 1.0
	for e in list:
		if is_instance_valid(e):
			(e as Entity).frame(alpha)


## Soft entity-entity separation (mobs don't stack inside each other).
func _push() -> void:
	var mobs := []
	for e in list:
		if e is Mob and not (e as Mob).dead:
			mobs.append(e)
	var n := mobs.size()
	if n < 2 or n > 300:
		return
	for i in n:
		var a: Mob = mobs[i]
		for j in range(i + 1, n):
			var b: Mob = mobs[j]
			var d := Vector2(b.body.pos.x - a.body.pos.x, b.body.pos.z - a.body.pos.z)
			var r := (a.body.width + b.body.width) * 0.5
			var l2 := d.length_squared()
			if l2 < r * r and l2 > 1e-6 and absf(a.body.pos.y - b.body.pos.y) < 1.5:
				var push := d.normalized() * 0.03
				if a.can_be_pushed():
					a.body.vel.x -= push.x
					a.body.vel.z -= push.y
				if b.can_be_pushed():
					b.body.vel.x += push.x
					b.body.vel.z += push.y


# ------------------------------------------------------------------------------------------------
# Queries
func raycast_entity(origin: Vector3, dir: Vector3, max_dist: float, exclude = null):
	var best = null
	var best_t := max_dist
	var mount = exclude.riding if exclude != null and exclude is Player else null
	for e in list:
		if e == exclude or e == mount or not is_instance_valid(e):
			continue
		var ent: Entity = e
		if ent is SimpleEntities.ItemEntity or ent is SimpleEntities.XpOrb:
			continue
		if ent is SimpleEntities.Projectile and ent.kind != "fireball":
			continue
		var box := ent.entity_aabb().grow(0.1)
		var hit := World._ray_box(origin, dir, box.position, box.end)
		if hit.x >= 0.0 and hit.x < best_t:
			best_t = hit.x
			best = ent
	return best


func entity_at(p: Vector3, radius: float, exclude = null):
	for e in list:
		if e == exclude or not is_instance_valid(e):
			continue
		var ent: Entity = e
		if ent is SimpleEntities.ItemEntity or ent is SimpleEntities.XpOrb or ent is SimpleEntities.Projectile:
			continue
		if ent.entity_aabb().grow(radius).has_point(p):
			return ent
	return null


func any_in_box(_w: World, box: AABB, type_filter := "") -> bool:
	for e in list:
		if not is_instance_valid(e):
			continue
		var ent: Entity = e
		if type_filter == "arrow" and not (ent is SimpleEntities.Projectile):
			continue
		if ent.entity_aabb().intersects(box):
			return true
	return false


func count_in_box(box: AABB, items_only := false, mobs_only := false) -> int:
	var n := 0
	for e in list:
		if not is_instance_valid(e):
			continue
		var ent: Entity = e
		if items_only and not (ent is SimpleEntities.ItemEntity):
			continue
		if mobs_only and not (ent is Mob):
			continue
		if ent.entity_aabb().intersects(box):
			n += 1 if not (ent is SimpleEntities.ItemEntity) else (ent as SimpleEntities.ItemEntity).stack.count
	return n


func blocks_placement(box: AABB) -> bool:
	for e in list:
		if not is_instance_valid(e):
			continue
		var ent: Entity = e
		if ent is SimpleEntities.ItemEntity or ent is SimpleEntities.XpOrb or ent is SimpleEntities.Projectile:
			continue
		if ent.entity_aabb().grow(-0.01).intersects(box):
			return true
	return false


func sweep_attack(player, main_target, strength: float) -> void:
	var center: Vector3 = main_target.body.pos
	for e in list:
		if e == main_target or not (e is Mob):
			continue
		var m: Mob = e
		if m.body.pos.distance_to(center) < 1.5 and m.body.pos.distance_to(player.body.pos) < 4.0:
			var dir := Vector2(m.body.pos.x - player.body.pos.x, m.body.pos.z - player.body.pos.z).normalized()
			m.hurt(1.0 * strength, "player", player, dir, 0.4)
	world.session.particles.burst(center + Vector3(0, 1, 0), Color(0.9, 0.9, 0.9), 6, 2.0, "crit", 0.3)
	Sfx.play_at("sweep", center, 0.6)


func shear_at(_w: World, p: Vector3i) -> bool:
	for e in list:
		if e is Mob:
			var m: Mob = e
			if m.mob == "sheep" and not m.sheared and m.entity_aabb().intersects(AABB(Vector3(p), Vector3.ONE)):
				m.sheared = true
				spawn_item(world, m.body.pos + Vector3(0, 0.7, 0), ItemStack.of(m.wool_color + "_wool", 1 + _rng.randi_range(0, 2)))
				return true
	return false


func equip_nearby(_w: World, p: Vector3i, st: ItemStack) -> bool:
	var pl = world.session.player
	if pl != null and pl.body.aabb().intersects(AABB(Vector3(p), Vector3.ONE)):
		var slot: int = Inventory.ARMOR + st.item().armor_slot
		if pl.inventory.stack_at(slot) == null:
			pl.inventory.set_stack(slot, st.with_count(1))
			return true
	return false


## Damage + knockback to all entities from an explosion.
func explosion_impact(w: World, center: Vector3, power: float, source) -> void:
	var r := power * 2.0
	for e in list.duplicate():
		if not is_instance_valid(e) or e == source:
			continue
		var ent: Entity = e
		var pos := ent.body.pos + Vector3(0, ent.body.height * 0.5, 0)
		var dist := pos.distance_to(center)
		if dist > r:
			continue
		var expo := Explosions.exposure(w, center, ent.entity_aabb())
		var dmg := Explosions.damage_for(center, pos, power, expo)
		var kb := Explosions.knockback_for(center, pos, power, expo)
		if ent is SimpleEntities.ItemEntity:
			if dmg > 4.0:
				ent.queue_free()
			else:
				ent.body.vel += kb
			continue
		if ent is SimpleEntities.XpOrb or ent is SimpleEntities.Projectile:
			ent.body.vel += kb
			continue
		if ent is SimpleEntities.TntEntity:
			ent.body.vel += kb
			continue
		if dmg > 0.0:
			ent.hurt(dmg, "explosion", source)
		ent.body.vel += kb


# ------------------------------------------------------------------------------------------------
# Natural spawning (every second)
func natural_spawn() -> void:
	var session = world.session
	var pl = session.player
	if pl == null or pl.dead:
		return
	if not bool(session.gamerule("doMobSpawning", true)):
		return
	var counts := count_mobs()
	var diff: int = session.difficulty
	var tries := 3
	for t in tries:
		var ang := _rng.randf() * TAU
		var dist := _rng.randf_range(24.0, 72.0)
		var x := floori(pl.body.pos.x + cos(ang) * dist)
		var z := floori(pl.body.pos.z + sin(ang) * dist)
		if not world.is_ready_at(x, z):
			continue
		var y := _pick_y(x, z, pl.body.pos.y)
		if y == -99999:
			continue
		var p := Vector3i(x, y, z)
		var biome_id := world.biome_at(x, y, z)
		var biome := BiomeDB.name_of(biome_id)
		var sky_dark: int = session.sky_darken()
		var light := world.light_level(x, y, z, sky_dark)
		var blk := world.get_light(x, y, z).y
		var in_water := BlockDB.fluid[world.get_id(x, y, z)] == 1
		var choice := ""
		if world.dim == 0:
			if in_water:
				if counts.water < WATER_CAP:
					choice = _pick_water(biome)
			elif light == 0 and blk == 0 and diff > 0 and counts.hostile < HOSTILE_CAP:
				choice = _pick_hostile(biome, y)
			elif light >= 9 and counts.passive < PASSIVE_CAP and _rng.randf() < 0.15:
				var ground := BlockDB.name_of(world.get_blockv(p + Vector3i(0, -1, 0)))
				if ground == "grass_block" or ground == "snow" or ground == "sand" or ground == "mycelium" or ground == "podzol":
					choice = _pick_passive(biome)
			elif light < 4 and y < 60 and counts.ambient < AMBIENT_CAP and _rng.randf() < 0.1:
				choice = "bat"
		elif world.dim == 1:
			if diff > 0 and counts.hostile < HOSTILE_CAP:
				choice = _pick_nether(biome, p)
		elif world.dim == 2:
			if diff > 0 and counts.hostile < 20 and _rng.randf() < 0.5 and not session.dragon_alive_near(pl.body.pos, 80.0):
				choice = "enderman"
			elif diff > 0 and counts.hostile < 12 and session.dragon_alive_near(pl.body.pos, 120.0) and _rng.randf() < 0.3:
				choice = "enderman"
		if choice == "":
			continue
		var d: Dictionary = MobDB.get_def(choice)
		if d.is_empty():
			continue
		var grp: Array = d.get("group", [1, 1])
		var n := _rng.randi_range(int(grp[0]), int(grp[1]))
		for i in n:
			var sp := Vector3(x + _rng.randf_range(-2, 2), y, z + _rng.randf_range(-2, 2))
			if _space_ok(sp, float(d.get("height", 1.8)), bool(d.get("water", false)) or in_water):
				spawn_natural(world, choice, sp)


func _pick_y(x: int, z: int, py: float) -> int:
	var top := world.top_solid_y(x, z)
	if world.dim == 1:
		# the Nether: random height between floor and ceiling
		for k in 12:
			var y := _rng.randi_range(8, 120)
			if world.get_block(x, y, z) == 0 and world.get_block(x, y + 1, z) == 0 and BlockDB.full[world.get_id(x, y - 1, z)] == 1:
				return y
		return -99999
	if _rng.randf() < 0.5 and world.dim == 0:
		# caves near the player's height
		var y2 := clampi(int(py) + _rng.randi_range(-24, 24), world.min_y + 2, top)
		for k in 16:
			var yy := y2 - k
			if yy < world.min_y + 1:
				break
			if world.get_block(x, yy, z) == 0 and world.get_block(x, yy + 1, z) == 0 and BlockDB.full[world.get_id(x, yy - 1, z)] == 1:
				return yy
	var y3 := top + 1
	if world.get_block(x, y3, z) == 0 or BlockDB.fluid[world.get_id(x, y3, z)] == 1:
		return y3
	return -99999


func _space_ok(p: Vector3, h: float, water: bool) -> bool:
	var x := floori(p.x)
	var y := floori(p.y)
	var z := floori(p.z)
	for i in maxi(1, ceili(h)):
		var id := world.get_id(x, y + i, z)
		if id != 0 and BlockDB.solid[id] == 1:
			return false
		if not water and BlockDB.fluid[id] != 0:
			return false
	if not water and BlockDB.solid[world.get_id(x, y - 1, z)] == 0:
		return false
	return true


func _pick_hostile(biome: String, y: int) -> String:
	var r := _rng.randi_range(0, 515)
	if biome == "mushroom_fields":
		return ""
	if biome == "deep_dark":
		return "" if _rng.randf() < 0.9 else "creeper"
	if biome == "sulfur_caves" and _rng.randf() < 0.35:
		return "sulfur_cube"
	if biome == "pale_garden" and _rng.randf() < 0.2:
		return "creaking"
	if r < 100:
		if biome == "desert" and _rng.randf() < 0.8:
			return "husk"
		return "zombie" if _rng.randf() > 0.05 else "zombie_villager"
	if r < 200:
		if biome in ["snowy_plains", "snowy_taiga", "ice_spikes", "frozen_ocean"] and _rng.randf() < 0.8:
			return "stray"
		if biome in ["swamp", "mangrove_swamp"] and _rng.randf() < 0.5:
			return "bogged"
		if biome == "desert" and _rng.randf() < 0.5:
			return "parched"
		return "skeleton"
	if r < 300:
		return "creeper"
	if r < 400:
		return "spider"
	if r < 410:
		return "enderman"
	if r < 415:
		return "witch"
	if r < 430 and (biome == "swamp" or y < 40):
		return "slime"
	if r < 440 and biome == "lush_caves":
		return "cave_spider" if _rng.randf() < 0.3 else ""
	return "zombie"


func _pick_passive(biome: String) -> String:
	var options := []
	for n in MobDB.order:
		var d: Dictionary = MobDB.get_def(String(n))
		if bool(d.get("hostile", false)) or String(d.get("arch", "")) in ["fish", "ambient", "golem", "copper_golem", "boss_dragon", "boss_wither"]:
			continue
		if int(d.get("dim", 0)) != 0:
			continue
		var biomes: PackedStringArray = PackedStringArray(d.get("biomes", []))
		if biomes.has(biome):
			options.append(n)
	if options.is_empty():
		return ""
	return options[_rng.randi_range(0, options.size() - 1)]


func _pick_water(biome: String) -> String:
	if not (biome.contains("ocean") or biome.contains("river") or biome == "swamp" or biome == "beach"):
		return ""
	var r := _rng.randf()
	if r < 0.3:
		return "squid"
	if r < 0.55:
		return "cod" if not biome.contains("warm") else "tropical_fish"
	if r < 0.7:
		return "salmon" if biome.contains("cold") or biome.contains("river") else "pufferfish"
	if r < 0.8 and biome.contains("ocean") and not biome.contains("frozen"):
		return "dolphin"
	if r < 0.95:
		return "drowned" if world.session.sky_darken() > 6 else ""
	return "glow_squid"


func _pick_nether(biome: String, p: Vector3i) -> String:
	# fortress mobs
	var st = world.session.structures_at(world, p)
	if st == "nether_fortress":
		var r := _rng.randf()
		if r < 0.35:
			return "blaze"
		if r < 0.7:
			return "wither_skeleton"
		if r < 0.85:
			return "magma_cube"
		return "skeleton"
	match biome:
		"crimson_forest":
			var r2 := _rng.randf()
			return "piglin" if r2 < 0.4 else ("hoglin" if r2 < 0.75 else "zombified_piglin")
		"warped_forest":
			return "enderman"
		"soul_sand_valley":
			var r3 := _rng.randf()
			return "skeleton" if r3 < 0.5 else ("ghast" if r3 < 0.7 else "enderman")
		"basalt_deltas":
			return "magma_cube" if _rng.randf() < 0.8 else "ghast"
	var r4 := _rng.randf()
	if r4 < 0.55:
		return "zombified_piglin"
	if r4 < 0.7:
		return "ghast"
	if r4 < 0.85:
		return "magma_cube"
	if r4 < 0.93:
		return "piglin"
	return "enderman"


# ------------------------------------------------------------------------------------------------
# Persistence per chunk
func collect_chunk(c: Chunk) -> void:
	var x0 := c.cx * 16
	var z0 := c.cz * 16
	var saved := []
	for e in list.duplicate():
		if not is_instance_valid(e):
			continue
		var ent: Entity = e
		var p := ent.body.pos
		if p.x >= x0 and p.x < x0 + 16 and p.z >= z0 and p.z < z0 + 16:
			var d := serialize(ent)
			if not d.is_empty():
				saved.append(d)
			list.erase(ent)
			ent.queue_free()
	# unspawned saved entities (chunk never became ready) stay; otherwise the live ones replace them
	if c.state >= Chunk.S_LIT:
		if not saved.is_empty() or c.saved_entities or c.loaded_from_save:
			c.pending_entities = saved
			c.modified = true
			c.saved_entities = not saved.is_empty()
	elif not saved.is_empty():
		c.pending_entities.append_array(saved)
		c.modified = true


## Saveable state of every live entity in `w`, grouped by chunk key (entities stay in the world).
func serialize_by_chunk(w: World) -> Dictionary:
	var out := {}
	for e in list:
		if not is_instance_valid(e) or (e as Node).is_queued_for_deletion():
			continue
		var ent: Entity = e
		if ent.world != w:
			continue
		var d := serialize(ent)
		if d.is_empty():
			continue
		var k := Vector2i(floori(ent.body.pos.x) >> 4, floori(ent.body.pos.z) >> 4)
		if not out.has(k):
			out[k] = []
		(out[k] as Array).append(d)
	return out


func serialize(ent: Entity) -> Dictionary:
	if ent is Mob:
		var m: Mob = ent
		if bool(m.def.get("despawn", true)) and m.data.has("despawn") and not m.tamed:
			return {}
		if m.def.get("boss", false):
			return {}
		return {"t": "mob", "mob": m.mob, "p": [m.body.pos.x, m.body.pos.y, m.body.pos.z], "hp": m.health,
			"baby": m.baby, "tamed": m.tamed, "sheared": m.sheared, "color": m.wool_color, "data": m.data.duplicate()}
	if ent is SimpleEntities.ItemEntity:
		var ie: SimpleEntities.ItemEntity = ent
		if ie.stack == null:
			return {}
		return {"t": "item", "p": [ie.body.pos.x, ie.body.pos.y, ie.body.pos.z], "s": ie.stack.to_dict(), "age": ie.age}
	if ent is SimpleEntities.Vehicle:
		var v: SimpleEntities.Vehicle = ent
		return {"t": "vehicle", "k": v.kind, "w": v.wood, "p": [v.body.pos.x, v.body.pos.y, v.body.pos.z]}
	return {}


func spawn_saved(c: Chunk) -> void:
	for d in c.pending_entities:
		if not (d is Dictionary):
			continue
		var dd: Dictionary = d
		var pa: Array = dd.get("p", [c.cx * 16 + 8, 80, c.cz * 16 + 8])
		var pos := Vector3(float(pa[0]), float(pa[1]), float(pa[2]))
		match String(dd.get("t", "")):
			"mob":
				var extra := {"persistent": true}
				if dd.get("baby", false):
					extra["baby"] = true
				if dd.has("color"):
					extra["color"] = dd["color"]
				var m := spawn_mob(world, String(dd.get("mob", "pig")), pos, extra)
				if m != null:
					m.health = float(dd.get("hp", m.health))
					m.tamed = bool(dd.get("tamed", false))
					m.sheared = bool(dd.get("sheared", false))
					var md: Dictionary = dd.get("data", {})
					for k in md:
						m.data[k] = md[k]
					m.data.erase("despawn")
					ItemExtras.show_name(m)
			"item":
				var st := ItemStack.from_dict(dd.get("s", {}))
				if st != null:
					var e = spawn_item(world, pos, st, Vector3.ZERO)
					if e != null:
						e.age = int(dd.get("age", 0))
			"vehicle":
				spawn_vehicle(world, String(dd.get("k", "boat")), pos, {"wood": dd.get("w", "oak")})
			"structure_mob":
				var extra2 := {"persistent": true}
				var sd: Dictionary = dd.get("data", {})
				for k2 in sd:
					if k2 != "mob":
						extra2[k2] = sd[k2]
				var m2 := spawn_mob(world, String(dd.get("mob", sd.get("mob", "villager"))), pos, extra2)
				if m2 != null:
					m2.data.erase("despawn")
			"end_crystal":
				if world.session != null:
					world.session.spawn_end_crystal(pos)
	c.pending_entities = []


func clear_all(keep_items := false) -> void:
	for e in list.duplicate():
		if keep_items and (e is SimpleEntities.ItemEntity):
			continue
		(e as Node).queue_free()
	if not keep_items:
		list.clear()
