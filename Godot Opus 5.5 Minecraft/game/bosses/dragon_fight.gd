class_name DragonFight
extends RefCounted
## The End boss encounter: the central arena state (dragon alive, crystals on the obsidian pillars),
## crystal spawning/destruction, the arrival point, the exit portal and gateway that open when the
## dragon dies, and the /summon path used by the creative catalogue.

static var started := false
static var defeated := false
static var dragon_ref = null
static var crystals_list: Array = []
static var arena_center := Vector3(0.5, 65.0, 0.5)
static var exit_opened := false
static var gateways_built := false



static func reset() -> void:
	started = false
	defeated = false
	dragon_ref = null
	crystals_list.clear()
	exit_opened = false
	gateways_built = false


## True on the first call for a fresh End world.
static func prepare(world: World) -> void:
	started = true
	if world == null or world.dim != 2:
		return
	crystals_list.clear()
	dragon_ref = null
	arena_center = Vector3(0.5, float(EndGen.PODIUM_Y), 0.5)
	if not defeated:
		_spawn_crystals(world)
		spawn_dragon(world, arena_center + Vector3(0, 30.0, 0))
	else:
		_build_exit_portal(world)


## Where /dimension end puts the player: on the island beside the exit portal.
static func arrival_point(world: World) -> Vector3:
	if world != null:
		var top := world.top_solid_y(0, 6)
		if top > 0:
			return Vector3(0.5, float(top) + 1.0, 6.5)
	return Vector3(0.5, float(EndGen.PODIUM_Y), 6.5)


static func dragon_entity(session):
	if dragon_ref != null and is_instance_valid(dragon_ref):
		return dragon_ref
	if session == null:
		return null
	for e in session.entities.all():
		if e is Mob and (e as Mob).mob == "ender_dragon":
			dragon_ref = e
			return e
	return null


static func crystals() -> Array:
	var out := []
	for c in crystals_list:
		if is_instance_valid(c) and not (c as Node).is_queued_for_deletion():
			out.append(c)
	crystals_list = out
	return out.duplicate()      # callers may destroy crystals while iterating


static func _spawn_crystals(world: World) -> void:
	for sp in EndGen.spikes(world.seed_value):
		spawn_crystal(world, EndGen.spike_crystal_pos(sp))


static func spawn_crystal(world: World, pos: Vector3):
	if world == null or world.session == null:
		return null
	var c := SpecialEntities.EndCrystal.new()
	world.session.entities.add_entity(c)
	c.setup_crystal(world, pos)
	crystals_list.append(c)
	return c


static func crystal_destroyed(c, attacker) -> void:
	crystals_list.erase(c)
	var session = c.session if c != null else null
	if session != null:
		Sfx.play_at("explode_small", c.body.pos, 1.0)
		# a crystal that dies while the dragon perches deals heavy damage
		var d = dragon_entity(session)
		if d != null and (d as Node3D).global_position.distance_to(c.body.pos) < 24.0:
			(d as Mob).hurt(20.0, "explosion", attacker)


static func spawn_dragon(world: World, pos: Vector3):
	if world == null or world.session == null:
		return null
	var m: Mob = world.session.entities.spawn_mob(world, "ender_dragon", pos, {"persistent": true, "summoned": true, "boss": true})
	if m != null:
		dragon_ref = m
		m.data["perch_y"] = 86.0
		Sfx.play_at("dragon_roar", pos, 1.0)
	return m


## Called by Mob.die when the dragon is killed.
static func on_dragon_defeated(session) -> void:
	defeated = true
	dragon_ref = null
	if session == null:
		return
	var world: World = session.world
	Sfx.play_at("dragon_death", session.player.body.pos, 1.0)
	session.chat("The Ender Dragon has been defeated!", Color(1.0, 0.85, 0.4))
	session.entities.spawn_xp(world, arena_center + Vector3(0, 4, 0), 12000)
	# every crystal that is still alive pops as well
	for c in crystals():
		c.queue_free()
	crystals_list.clear()
	_build_exit_portal(world)
	_build_gateways(world)


## Activates the exit portal (portal blocks inside the bedrock rim) and puts the egg on the pillar.
static func _build_exit_portal(world: World) -> void:
	if world == null:
		return
	exit_opened = true
	for e in EndGen.podium_blocks(true):
		var p: Vector3i = e[0]
		world.set_block(p.x, p.y, p.z, int(e[1]), World.F_URGENT)
	world.set_block(0, EndGen.PODIUM_Y + 4, 0, BlockDB.id("dragon_egg"), World.F_URGENT)
	Sfx.play_ui("end_portal_open", 1.0)


## Up to 20 exit gateways spread along the outer ring of the central island.
static func _build_gateways(world: World) -> void:
	if world == null or gateways_built:
		return
	gateways_built = true
	var gw := BlockDB.id("end_gateway")
	var rng := RandomNumberGenerator.new()
	rng.seed = world.seed_value ^ 0x9a71
	var angles := []
	for i in 20:
		angles.append(TAU * float(i) / 20.0 + 0.07)
	var used := 0
	for i in angles.size():
		if used >= 20:
			break
		var ang: float = angles[i]
		var r: float = rng.randf_range(80.0, 116.0)
		var x := int(round(cos(ang) * r))
		var z := int(round(sin(ang) * r))
		var y := world.top_solid_y(x, z)
		if y < 40:
			continue
		world.set_block(x, y + 1, z, gw, World.F_URGENT)
		used += 1
