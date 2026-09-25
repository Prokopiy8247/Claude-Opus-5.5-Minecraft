class_name Pathfinder
extends RefCounted
## Bounded A* over the voxel grid for walking mobs (2 blocks tall by default): walking, stepping
## up one block, dropping up to `max_drop` blocks, swimming through water, avoiding hazards.

const DIRS := [Vector3i(1, 0, 0), Vector3i(-1, 0, 0), Vector3i(0, 0, 1), Vector3i(0, 0, -1)]


static func _passable(world: World, x: int, y: int, z: int) -> bool:
	var v := world.get_block(x, y, z)
	if v == 0:
		return true
	var id := v & 0xFFF
	if BlockDB.fluid[id] == 2:
		return false
	var d: BlockDef = BlockDB.defs[id]
	if d.model == BlockDB.M_FIRE or d.name == "sweet_berry_bush" or d.name == "cactus" or d.props.get("web", false):
		return false
	if d.model == BlockDB.M_DOOR:
		return ((v >> 12) & 4) != 0   # open doors are passable
	if d.model == BlockDB.M_FENCE_GATE:
		return ((v >> 12) & 4) != 0
	return BlockDB.solid[id] == 0


static func _floor_ok(world: World, x: int, y: int, z: int) -> bool:
	var v := world.get_block(x, y - 1, z)
	var id := v & 0xFFF
	if BlockDB.fluid[id] == 1:
		return true
	if id == 0 or BlockDB.solid[id] == 0:
		return false
	var d: BlockDef = BlockDB.defs[id]
	if d.name == "magma_block" or d.name == "cactus" or d.model == BlockDB.M_FENCE or d.model == BlockDB.M_WALL:
		return false
	return true


static func walkable(world: World, x: int, y: int, z: int, h: int) -> bool:
	for i in h:
		if not _passable(world, x, y + i, z):
			return false
	return _floor_ok(world, x, y, z) or BlockDB.fluid[world.get_id(x, y, z)] == 1


## Returns world-space waypoints (block centres at feet level) or an empty array. Straight
## stretches are merged (see _smooth) so 4-connected block paths do not turn into zig-zags.
static func find(world: World, from: Vector3, to: Vector3, max_nodes := 300, max_drop := 3, h := 2, width := 0.6) -> PackedVector3Array:
	var start := Vector3i(floori(from.x), floori(from.y + 0.01), floori(from.z))
	var goal := Vector3i(floori(to.x), floori(to.y + 0.01), floori(to.z))
	var out := PackedVector3Array()
	if start == goal:
		return out
	# snap goal down onto the floor
	var g := 0
	while g < 4 and not _floor_ok(world, goal.x, goal.y, goal.z) and BlockDB.fluid[world.get_id(goal.x, goal.y, goal.z)] == 0:
		goal.y -= 1
		g += 1
	var open: Array = [start]
	var came := {}
	var gs := {start: 0.0}
	var fs := {start: _h(start, goal)}
	var closed := {}
	var best := start
	var best_h := _h(start, goal)
	var expanded := 0
	while not open.is_empty() and expanded < max_nodes:
		# pick lowest f (small open lists; linear scan is fine)
		var bi := 0
		var bf := 1e30
		for i in open.size():
			var f: float = fs[open[i]]
			if f < bf:
				bf = f
				bi = i
		var cur: Vector3i = open[bi]
		open.remove_at(bi)
		if cur == goal:
			best = cur
			break
		closed[cur] = true
		expanded += 1
		var hc := _h(cur, goal)
		if hc < best_h:
			best_h = hc
			best = cur
		for dv in DIRS:
			var dir: Vector3i = dv
			var nxt := cur + dir
			var cost := 1.0
			if walkable(world, nxt.x, nxt.y, nxt.z, h):
				pass
			elif walkable(world, nxt.x, nxt.y + 1, nxt.z, h) and _passable(world, cur.x, cur.y + h, cur.z):
				nxt.y += 1
				cost = 1.5
			else:
				var found := false
				if _passable(world, nxt.x, nxt.y, nxt.z) and _passable(world, nxt.x, nxt.y + 1, nxt.z):
					for dy in range(1, max_drop + 1):
						if walkable(world, nxt.x, nxt.y - dy, nxt.z, h):
							nxt.y -= dy
							cost = 1.0 + dy * 0.3
							found = true
							break
				if not found:
					continue
			if closed.has(nxt):
				continue
			if BlockDB.fluid[world.get_id(nxt.x, nxt.y, nxt.z)] == 1:
				cost += 1.0
			var ng: float = float(gs[cur]) + cost
			if not gs.has(nxt) or ng < float(gs[nxt]):
				gs[nxt] = ng
				fs[nxt] = ng + _h(nxt, goal)
				came[nxt] = cur
				if not open.has(nxt):
					open.append(nxt)
	# reconstruct toward the best node reached
	var node := best
	var rev := []
	var guard := 0
	while came.has(node) and guard < 1000:
		rev.append(node)
		node = came[node]
		guard += 1
	rev.reverse()
	for p in rev:
		var pv: Vector3i = p
		out.append(Vector3(pv.x + 0.5, pv.y, pv.z + 0.5))
	return _smooth(world, out, from, width, h)


## String pulling: from the start and from every kept waypoint, skip ahead to the farthest
## waypoint on the same level that can be walked to in a straight line.
static func _smooth(world: World, path: PackedVector3Array, from: Vector3, width: float, h: int) -> PackedVector3Array:
	if path.size() < 2:
		return path
	var out := PackedVector3Array()
	var cur := from
	var i := 0
	while i < path.size():
		var j := i
		while j + 1 < path.size() and _straight(world, cur, path[j + 1], width, h):
			j += 1
		out.append(path[j])
		cur = path[j]
		i = j + 1
	return out


## True when the whole footprint (width) stays on walkable cells along the segment a -> b.
static func _straight(world: World, a: Vector3, b: Vector3, width: float, h: int) -> bool:
	var y := floori(a.y + 0.01)
	if floori(b.y + 0.01) != y:
		return false
	var d := Vector2(b.x - a.x, b.z - a.z)
	var n := ceili(d.length() * 4.0)
	if n > 64:
		return false
	var r := width * 0.5 + 0.05
	for s in n + 1:
		var t := float(s) / float(maxi(n, 1))
		var px := a.x + d.x * t
		var pz := a.z + d.y * t
		if not (walkable(world, floori(px - r), y, floori(pz - r), h) and walkable(world, floori(px + r), y, floori(pz - r), h)
				and walkable(world, floori(px - r), y, floori(pz + r), h) and walkable(world, floori(px + r), y, floori(pz + r), h)):
			return false
	return true


static func _h(a: Vector3i, b: Vector3i) -> float:
	return absf(a.x - b.x) + absf(a.y - b.y) * 1.2 + absf(a.z - b.z)
