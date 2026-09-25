class_name Rails
extends RefCounted
## Rail shapes and minecart movement.
##
## Shapes (block meta, Minecraft order): 0 north-south, 1 east-west, 2 ascending east,
## 3 ascending west, 4 ascending north, 5 ascending south, 6 south-east, 7 south-west,
## 8 north-west, 9 north-east. Powered / detector / activator rails cannot curve and keep their
## powered bit in meta & 8.
##
## A cart on a rail moves along the segment between the rail's two exits (edge midpoints, one of
## them a block higher on slopes): speed is kept through curves, slopes add gravity, powered rails
## boost and unpowered ones brake, the rider's forward key nudges the cart along its look direction.

const NONE := Vector3i(-99999, -99999, -99999)
const MAX_SPEED := 0.4          # blocks per tick (8 m/s)

## Exits per shape as offsets from the block's bottom centre.
const EXITS := [
	[Vector3(0, 0, -0.5), Vector3(0, 0, 0.5)],
	[Vector3(-0.5, 0, 0), Vector3(0.5, 0, 0)],
	[Vector3(-0.5, 0, 0), Vector3(0.5, 1, 0)],
	[Vector3(-0.5, 1, 0), Vector3(0.5, 0, 0)],
	[Vector3(0, 1, -0.5), Vector3(0, 0, 0.5)],
	[Vector3(0, 0, -0.5), Vector3(0, 1, 0.5)],
	[Vector3(0, 0, 0.5), Vector3(0.5, 0, 0)],
	[Vector3(0, 0, 0.5), Vector3(-0.5, 0, 0)],
	[Vector3(0, 0, -0.5), Vector3(-0.5, 0, 0)],
	[Vector3(0, 0, -0.5), Vector3(0.5, 0, 0)],
]

const N := Vector3i(0, 0, -1)
const S := Vector3i(0, 0, 1)
const E := Vector3i(1, 0, 0)
const W := Vector3i(-1, 0, 0)


static func is_rail(w: World, p: Vector3i) -> bool:
	return BlockDB.model[w.get_id(p.x, p.y, p.z)] == BlockDB.M_RAIL


static func is_special(id: int) -> bool:
	return String((BlockDB.defs[id] as BlockDef).props.get("rail", "")) != ""


static func shape_of(v: int) -> int:
	var id := v & 0xFFF
	var meta := v >> 12
	return meta & (7 if is_special(id) else 15)


## Rail block the cart at pos runs on (its own block, or the one below on the upper half of a slope).
static func rail_under(w: World, pos: Vector3) -> Vector3i:
	var p := Vector3i(floori(pos.x), floori(pos.y + 0.1), floori(pos.z))
	if is_rail(w, p):
		return p
	if is_rail(w, p + Vector3i(0, -1, 0)):
		return p + Vector3i(0, -1, 0)
	return NONE


# ------------------------------------------------------------------------------------------------
# Shapes
## Horizontal neighbours with a rail: [dir, dy] where dy = +1 when that rail sits a block higher.
static func _neighbours(w: World, p: Vector3i) -> Array:
	var out := []
	for d in [N, S, E, W]:
		var q: Vector3i = p + d
		if is_rail(w, q):
			out.append([d, 0])
		elif is_rail(w, q + Vector3i(0, 1, 0)):
			out.append([d, 1])
		elif is_rail(w, q + Vector3i(0, -1, 0)):
			out.append([d, -1])
	return out


static func _pick(nb: Array, special: bool, current: int) -> int:
	var has := {}
	var up := {}
	for e in nb:
		has[e[0]] = true
		if int(e[1]) == 1:
			up[e[0]] = true
	var n := has.has(N)
	var s := has.has(S)
	var e_ := has.has(E)
	var w_ := has.has(W)
	if n and s:
		return 4 if up.has(N) else (5 if up.has(S) else 0)
	if e_ and w_:
		return 2 if up.has(E) else (3 if up.has(W) else 1)
	if not special:
		if s and e_:
			return 6
		if s and w_:
			return 7
		if n and w_:
			return 8
		if n and e_:
			return 9
	if n or s:
		return 4 if up.has(N) else (5 if up.has(S) else 0)
	if e_ or w_:
		return 2 if up.has(E) else (3 if up.has(W) else 1)
	return current if current <= 1 else 0


## Number of this rail's exits that lead to another rail.
static func _linked_exits(w: World, p: Vector3i, shape: int) -> int:
	var n := 0
	for ex in EXITS[shape]:
		var o: Vector3 = ex
		var q := p + Vector3i(roundi(o.x * 2.0), 0, roundi(o.z * 2.0))
		if is_rail(w, q) or is_rail(w, q + Vector3i(0, 1, 0)) or is_rail(w, q + Vector3i(0, -1, 0)):
			n += 1
	return n


static func _set_shape(w: World, p: Vector3i, shape: int) -> void:
	var v := w.get_block(p.x, p.y, p.z)
	var id := v & 0xFFF
	var meta := v >> 12
	var nm := (meta & 8) | shape if is_special(id) else shape
	if nm != meta:
		w.set_block(p.x, p.y, p.z, Vox.make(id, nm), World.F_URGENT | World.F_NOTIFY)


## Called after a rail is placed: shape it towards its neighbours, then let neighbours that are
## not yet fully linked turn towards it (making curves and slopes like the original).
static func on_placed(w: World, p: Vector3i) -> void:
	var v := w.get_block(p.x, p.y, p.z)
	var special := is_special(v & 0xFFF)
	_set_shape(w, p, _pick(_neighbours(w, p), special, shape_of(v)))
	for d in [N, S, E, W]:
		for dy in [0, 1, -1]:
			var q: Vector3i = p + d + Vector3i(0, dy, 0)
			if not is_rail(w, q):
				continue
			var qv := w.get_block(q.x, q.y, q.z)
			# fully linked rails keep their shape (existing tracks are not rewired)
			if _linked_exits(w, q, shape_of(qv)) >= 2:
				continue
			_set_shape(w, q, _pick(_neighbours(w, q), is_special(qv & 0xFFF), shape_of(qv)))


# ------------------------------------------------------------------------------------------------
# Cart movement
## One tick of a cart on the rail at rp. inp/ryaw: rider's input and look yaw.
static func move_cart(cart, rp: Vector3i, inp: Vector2, ryaw: float) -> void:
	var w: World = cart.world
	var v := w.get_block(rp.x, rp.y, rp.z)
	var id := v & 0xFFF
	var kind := String((BlockDB.defs[id] as BlockDef).props.get("rail", ""))
	var shape := shape_of(v)
	var powered := kind != "" and ((v >> 12) & 8) != 0
	var base := Vector3(rp.x + 0.5, rp.y, rp.z + 0.5)
	var pa: Vector3 = base + (EXITS[shape][0] as Vector3)
	var pb: Vector3 = base + (EXITS[shape][1] as Vector3)
	var seg := pb - pa
	var seg_len := seg.length()
	var dir := seg / seg_len
	# signed speed along the segment: keep the magnitude through curves
	var vel: Vector3 = cart.body.vel
	var sgn := 1.0 if vel.dot(dir) >= 0.0 else -1.0
	var s := vel.length() * sgn
	if vel.length() < 0.001:
		s = 0.0
	# slopes
	if absf(dir.y) > 0.01:
		s -= dir.y * 0.0078125 * 2.0
	# rider pushes along the look direction
	if inp.y > 0.1 and absf(s) < 0.25:
		var look := Vector3(-sin(ryaw), 0, -cos(ryaw))
		var flat := Vector3(dir.x, 0, dir.z).normalized()
		var d := look.dot(flat)
		if absf(d) > 0.2:
			s += signf(d) * 0.02
	# powered rails boost, unpowered ones brake
	if kind == "powered":
		if powered:
			if absf(s) > 0.01:
				s += signf(s) * 0.06
			else:
				# kick off away from a solid block at one end
				var ba := pa - dir * 0.3
				var bb := pb + dir * 0.3
				if BlockDB.solid[w.get_id(floori(ba.x), floori(ba.y + 0.2), floori(ba.z))] == 1:
					s = 0.1
				elif BlockDB.solid[w.get_id(floori(bb.x), floori(bb.y + 0.2), floori(bb.z))] == 1:
					s = -0.1
		else:
			s *= 0.5
			if absf(s) < 0.03:
				s = 0.0
	s *= 0.997 if cart.rider != null else 0.99
	s = clampf(s, -MAX_SPEED, MAX_SPEED)
	# where the cart is along the segment now, and where it goes
	var t := clampf((cart.body.pos - pa).dot(dir) / seg_len, 0.0, 1.0)
	var nt := t + s / seg_len
	var new_pos: Vector3
	if nt > 1.0 or nt < 0.0:
		var over := (nt - 1.0) * seg_len if nt > 1.0 else -nt * seg_len
		var edge := pb if nt > 1.0 else pa
		var out_dir := dir if nt > 1.0 else -dir
		var flat_out := Vector3(out_dir.x, 0, out_dir.z).normalized()
		var next := Vector3i(floori(edge.x + flat_out.x * 0.5), floori(edge.y + 0.01), floori(edge.z + flat_out.z * 0.5))
		var nrp := NONE
		if is_rail(w, next):
			nrp = next
		elif is_rail(w, next + Vector3i(0, -1, 0)):
			nrp = next + Vector3i(0, -1, 0)
		if nrp == NONE:
			# end of the track: stop at the buffer
			new_pos = edge - out_dir * 0.05
			s = 0.0
			cart.body.vel = Vector3.ZERO
		else:
			# continue into the next rail along its own segment
			var nv := w.get_block(nrp.x, nrp.y, nrp.z)
			var nshape := shape_of(nv)
			var nbase := Vector3(nrp.x + 0.5, nrp.y, nrp.z + 0.5)
			var na: Vector3 = nbase + (EXITS[nshape][0] as Vector3)
			var nb: Vector3 = nbase + (EXITS[nshape][1] as Vector3)
			var ndir := (nb - na).normalized()
			# enter from the end nearest to where we left
			var from_a := na.distance_to(edge) <= nb.distance_to(edge)
			var move_dir := ndir if from_a else -ndir
			new_pos = (na if from_a else nb) + move_dir * minf(over, 0.99)
			cart.body.vel = move_dir * absf(s)
			cart.facing = atan2(-move_dir.x, -move_dir.z)
			cart.body.pos = new_pos
			return
	else:
		new_pos = pa + dir * (nt * seg_len)
	cart.body.pos = new_pos
	cart.body.vel = dir * s
	if absf(s) > 0.001:
		var fdir := dir * signf(s)
		cart.facing = atan2(-fdir.x, -fdir.z)
