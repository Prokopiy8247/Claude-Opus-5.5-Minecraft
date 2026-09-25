class_name VoxelBody
extends RefCounted
## Minecraft-style axis-aligned bounding box physics against voxel collision shapes.
## Units: blocks and blocks-per-tick (20 ticks per second), exactly like the original game.

var pos := Vector3.ZERO          # feet centre
var vel := Vector3.ZERO          # blocks / tick
var width := 0.6
var height := 1.8
var step_height := 0.6
var on_ground := false
var collided_h := false
var collided_v := false
var collided_up := false
var fall_distance := 0.0
var no_clip := false
var _boxes: Array = []

const EPS := 1e-7


func aabb() -> AABB:
	var hw := width * 0.5
	return AABB(Vector3(pos.x - hw, pos.y, pos.z - hw), Vector3(width, height, width))


func aabb_at(p: Vector3) -> AABB:
	var hw := width * 0.5
	return AABB(Vector3(p.x - hw, p.y, p.z - hw), Vector3(width, height, width))


static func _clip_y(box: AABB, other: AABB, dy: float) -> float:
	if other.end.x <= box.position.x + EPS or other.position.x >= box.end.x - EPS:
		return dy
	if other.end.z <= box.position.z + EPS or other.position.z >= box.end.z - EPS:
		return dy
	if dy > 0.0 and other.position.y >= box.end.y - EPS:
		dy = minf(dy, other.position.y - box.end.y)
	elif dy < 0.0 and other.end.y <= box.position.y + EPS:
		dy = maxf(dy, other.end.y - box.position.y)
	return dy


static func _clip_x(box: AABB, other: AABB, dx: float) -> float:
	if other.end.y <= box.position.y + EPS or other.position.y >= box.end.y - EPS:
		return dx
	if other.end.z <= box.position.z + EPS or other.position.z >= box.end.z - EPS:
		return dx
	if dx > 0.0 and other.position.x >= box.end.x - EPS:
		dx = minf(dx, other.position.x - box.end.x)
	elif dx < 0.0 and other.end.x <= box.position.x + EPS:
		dx = maxf(dx, other.end.x - box.position.x)
	return dx


static func _clip_z(box: AABB, other: AABB, dz: float) -> float:
	if other.end.y <= box.position.y + EPS or other.position.y >= box.end.y - EPS:
		return dz
	if other.end.x <= box.position.x + EPS or other.position.x >= box.end.x - EPS:
		return dz
	if dz > 0.0 and other.position.z >= box.end.z - EPS:
		dz = minf(dz, other.position.z - box.end.z)
	elif dz < 0.0 and other.end.z <= box.position.z + EPS:
		dz = maxf(dz, other.end.z - box.position.z)
	return dz


func _gather(world, box: AABB, motion: Vector3) -> void:
	_boxes.clear()
	var area := box.merge(AABB(box.position + motion, box.size)).grow(0.001)
	world.collect_boxes(area, _boxes)


func is_free(world, box: AABB) -> bool:
	var tmp := []
	world.collect_boxes(box.grow(-0.001), tmp)
	for b in tmp:
		if (b as AABB).intersects(box.grow(-0.001)):
			return false
	return true


func _sweep(box: AABB, m: Vector3) -> Vector3:
	var dy := m.y
	for b in _boxes:
		dy = _clip_y(box, b, dy)
	box.position.y += dy
	var dx := m.x
	var dz := m.z
	if absf(dz) > absf(dx):
		for b in _boxes:
			dz = _clip_z(box, b, dz)
		box.position.z += dz
		for b in _boxes:
			dx = _clip_x(box, b, dx)
		box.position.x += dx
	else:
		for b in _boxes:
			dx = _clip_x(box, b, dx)
		box.position.x += dx
		for b in _boxes:
			dz = _clip_z(box, b, dz)
		box.position.z += dz
	return Vector3(dx, dy, dz)


## Moves by `motion` (blocks) with collisions, step-up and optional sneak edge protection.
func move(world, motion: Vector3, sneak_edge := false) -> void:
	if no_clip:
		pos += motion
		return
	var box := aabb()
	if sneak_edge and on_ground:
		motion = _back_off_edge(world, box, motion)
	_gather(world, box, motion + Vector3(0, step_height if motion.y <= 0.0 else 0.0, 0))
	var moved := _sweep(box, motion)
	# step up (only when grounded or landing this tick)
	var grounded_now := on_ground or (motion.y < 0.0 and absf(moved.y - motion.y) > EPS)
	if step_height > 0.0 and grounded_now and (absf(moved.x - motion.x) > EPS or absf(moved.z - motion.z) > EPS):
		var up := _sweep(box, Vector3(0, step_height, 0))
		var box2 := box
		box2.position.y += up.y
		var hmove := _sweep(box2, Vector3(motion.x, 0, motion.z))
		box2.position += Vector3(hmove.x, 0, hmove.z)
		var down := _sweep(box2, Vector3(0, -(up.y) + minf(motion.y, 0.0), 0))
		var stepped := Vector3(hmove.x, up.y + down.y, hmove.z)
		if Vector2(stepped.x, stepped.z).length_squared() > Vector2(moved.x, moved.z).length_squared() + EPS:
			moved = stepped
	collided_h = absf(moved.x - motion.x) > EPS or absf(moved.z - motion.z) > EPS
	collided_v = absf(moved.y - motion.y) > EPS
	collided_up = collided_v and motion.y > 0.0
	on_ground = collided_v and motion.y < 0.0
	pos += moved
	if on_ground:
		vel.y = 0.0
	elif collided_up:
		vel.y = 0.0
	if absf(moved.x - motion.x) > EPS:
		vel.x = 0.0
	if absf(moved.z - motion.z) > EPS:
		vel.z = 0.0
	if moved.y < 0.0:
		fall_distance -= moved.y


func _back_off_edge(world, box: AABB, m: Vector3) -> Vector3:
	var dx := m.x
	var dz := m.z
	var step := 0.05
	var probe := func(ox: float, oz: float) -> bool:
		var b := box
		b.position += Vector3(ox, -step_height, oz)
		return is_free(world, b)
	while dx != 0.0 and probe.call(dx, 0.0):
		if absf(dx) < step:
			dx = 0.0
		else:
			dx -= step * signf(dx)
	while dz != 0.0 and probe.call(0.0, dz):
		if absf(dz) < step:
			dz = 0.0
		else:
			dz -= step * signf(dz)
	while dx != 0.0 and dz != 0.0 and probe.call(dx, dz):
		if absf(dx) < step:
			dx = 0.0
		else:
			dx -= step * signf(dx)
		if absf(dz) < step:
			dz = 0.0
		else:
			dz -= step * signf(dz)
	return Vector3(dx, m.y, dz)


## Fluid / climbable state of the body. Returns {water, lava, eyes_water, eyes_lava, climb, powder, web}.
func sample_env(world, eye: float) -> Dictionary:
	var box := aabb().grow(-0.001)
	var res := {"water": false, "lava": false, "eyes_water": false, "eyes_lava": false, "climb": false, "web": false,
		"bubble": 0, "water_depth": 0.0}
	var x0 := floori(box.position.x)
	var x1 := floori(box.end.x)
	var y0 := floori(box.position.y)
	var y1 := floori(box.end.y)
	var z0 := floori(box.position.z)
	var z1 := floori(box.end.z)
	var fluid := BlockDB.fluid
	var wl := BlockDB.waterlogged
	for y in range(y0, y1 + 1):
		for z in range(z0, z1 + 1):
			for x in range(x0, x1 + 1):
				var v: int = world.get_block(x, y, z)
				if v == 0:
					continue
				var id := v & 0xFFF
				var f := fluid[id]
				if f != 0 or wl[id] == 1:
					var top := float(y) + MeshModels.fluid_height(v)
					if box.position.y < top:
						if f == 2:
							res.lava = true
						else:
							res.water = true
							res.water_depth = maxf(res.water_depth, top - box.position.y)
				elif BlockDB.defs[id].props.get("web", false):
					res.web = true
	var ey := pos.y + eye
	var ev: int = world.get_block(floori(pos.x), floori(ey), floori(pos.z))
	var eid := ev & 0xFFF
	if ev != 0:
		var top2 := floorf(ey) + MeshModels.fluid_height(ev)
		if (fluid[eid] == 1 or wl[eid] == 1) and ey < top2:
			res.eyes_water = true
		elif fluid[eid] == 2 and ey < top2:
			res.eyes_lava = true
	var fv: int = world.get_block(floori(pos.x), floori(pos.y + 0.05), floori(pos.z))
	var fid := fv & 0xFFF
	if fv != 0 and BlockDB.defs[fid].climb:
		res.climb = true
	return res
