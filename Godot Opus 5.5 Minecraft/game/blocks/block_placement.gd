class_name BlockPlacement
extends RefCounted
## Converts "place this block against the targeted face" into concrete block states, following the
## Minecraft placement rules (orientation, slabs merging, double-height blocks, support checks).
## Returns an Array of [Vector3i, value] placements, or [] when the placement is invalid.

static func look_facing(yaw: float) -> int:
	return Vox.facing_from_yaw(yaw)


static func nearest_look_dir(yaw: float, pitch: float) -> int:
	if pitch > 0.785:
		return Vox.UP
	if pitch < -0.785:
		return Vox.DOWN
	return Vox.facing_to_dir(Vox.facing_from_yaw(yaw))


static func is_soil(id: int) -> bool:
	var d: BlockDef = BlockDB.defs[id]
	return d.tags.has("dirt") or d.name in ["grass_block", "farmland", "podzol", "mycelium", "coarse_dirt", "rooted_dirt", "moss_block",
		"mud", "muddy_mangrove_roots", "pale_moss_block", "dirt"]


static func support_ok(world, d: BlockDef, pos: Vector3i, meta: int) -> bool:
	var below: int = world.get_block(pos.x, pos.y - 1, pos.z)
	var bid := below & 0xFFF
	var above: int = world.get_block(pos.x, pos.y + 1, pos.z)
	var aid := above & 0xFFF
	var bd: BlockDef = BlockDB.defs[bid]
	match d.place:
		"plant", "tall_plant":
			if d.name.ends_with("_sapling") and d.name == "mangrove_propagule":
				return is_soil(bid) or bd.name == "clay"
			if d.name in ["azalea", "flowering_azalea"]:
				return is_soil(bid) or bd.name == "clay"
			if d.name == "sweet_berry_bush":
				return is_soil(bid)
			return is_soil(bid)
		"dry_plant":
			return is_soil(bid) or bd.tags.has("sand") or bd.tags.has("terracotta")
		"cactus_flower":
			return bd.name == "cactus" or bd.full
		"mushroom":
			return BlockDB.full[bid] == 1
		"crop":
			return bd.name == "farmland"
		"nether_wart":
			return bd.name == "soul_sand"
		"nether_plant":
			return bd.name in ["crimson_nylium", "warped_nylium", "soul_soil", "netherrack", "soul_sand"] or is_soil(bid)
		"plant_up":
			return BlockDB.full[bid] == 1 or bid == BlockDB.id(d.name)
		"sugar_cane":
			if bid == BlockDB.id("sugar_cane"):
				return true
			if not (is_soil(bid) or bd.tags.has("sand")):
				return false
			for f in 4:
				var hv: Vector3i = Vox.H_FACING_VEC[f]
				var nv: int = world.get_block(pos.x + hv.x, pos.y - 1, pos.z + hv.z)
				var nid := nv & 0xFFF
				if BlockDB.fluid[nid] == 1 or BlockDB.waterlogged[nid] == 1 or BlockDB.defs[nid].name == "frosted_ice":
					return true
			return false
		"cactus":
			if not (bd.tags.has("sand") or bd.name == "cactus"):
				return false
			for f in 4:
				var hv2: Vector3i = Vox.H_FACING_VEC[f]
				if BlockDB.solid[world.get_block(pos.x + hv2.x, pos.y, pos.z + hv2.z) & 0xFFF] == 1:
					return false
			return true
		"bamboo":
			return is_soil(bid) or bd.tags.has("sand") or bd.name == "gravel" or bd.name == "bamboo"
		"hanging":
			return BlockDB.full[aid] == 1 or aid == BlockDB.id(d.name) or BlockDB.model[aid] == BlockDB.M_LEAVES
		"lily_pad":
			return BlockDB.fluid[bid] == 1 and ((below >> 12) & 15) == 0
		"kelp", "seagrass", "coral", "pickle":
			var here: int = world.get_block(pos.x, pos.y, pos.z)
			if d.place == "pickle" and BlockDB.fluid[here & 0xFFF] != 1:
				return BlockDB.full[bid] == 1
			return BlockDB.fluid[here & 0xFFF] == 1 and (BlockDB.full[bid] == 1 or bid == BlockDB.id(d.name))
		"carpet", "snow_layer", "plate", "wire", "rail", "diode":
			return BlockDB.full[bid] == 1 or BlockDB.model[bid] == BlockDB.M_SLAB and ((below >> 12) & 3) != 0 or \
				(d.place == "carpet" and bid != 0 and BlockDB.fluid[bid] == 0)
		"door":
			return BlockDB.full[bid] == 1
	return true


## Main entry: item block placement.
static func placements(world, bid: int, hit: Dictionary, player, sneaking := false) -> Array:
	var d: BlockDef = BlockDB.defs[bid]
	var hp: Vector3i = hit.pos
	var face: int = hit.face
	var hv: int = hit.block
	var hd: BlockDef = BlockDB.defs[hv & 0xFFF]
	var point: Vector3 = hit.point
	var frac := point - Vector3(hp)
	var yaw: float = player.yaw
	var pitch: float = player.pitch
	# replaceable target (grass, snow layer, water) -> place into it
	var pos: Vector3i = hp + Vox.DIR_VEC[face]
	if BlockDB.replaceable[hv & 0xFFF] == 1 and not (hd.place == "snow_layer" and d.place == "snow_layer"):
		if not (hd.name == d.name and (d.place == "candle" or d.place == "pickle")):
			pos = hp
			face = Vox.UP
			frac = Vector3(0.5, 1.0, 0.5)
	# merging placements on the clicked block itself
	if hd.name == d.name:
		var hm := (hv >> 12) & 15
		match d.place:
			"slab":
				if (hm == 0 and face == Vox.UP) or (hm == 1 and face == Vox.DOWN):
					return [[hp, Vox.make(bid, 2)]]
			"snow_layer":
				if hm < 7:
					return [[hp, Vox.make(bid, hm + 1)]] if hm < 6 else [[hp, BlockDB.id("snow_block")]]
			"candle":
				if (hm & 3) < 3:
					return [[hp, Vox.make(bid, hm + 1)]]
			"pickle":
				if (hm & 3) < 3:
					return [[hp, Vox.make(bid, hm + 1)]]
	var cur: int = world.get_block(pos.x, pos.y, pos.z)
	var cid := cur & 0xFFF
	if cid != 0 and BlockDB.replaceable[cid] == 0:
		# slab into existing half slab of the same type
		if BlockDB.defs[cid].name == d.name and d.place == "slab":
			var cm := (cur >> 12) & 3
			if cm != 2:
				return [[pos, Vox.make(bid, 2)]]
		return []
	if pos.y < world.min_y or pos.y > world.max_y:
		return []
	var meta := 0
	var extra := []
	var lf := look_facing(yaw)
	match d.place:
		"axis":
			if face == Vox.UP or face == Vox.DOWN:
				meta = 0
			elif face == Vox.EAST or face == Vox.WEST:
				meta = 1
			else:
				meta = 2
		"facing", "facing_lit", "glazed", "anchor", "chest":
			meta = (lf + 2) & 3
			if d.place == "chest":
				meta = _chest_join(world, bid, pos, meta)
		"facing_side":
			meta = lf & 3
		"facing_all", "facing_all_player":
			if d.model == BlockDB.M_ROD or d.model == BlockDB.M_AMETHYST or d.place == "facing_all" and d.name != "barrel":
				meta = face
			else:
				meta = Vox.OPPOSITE[nearest_look_dir(yaw, pitch)]
		"facing_all_observer":
			meta = nearest_look_dir(yaw, pitch)
		"facing_all_shulker":
			meta = face
		"slab":
			if face == Vox.DOWN or (face != Vox.UP and frac.y > 0.5):
				meta = 1
		"stairs":
			meta = lf
			if face == Vox.DOWN or (face != Vox.UP and frac.y > 0.5):
				meta |= 4
		"door":
			if not support_ok(world, d, pos, 0):
				return []
			var up: int = world.get_block(pos.x, pos.y + 1, pos.z)
			if up != 0 and BlockDB.replaceable[up & 0xFFF] == 0:
				return []
			meta = lf
			extra.append([pos + Vector3i(0, 1, 0), Vox.make(bid, lf | 8)])
		"trapdoor":
			if face == Vox.UP or face == Vox.DOWN:
				meta = (lf + 2) & 3
				if face == Vox.DOWN:
					meta |= 8
			else:
				meta = Vox.dir_to_facing(face)
				if frac.y > 0.5:
					meta |= 8
		"torch":
			if face == Vox.UP:
				var below: int = world.get_block(pos.x, pos.y - 1, pos.z) & 0xFFF
				if BlockDB.solid[below] == 0 and BlockDB.model[below] != BlockDB.M_FENCE and BlockDB.model[below] != BlockDB.M_WALL:
					return []
				meta = 0
			elif face == Vox.DOWN:
				return []
			else:
				if BlockDB.full[hv & 0xFFF] == 0:
					return []
				meta = 1 + Vox.dir_to_facing(face)
		"button":
			var att: int = Vox.OPPOSITE[face]
			var sup: int = world.get_block(pos.x + Vox.DIR_X[att], pos.y + Vox.DIR_Y[att], pos.z + Vox.DIR_Z[att])
			if BlockDB.full[sup & 0xFFF] == 0:
				return []
			meta = att
		"ladder":
			if face == Vox.UP or face == Vox.DOWN:
				return []
			if BlockDB.full[hv & 0xFFF] == 0:
				return []
			meta = Vox.dir_to_facing(face)
		"lantern":
			meta = 1 if face == Vox.DOWN else 0
			if meta == 0 and BlockDB.solid[world.get_block(pos.x, pos.y - 1, pos.z) & 0xFFF] == 0:
				meta = 1
		"bed":
			var fvec: Vector3i = Vox.H_FACING_VEC[lf]
			var head: Vector3i = pos + fvec
			var hb: int = world.get_block(head.x, head.y, head.z)
			if hb != 0 and BlockDB.replaceable[hb & 0xFFF] == 0:
				return []
			if BlockDB.solid[world.get_block(pos.x, pos.y - 1, pos.z) & 0xFFF] == 0:
				return []
			meta = lf
			extra.append([head, Vox.make(bid, lf | 4)])
		"tall_plant":
			if not support_ok(world, d, pos, 0):
				return []
			var up2: int = world.get_block(pos.x, pos.y + 1, pos.z)
			if up2 != 0 and BlockDB.replaceable[up2 & 0xFFF] == 0:
				return []
			extra.append([pos + Vector3i(0, 1, 0), Vox.make(bid, 1)])
		"diode":
			meta = (lf + 2) & 3
		"hopper":
			meta = Vox.DOWN if (face == Vox.UP or face == Vox.DOWN) else Vox.OPPOSITE[face]
		"head":
			if face == Vox.UP or face == Vox.DOWN:
				meta = (lf + 2) & 3
			else:
				meta = Vox.dir_to_facing(face) | 4
		"vine":
			if face == Vox.UP or face == Vox.DOWN:
				if d.name == "vine":
					return []
				meta = 0
			else:
				meta = 1 << ((Vox.dir_to_facing(face) + 2) & 3)
				var wall := BlockDB.full[hv & 0xFFF]
				if wall == 0:
					return []
		"dripstone":
			meta = 1 if face == Vox.UP else 0
			if face != Vox.UP and face != Vox.DOWN:
				return []
		"candle", "pickle":
			meta = 0
		"lit":
			meta = 0
		"rail":
			meta = 1 if (lf == 1 or lf == 3) else 0
		"snow_layer":
			meta = 0
	if not support_ok(world, d, pos, meta):
		return []
	var out := [[pos, Vox.make(bid, meta)]]
	out.append_array(extra)
	return out


## Joins a new chest with an adjacent single chest of the same kind and facing.
static func _chest_join(world, bid: int, pos: Vector3i, facing: int) -> int:
	var d: BlockDef = BlockDB.defs[bid]
	if d.props.get("ender", false) or d.props.get("copper_chest", false):
		return facing
	# left/right relative to facing: meta bits 2-3: 0 single, 1 left, 2 right
	var fvec: Vector3i = Vox.H_FACING_VEC[facing]
	var right := Vector3i(-fvec.z, 0, fvec.x)
	for side in [1, -1]:
		var np: Vector3i = pos + right * side
		var nv: int = world.get_block(np.x, np.y, np.z)
		if (nv & 0xFFF) == bid:
			var nm := (nv >> 12) & 15
			if (nm & 3) == facing and (nm >> 2) == 0:
				# neighbour becomes the other half
				world.set_block(np.x, np.y, np.z, Vox.make(bid, facing | ((2 if side == 1 else 1) << 2)), World.F_URGENT)
				return facing | ((1 if side == 1 else 2) << 2)
	return facing
