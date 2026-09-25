class_name BlockShapes
extends RefCounted
## Collision and selection boxes per block state (local 0..1 coordinates).

const PX := 1.0 / 16.0
static var FULL := [AABB(Vector3.ZERO, Vector3.ONE)]


static func _b(x0: float, y0: float, z0: float, x1: float, y1: float, z1: float) -> AABB:
	return AABB(Vector3(x0, y0, z0) * PX, Vector3(x1 - x0, y1 - y0, z1 - z0) * PX)


static func _rot(box: AABB, facing: int) -> AABB:
	# rotate a box authored facing south (+Z) to the given horizontal facing (0 S, 1 W, 2 N, 3 E)
	var a := box.position
	var b := box.end
	var p0 := MeshModels.xfp(facing, a)
	var p1 := MeshModels.xfp(facing, b)
	var mn := Vector3(minf(p0.x, p1.x), minf(p0.y, p1.y), minf(p0.z, p1.z))
	var mx := Vector3(maxf(p0.x, p1.x), maxf(p0.y, p1.y), maxf(p0.z, p1.z))
	return AABB(mn, mx - mn)


static func _xf(box: AABB, xf: int) -> AABB:
	var p0 := MeshModels.xfp(xf, box.position)
	var p1 := MeshModels.xfp(xf, box.end)
	var mn := Vector3(minf(p0.x, p1.x), minf(p0.y, p1.y), minf(p0.z, p1.z))
	var mx := Vector3(maxf(p0.x, p1.x), maxf(p0.y, p1.y), maxf(p0.z, p1.z))
	return AABB(mn, mx - mn)


## Collision boxes (empty array = no collision). world may be null (then connections are ignored).
static func collision(v: int, world, x: int, y: int, z: int) -> Array:
	var id := v & 0xFFF
	if BlockDB.solid[id] == 0:
		return []
	var m := BlockDB.model[id]
	var meta := (v >> 12) & 15
	match m:
		BlockDB.M_CUBE, BlockDB.M_LEAVES, BlockDB.M_PISTON:
			if BlockDB.defs[id].props.get("honey", false):
				return [_b(1, 0, 1, 15, 15, 15)]
			if id == MeshModels.ids.get("soul_sand", -1):
				return [_b(0, 0, 0, 16, 14, 16)]
			if m == BlockDB.M_PISTON and (meta & 8) != 0:
				return [_xf(_b(0, 0, 0, 16, 12, 16), MeshModels.XF_BY_DIR[mini(meta & 7, 5)])]
			return FULL
		BlockDB.M_SLAB:
			match meta & 3:
				0: return [_b(0, 0, 0, 16, 8, 16)]
				1: return [_b(0, 8, 0, 16, 16, 16)]
			return FULL
		BlockDB.M_STAIRS:
			var top := (meta & 4) != 0
			var f := meta & 3
			var base := _b(0, 8, 0, 16, 16, 16) if top else _b(0, 0, 0, 16, 8, 16)
			var step := _b(0, 0, 8, 16, 8, 16) if top else _b(0, 8, 8, 16, 16, 16)
			return [base, _rot(step, f)]
		BlockDB.M_FENCE, BlockDB.M_WALL:
			var out := [_b(6, 0, 6, 10, 24, 10)] if m == BlockDB.M_FENCE else [_b(4, 0, 4, 12, 24, 12)]
			if world != null:
				var tbl := MeshModels.connect_fence if m == BlockDB.M_FENCE else MeshModels.connect_wall
				var w0 := 6.0 if m == BlockDB.M_FENCE else 5.0
				var w1 := 10.0 if m == BlockDB.M_FENCE else 11.0
				if tbl[world.get_block(x + 1, y, z) & 0xFFF] == 1:
					out.append(_b(w1, 0, w0, 16, 24, w1))
				if tbl[world.get_block(x - 1, y, z) & 0xFFF] == 1:
					out.append(_b(0, 0, w0, w0, 24, w1))
				if tbl[world.get_block(x, y, z + 1) & 0xFFF] == 1:
					out.append(_b(w0, 0, w1, w1, 24, 16))
				if tbl[world.get_block(x, y, z - 1) & 0xFFF] == 1:
					out.append(_b(w0, 0, 0, w1, 24, w0))
			return out
		BlockDB.M_FENCE_GATE:
			if (meta & 4) != 0:
				return []
			return [_rot(_b(0, 0, 6, 16, 24, 10), meta & 3)]
		BlockDB.M_PANE:
			var out2 := [_b(7, 0, 7, 9, 16, 9)]
			if world != null:
				var t := MeshModels.connect_pane
				if t[world.get_block(x + 1, y, z) & 0xFFF] == 1:
					out2.append(_b(9, 0, 7, 16, 16, 9))
				if t[world.get_block(x - 1, y, z) & 0xFFF] == 1:
					out2.append(_b(0, 0, 7, 7, 16, 9))
				if t[world.get_block(x, y, z + 1) & 0xFFF] == 1:
					out2.append(_b(7, 0, 9, 9, 16, 16))
				if t[world.get_block(x, y, z - 1) & 0xFFF] == 1:
					out2.append(_b(7, 0, 0, 9, 16, 7))
			return out2
		BlockDB.M_DOOR:
			var f2 := meta & 3
			if (meta & 8) != 0 and world != null:
				var lower: int = world.get_block(x, y - 1, z)
				f2 = (lower >> 12) & 3
				meta = ((lower >> 12) & 7) | 8
			if (meta & 4) != 0:
				return [_rot(_b(0, 0, 0, 3, 16, 16), f2)]
			return [_rot(_b(0, 0, 0, 16, 16, 3), f2)]
		BlockDB.M_TRAPDOOR:
			if (meta & 4) != 0:
				return [_rot(_b(0, 0, 0, 16, 16, 3), meta & 3)]
			if (meta & 8) != 0:
				return [_b(0, 13, 0, 16, 16, 16)]
			return [_b(0, 0, 0, 16, 3, 16)]
		BlockDB.M_CARPET:
			return [_b(0, 0, 0, 16, 1, 16)]
		BlockDB.M_SNOW:
			var layers := (meta & 7)
			if layers == 0:
				return []
			return [_b(0, 0, 0, 16, layers * 2, 16)]
		BlockDB.M_PATH:
			return [_b(0, 0, 0, 16, 15, 16)]
		BlockDB.M_CACTUS:
			return [_b(1, 0, 1, 15, 15, 15)]
		BlockDB.M_BED:
			return [_b(0, 0, 0, 16, 9, 16)]
		BlockDB.M_CHEST:
			return [_b(1, 0, 1, 15, 14, 15)]
		BlockDB.M_LANTERN:
			return [_b(5, 1 if (meta & 1) else 0, 5, 11, 9 if (meta & 1) else 8, 11)]
		BlockDB.M_CHAIN, BlockDB.M_ROD:
			return [_b(6, 0, 6, 10, 16, 10)]
		BlockDB.M_CAMPFIRE, BlockDB.M_DAYLIGHT:
			return [_b(0, 0, 0, 16, 7 if m == BlockDB.M_CAMPFIRE else 6, 16)]
		BlockDB.M_ANVIL:
			return [_rot(_b(0, 0, 2, 16, 16, 14), (meta + 1) & 3)]
		BlockDB.M_HOPPER, BlockDB.M_CAULDRON:
			return FULL
		BlockDB.M_BREWING:
			return [_b(7, 0, 7, 9, 14, 9), _b(1, 0, 1, 15, 2, 15)]
		BlockDB.M_ENCHANT:
			return [_b(0, 0, 0, 16, 12, 16)]
		BlockDB.M_END_FRAME:
			return [_b(0, 0, 0, 16, 13, 16)]
		BlockDB.M_POT:
			return [_b(5, 0, 5, 11, 6, 11)] if not BlockDB.defs[id].props.get("decorated", false) else [_b(1, 0, 1, 15, 16, 15)]
		BlockDB.M_CAKE:
			return [_b(1 + (meta & 7) * 2, 0, 1, 15, 8, 15)]
		BlockDB.M_EGG:
			return [_b(1, 0, 1, 15, 16, 15)]
		BlockDB.M_HEAD:
			if (meta & 4) != 0:
				return [_rot(_b(4, 4, 0, 12, 12, 8), meta & 3)]
			return [_b(4, 0, 4, 12, 8, 12)]
		BlockDB.M_CANDLE, BlockDB.M_PICKLE:
			return [_b(5, 0, 5, 11, 6, 11)]
		BlockDB.M_BAMBOO:
			return [_b(6.5, 0, 6.5, 9.5, 16, 9.5)]
		BlockDB.M_SCAFFOLD:
			return [_b(0, 15, 0, 16, 16, 16)]
		BlockDB.M_LECTERN, BlockDB.M_GRINDSTONE, BlockDB.M_BELL:
			return [_b(0, 0, 0, 16, 14, 16)]
		BlockDB.M_STONECUTTER:
			return [_b(0, 0, 0, 16, 9, 16)]
		BlockDB.M_LILY:
			return [_b(1, 0, 1, 15, 1.5, 15)]
		BlockDB.M_LADDER:
			return [_rot(_b(0, 0, 0, 16, 16, 3), meta & 3)]
		BlockDB.M_CHORUS:
			return [_b(3, 0, 3, 13, 16, 13)]
		BlockDB.M_SHORT:
			var h: float = BlockDB.defs[id].props.get("height", 0.5)
			return [AABB(Vector3.ZERO, Vector3(1, h, 1))]
		BlockDB.M_BEACON, BlockDB.M_SHELF, BlockDB.M_OBSERVER, BlockDB.M_BARREL:
			return FULL
		BlockDB.M_HEAVY:
			return [_b(4, 0, 4, 12, 8, 12)]
		BlockDB.M_AMETHYST, BlockDB.M_DRIPSTONE, BlockDB.M_SPIKE:
			return [_b(3, 0, 3, 13, 16, 13)]
		BlockDB.M_REPEATER, BlockDB.M_COMPARATOR:
			return [_b(0, 0, 0, 16, 2, 16)]
		BlockDB.M_PISTON_HEAD:
			return [_xf(_b(0, 12, 0, 16, 16, 16), MeshModels.XF_BY_DIR[mini(meta & 7, 5)]), _xf(_b(6, -4, 6, 10, 12, 10), MeshModels.XF_BY_DIR[mini(meta & 7, 5)])]
	return FULL


## Selection outline boxes (also used for ray targeting of non-solid blocks).
static func selection(v: int, world, x: int, y: int, z: int) -> Array:
	var id := v & 0xFFF
	if id == 0 or BlockDB.fluid[id] != 0:
		return []
	var m := BlockDB.model[id]
	var meta := (v >> 12) & 15
	var col := collision(v, world, x, y, z)
	if not col.is_empty() and m != BlockDB.M_FENCE and m != BlockDB.M_WALL:
		return col
	match m:
		BlockDB.M_FENCE:
			return [_b(6, 0, 6, 10, 16, 10)] + col.slice(1).map(func(bx): return AABB(bx.position, Vector3(bx.size.x, 1.0, bx.size.z)))
		BlockDB.M_WALL:
			return [_b(4, 0, 4, 12, 16, 12)] + col.slice(1).map(func(bx): return AABB(bx.position, Vector3(bx.size.x, 0.875, bx.size.z)))
		BlockDB.M_CROSS, BlockDB.M_COBWEB, BlockDB.M_HANGING:
			if m == BlockDB.M_COBWEB:
				return FULL
			return [_b(2, 0, 2, 14, 13, 14)]
		BlockDB.M_TALL_CROSS:
			return [_b(2, 0, 2, 14, 16, 14)]
		BlockDB.M_CROP:
			return [_b(0, 0, 0, 16, 2 + (meta & 7) * 2, 16)]
		BlockDB.M_TORCH:
			var att := meta & 7
			if att >= 1 and att <= 4:
				return [_rot(_b(5.5, 3, 0, 10.5, 13, 5), att - 1)]
			return [_b(6, 0, 6, 10, 10, 10)]
		BlockDB.M_WIRE, BlockDB.M_RAIL, BlockDB.M_PLATE:
			return [_b(0, 0, 0, 16, 1 if m != BlockDB.M_RAIL else 2, 16)]
		BlockDB.M_BUTTON:
			return [_xf(_b(5, 0, 6, 11, 2, 10), MeshModels._attach_xf(meta & 7))]
		BlockDB.M_LEVER:
			return [_xf(_b(4, 0, 4, 12, 6, 12), MeshModels._attach_xf(meta & 7))]
		BlockDB.M_VINE:
			return [_b(0, 0, 0, 16, 16, 16)]
		BlockDB.M_PORTAL:
			return [_b(0, 0, 6, 16, 16, 10)] if (meta & 1) == 0 else [_b(6, 0, 0, 10, 16, 16)]
		BlockDB.M_FIRE, BlockDB.M_END_PORTAL:
			return []
		BlockDB.M_SNOW:
			return [_b(0, 0, 0, 16, ((meta & 7) + 1) * 2, 16)]
	return FULL
