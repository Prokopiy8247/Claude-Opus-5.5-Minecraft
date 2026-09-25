class_name Summons
extends RefCounted
## Block-pattern summoning: Wither (T of soul sand/soil + 3 wither skeleton skulls),
## Iron Golem (T of iron blocks + carved pumpkin), Snow Golem (2 snow blocks + pumpkin),
## Copper Golem (copper block + carved pumpkin).

static func _is_soul(world: World, p: Vector3i) -> bool:
	var n := BlockDB.name_of(world.get_blockv(p))
	return n == "soul_sand" or n == "soul_soil"


static func _is_wskull(world: World, p: Vector3i) -> bool:
	return BlockDB.name_of(world.get_blockv(p)) == "wither_skeleton_skull"


static func _clear(world: World, p: Vector3i) -> void:
	world.set_block(p.x, p.y, p.z, 0, World.F_DEFAULT)


static func check_wither(world: World, skull_pos: Vector3i) -> bool:
	if not _is_wskull(world, skull_pos):
		return false
	for axis in [Vector3i(1, 0, 0), Vector3i(0, 0, 1)]:
		var ax: Vector3i = axis
		for idx in [-1, 0, 1]:
			var c: Vector3i = skull_pos - ax * idx + Vector3i(0, -1, 0)   # centre of the soul-sand row
			var stem: Vector3i = c + Vector3i(0, -1, 0)
			if not (_is_soul(world, c) and _is_soul(world, c - ax) and _is_soul(world, c + ax) and _is_soul(world, stem)):
				continue
			var top: Vector3i = c + Vector3i(0, 1, 0)
			if not (_is_wskull(world, top) and _is_wskull(world, top - ax) and _is_wskull(world, top + ax)):
				continue
			for q in [c, c - ax, c + ax, stem, top, top - ax, top + ax]:
				_clear(world, q)
			if world.session != null:
				world.session.entities.spawn_mob(world, "wither", Vector3(stem) + Vector3(0.5, 0.0, 0.5), {"summoned": true})
				world.session.particles.burst(Vector3(c) + Vector3(0.5, 0.5, 0.5), Color(0.2, 0.2, 0.25), 40, 3.0, "large_smoke", 0.4)
			return true
	return false


static func check_golem(world: World, head_pos: Vector3i) -> bool:
	var n1 := BlockDB.name_of(world.get_blockv(head_pos + Vector3i(0, -1, 0)))
	var n2 := BlockDB.name_of(world.get_blockv(head_pos + Vector3i(0, -2, 0)))
	var s = world.session
	# snow golem
	if n1 == "snow_block" and n2 == "snow_block":
		for q in [head_pos, head_pos + Vector3i(0, -1, 0), head_pos + Vector3i(0, -2, 0)]:
			_clear(world, q)
		if s != null:
			s.entities.spawn_mob(world, "snow_golem", Vector3(head_pos) + Vector3(0.5, -2.0, 0.5))
			s.particles.burst(Vector3(head_pos) + Vector3(0.5, -1.0, 0.5), Color(1, 1, 1), 20, 2.0, "snow", 0.15)
		return true
	# copper golem
	if n1.ends_with("copper_block") or n1 == "copper_block":
		_clear(world, head_pos)
		_clear(world, head_pos + Vector3i(0, -1, 0))
		if s != null:
			s.entities.spawn_mob(world, "copper_golem", Vector3(head_pos) + Vector3(0.5, -1.0, 0.5))
		return true
	# iron golem
	if n1 == "iron_block" and n2 == "iron_block":
		for axis in [Vector3i(1, 0, 0), Vector3i(0, 0, 1)]:
			var ax: Vector3i = axis
			var body: Vector3i = head_pos + Vector3i(0, -1, 0)
			if BlockDB.name_of(world.get_blockv(body + ax)) == "iron_block" and BlockDB.name_of(world.get_blockv(body - ax)) == "iron_block":
				var legs: Vector3i = body + Vector3i(0, -1, 0)
				if world.get_blockv(legs + ax) != 0 or world.get_blockv(legs - ax) != 0:
					continue
				for q in [head_pos, body, body + ax, body - ax, legs]:
					_clear(world, q)
				if s != null:
					s.entities.spawn_mob(world, "iron_golem", Vector3(legs) + Vector3(0.5, 0.0, 0.5), {"player_created": true})
				return true
	return false
