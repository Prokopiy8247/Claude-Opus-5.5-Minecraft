class_name Portals
extends RefCounted
## Nether portal frame detection/lighting (any obsidian rectangle with a 2x3 .. 21x21 opening,
## corners optional), frame validation, End portal ring activation and destination portal search.

const MAX_SIZE := 21


static func _obs(world: World, p: Vector3i) -> bool:
	return world.get_id(p.x, p.y, p.z) == BlockDB.id("obsidian")


static func _open(world: World, p: Vector3i) -> bool:
	var v := world.get_block(p.x, p.y, p.z)
	if v == 0:
		return true
	var d: BlockDef = BlockDB.defs[v & 0xFFF]
	return d.model == BlockDB.M_FIRE or d.name == "nether_portal"


## Tries to light a nether portal whose interior contains p. Returns true on success.
static func try_light(world: World, p: Vector3i) -> bool:
	if world.dim == 2:
		return false
	for axis in [0, 1]:
		var shape := find_shape(world, p, axis)
		if shape.is_empty():
			continue
		var origin: Vector3i = shape.origin
		var w: int = shape.w
		var h: int = shape.h
		var ax := Vector3i(1, 0, 0) if axis == 0 else Vector3i(0, 0, 1)
		var pid := BlockDB.id("nether_portal")
		for i in w:
			for j in h:
				var q: Vector3i = origin + ax * i + Vector3i(0, j, 0)
				world.set_block(q.x, q.y, q.z, Vox.make(pid, axis), World.F_URGENT | World.F_DROP_BE)
		Sfx.play_at("portal_trigger", Vector3(origin) + Vector3(0.5, 1.5, 0.5))
		if world.session != null:
			world.session.register_portal(world.dim, origin + ax * (w / 2))
		return true
	return false


## Finds a valid frame. Returns {origin (bottom-left interior), w, h} or {}.
static func find_shape(world: World, p: Vector3i, axis: int) -> Dictionary:
	if not _open(world, p):
		return {}
	var ax := Vector3i(1, 0, 0) if axis == 0 else Vector3i(0, 0, 1)
	# drop to the bottom of the opening
	var q := p
	var guard := 0
	while _open(world, q + Vector3i(0, -1, 0)) and guard < MAX_SIZE:
		q.y -= 1
		guard += 1
	if not _obs(world, q + Vector3i(0, -1, 0)):
		return {}
	# walk to the left edge
	guard = 0
	while _open(world, q - ax) and _obs(world, q - ax + Vector3i(0, -1, 0)) and guard < MAX_SIZE:
		q -= ax
		guard += 1
	if not _obs(world, q - ax):
		return {}
	# width
	var w := 0
	while w < MAX_SIZE + 1 and _open(world, q + ax * w) and _obs(world, q + ax * w + Vector3i(0, -1, 0)):
		w += 1
	if w < 2 or w > MAX_SIZE or not _obs(world, q + ax * w):
		return {}
	# height
	var h := 0
	var ok := true
	while h < MAX_SIZE + 1:
		var row_ok := true
		for i in w:
			if not _open(world, q + ax * i + Vector3i(0, h, 0)):
				row_ok = false
				break
		if not row_ok:
			break
		if not _obs(world, q - ax + Vector3i(0, h, 0)) or not _obs(world, q + ax * w + Vector3i(0, h, 0)):
			ok = false
			break
		h += 1
	if not ok or h < 3 or h > MAX_SIZE:
		return {}
	for i in w:
		if not _obs(world, q + ax * i + Vector3i(0, h, 0)):
			return {}
	return {"origin": q, "w": w, "h": h}


## A portal block is valid while its in-plane neighbours are portal or obsidian.
static func frame_valid(world: World, p: Vector3i, axis: int) -> bool:
	var ax := Vector3i(1, 0, 0) if axis == 0 else Vector3i(0, 0, 1)
	for off in [ax, -ax, Vector3i(0, 1, 0), Vector3i(0, -1, 0)]:
		var n: Vector3i = p + off
		var id := world.get_id(n.x, n.y, n.z)
		if id != BlockDB.id("nether_portal") and id != BlockDB.id("obsidian"):
			return false
	return true


## End portal: after inserting an eye, check the 12-frame ring around any 3x3 centre nearby.
static func check_end_portal(world: World, frame_pos: Vector3i) -> bool:
	var fid := BlockDB.id("end_portal_frame")
	for dx in range(-4, 5):
		for dz in range(-4, 5):
			var c := frame_pos + Vector3i(dx, 0, dz)
			var ring := []
			for i in range(-1, 2):
				ring.append(c + Vector3i(i, 0, -2))
				ring.append(c + Vector3i(i, 0, 2))
				ring.append(c + Vector3i(-2, 0, i))
				ring.append(c + Vector3i(2, 0, i))
			if not ring.has(frame_pos):
				continue
			var all := true
			for r in ring:
				var rv: Vector3i = r
				var v := world.get_block(rv.x, rv.y, rv.z)
				if (v & 0xFFF) != fid or ((v >> 12) & 4) == 0:
					all = false
					break
			if not all:
				continue
			var ep := BlockDB.id("end_portal")
			for i in range(-1, 2):
				for j in range(-1, 2):
					world.set_block(c.x + i, c.y, c.z + j, ep, World.F_URGENT)
			Sfx.play_ui("end_portal_open", 1.0)
			return true
	return false


## Overworld <-> Nether coordinate mapping.
static func map_pos(from_dim: int, to_dim: int, p: Vector3) -> Vector3:
	return DimensionDB.map_position(p, from_dim, to_dim)


## Finds a safe spot for a new portal near target (chunks must be loaded). Returns {pos, axis, platform}.
static func find_portal_site(world: World, target: Vector3i) -> Dictionary:
	var best := {}
	var best_d := 1e30
	var top := world.max_y - 10 if world.dim != 1 else 120
	for r in [0, 4, 8, 12, 16]:
		for dx in range(-r, r + 1, 2):
			for dz in range(-r, r + 1, 2):
				if absi(dx) != r and absi(dz) != r:
					continue
				var x := target.x + dx
				var z := target.z + dz
				if not world.is_ready_at(x, z):
					continue
				var y_start := mini(top, target.y + 20) if world.dim == 1 else mini(top, world.top_solid_y(x, z) + 1)
				var y_end := maxi(world.min_y + 5, target.y - 40) if world.dim == 1 else maxi(world.min_y + 5, world.top_solid_y(x, z) - 1)
				var y := y_start
				while y >= y_end:
					if _site_ok(world, Vector3i(x, y, z), 0):
						var dist := Vector3(dx, y - target.y, dz).length()
						if dist < best_d:
							best_d = dist
							best = {"pos": Vector3i(x, y, z), "axis": 0, "platform": false}
						break
					y -= 1
		if not best.is_empty():
			return best
	var ty := clampi(target.y, 32, top - 8) if world.dim == 1 else clampi(world.top_solid_y(target.x, target.z) + 1, 70, top - 8)
	return {"pos": Vector3i(target.x, ty, target.z), "axis": 0, "platform": true}


static func _site_ok(world: World, p: Vector3i, axis: int) -> bool:
	var ax := Vector3i(1, 0, 0) if axis == 0 else Vector3i(0, 0, 1)
	for i in range(-1, 3):
		var g: Vector3i = p + ax * i + Vector3i(0, -1, 0)
		var gid := world.get_id(g.x, g.y, g.z)
		if BlockDB.full[gid] == 0 or BlockDB.fluid[gid] != 0:
			return false
		for j in 4:
			var q: Vector3i = p + ax * i + Vector3i(0, j, 0)
			if world.get_block(q.x, q.y, q.z) != 0:
				return false
	return true


## Builds a 4x5 obsidian frame with lit portal at p (bottom-left interior block). Returns the arrival point.
static func build_portal(world: World, p: Vector3i, axis: int, platform: bool) -> Vector3:
	var ax := Vector3i(1, 0, 0) if axis == 0 else Vector3i(0, 0, 1)
	var perp := Vector3i(0, 0, 1) if axis == 0 else Vector3i(1, 0, 0)
	var obs := BlockDB.id("obsidian")
	if platform:
		for i in range(-1, 3):
			for k in range(-1, 2):
				var g: Vector3i = p + ax * i + perp * k + Vector3i(0, -1, 0)
				world.set_block(g.x, g.y, g.z, obs, World.F_URGENT)
				for j in 4:
					var q: Vector3i = p + ax * i + perp * k + Vector3i(0, j, 0)
					if k != 0:
						world.set_block(q.x, q.y, q.z, 0, World.F_URGENT)
	for i in range(-1, 3):
		for j in range(-1, 4):
			var q2: Vector3i = p + ax * i + Vector3i(0, j, 0)
			var frame := i == -1 or i == 2 or j == -1 or j == 3
			world.set_block(q2.x, q2.y, q2.z, obs if frame else Vox.make(BlockDB.id("nether_portal"), axis), World.F_URGENT | World.F_DROP_BE)
	if world.session != null:
		world.session.register_portal(world.dim, p)
	return Vector3(p) + Vector3(ax) * 0.5 + Vector3(0.5, 0.0, 0.5)


## Finds an existing lit portal block near p in loaded chunks (radius in blocks).
static func find_existing(world: World, p: Vector3i, radius: int, known: Array) -> Variant:
	var best = null
	var best_d := 1e30
	var pid := BlockDB.id("nether_portal")
	for k in known:
		var kp: Vector3i = k
		if absi(kp.x - p.x) > radius or absi(kp.z - p.z) > radius:
			continue
		if not world.is_loaded(kp.x, kp.z):
			continue
		# walk to the bottom of the portal column
		var q := kp
		if world.get_id(q.x, q.y, q.z) != pid:
			var found := false
			for dy in range(-3, 4):
				for dx in range(-2, 3):
					for dz in range(-2, 3):
						if world.get_id(q.x + dx, q.y + dy, q.z + dz) == pid:
							q = Vector3i(q.x + dx, q.y + dy, q.z + dz)
							found = true
							break
					if found:
						break
				if found:
					break
			if not found:
				continue
		while world.get_id(q.x, q.y - 1, q.z) == pid:
			q.y -= 1
		var dd := Vector3(q - p).length()
		if dd < best_d:
			best_d = dd
			best = q
	return best
