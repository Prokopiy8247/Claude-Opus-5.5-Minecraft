class_name Fire
extends RefCounted
## Fire aging, spreading and block burning (encouragement / flammability tables by material),
## rain extinguishing, infinite fire on netherrack / magma / soul blocks, lava ignition.

static var _rng := RandomNumberGenerator.new()
static var _burn_cache := {}


## [encouragement (catch chance), flammability (burn-away chance)] for a block.
static func burn_odds(id: int) -> Vector2i:
	if _burn_cache.has(id):
		return _burn_cache[id]
	var d: BlockDef = BlockDB.defs[id]
	var r := Vector2i.ZERO
	if d.flammable or d.tags.has("logs") or d.tags.has("planks") or d.tags.has("leaves") or d.tags.has("wool"):
		var n := d.name
		if d.tags.has("leaves") or d.model == BlockDB.M_LEAVES:
			r = Vector2i(30, 60)
		elif d.tags.has("wool") or n.ends_with("_carpet"):
			r = Vector2i(30, 60)
		elif d.tags.has("logs") or n.ends_with("_log") or n.ends_with("_wood"):
			r = Vector2i(5, 5)
		elif n == "tnt":
			r = Vector2i(15, 100)
		elif n in ["bookshelf", "chiseled_bookshelf"]:
			r = Vector2i(30, 20)
		elif d.model in [BlockDB.M_CROSS, BlockDB.M_TALL_CROSS, BlockDB.M_VINE, BlockDB.M_SHORT]:
			r = Vector2i(60, 100)
		elif n in ["hay_block", "dried_kelp_block", "target"]:
			r = Vector2i(60, 20)
		elif n.contains("bamboo") or n == "scaffolding":
			r = Vector2i(60, 60)
		else:
			r = Vector2i(5, 20)
	if d.name.begins_with("crimson") or d.name.begins_with("warped"):
		r = Vector2i.ZERO
	_burn_cache[id] = r
	return r


static func _infiniburn(id: int) -> bool:
	var d: BlockDef = BlockDB.defs[id]
	return d.props.get("infiniburn", false) or d.name in ["netherrack", "magma_block", "soul_sand", "soul_soil", "bedrock"]


static func tick(world: World, x: int, y: int, z: int, v: int) -> void:
	var id := v & 0xFFF
	var meta := (v >> 12) & 15
	var below_id := world.get_id(x, y - 1, z)
	var infinite := _infiniburn(below_id)
	var session = world.session
	if session != null and not bool(session.gamerule("doFireTick", true)):
		return
	# rain puts fires out
	if not infinite and session != null and session.is_raining_at(world, x, y, z) and _rng.randf() < 0.2 + meta * 0.03:
		world.set_block(x, y, z, 0)
		return
	var soul: bool = BlockDB.defs[id].name == "soul_fire"
	if soul:
		if not (BlockDB.defs[below_id].name in ["soul_sand", "soul_soil"]):
			world.set_block(x, y, z, 0)
		return
	var age := mini(15, meta + _rng.randi_range(0, 2) / 2)
	if age != meta:
		world.set_block(x, y, z, Vox.make(id, age), 0)
	if not infinite:
		if not _has_fuel(world, x, y, z):
			if BlockDB.solid[below_id] == 0 or age > 3:
				world.set_block(x, y, z, 0)
			return
		if age == 15 and burn_odds(below_id).x == 0 and _rng.randi_range(0, 3) == 0:
			world.set_block(x, y, z, 0)
			return
	# burn neighbours
	for d in 6:
		var chance := 300 if (d == Vox.UP or d == Vox.DOWN) else 250
		_try_burn(world, x + Vox.DIR_X[d], y + Vox.DIR_Y[d], z + Vox.DIR_Z[d], chance, age)
	# spread into nearby air
	for dx in range(-1, 2):
		for dz in range(-1, 2):
			for dy in range(-1, 5):
				if dx == 0 and dy == 0 and dz == 0:
					continue
				var tx := x + dx
				var ty := y + dy
				var tz := z + dz
				if world.get_block(tx, ty, tz) != 0:
					continue
				var enc := _encouragement(world, tx, ty, tz)
				if enc <= 0:
					continue
				var diff := 100 + (dy - 1) * 100 if dy > 1 else 100
				var odds := (enc + 40 + 7 * 2) / (age + 30)
				if odds > 0 and _rng.randi_range(0, diff - 1) <= odds:
					world.set_block(tx, ty, tz, Vox.make(id, mini(15, age + _rng.randi_range(0, 4) / 3)))


static func _has_fuel(world: World, x: int, y: int, z: int) -> bool:
	for d in 6:
		if burn_odds(world.get_id(x + Vox.DIR_X[d], y + Vox.DIR_Y[d], z + Vox.DIR_Z[d])).x > 0:
			return true
	return false


static func _encouragement(world: World, x: int, y: int, z: int) -> int:
	var best := 0
	for d in 6:
		best = maxi(best, burn_odds(world.get_id(x + Vox.DIR_X[d], y + Vox.DIR_Y[d], z + Vox.DIR_Z[d])).x)
	return best


static func _try_burn(world: World, x: int, y: int, z: int, chance: int, age: int) -> void:
	var v := world.get_block(x, y, z)
	if v == 0:
		return
	var odds := burn_odds(v & 0xFFF)
	if odds.y <= 0:
		return
	if _rng.randi_range(0, chance - 1) < odds.y:
		var d: BlockDef = BlockDB.defs[v & 0xFFF]
		if d.props.get("tnt", false):
			world.set_block(x, y, z, 0)
			Explosions.prime_tnt(world, Vector3(x, y, z), 80)
			return
		if _rng.randi_range(0, age + 9) < 5:
			world.set_block(x, y, z, Vox.make(BlockDB.id("fire"), mini(15, age + _rng.randi_range(0, 4) / 3)))
		else:
			world.set_block(x, y, z, 0)


## Lava random tick: ignite flammable surroundings (Minecraft lava fire spread).
static func lava_spread(world: World, x: int, y: int, z: int) -> void:
	if world.session != null and not bool(world.session.gamerule("doFireTick", true)):
		return
	var n := _rng.randi_range(0, 2)
	var px := x
	var py := y
	var pz := z
	if n > 0:
		for i in n:
			px += _rng.randi_range(-1, 1)
			py += 1
			pz += _rng.randi_range(-1, 1)
			var v := world.get_block(px, py, pz)
			if v == 0:
				if _encouragement(world, px, py, pz) > 0:
					world.set_block(px, py, pz, BlockDB.id("fire"))
					return
			elif BlockDB.solid[v & 0xFFF] == 1:
				return
	else:
		for i in 3:
			var tx := x + _rng.randi_range(-1, 1)
			var tz := z + _rng.randi_range(-1, 1)
			if world.get_block(tx, y + 1, tz) == 0 and burn_odds(world.get_id(tx, y, tz)).x > 0:
				world.set_block(tx, y + 1, tz, BlockDB.id("fire"))


## Places fire (flint and steel, fire charge). Also lights nether portals.
static func ignite(world: World, p: Vector3i, igniter = null) -> bool:
	if world.get_block(p.x, p.y, p.z) != 0:
		return false
	if Portals.try_light(world, p):
		return true
	var below := world.get_id(p.x, p.y - 1, p.z)
	var fire_id := BlockDB.id("soul_fire") if BlockDB.defs[below].name in ["soul_sand", "soul_soil"] else BlockDB.id("fire")
	if BlockDB.solid[below] == 0 and _encouragement(world, p.x, p.y, p.z) == 0:
		return false
	world.set_block(p.x, p.y, p.z, fire_id)
	return true
