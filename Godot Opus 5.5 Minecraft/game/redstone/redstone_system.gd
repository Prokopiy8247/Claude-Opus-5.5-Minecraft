class_name RedstoneSystem
extends RefCounted
## Redstone simulation modelled on Java Edition semantics:
##  * sources (lever, button, plate, torch, redstone block, observer, target, daylight detector, trapped chest)
##  * dust networks with 15 → 0 falloff (recomputed as a whole network so removal is always correct)
##  * strong / weak powering of conductor blocks, quasi-connectivity for pistons/dispensers/droppers
##  * scheduled components (torch 1 rt, repeater 1-4 rt, comparator 1 rt, lamp off 2 rt, observer pulse)
##  * consumers: lamps, copper bulbs, pistons (push 12 / sticky pull), TNT, doors, trapdoors, gates,
##    note blocks, dispensers/droppers, powered/activator rails, hoppers, bells.

const K_NONE := 0
const K_WIRE := 1
const K_TORCH := 2
const K_LEVER := 3
const K_BUTTON := 4
const K_PLATE := 5
const K_BLOCK := 6
const K_REPEATER := 7
const K_COMPARATOR := 8
const K_LAMP := 9
const K_PISTON := 10
const K_OBSERVER := 11
const K_TNT := 12
const K_DISPENSER := 13
const K_DROPPER := 14
const K_DOOR := 15
const K_TRAPDOOR := 16
const K_GATE := 17
const K_NOTE := 18
const K_RAIL := 19
const K_TARGET := 20
const K_DAYLIGHT := 21
const K_HOPPER := 22
const K_TRAPPED := 23
const K_HOOK := 24
const K_BELL := 25
const K_BULB := 26
const K_LECTERN := 27

const MAX_PUSH := 12

static var comp := PackedByteArray()
static var I := {}
static var inited := false


static func init() -> void:
	comp.resize(BlockDB.count)
	for d in BlockDB.defs:
		var bd: BlockDef = d
		var k := K_NONE
		match bd.model:
			BlockDB.M_WIRE: k = K_WIRE
			BlockDB.M_BUTTON: k = K_BUTTON
			BlockDB.M_PLATE: k = K_PLATE
			BlockDB.M_REPEATER: k = K_REPEATER
			BlockDB.M_COMPARATOR: k = K_COMPARATOR
			BlockDB.M_PISTON: k = K_PISTON
			BlockDB.M_DOOR: k = K_DOOR
			BlockDB.M_TRAPDOOR: k = K_TRAPDOOR
			BlockDB.M_FENCE_GATE: k = K_GATE
			BlockDB.M_RAIL:
				if String(bd.props.get("rail", "")) in ["powered", "activator", "detector"]:
					k = K_RAIL
			BlockDB.M_DAYLIGHT: k = K_DAYLIGHT
			BlockDB.M_HOPPER: k = K_HOPPER
			BlockDB.M_BELL: k = K_BELL
			BlockDB.M_LECTERN: k = K_LECTERN
			BlockDB.M_LEVER:
				k = K_HOOK if bd.props.get("hook", false) else K_LEVER
		match bd.name:
			"redstone_torch": k = K_TORCH
			"redstone_block": k = K_BLOCK
			"redstone_lamp": k = K_LAMP
			"observer": k = K_OBSERVER
			"tnt": k = K_TNT
			"dispenser": k = K_DISPENSER
			"dropper", "crafter": k = K_DROPPER
			"note_block": k = K_NOTE
			"target": k = K_TARGET
			"trapped_chest": k = K_TRAPPED
		if bd.name.ends_with("copper_bulb"):
			k = K_BULB
		comp[bd.id] = k
	for n in ["redstone_wire", "repeater", "repeater_on", "comparator", "piston", "sticky_piston", "piston_head", "moving_piston",
			"redstone_torch", "obsidian", "bedrock", "crying_obsidian", "end_portal_frame", "end_portal", "nether_portal", "barrier",
			"reinforced_deepslate", "end_gateway", "slime_block", "honey_block", "air"]:
		I[n] = BlockDB.id(n)
	inited = true


static func kind(v: int) -> int:
	return comp[v & 0xFFF]


static func is_component(v: int) -> bool:
	return v != 0 and comp[v & 0xFFF] != K_NONE


static func reacts_to_power(v: int) -> bool:
	return is_component(v)


static func is_conductor(id: int) -> bool:
	return BlockDB.full[id] == 1


static func _meta(v: int) -> int:
	return (v >> 12) & 15


static func _dir_vec(d: int) -> Vector3i:
	return Vox.DIR_VEC[d]


## Direction from a lever/button toward the block it is attached to.
static func _attach_dir(v: int) -> int:
	return mini(_meta(v) & 7, 5)


## Direction from a torch toward its support block.
static func _torch_attach(v: int) -> int:
	var att := _meta(v) & 7
	if att == 0:
		return Vox.DOWN
	return Vox.OPPOSITE[Vox.H_FACING_DIR[(att - 1) & 3]]


## Diodes: meta & 3 = facing toward the INPUT side; output is the opposite side.
static func _diode_back(v: int) -> int:
	return Vox.H_FACING_DIR[_meta(v) & 3]


static func _diode_out(v: int) -> int:
	return Vox.OPPOSITE[_diode_back(v)]


# ------------------------------------------------------------------------------------------------
# Emission
## Power emitted by v (at p) toward direction d (d points from p to the receiver).
static func emit_toward(world: World, p: Vector3i, v: int, d: int) -> int:
	var meta := _meta(v)
	match comp[v & 0xFFF]:
		K_BLOCK:
			return 15
		K_LEVER, K_BUTTON, K_HOOK:
			return 15 if (meta & 8) != 0 else 0
		K_PLATE:
			return meta
		K_TORCH:
			if (meta & 8) != 0 or d == _torch_attach(v):
				return 0
			return 15
		K_WIRE:
			if d == Vox.UP:
				return 0
			if d == Vox.DOWN:
				return meta
			return meta if wire_points(world, p, d) else 0
		K_REPEATER:
			if (v & 0xFFF) == I["repeater_on"] and d == _diode_out(v):
				return 15
			return 0
		K_COMPARATOR:
			if d == _diode_out(v):
				return int(world.get_be(p.x, p.y, p.z).get("out", 0))
			return 0
		K_OBSERVER:
			if (meta & 8) != 0 and d == Vox.OPPOSITE[meta & 7]:
				return 15
			return 0
		K_TARGET:
			return meta
		K_DAYLIGHT:
			return int(world.get_be(p.x, p.y, p.z).get("p", 0))
		K_TRAPPED:
			return mini(15, int(world.get_be(p.x, p.y, p.z).get("viewers", 0)))
		K_LECTERN:
			return 15 if (meta & 8) != 0 else 0
	return 0


## Strong power supplied by v (at p) into the conductor block in direction d.
static func strong_toward(world: World, p: Vector3i, v: int, d: int) -> int:
	var meta := _meta(v)
	match comp[v & 0xFFF]:
		K_LEVER, K_BUTTON, K_HOOK:
			return 15 if (meta & 8) != 0 and d == _attach_dir(v) else 0
		K_PLATE:
			return meta if d == Vox.DOWN else 0
		K_TORCH:
			return 15 if (meta & 8) == 0 and d == Vox.UP else 0
		K_REPEATER, K_COMPARATOR, K_OBSERVER:
			return emit_toward(world, p, v, d)
		K_TRAPPED:
			return emit_toward(world, p, v, d) if d == Vox.DOWN else 0
		K_LECTERN:
			return 15 if (meta & 8) != 0 and d == Vox.DOWN else 0
	return 0


## Power level of a conductor block (max of strong power and, optionally, dust weak power).
static func block_power(world: World, q: Vector3i, include_weak := true) -> int:
	var best := 0
	for d in 6:
		var n: Vector3i = q + Vox.DIR_VEC[d]
		var nv := world.get_block(n.x, n.y, n.z)
		if nv == 0:
			continue
		var k := comp[nv & 0xFFF]
		if k == K_NONE:
			continue
		var from_dir: int = Vox.OPPOSITE[d]
		var s := strong_toward(world, n, nv, from_dir)
		if k == K_WIRE and include_weak:
			s = emit_toward(world, n, nv, from_dir)
		if s > best:
			best = s
			if best >= 15:
				return 15
	return best


## Redstone power received by a consumer at p from all sides (optionally ignoring one side).
static func received(world: World, p: Vector3i, ignore_dir := -1) -> int:
	var best := 0
	for d in 6:
		if d == ignore_dir:
			continue
		var n: Vector3i = p + Vox.DIR_VEC[d]
		var nv := world.get_block(n.x, n.y, n.z)
		if nv == 0:
			continue
		var nid := nv & 0xFFF
		var got := 0
		if comp[nid] != K_NONE and not (comp[nid] in [K_LAMP, K_NOTE, K_TNT, K_DISPENSER, K_DROPPER, K_BULB, K_PISTON, K_DOOR, K_TRAPDOOR, K_GATE, K_RAIL, K_HOPPER, K_BELL]):
			got = emit_toward(world, n, nv, Vox.OPPOSITE[d])
		elif BlockDB.full[nid] == 1 and comp[nid] != K_PISTON:
			got = block_power(world, n)
		if got > best:
			best = got
			if best >= 15:
				return 15
	return best


static func is_powered(world: World, x: int, y: int, z: int) -> bool:
	return received(world, Vector3i(x, y, z)) > 0


## Piston/dispenser style power check including quasi-connectivity (block above counts).
static func qc_powered(world: World, p: Vector3i, ignore_dir := -1) -> bool:
	if received(world, p, ignore_dir) > 0:
		return true
	var up := p + Vector3i(0, 1, 0)
	var uv := world.get_block(up.x, up.y, up.z)
	if comp[uv & 0xFFF] == K_PISTON:
		return false
	return received(world, up, Vox.DOWN) > 0


# ------------------------------------------------------------------------------------------------
# Dust connections
static func _connects_to(world: World, p: Vector3i, d: int) -> bool:
	var n: Vector3i = p + Vox.DIR_VEC[d]
	var nv := world.get_block(n.x, n.y, n.z)
	var nid := nv & 0xFFF
	var k := comp[nid]
	if k != K_NONE:
		match k:
			K_REPEATER:
				var back := _diode_back(nv)
				return d == back or d == Vox.OPPOSITE[back]
			K_OBSERVER:
				return d == (_meta(nv) & 7)
			K_LAMP, K_NOTE, K_TNT, K_DISPENSER, K_DROPPER, K_BULB, K_PISTON, K_DOOR, K_TRAPDOOR, K_GATE, K_RAIL, K_HOPPER, K_BELL:
				return false
		return true
	if BlockDB.full[nid] == 0:
		var below := world.get_block(n.x, n.y - 1, n.z)
		return comp[below & 0xFFF] == K_WIRE
	var above_p := world.get_block(p.x, p.y + 1, p.z)
	if BlockDB.full[above_p & 0xFFF] == 1:
		return false
	var above_n := world.get_block(n.x, n.y + 1, n.z)
	return comp[above_n & 0xFFF] == K_WIRE


## Does the dust at p "point" toward direction d (Java shape rules: isolated dust is a cross,
## a single line points both ways along its axis, corners/tees only toward their connections).
static func wire_points(world: World, p: Vector3i, d: int) -> bool:
	var cn := [false, false, false, false, false, false]
	var n := 0
	for hd in [Vox.EAST, Vox.WEST, Vox.SOUTH, Vox.NORTH]:
		if _connects_to(world, p, hd):
			cn[hd] = true
			n += 1
	if n == 0:
		return true
	var ew: bool = cn[Vox.EAST] or cn[Vox.WEST]
	var ns: bool = cn[Vox.SOUTH] or cn[Vox.NORTH]
	if ew and not ns:
		return d == Vox.EAST or d == Vox.WEST
	if ns and not ew:
		return d == Vox.SOUTH or d == Vox.NORTH
	return cn[d]


## Wire neighbours of a wire (same level, stepping up, stepping down).
static func _wire_links(world: World, p: Vector3i) -> Array:
	var out := []
	var above_full := BlockDB.full[world.get_id(p.x, p.y + 1, p.z)] == 1
	for d in [Vox.EAST, Vox.WEST, Vox.SOUTH, Vox.NORTH]:
		var n: Vector3i = p + Vox.DIR_VEC[d]
		var nv := world.get_block(n.x, n.y, n.z)
		var nid := nv & 0xFFF
		if comp[nid] == K_WIRE:
			out.append(n)
			continue
		if BlockDB.full[nid] == 1:
			if not above_full:
				var up := n + Vector3i(0, 1, 0)
				if comp[world.get_id(up.x, up.y, up.z)] == K_WIRE:
					out.append(up)
		else:
			var dn := n + Vector3i(0, -1, 0)
			if comp[world.get_id(dn.x, dn.y, dn.z)] == K_WIRE:
				out.append(dn)
	return out


## Recomputes the power levels of the whole dust network containing `start`.
static func update_wire_network(world: World, start: Vector3i) -> Dictionary:
	if comp[world.get_id(start.x, start.y, start.z)] != K_WIRE:
		return {}
	var nodes := {start: true}
	var order: Array = [start]
	var qi := 0
	while qi < order.size() and order.size() < 4096:
		var p: Vector3i = order[qi]
		qi += 1
		for n in _wire_links(world, p):
			if not nodes.has(n):
				nodes[n] = true
				order.append(n)
	# base power from non-dust inputs
	var level := {}
	var buckets := []
	buckets.resize(16)
	for i in 16:
		buckets[i] = []
	for p in order:
		var pv: Vector3i = p
		var base := 0
		for d in 6:
			var n: Vector3i = pv + Vox.DIR_VEC[d]
			var nv := world.get_block(n.x, n.y, n.z)
			if nv == 0:
				continue
			var nid := nv & 0xFFF
			var k := comp[nid]
			if k == K_WIRE:
				continue
			var got := 0
			if k != K_NONE:
				got = emit_toward(world, n, nv, Vox.OPPOSITE[d])
			elif BlockDB.full[nid] == 1:
				got = block_power(world, n, false)
			if got > base:
				base = got
		level[pv] = base
		if base > 0:
			(buckets[base] as Array).append(pv)
	# propagate (max-plus with -1 per link), bucket queue from 15 down
	var lvl := 15
	while lvl > 1:
		var bucket: Array = buckets[lvl]
		var bi := 0
		while bi < bucket.size():
			var p: Vector3i = bucket[bi]
			bi += 1
			if int(level[p]) != lvl:
				continue
			for n in _wire_links(world, p):
				if nodes.has(n) and int(level[n]) < lvl - 1:
					level[n] = lvl - 1
					(buckets[lvl - 1] as Array).append(n)
		lvl -= 1
	# apply
	var changed := []
	for p in order:
		var pv: Vector3i = p
		var v := world.get_block(pv.x, pv.y, pv.z)
		var want: int = level[pv]
		if _meta(v) != want:
			world.set_block(pv.x, pv.y, pv.z, Vox.make(v & 0xFFF, want), World.F_URGENT)
			changed.append(pv)
	if not changed.is_empty():
		_notify_around(world, changed)
	return nodes


## Updates consumers within 2 blocks of each changed dust / source position.
static func _notify_around(world: World, positions: Array) -> void:
	var seen := {}
	for p in positions:
		var pv: Vector3i = p
		for d in 6:
			var n: Vector3i = pv + Vox.DIR_VEC[d]
			if not seen.has(n):
				seen[n] = true
			for d2 in 6:
				var n2: Vector3i = n + Vox.DIR_VEC[d2]
				if not seen.has(n2):
					seen[n2] = true
	for q in seen:
		var qv: Vector3i = q
		var v := world.get_block(qv.x, qv.y, qv.z)
		if v == 0:
			continue
		var k := comp[v & 0xFFF]
		if k != K_NONE and k != K_WIRE:
			consumer_update(world, qv, v)


# ------------------------------------------------------------------------------------------------
# Change hooks (from BlockBehaviors)
static func on_block_changed(world: World, x: int, y: int, z: int, old_v: int, new_v: int) -> void:
	var p := Vector3i(x, y, z)
	var ok := comp[old_v & 0xFFF]
	var nk := comp[new_v & 0xFFF]
	# wires: recompute adjacent networks (removal or placement)
	var wires := []
	for d in 6:
		var n: Vector3i = p + Vox.DIR_VEC[d]
		if comp[world.get_id(n.x, n.y, n.z)] == K_WIRE:
			wires.append(n)
		if d < 2 or d > 3:
			for dy in [-1, 1]:
				var nd := n + Vector3i(0, dy, 0)
				if comp[world.get_id(nd.x, nd.y, nd.z)] == K_WIRE:
					wires.append(nd)
	# conductor blocks next to a changed source also feed wires around them
	for d in 6:
		var n: Vector3i = p + Vox.DIR_VEC[d]
		if BlockDB.full[world.get_id(n.x, n.y, n.z)] == 1:
			for d2 in 6:
				var n2: Vector3i = n + Vox.DIR_VEC[d2]
				if comp[world.get_id(n2.x, n2.y, n2.z)] == K_WIRE:
					wires.append(n2)
	var done := {}
	if nk == K_WIRE:
		done = update_wire_network(world, p)
	for w in wires:
		if not done.has(w):
			done.merge(update_wire_network(world, w))
	if nk != K_NONE and nk != K_WIRE:
		consumer_update(world, p, new_v)
	if ok != K_NONE or nk != K_NONE or BlockDB.full[new_v & 0xFFF] != BlockDB.full[old_v & 0xFFF]:
		_notify_around(world, [p])


static func neighbor_update(world: World, x: int, y: int, z: int, v: int) -> void:
	var p := Vector3i(x, y, z)
	var k := comp[v & 0xFFF]
	if k == K_WIRE:
		update_wire_network(world, p)
	elif k != K_NONE:
		consumer_update(world, p, v)


## Observers: called when the block in front of an observer changed.
static func observer_triggered(world: World, p: Vector3i, v: int) -> void:
	if (_meta(v) & 8) != 0:
		return
	world.schedule_tick(p.x, p.y, p.z, 2)


# ------------------------------------------------------------------------------------------------
## Re-evaluates a component after a neighbouring power change.
static func consumer_update(world: World, p: Vector3i, v: int) -> void:
	var meta := _meta(v)
	var id := v & 0xFFF
	match comp[id]:
		K_LAMP, K_BULB:
			var powered := received(world, p) > 0
			var lit := (meta & 1) != 0
			if comp[id] == K_BULB:
				var was: bool = world.rs_state.get(p, false)
				if powered and not was:
					world.set_block(p.x, p.y, p.z, Vox.make(id, meta ^ 1))
					Sfx.play_at("click", Vector3(p) + Vector3(0.5, 0.5, 0.5), 0.5, 1.4)
				world.rs_state[p] = powered
				if not powered:
					world.rs_state.erase(p)
			elif powered and not lit:
				world.set_block(p.x, p.y, p.z, Vox.make(id, meta | 1))
			elif not powered and lit:
				world.schedule_tick(p.x, p.y, p.z, 4)
		K_TORCH:
			var att := _torch_attach(v)
			var sp: Vector3i = p + Vox.DIR_VEC[att]
			var sup_powered := block_power(world, sp) > 0 if BlockDB.full[world.get_id(sp.x, sp.y, sp.z)] == 1 else false
			var on := (meta & 8) == 0
			if on == sup_powered:
				world.schedule_tick(p.x, p.y, p.z, 2)
		K_REPEATER:
			var want := _diode_input(world, p, v) > 0
			var is_on: bool = id == I["repeater_on"]
			if want != is_on:
				world.schedule_tick(p.x, p.y, p.z, (((meta >> 2) & 3) + 1) * 2)
		K_COMPARATOR:
			var out := _comparator_output(world, p, v)
			if out != int(world.get_be(p.x, p.y, p.z).get("out", 0)):
				world.schedule_tick(p.x, p.y, p.z, 2)
		K_PISTON:
			var ext := (meta & 8) != 0
			var dir := mini(meta & 7, 5)
			var pw := qc_powered(world, p, dir)
			if pw != ext:
				world.schedule_tick(p.x, p.y, p.z, 1)
		K_TNT:
			if received(world, p) > 0:
				world.set_block(p.x, p.y, p.z, 0)
				Explosions.prime_tnt(world, Vector3(p), 80)
		K_DISPENSER, K_DROPPER:
			var pw2 := qc_powered(world, p)
			var trig := (meta & 8) != 0
			if pw2 and not trig:
				world.set_block(p.x, p.y, p.z, Vox.make(id, meta | 8), World.F_URGENT)
				world.schedule_tick(p.x, p.y, p.z, 4)
			elif not pw2 and trig:
				world.set_block(p.x, p.y, p.z, Vox.make(id, meta & 7), World.F_URGENT)
		K_DOOR:
			var upper := (meta & 8) != 0
			var other := p + (Vector3i(0, -1, 0) if upper else Vector3i(0, 1, 0))
			var pw3 := received(world, p) > 0 or received(world, other) > 0
			var key := p if not upper else other
			var was3: bool = world.rs_state.get(key, false)
			if pw3 != was3:
				world.rs_state[key] = pw3
				if not pw3:
					world.rs_state.erase(key)
				_set_door(world, key, pw3)
		K_TRAPDOOR, K_GATE:
			var pw4 := received(world, p) > 0
			var was4: bool = world.rs_state.get(p, false)
			if pw4 != was4:
				if pw4:
					world.rs_state[p] = true
				else:
					world.rs_state.erase(p)
				var open := (meta & 4) != 0
				if open != pw4:
					world.set_block(p.x, p.y, p.z, Vox.make(id, meta ^ 4))
					Sfx.play_at("door_open" if pw4 else "door_close", Vector3(p) + Vector3(0.5, 0.5, 0.5))
		K_NOTE:
			var pw5 := received(world, p) > 0
			var was5: bool = world.rs_state.get(p, false)
			if pw5 and not was5:
				BlockInteract.play_note(world, p)
			if pw5:
				world.rs_state[p] = true
			else:
				world.rs_state.erase(p)
		K_BELL:
			var pw6 := received(world, p) > 0
			var was6: bool = world.rs_state.get(p, false)
			if pw6 and not was6:
				Sfx.play_at("bell", Vector3(p) + Vector3(0.5, 0.5, 0.5))
			if pw6:
				world.rs_state[p] = true
			else:
				world.rs_state.erase(p)
		K_RAIL:
			_update_rail(world, p, v)
		K_HOPPER:
			var be := world.get_be(p.x, p.y, p.z, true)
			be["locked"] = received(world, p) > 0


static func _set_door(world: World, lower: Vector3i, open: bool) -> void:
	var lv := world.get_block(lower.x, lower.y, lower.z)
	var uv := world.get_block(lower.x, lower.y + 1, lower.z)
	if BlockDB.model[lv & 0xFFF] != BlockDB.M_DOOR:
		return
	var lm := _meta(lv)
	if ((lm & 4) != 0) == open:
		return
	world.set_block(lower.x, lower.y, lower.z, Vox.make(lv & 0xFFF, lm ^ 4), World.F_URGENT)
	if (uv & 0xFFF) == (lv & 0xFFF):
		world.set_block(lower.x, lower.y + 1, lower.z, Vox.make(uv & 0xFFF, _meta(uv) ^ 4), World.F_URGENT)
	var iron: bool = BlockDB.defs[lv & 0xFFF].props.get("iron", false)
	Sfx.play_at(("iron_door_" if iron else "door_") + ("open" if open else "close"), Vector3(lower) + Vector3(0.5, 0.5, 0.5))


static func _diode_input(world: World, p: Vector3i, v: int) -> int:
	var back := _diode_back(v)
	var n: Vector3i = p + Vox.DIR_VEC[back]
	var nv := world.get_block(n.x, n.y, n.z)
	var nid := nv & 0xFFF
	var toward: int = Vox.OPPOSITE[back]
	if comp[nid] != K_NONE:
		return emit_toward(world, n, nv, toward)
	if BlockDB.full[nid] == 1:
		return block_power(world, n)
	return 0


static func _side_input(world: World, p: Vector3i, v: int) -> int:
	var back := _diode_back(v)
	var best := 0
	for d in [Vox.EAST, Vox.WEST, Vox.SOUTH, Vox.NORTH]:
		if d == back or d == Vox.OPPOSITE[back]:
			continue
		var n: Vector3i = p + Vox.DIR_VEC[d]
		var nv := world.get_block(n.x, n.y, n.z)
		var k := comp[nv & 0xFFF]
		if k == K_WIRE or k == K_REPEATER or k == K_COMPARATOR or k == K_BLOCK:
			best = maxi(best, emit_toward(world, n, nv, Vox.OPPOSITE[d]))
	return best


static func _comparator_output(world: World, p: Vector3i, v: int) -> int:
	var back := _diode_back(v)
	var n: Vector3i = p + Vox.DIR_VEC[back]
	var rear := _diode_input(world, p, v)
	# containers and special blocks behind the comparator
	var cv := world.get_block(n.x, n.y, n.z)
	var ms := _measure(world, n, cv)
	if ms < 0 and BlockDB.full[cv & 0xFFF] == 1:
		var n2: Vector3i = n + Vox.DIR_VEC[back]
		ms = _measure(world, n2, world.get_block(n2.x, n2.y, n2.z))
	if ms >= 0:
		rear = ms
	var side := _side_input(world, p, v)
	if (_meta(v) & 4) != 0:
		return maxi(0, rear - side)
	return rear if rear >= side else 0


## Comparator measurement of containers / cake / cauldron / composter / end frame. -1 when not measurable.
static func _measure(world: World, p: Vector3i, v: int) -> int:
	if v == 0:
		return -1
	var d: BlockDef = BlockDB.defs[v & 0xFFF]
	var meta := _meta(v)
	if d.props.has("container"):
		var be := world.get_be(p.x, p.y, p.z)
		var items: Array = be.get("items", [])
		if items.is_empty():
			return 0
		var frac := 0.0
		for it in items:
			if it is Dictionary and not (it as Dictionary).is_empty():
				var st := ItemStack.from_dict(it)
				if st != null:
					frac += float(st.count) / float(st.max_stack())
		frac /= float(items.size())
		return (floori(frac * 14.0) + (1 if frac > 0.0 else 0))
	match d.model:
		BlockDB.M_CAKE:
			return (7 - (meta & 7)) * 2
		BlockDB.M_CAULDRON:
			return meta & 3
		BlockDB.M_END_FRAME:
			return 15 if (meta & 4) != 0 else 0
	if d.name == "jukebox":
		return 15 if not world.get_be(p.x, p.y, p.z).get("disc", {}).is_empty() else 0
	return -1


# ------------------------------------------------------------------------------------------------
## Scheduled ticks for redstone components.
static func scheduled_tick(world: World, x: int, y: int, z: int, v: int) -> void:
	var p := Vector3i(x, y, z)
	var id := v & 0xFFF
	var meta := _meta(v)
	match comp[id]:
		K_LAMP:
			if (meta & 1) != 0 and received(world, p) == 0:
				world.set_block(x, y, z, Vox.make(id, meta & ~1))
		K_TORCH:
			var att := _torch_attach(v)
			var sp: Vector3i = p + Vox.DIR_VEC[att]
			var sup_powered := block_power(world, sp) > 0 if BlockDB.full[world.get_id(sp.x, sp.y, sp.z)] == 1 else false
			var on := (meta & 8) == 0
			if on and sup_powered:
				world.set_block(x, y, z, Vox.make(id, meta | 8))
			elif not on and not sup_powered:
				world.set_block(x, y, z, Vox.make(id, meta & 7))
		K_REPEATER:
			var want := _diode_input(world, p, v) > 0
			var is_on: bool = id == I["repeater_on"]
			if want and not is_on:
				world.set_block(x, y, z, Vox.make(I["repeater_on"], meta))
			elif not want and is_on:
				world.set_block(x, y, z, Vox.make(I["repeater"], meta))
			_update_output(world, p, _diode_out(v))
		K_COMPARATOR:
			var out := _comparator_output(world, p, v)
			var be := world.get_be(x, y, z, true)
			if int(be.get("out", 0)) != out:
				be["out"] = out
				var nm := (meta | 8) if out > 0 else (meta & 7)
				if nm != meta:
					world.set_block(x, y, z, Vox.make(id, nm))
				_update_output(world, p, _diode_out(v))
		K_PISTON:
			var dir := mini(meta & 7, 5)
			var ext := (meta & 8) != 0
			var pw := qc_powered(world, p, dir)
			if pw and not ext:
				extend(world, p, v)
			elif not pw and ext:
				retract(world, p, v)
		K_OBSERVER:
			if (meta & 8) == 0:
				world.set_block(x, y, z, Vox.make(id, meta | 8), World.F_URGENT | World.F_DROP_BE)
				world.schedule_tick(x, y, z, 2)
			else:
				world.set_block(x, y, z, Vox.make(id, meta & 7), World.F_URGENT | World.F_DROP_BE)
			_update_output(world, p, Vox.OPPOSITE[meta & 7])
		K_BUTTON:
			if (meta & 8) != 0:
				if BlockDB.defs[id].props.get("wooden", false) and _arrow_in(world, p):
					world.schedule_tick(x, y, z, 10)
					return
				world.set_block(x, y, z, Vox.make(id, meta & 7))
				Sfx.play_at("click", Vector3(p) + Vector3(0.5, 0.5, 0.5), 0.5, 0.5)
				_update_attached(world, p, v)
		K_DISPENSER, K_DROPPER:
			Dispensers.fire(world, p, v)
		K_TARGET:
			if meta > 0:
				world.set_block(x, y, z, Vox.make(id, 0))
		K_DAYLIGHT:
			update_daylight(world, p, v)
			world.schedule_tick(x, y, z, 20)
		K_PLATE:
			plate_tick(world, x, y, z, v)
		K_RAIL:
			_update_rail(world, p, v)


static func _arrow_in(world: World, p: Vector3i) -> bool:
	if world.session == null:
		return false
	return world.session.entities.any_in_box(world, AABB(Vector3(p), Vector3.ONE), "arrow")


## After a source changes, update blocks and wires fed through the output direction.
static func _update_output(world: World, p: Vector3i, out_dir: int) -> void:
	var n: Vector3i = p + Vox.DIR_VEC[out_dir]
	var lst := [n]
	var nv := world.get_block(n.x, n.y, n.z)
	if comp[nv & 0xFFF] == K_WIRE:
		update_wire_network(world, n)
	elif comp[nv & 0xFFF] != K_NONE:
		consumer_update(world, n, nv)
	if BlockDB.full[nv & 0xFFF] == 1:
		for d in 6:
			var n2: Vector3i = n + Vox.DIR_VEC[d]
			var v2 := world.get_block(n2.x, n2.y, n2.z)
			if comp[v2 & 0xFFF] == K_WIRE:
				update_wire_network(world, n2)
			elif comp[v2 & 0xFFF] != K_NONE and n2 != p:
				consumer_update(world, n2, v2)
	_notify_around(world, lst)


## Levers/buttons: update both their own neighbourhood and the attached block's.
static func _update_attached(world: World, p: Vector3i, v: int) -> void:
	var att := _attach_dir(v)
	on_block_changed(world, p.x, p.y, p.z, 0, world.get_block(p.x, p.y, p.z))
	_update_output(world, p, att)


# ------------------------------------------------------------------------------------------------
# Player-driven sources
static func toggle_lever(world: World, p: Vector3i) -> void:
	var v := world.get_block(p.x, p.y, p.z)
	var on := (_meta(v) & 8) == 0
	world.set_block(p.x, p.y, p.z, Vox.make(v & 0xFFF, _meta(v) ^ 8), World.F_URGENT | World.F_DROP_BE)
	Sfx.play_at("click", Vector3(p) + Vector3(0.5, 0.5, 0.5), 0.6, 0.6 if on else 0.5)
	_update_attached(world, p, world.get_block(p.x, p.y, p.z))


static func press_button(world: World, p: Vector3i) -> void:
	var v := world.get_block(p.x, p.y, p.z)
	if (_meta(v) & 8) != 0:
		return
	world.set_block(p.x, p.y, p.z, Vox.make(v & 0xFFF, _meta(v) | 8), World.F_URGENT | World.F_DROP_BE)
	Sfx.play_at("click", Vector3(p) + Vector3(0.5, 0.5, 0.5), 0.6, 0.6)
	var wooden: bool = BlockDB.defs[v & 0xFFF].props.get("wooden", false) or BlockDB.defs[v & 0xFFF].sound == "wood"
	world.schedule_tick(p.x, p.y, p.z, 30 if wooden else 20)
	_update_attached(world, p, world.get_block(p.x, p.y, p.z))


## Target block hit by a projectile: power by accuracy for 8 (arrows 20) ticks.
static func target_hit(world: World, p: Vector3i, hit_point: Vector3, arrow := true) -> void:
	var v := world.get_block(p.x, p.y, p.z)
	if comp[v & 0xFFF] != K_TARGET:
		return
	var local := hit_point - Vector3(p) - Vector3(0.5, 0.5, 0.5)
	var off := maxf(maxf(absf(local.x), absf(local.y)), absf(local.z))
	var edge := 0.0
	for a in 3:
		if absf(absf(local[a]) - 0.5) < 0.01:
			continue
		edge = maxf(edge, absf(local[a]))
	var power := clampi(int(ceil(15.0 * (1.0 - edge * 2.0))), 1, 15)
	if off < 0.0:
		power = 1
	world.set_block(p.x, p.y, p.z, Vox.make(v & 0xFFF, power))
	world.schedule_tick(p.x, p.y, p.z, 20 if arrow else 8)
	on_block_changed(world, p.x, p.y, p.z, 0, world.get_block(p.x, p.y, p.z))


## Pressure plate: counts entities on the plate and sets its power.
static func plate_tick(world: World, x: int, y: int, z: int, v: int) -> void:
	var d: BlockDef = BlockDB.defs[v & 0xFFF]
	var kind_s: String = d.props.get("plate", "wood")
	var box := AABB(Vector3(x + 0.0625, y, z + 0.0625), Vector3(0.875, 0.25, 0.875))
	var n := 0
	if world.session != null:
		n = world.session.entities_on_plate(world, box, kind_s)
	var power := 0
	match kind_s:
		"light":
			power = mini(15, n)
		"heavy":
			power = mini(15, int(ceil(n / 10.0)))
		_:
			power = 15 if n > 0 else 0
	var meta := _meta(v)
	if power != meta:
		world.set_block(x, y, z, Vox.make(v & 0xFFF, power), World.F_URGENT)
		Sfx.play_at("click", Vector3(x, y, z) + Vector3(0.5, 0.1, 0.5), 0.4, 0.6 if power > 0 else 0.5)
		on_block_changed(world, x, y, z, v, world.get_block(x, y, z))
		_update_output(world, Vector3i(x, y, z), Vox.DOWN)
	if power > 0:
		world.schedule_tick(x, y, z, 10 if kind_s in ["light", "heavy"] else 20)


## Entities stepping on a plate call this (cheap check, schedules a re-evaluation).
static func plate_step(world: World, p: Vector3i) -> void:
	var v := world.get_block(p.x, p.y, p.z)
	if comp[v & 0xFFF] == K_PLATE and _meta(v) == 0:
		plate_tick(world, p.x, p.y, p.z, v)


static func update_daylight(world: World, p: Vector3i, v: int) -> void:
	var inverted := (_meta(v) & 1) != 0
	var sky := world.get_light(p.x, p.y, p.z).x
	var sess = world.session
	var power := 0
	if sess != null and world.gen.has_sky:
		var t: float = sess.sun_angle_factor()
		power = clampi(roundi(sky * t), 0, 15)
		if inverted:
			power = 15 - power
	var be := world.get_be(p.x, p.y, p.z, true)
	if int(be.get("p", -1)) != power:
		be["p"] = power
		on_block_changed(world, p.x, p.y, p.z, 0, v)


static func _update_rail(world: World, p: Vector3i, v: int) -> void:
	var d: BlockDef = BlockDB.defs[v & 0xFFF]
	if String(d.props.get("rail", "")) == "detector":
		return
	var powered := received(world, p) > 0
	if not powered:
		# propagate along connected powered rails up to 8 blocks
		for dir in [Vox.EAST, Vox.WEST, Vox.SOUTH, Vox.NORTH]:
			var q: Vector3i = p
			for i in 8:
				q += Vox.DIR_VEC[dir]
				var qv := world.get_block(q.x, q.y, q.z)
				if (qv & 0xFFF) != (v & 0xFFF):
					break
				if received(world, q) > 0:
					powered = true
					break
			if powered:
				break
	var meta := _meta(v)
	var on := (meta & 8) != 0
	if powered != on:
		world.set_block(p.x, p.y, p.z, Vox.make(v & 0xFFF, meta ^ 8), World.F_URGENT)
		for dir in [Vox.EAST, Vox.WEST, Vox.SOUTH, Vox.NORTH]:
			var q2: Vector3i = p + Vox.DIR_VEC[dir]
			var qv2 := world.get_block(q2.x, q2.y, q2.z)
			if (qv2 & 0xFFF) == (v & 0xFFF):
				world.schedule_tick(q2.x, q2.y, q2.z, 1)


# ------------------------------------------------------------------------------------------------
# Pistons
static func _movable(world: World, p: Vector3i, v: int, push_dir: int) -> int:
	# 0 = movable, 1 = breaks (drops), 2 = immovable
	var id := v & 0xFFF
	if id == 0:
		return 1
	var d: BlockDef = BlockDB.defs[id]
	if d.hardness < 0.0 or id == I["obsidian"] or id == I["crying_obsidian"] or d.name in ["end_portal_frame", "respawn_anchor",
			"enchanting_table", "ender_chest", "spawner", "beacon", "reinforced_deepslate", "end_gateway", "end_portal",
			"nether_portal", "moving_piston", "jukebox", "lectern", "trial_spawner", "vault"]:
		return 2
	if d.props.has("container") or d.model == BlockDB.M_CHEST:
		return 2
	if comp[id] == K_PISTON and (_meta(v) & 8) != 0:
		return 2
	if d.model == BlockDB.M_PISTON_HEAD:
		return 2
	if BlockDB.fluid[id] != 0 or BlockDB.replaceable[id] == 1:
		return 1
	if not d.solid or d.model in [BlockDB.M_TORCH, BlockDB.M_WIRE, BlockDB.M_CROSS, BlockDB.M_TALL_CROSS, BlockDB.M_CROP,
			BlockDB.M_BUTTON, BlockDB.M_LEVER, BlockDB.M_PLATE, BlockDB.M_CARPET, BlockDB.M_DOOR, BlockDB.M_BED, BlockDB.M_CAKE,
			BlockDB.M_LANTERN, BlockDB.M_POT, BlockDB.M_HEAD, BlockDB.M_DRIPSTONE, BlockDB.M_VINE, BlockDB.M_LILY]:
		return 1
	return 0


static func extend(world: World, p: Vector3i, v: int) -> bool:
	var meta := _meta(v)
	var dir := mini(meta & 7, 5)
	var dv: Vector3i = Vox.DIR_VEC[dir]
	var line := []
	var q: Vector3i = p + dv
	var breaks := -1
	for i in MAX_PUSH + 1:
		if q.y < world.min_y or q.y > world.max_y:
			return false
		var qv := world.get_block(q.x, q.y, q.z)
		var mv := _movable(world, q, qv, dir)
		if qv == 0 or mv == 1:
			if qv != 0:
				breaks = line.size()
			break
		if mv == 2:
			return false
		if i == MAX_PUSH:
			return false
		line.append([q, qv])
		q += dv
	var end_pos: Vector3i = q
	if not world.is_loaded(end_pos.x, end_pos.z):
		return false
	# break the block at the end of the line (plants, torches...)
	if breaks >= 0 or world.get_block(end_pos.x, end_pos.y, end_pos.z) != 0:
		var ev := world.get_block(end_pos.x, end_pos.y, end_pos.z)
		if ev != 0 and BlockDB.fluid[ev & 0xFFF] == 0:
			BlockBehaviors.break_naturally(world, end_pos, ev)
		elif ev != 0:
			world.set_block(end_pos.x, end_pos.y, end_pos.z, 0, World.F_URGENT)
	# move from the far end back toward the piston
	var moved := []
	for i in range(line.size() - 1, -1, -1):
		var e: Array = line[i]
		var from: Vector3i = e[0]
		var bv: int = e[1]
		var be := world.remove_be(from.x, from.y, from.z)
		var to: Vector3i = from + dv
		world.set_block(to.x, to.y, to.z, bv, World.F_URGENT | World.F_DROP_BE)
		if not be.is_empty():
			world.set_be(to.x, to.y, to.z, be)
		moved.append([from, to, bv])
	var head_pos: Vector3i = p + dv
	var sticky: bool = BlockDB.defs[v & 0xFFF].props.get("sticky", false)
	world.set_block(p.x, p.y, p.z, Vox.make(v & 0xFFF, dir | 8), World.F_URGENT | World.F_DROP_BE)
	world.set_block(head_pos.x, head_pos.y, head_pos.z, Vox.make(I["piston_head"], dir | (8 if sticky else 0)), World.F_DEFAULT)
	Sfx.play_at("piston_out", Vector3(p) + Vector3(0.5, 0.5, 0.5), 0.5)
	if world.session != null:
		world.session.piston_moved(world, moved, dv)
	for m in moved:
		var to2: Vector3i = m[1]
		world.set_block(to2.x, to2.y, to2.z, world.get_block(to2.x, to2.y, to2.z), World.F_NOTIFY)
		BlockBehaviors.on_changed(world, to2.x, to2.y, to2.z, 0, world.get_block(to2.x, to2.y, to2.z))
	on_block_changed(world, p.x, p.y, p.z, v, world.get_block(p.x, p.y, p.z))
	return true


static func retract(world: World, p: Vector3i, v: int) -> void:
	var meta := _meta(v)
	var dir := mini(meta & 7, 5)
	var dv: Vector3i = Vox.DIR_VEC[dir]
	var head_pos: Vector3i = p + dv
	var hv := world.get_block(head_pos.x, head_pos.y, head_pos.z)
	if (hv & 0xFFF) == I["piston_head"]:
		world.set_block(head_pos.x, head_pos.y, head_pos.z, 0, World.F_URGENT | World.F_DROP_BE)
	world.set_block(p.x, p.y, p.z, Vox.make(v & 0xFFF, dir), World.F_URGENT | World.F_DROP_BE)
	Sfx.play_at("piston_in", Vector3(p) + Vector3(0.5, 0.5, 0.5), 0.5)
	var sticky: bool = BlockDB.defs[v & 0xFFF].props.get("sticky", false)
	if sticky:
		var pull: Vector3i = head_pos + dv
		var pv := world.get_block(pull.x, pull.y, pull.z)
		if pv != 0 and _movable(world, pull, pv, Vox.OPPOSITE[dir]) == 0:
			var be := world.remove_be(pull.x, pull.y, pull.z)
			world.set_block(pull.x, pull.y, pull.z, 0, World.F_DEFAULT)
			world.set_block(head_pos.x, head_pos.y, head_pos.z, pv, World.F_DEFAULT)
			if not be.is_empty():
				world.set_be(head_pos.x, head_pos.y, head_pos.z, be)
			if world.session != null:
				world.session.piston_moved(world, [[pull, head_pos, pv]], -dv)
	world.set_block(head_pos.x, head_pos.y, head_pos.z, world.get_block(head_pos.x, head_pos.y, head_pos.z), World.F_NOTIFY)
	on_block_changed(world, p.x, p.y, p.z, v, world.get_block(p.x, p.y, p.z))
