class_name Explosions
extends RefCounted
## Java-style explosions: 16x16x16 ray grid with per-block blast resistance attenuation, entity
## exposure sampling, knockback, chain-reacting TNT, drops at 1/power, optional fire.

static var _rays: PackedVector3Array = PackedVector3Array()


static func _ensure_rays() -> void:
	if not _rays.is_empty():
		return
	for x in 16:
		for y in 16:
			for z in 16:
				if x == 0 or x == 15 or y == 0 or y == 15 or z == 0 or z == 15:
					var d := Vector3(x / 15.0 * 2.0 - 1.0, y / 15.0 * 2.0 - 1.0, z / 15.0 * 2.0 - 1.0)
					_rays.append(d.normalized())


## Spawns a primed TNT entity at a block position (fuse in ticks).
static func prime_tnt(world: World, block_pos: Vector3, fuse: int = 80, igniter = null) -> void:
	if world.session == null:
		return
	world.session.entities.spawn_tnt(world, block_pos + Vector3(0.5, 0.0, 0.5), fuse, igniter)
	Sfx.play_at("fuse", block_pos + Vector3(0.5, 0.5, 0.5))


## Main explosion entry point.
static func explode(world: World, center: Vector3, power: float, fire := false, source = null, destroy := true,
		drop_all := false) -> void:
	_ensure_rays()
	var rng := RandomNumberGenerator.new()
	rng.randomize()
	var session = world.session
	var mob_griefing := true
	if session != null:
		mob_griefing = bool(session.gamerule("mobGriefing", true))
		if source != null and source.has_method("is_mob") and source.is_mob() and not mob_griefing:
			destroy = false
	var in_fluid := BlockDB.fluid[world.get_id(floori(center.x), floori(center.y), floori(center.z))] != 0
	var affected := {}
	if destroy and not in_fluid:
		var cache := {}
		for r in _rays:
			var ray: Vector3 = r
			var intensity := power * (0.7 + rng.randf() * 0.6)
			var p := center
			while intensity > 0.0:
				var bp := Vector3i(floori(p.x), floori(p.y), floori(p.z))
				var v: int
				if cache.has(bp):
					v = cache[bp]
				else:
					v = world.get_block(bp.x, bp.y, bp.z)
					cache[bp] = v
				if v != 0:
					var d: BlockDef = BlockDB.defs[v & 0xFFF]
					var res := d.resistance
					if BlockDB.fluid[v & 0xFFF] != 0:
						res = 100.0
					intensity -= (res + 0.3) * 0.3
					if intensity > 0.0 and d.hardness >= 0.0 and BlockDB.fluid[v & 0xFFF] == 0:
						affected[bp] = v
				intensity -= 0.22500001
				p += ray * 0.3
	# entities
	if session != null:
		session.entities.explosion_impact(world, center, power, source)
		session.explosion_player_impact(world, center, power, source)
	# blocks
	var removed := []
	var drop_chance := 1.0 if drop_all else 1.0 / power
	for bp in affected:
		var pos: Vector3i = bp
		var v: int = affected[bp]
		var d: BlockDef = BlockDB.defs[v & 0xFFF]
		if d.props.get("tnt", false):
			world.set_block(pos.x, pos.y, pos.z, 0, World.F_URGENT | World.F_DROP_BE)
			prime_tnt(world, Vector3(pos), rng.randi_range(10, 30), source)
			continue
		var be := world.get_be(pos.x, pos.y, pos.z).duplicate(true)
		world.set_block(pos.x, pos.y, pos.z, 0, World.F_DROP_BE)
		removed.append(pos)
		if session != null and rng.randf() < drop_chance and bool(session.gamerule("doTileDrops", true)):
			Loot.drop_block(world, pos, v, null, null, be, true)
		elif session != null and be.has("items"):
			for it in be["items"]:
				if it is Dictionary and not (it as Dictionary).is_empty():
					var st := ItemStack.from_dict(it)
					if st != null:
						session.entities.spawn_item(world, Vector3(pos) + Vector3(0.5, 0.5, 0.5), st)
	# neighbour notifications at the crater edge (fluids flow in, sand falls, torches pop)
	var edge := {}
	for pos in removed:
		var pv: Vector3i = pos
		for dd in 6:
			var n: Vector3i = pv + Vox.DIR_VEC[dd]
			if not affected.has(n):
				edge[n] = true
	for n in edge:
		var nv: Vector3i = n
		var bv := world.get_block(nv.x, nv.y, nv.z)
		if bv != 0:
			BlockBehaviors.neighbor_changed(world, nv.x, nv.y, nv.z, bv, Vox.UP)
	if fire:
		for pos in removed:
			var pv2: Vector3i = pos
			if rng.randf() < 0.33 and world.get_block(pv2.x, pv2.y, pv2.z) == 0 and BlockDB.full[world.get_id(pv2.x, pv2.y - 1, pv2.z)] == 1:
				world.set_block(pv2.x, pv2.y, pv2.z, BlockDB.id("fire"))
	# effects
	if session != null:
		session.particles.explosion(center, power)
		session.camera_shake(center, power)
	Sfx.play_at("explode", center, 1.0 + power * 0.1, rng.randf_range(0.8, 1.0))


## Fraction of sample points of a box that are directly exposed to the explosion centre.
static func exposure(world: World, center: Vector3, box: AABB) -> float:
	var hits := 0
	var total := 0
	var steps := 2
	for ix in steps + 1:
		for iy in steps + 1:
			for iz in steps + 1:
				var sp := box.position + Vector3(box.size.x * ix / steps, box.size.y * iy / steps, box.size.z * iz / steps)
				total += 1
				if _clear_line(world, sp, center):
					hits += 1
	return float(hits) / float(maxi(total, 1))


static func _clear_line(world: World, a: Vector3, b: Vector3) -> bool:
	var d := b - a
	var dist := d.length()
	if dist < 0.01:
		return true
	var n := int(ceil(dist / 0.4))
	for i in n:
		var p := a + d * (float(i) / n)
		var id := world.get_id(floori(p.x), floori(p.y), floori(p.z))
		if BlockDB.full[id] == 1 and BlockDB.solid[id] == 1:
			return false
	return true


## Damage formula shared by entities and the player.
static func damage_for(center: Vector3, pos: Vector3, power: float, expo: float) -> float:
	var dist := pos.distance_to(center)
	var impact := (1.0 - dist / (power * 2.0)) * expo
	if impact <= 0.0:
		return 0.0
	return floorf((impact * impact + impact) / 2.0 * 7.0 * power * 2.0 + 1.0)


static func knockback_for(center: Vector3, pos: Vector3, power: float, expo: float) -> Vector3:
	var dist := pos.distance_to(center)
	var impact := (1.0 - dist / (power * 2.0)) * expo
	if impact <= 0.0:
		return Vector3.ZERO
	var dir := (pos - center)
	if dir.length() < 0.001:
		dir = Vector3.UP
	return dir.normalized() * impact
