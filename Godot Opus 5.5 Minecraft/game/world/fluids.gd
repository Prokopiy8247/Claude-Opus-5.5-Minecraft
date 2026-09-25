class_name Fluids
extends RefCounted
## Voxel fluid simulation with Minecraft semantics: meta 0 = source, 1..7 = flowing distance,
## 8+ = falling. Water spreads 7 blocks (lava 3 in the Overworld, 7 in the Nether), prefers the
## shortest path to a drop (slope finding), forms infinite sources, and reacts with lava.

const BREAKABLE_MODELS := [BlockDB.M_CROSS, BlockDB.M_TALL_CROSS, BlockDB.M_CROP, BlockDB.M_TORCH, BlockDB.M_WIRE,
	BlockDB.M_RAIL, BlockDB.M_REPEATER, BlockDB.M_COMPARATOR, BlockDB.M_SNOW, BlockDB.M_FIRE, BlockDB.M_HANGING,
	BlockDB.M_TRIPWIRE, BlockDB.M_COBWEB]


static func delay_for(world: World, kind: int) -> int:
	if kind == 1:
		return 5
	return 10 if world.dim == 1 else 30


static func _kind_at(world: World, x: int, y: int, z: int) -> int:
	var v := world.get_block(x, y, z)
	var id := v & 0xFFF
	var f := BlockDB.fluid[id]
	if f != 0:
		return f
	if BlockDB.waterlogged[id] == 1:
		return 1
	return 0


## Effective level of a neighbour of the same kind: 0 = source, 1..7 flowing, falling counts as 0 for spreading.
static func _level_of(v: int) -> int:
	var id := v & 0xFFF
	if BlockDB.waterlogged[id] == 1 and BlockDB.fluid[id] == 0:
		return 0
	var m := (v >> 12) & 15
	return 0 if m >= 8 else m


static func _is_source(v: int) -> bool:
	var id := v & 0xFFF
	if BlockDB.fluid[id] == 0:
		return BlockDB.waterlogged[id] == 1
	return ((v >> 12) & 15) == 0


## Can fluid of `kind` flow into the block v at a position?
static func can_replace(v: int, kind: int) -> bool:
	if v == 0:
		return true
	var id := v & 0xFFF
	var f := BlockDB.fluid[id]
	if f != 0:
		return f == kind and ((v >> 12) & 15) != 0
	if BlockDB.waterlogged[id] == 1:
		return false
	if BlockDB.replaceable[id] == 1:
		return true
	return BlockDB.model[id] in BREAKABLE_MODELS


static func tick(world: World, x: int, y: int, z: int, v: int) -> void:
	var id := v & 0xFFF
	var kind := BlockDB.fluid[id]
	if kind == 0:
		return
	var meta := (v >> 12) & 15
	var nether := world.dim == 1
	var step := 1 if (kind == 1 or nether) else 2
	var delay := delay_for(world, kind)
	# lava touching water hardens
	if kind == 2 and _lava_hardens(world, x, y, z, meta):
		return
	# 1) re-evaluate non-source levels
	if meta != 0:
		var nm := _compute(world, x, y, z, kind, step)
		if nm < 0:
			world.set_block(x, y, z, 0)
			return
		if nm != meta:
			world.set_block(x, y, z, Vox.make(id, nm))
			world.schedule_tick(x, y, z, delay)
			return
	# 2) flow down
	var below := world.get_block(x, y - 1, z)
	var bid := below & 0xFFF
	if y - 1 >= world.min_y and kind == 2 and BlockDB.fluid[bid] == 1:
		world.set_block(x, y - 1, z, BlockDB.id("stone"))
		_fizz(world, Vector3(x, y - 1, z))
		return
	if y - 1 >= world.min_y and can_replace(below, kind) and not (BlockDB.fluid[bid] == kind and ((below >> 12) & 15) >= 8):
		_flow_into(world, x, y - 1, z, below, Vox.make(id, 8), kind)
		if meta != 0:
			return
		# sources above a drop still spread a little when surrounded
		if _count_sources(world, x, y, z, kind) < 3:
			return
	elif y - 1 >= world.min_y and BlockDB.fluid[bid] == kind and ((below >> 12) & 15) >= 8 and meta != 0:
		return
	# 3) spread sideways
	var level := 0 if meta >= 8 else meta
	var next := level + step
	if next > 7:
		return
	if meta >= 8 and BlockDB.fluid[bid] == kind:
		return
	var dirs := _flow_dirs(world, x, y, z, kind, nether)
	for d in dirs:
		var hv: Vector3i = Vox.DIR_VEC[d]
		var nx := x + hv.x
		var nz := z + hv.z
		var nv := world.get_block(nx, y, nz)
		var nid := nv & 0xFFF
		if kind == 1 and BlockDB.fluid[nid] == 2:
			world.set_block(nx, y, nz, BlockDB.id("obsidian") if ((nv >> 12) & 15) == 0 else BlockDB.id("cobblestone"))
			_fizz(world, Vector3(nx, y, nz))
			continue
		if not can_replace(nv, kind):
			continue
		if BlockDB.fluid[nid] == kind:
			var cur := (nv >> 12) & 15
			if cur != 0 and cur < 8 and cur <= next:
				continue
		_flow_into(world, nx, y, nz, nv, Vox.make(id, next), kind)


static func _compute(world: World, x: int, y: int, z: int, kind: int, step: int) -> int:
	var above := world.get_block(x, y + 1, z)
	if BlockDB.fluid[above & 0xFFF] == kind or (kind == 1 and BlockDB.waterlogged[above & 0xFFF] == 1):
		return 8
	var best := 99
	var sources := 0
	for d in [Vox.EAST, Vox.WEST, Vox.SOUTH, Vox.NORTH]:
		var hv: Vector3i = Vox.DIR_VEC[d]
		var nv := world.get_block(x + hv.x, y, z + hv.z)
		var nid := nv & 0xFFF
		var same := BlockDB.fluid[nid] == kind or (kind == 1 and BlockDB.waterlogged[nid] == 1)
		if not same:
			continue
		if _is_source(nv):
			sources += 1
		best = mini(best, _level_of(nv))
	if kind == 1 and sources >= 2:
		var below := world.get_block(x, y - 1, z)
		var bid := below & 0xFFF
		if BlockDB.solid[bid] == 1 or (BlockDB.fluid[bid] == 1 and ((below >> 12) & 15) == 0):
			return 0
	if best == 99:
		return -1
	var nl := best + step
	if nl > 7:
		return -1
	return nl


static func _count_sources(world: World, x: int, y: int, z: int, kind: int) -> int:
	var n := 0
	for d in [Vox.EAST, Vox.WEST, Vox.SOUTH, Vox.NORTH]:
		var hv: Vector3i = Vox.DIR_VEC[d]
		var nv := world.get_block(x + hv.x, y, z + hv.z)
		if BlockDB.fluid[nv & 0xFFF] == kind and _is_source(nv):
			n += 1
	return n


static func _flow_into(world: World, x: int, y: int, z: int, old: int, nv: int, kind: int) -> void:
	var oid := old & 0xFFF
	if old != 0 and BlockDB.fluid[oid] == 0:
		if kind == 1:
			BlockBehaviors.break_naturally(world, Vector3i(x, y, z), old)
		else:
			_fizz(world, Vector3(x, y, z))
	world.set_block(x, y, z, nv)
	world.schedule_tick(x, y, z, delay_for(world, kind))


## Directions (horizontal) to spread toward: those with the shortest path to a hole.
static func _flow_dirs(world: World, x: int, y: int, z: int, kind: int, nether: bool) -> Array:
	var max_d := 4 if (kind == 1 or nether) else 2
	var best := 1000
	var out := []
	for d in [Vox.EAST, Vox.WEST, Vox.SOUTH, Vox.NORTH]:
		var hv: Vector3i = Vox.DIR_VEC[d]
		var nx := x + hv.x
		var nz := z + hv.z
		var nv := world.get_block(nx, y, nz)
		if not _passable(nv, kind):
			if kind == 1 and BlockDB.fluid[nv & 0xFFF] == 2:
				out.append(d)
			continue
		var dist := 1000
		if _passable(world.get_block(nx, y - 1, nz), kind):
			dist = 0
		else:
			dist = _slope(world, nx, y, nz, 1, Vox.OPPOSITE[d], kind, max_d)
		if dist < best:
			best = dist
			out = [d]
		elif dist == best:
			out.append(d)
	return out


static func _passable(v: int, kind: int) -> bool:
	if v == 0:
		return true
	var id := v & 0xFFF
	if BlockDB.fluid[id] == kind:
		return ((v >> 12) & 15) != 0
	if BlockDB.fluid[id] != 0 or BlockDB.waterlogged[id] == 1:
		return false
	return BlockDB.replaceable[id] == 1 or BlockDB.model[id] in BREAKABLE_MODELS


static func _slope(world: World, x: int, y: int, z: int, depth: int, from_dir: int, kind: int, max_d: int) -> int:
	var best := 1000
	for d in [Vox.EAST, Vox.WEST, Vox.SOUTH, Vox.NORTH]:
		if d == from_dir:
			continue
		var hv: Vector3i = Vox.DIR_VEC[d]
		var nx := x + hv.x
		var nz := z + hv.z
		if not _passable(world.get_block(nx, y, nz), kind):
			continue
		if _passable(world.get_block(nx, y - 1, nz), kind):
			return depth
		if depth < max_d:
			best = mini(best, _slope(world, nx, y, nz, depth + 1, Vox.OPPOSITE[d], kind, max_d))
	return best


static func _lava_hardens(world: World, x: int, y: int, z: int, meta: int) -> bool:
	for d in [Vox.EAST, Vox.WEST, Vox.UP, Vox.SOUTH, Vox.NORTH]:
		var n := world.get_block(x + Vox.DIR_X[d], y + Vox.DIR_Y[d], z + Vox.DIR_Z[d])
		if BlockDB.fluid[n & 0xFFF] == 1 or BlockDB.waterlogged[n & 0xFFF] == 1:
			world.set_block(x, y, z, BlockDB.id("obsidian") if meta == 0 else BlockDB.id("cobblestone"))
			_fizz(world, Vector3(x, y, z))
			return true
	# basalt generator: lava above soul soil next to blue ice
	if world.get_id(x, y - 1, z) == BlockDB.id("soul_soil"):
		for d in [Vox.EAST, Vox.WEST, Vox.SOUTH, Vox.NORTH]:
			if world.get_id(x + Vox.DIR_X[d], y, z + Vox.DIR_Z[d]) == BlockDB.id("blue_ice"):
				world.set_block(x, y, z, BlockDB.id("basalt"))
				_fizz(world, Vector3(x, y, z))
				return true
	return false


static func _fizz(world: World, p: Vector3) -> void:
	Sfx.play_at("fizz", p + Vector3(0.5, 0.5, 0.5), 0.5)
	if world.session != null:
		world.session.particles.smoke(p + Vector3(0.5, 1.0, 0.5), 8, true)


## Places a fluid source from a bucket. Returns false when not possible (e.g. water in the Nether evaporates).
static func place_source(world: World, p: Vector3i, kind: int) -> bool:
	var v := world.get_block(p.x, p.y, p.z)
	var id := v & 0xFFF
	if kind == 1 and world.dim == 1:
		Sfx.play_at("fizz", Vector3(p) + Vector3(0.5, 0.5, 0.5))
		if world.session != null:
			world.session.particles.smoke(Vector3(p) + Vector3(0.5, 0.5, 0.5), 10, true)
		return true
	var d: BlockDef = BlockDB.defs[id]
	if kind == 1 and d.props.has("waterloggable") and BlockDB.fluid[id] == 0:
		return false
	if v != 0 and not can_replace(v, kind) and not (BlockDB.fluid[id] != 0):
		return false
	if v != 0 and BlockDB.fluid[id] == 0:
		BlockBehaviors.break_naturally(world, p, v)
	world.set_block(p.x, p.y, p.z, BlockDB.WATER if kind == 1 else BlockDB.LAVA)
	world.schedule_tick(p.x, p.y, p.z, delay_for(world, kind))
	return true
