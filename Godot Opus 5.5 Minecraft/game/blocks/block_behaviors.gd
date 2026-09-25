class_name BlockBehaviors
extends RefCounted
## World-side block logic: neighbour updates (support, gravity, fluids, redstone), scheduled ticks,
## random ticks (growth, spreading, decay, melting) and breaking side effects.

static var enabled := false
static var random_tickable := PackedByteArray()
static var I := {}
static var _rng := RandomNumberGenerator.new()


static func init() -> void:
	random_tickable.resize(BlockDB.count)
	for d in BlockDB.defs:
		var t: String = d.tick
		if t in ["grass", "mycelium", "crop", "stem", "sapling", "leaves", "sugar_cane", "cactus", "bamboo", "kelp", "fire",
				"ice", "snow_melt", "farmland", "mushroom", "cave_vines", "weeping_vines", "chorus_flower", "budding", "oxidize",
				"coral", "redstone_ore", "vine", "geyser", "firefly", "creaking_heart"]:
			random_tickable[d.id] = 1
		if d.fluid == 2:
			random_tickable[d.id] = 1
	for n in ["air", "water", "lava", "fire", "soul_fire", "dirt", "grass_block", "farmland", "sand", "gravel", "cobblestone",
			"stone", "obsidian", "ice", "snow", "snow_block", "mycelium", "netherrack", "soul_sand", "soul_soil", "magma_block",
			"sugar_cane", "cactus", "bamboo", "kelp", "melon", "pumpkin", "dirt_path", "coarse_dirt", "podzol", "mud",
			"packed_ice", "cave_vines", "weeping_vines", "chorus_plant", "chorus_flower", "end_stone", "vine", "redstone_ore",
			"deepslate_redstone_ore", "moss_block", "sculk", "rooted_dirt", "basalt", "blue_ice", "nether_portal",
			"budding_amethyst", "small_amethyst_bud", "medium_amethyst_bud", "large_amethyst_bud", "amethyst_cluster",
			"creaking_heart", "pale_oak_log", "wet_sponge", "sponge", "clay"]:
		I[n] = BlockDB.id(n)
	_rng.randomize()
	enabled = true


static func _id(v: int) -> int:
	return v & 0xFFF


static func _meta(v: int) -> int:
	return (v >> 12) & 15


static func D(v: int) -> BlockDef:
	return BlockDB.defs[v & 0xFFF]


# ------------------------------------------------------------------------------------------------
## Called after a block changed at (x,y,z).
static func on_changed(world: World, x: int, y: int, z: int, old_v: int, new_v: int) -> void:
	var nd := D(new_v)
	if nd.fluid != 0:
		world.schedule_tick(x, y, z, 5 if nd.fluid == 1 else (10 if world.dim == 1 else 30))
	if nd.gravity:
		world.schedule_tick(x, y, z, 2)
	if nd.model == BlockDB.M_FIRE:
		world.schedule_tick(x, y, z, 30 + _rng.randi_range(0, 10))
	if RedstoneSystem.is_component(old_v) or RedstoneSystem.is_component(new_v):
		RedstoneSystem.on_block_changed(world, x, y, z, old_v, new_v)


## Called for each neighbour of a changed block; from_dir = direction from this block toward the change.
static func neighbor_changed(world: World, x: int, y: int, z: int, v: int, from_dir: int) -> void:
	if v == 0:
		return
	var d := D(v)
	var id := v & 0xFFF
	# fluids re-evaluate their flow
	if d.fluid != 0 or d.waterlogged:
		world.schedule_tick(x, y, z, 5 if d.fluid != 2 else (10 if world.dim == 1 else 30))
		if d.fluid == 2:
			_lava_contact(world, x, y, z, v)
	if d.gravity:
		world.schedule_tick(x, y, z, 2)
	if not _has_support(world, x, y, z, v):
		break_naturally(world, Vector3i(x, y, z), v)
		return
	match d.model:
		BlockDB.M_DOOR:
			_door_pair_check(world, x, y, z, v)
		BlockDB.M_BED:
			_bed_pair_check(world, x, y, z, v)
		BlockDB.M_TALL_CROSS:
			var upper := (_meta(v) & 1) == 1
			var other := world.get_block(x, y - 1 if upper else y + 1, z)
			if _id(other) != id:
				world.set_block(x, y, z, 0, World.F_DEFAULT)
	if d.name == "grass_block" or d.name == "mycelium" or d.name == "podzol":
		pass
	if d.name == "farmland" and BlockDB.solid[world.get_id(x, y + 1, z)] == 1 and BlockDB.full[world.get_id(x, y + 1, z)] == 1:
		world.set_block(x, y, z, I["dirt"])
	if d.name == "nether_portal":
		if not Portals.frame_valid(world, Vector3i(x, y, z), _meta(v) & 1):
			world.set_block(x, y, z, 0, World.F_DEFAULT)
	if d.name == "sponge":
		absorb_water(world, x, y, z)
	if d.props.has("concrete") and _touches_water(world, x, y, z):
		world.set_block(x, y, z, BlockDB.id(String(d.props["concrete"])))
	if RedstoneSystem.is_component(v):
		if RedstoneSystem.kind(v) == RedstoneSystem.K_OBSERVER and from_dir == (_meta(v) & 7):
			RedstoneSystem.observer_triggered(world, Vector3i(x, y, z), v)
		RedstoneSystem.neighbor_update(world, x, y, z, v)
	if d.props.get("tnt", false) and RedstoneSystem.is_powered(world, x, y, z):
		world.set_block(x, y, z, 0)
		Explosions.prime_tnt(world, Vector3(x, y, z), 80)


static func _touches_water(world: World, x: int, y: int, z: int) -> bool:
	for dd in 6:
		if dd == Vox.DOWN:
			continue
		var n := world.get_block(x + Vox.DIR_X[dd], y + Vox.DIR_Y[dd], z + Vox.DIR_Z[dd])
		if BlockDB.fluid[n & 0xFFF] == 1:
			return true
	return false


## Support rules for attached / planted blocks.
static func _has_support(world: World, x: int, y: int, z: int, v: int) -> bool:
	var d := D(v)
	var m := d.model
	var meta := _meta(v)
	var below := world.get_block(x, y - 1, z)
	var bid := below & 0xFFF
	match m:
		BlockDB.M_TORCH:
			var att := meta & 7
			if att == 0:
				return BlockDB.solid[bid] == 1 or BlockDB.model[bid] in [BlockDB.M_FENCE, BlockDB.M_WALL]
			var fvec: Vector3i = Vox.H_FACING_VEC[(att - 1) & 3]
			return BlockDB.full[world.get_id(x - fvec.x, y, z - fvec.z)] == 1
		BlockDB.M_BUTTON, BlockDB.M_LEVER:
			var a := meta & 7
			return BlockDB.solid[world.get_id(x + Vox.DIR_X[a], y + Vox.DIR_Y[a], z + Vox.DIR_Z[a])] == 1
		BlockDB.M_LADDER:
			var f: Vector3i = Vox.H_FACING_VEC[meta & 3]
			return BlockDB.full[world.get_id(x - f.x, y, z - f.z)] == 1
		BlockDB.M_WIRE, BlockDB.M_RAIL, BlockDB.M_REPEATER, BlockDB.M_COMPARATOR, BlockDB.M_PLATE, BlockDB.M_CARPET:
			return BlockDB.solid[bid] == 1 and (BlockDB.full[bid] == 1 or BlockDB.model[bid] in [BlockDB.M_SLAB, BlockDB.M_STAIRS, BlockDB.M_PATH, BlockDB.M_SNOW, BlockDB.M_CARPET, BlockDB.M_LEAVES, BlockDB.M_CHEST] or m == BlockDB.M_CARPET)
		BlockDB.M_SNOW:
			return BlockDB.full[bid] == 1 or BlockDB.model[bid] == BlockDB.M_LEAVES
		BlockDB.M_LANTERN:
			if (meta & 1) == 1:
				return BlockDB.solid[world.get_id(x, y + 1, z)] == 1 or BlockDB.model[world.get_id(x, y + 1, z)] == BlockDB.M_CHAIN
			return BlockDB.solid[bid] == 1
		BlockDB.M_DOOR:
			if (meta & 8) == 0:
				return BlockDB.full[bid] == 1
			return (below & 0xFFF) == (v & 0xFFF)
		BlockDB.M_CROSS, BlockDB.M_TALL_CROSS, BlockDB.M_CROP, BlockDB.M_SHORT, BlockDB.M_LILY:
			if d.place == "hanging":
				var above := world.get_block(x, y + 1, z)
				return BlockDB.solid[above & 0xFFF] == 1 or (above & 0xFFF) == (v & 0xFFF)
			if m == BlockDB.M_TALL_CROSS and (meta & 1) == 1:
				return bid == (v & 0xFFF)
			if d.waterlogged:
				return BlockDB.solid[bid] == 1 or bid == (v & 0xFFF)
			if d.name == "sugar_cane":
				return bid == (v & 0xFFF) or BlockPlacement.support_ok(world, d, Vector3i(x, y, z), meta)
			if d.place == "plant_up":
				return BlockDB.solid[bid] == 1 or bid == (v & 0xFFF)
			if m == BlockDB.M_SHORT and d.solid:
				return true
			return BlockPlacement.support_ok(world, d, Vector3i(x, y, z), meta)
		BlockDB.M_CACTUS:
			return BlockPlacement.support_ok(world, d, Vector3i(x, y, z), meta)
		BlockDB.M_BAMBOO:
			return bid == (v & 0xFFF) or BlockPlacement.support_ok(world, d, Vector3i(x, y, z), meta)
		BlockDB.M_VINE:
			return true
		BlockDB.M_DRIPSTONE, BlockDB.M_SPIKE:
			if (meta & 1) == 1:
				return BlockDB.solid[bid] == 1
			var ab := world.get_id(x, y + 1, z)
			return BlockDB.solid[ab] == 1
		BlockDB.M_FIRE:
			return BlockDB.solid[bid] == 1 or _flammable_near(world, x, y, z)
	return true


static func _flammable_near(world: World, x: int, y: int, z: int) -> bool:
	for dd in 6:
		var n := world.get_block(x + Vox.DIR_X[dd], y + Vox.DIR_Y[dd], z + Vox.DIR_Z[dd])
		if n != 0 and D(n).flammable:
			return true
	return false


static func _door_pair_check(world: World, x: int, y: int, z: int, v: int) -> void:
	var upper := (_meta(v) & 8) != 0
	var other := world.get_block(x, y - 1 if upper else y + 1, z)
	if (other & 0xFFF) != (v & 0xFFF):
		world.set_block(x, y, z, 0, World.F_DEFAULT)


static func _bed_pair_check(world: World, x: int, y: int, z: int, v: int) -> void:
	var meta := _meta(v)
	var fvec: Vector3i = Vox.H_FACING_VEC[meta & 3]
	var head := (meta & 4) != 0
	var op := Vector3i(x, y, z) + (-fvec if head else fvec)
	if (world.get_block(op.x, op.y, op.z) & 0xFFF) != (v & 0xFFF):
		world.set_block(x, y, z, 0, World.F_DEFAULT)


# ------------------------------------------------------------------------------------------------
static func scheduled_tick(world: World, x: int, y: int, z: int, v: int) -> void:
	if v == 0:
		return
	var d := D(v)
	if d.fluid != 0:
		Fluids.tick(world, x, y, z, v)
		return
	if d.gravity:
		var below := world.get_block(x, y - 1, z)
		var bid := below & 0xFFF
		if y > world.min_y and (below == 0 or BlockDB.replaceable[bid] == 1 or BlockDB.fluid[bid] != 0):
			world.set_block(x, y, z, 0, World.F_DEFAULT)
			if world.session != null:
				world.session.entities.spawn_falling_block(world, Vector3(x + 0.5, y, z + 0.5), v)
		return
	if d.tick == "redstone" or RedstoneSystem.is_component(v):
		RedstoneSystem.scheduled_tick(world, x, y, z, v)
		return
	match d.tick:
		"plate":
			RedstoneSystem.plate_tick(world, x, y, z, v)
		"fire":
			Fire.tick(world, x, y, z, v)
			if world.get_block(x, y, z) == v or BlockDB.model[world.get_id(x, y, z)] == BlockDB.M_FIRE:
				world.schedule_tick(x, y, z, 30 + _rng.randi_range(0, 10))
		"spawner", "trial_spawner":
			pass


# ------------------------------------------------------------------------------------------------
static func random_tick(world: World, x: int, y: int, z: int, v: int) -> void:
	var d := D(v)
	var meta := _meta(v)
	var r := _rng.randf()
	match d.tick:
		"daylight_detector":
			RedstoneSystem.update_daylight(world, Vector3i(x, y, z), v)
			world.schedule_tick(x, y, z, 20)
		"grass", "mycelium":
			var above := world.get_block(x, y + 1, z)
			var aid := above & 0xFFF
			var l := world.get_light(x, y + 1, z)
			if BlockDB.light_opacity[aid] >= 15 and aid != I["snow"]:
				world.set_block(x, y, z, I["dirt"])
				return
			if maxi(l.x, l.y) >= 9:
				for k in 4:
					var tx := x + _rng.randi_range(-1, 1)
					var ty := y + _rng.randi_range(-3, 1)
					var tz := z + _rng.randi_range(-1, 1)
					var t := world.get_block(tx, ty, tz)
					if (t & 0xFFF) == I["dirt"]:
						var ta := world.get_block(tx, ty + 1, tz)
						var tl := world.get_light(tx, ty + 1, tz)
						if BlockDB.light_opacity[ta & 0xFFF] < 15 and maxi(tl.x, tl.y) >= 4:
							world.set_block(tx, ty, tz, v & 0xFFF)
		"crop", "stem":
			_grow_crop(world, x, y, z, v, d, 1)
		"sapling":
			var l2 := world.get_light(x, y + 1, z)
			if maxi(l2.x, l2.y) >= 9 and r < 0.15:
				grow_tree(world, x, y, z, v)
		"leaves":
			if (meta & 1) == 0 and not _log_near(world, x, y, z, 6):
				break_naturally(world, Vector3i(x, y, z), v)
		"sugar_cane", "cactus":
			if world.get_block(x, y + 1, z) == 0:
				var h := 1
				while (world.get_block(x, y - h, z) & 0xFFF) == (v & 0xFFF) and h < 4:
					h += 1
				if h < 3:
					if meta >= 15:
						world.set_block(x, y + 1, z, v & 0xFFF)
						world.set_block(x, y, z, v & 0xFFF, World.F_NOTIFY)
					else:
						world.set_block(x, y, z, Vox.make(v & 0xFFF, meta + 1), 0)
		"bamboo":
			if world.get_block(x, y + 1, z) == 0 and r < 0.3:
				var h2 := 1
				while (world.get_block(x, y - h2, z) & 0xFFF) == (v & 0xFFF) and h2 < 16:
					h2 += 1
				if h2 < 14:
					world.set_block(x, y + 1, z, Vox.make(v & 0xFFF, 2))
					world.set_block(x, y, z, Vox.make(v & 0xFFF, 1), 0)
		"kelp":
			var up := world.get_block(x, y + 1, z)
			if BlockDB.fluid[up & 0xFFF] == 1 and ((up >> 12) & 15) == 0 and r < 0.14:
				world.set_block(x, y + 1, z, v & 0xFFF)
		"ice":
			var l3 := world.get_light(x, y, z)
			if l3.y > 11:
				world.set_block(x, y, z, I["water"] if world.dim != 1 else 0)
		"snow_melt":
			var l4 := world.get_light(x, y, z)
			if l4.y > 11:
				world.set_block(x, y, z, 0)
		"farmland":
			var wet := _water_near(world, x, y, z, 4)
			if wet and (meta & 7) < 7:
				world.set_block(x, y, z, Vox.make(v & 0xFFF, 7), 0)
			elif not wet:
				if (meta & 7) > 0:
					world.set_block(x, y, z, Vox.make(v & 0xFFF, (meta & 7) - 1), 0)
				elif D(world.get_block(x, y + 1, z)).model != BlockDB.M_CROP:
					world.set_block(x, y, z, I["dirt"])
		"mushroom":
			if r < 0.04:
				var tx2 := x + _rng.randi_range(-1, 1)
				var tz2 := z + _rng.randi_range(-1, 1)
				var ty2 := y + _rng.randi_range(-1, 1)
				if world.get_block(tx2, ty2, tz2) == 0 and BlockDB.full[world.get_id(tx2, ty2 - 1, tz2)] == 1:
					var ll := world.get_light(tx2, ty2, tz2)
					if maxi(ll.x, ll.y) < 13:
						world.set_block(tx2, ty2, tz2, v & 0xFFF)
		"cave_vines":
			if (meta & 1) == 0 and r < 0.1:
				world.set_block(x, y, z, Vox.make(v & 0xFFF, 1), 0)
			elif world.get_block(x, y - 1, z) == 0 and r < 0.1:
				world.set_block(x, y - 1, z, v & 0xFFF)
		"weeping_vines":
			if world.get_block(x, y - 1, z) == 0 and r < 0.1:
				world.set_block(x, y - 1, z, v & 0xFFF)
		"vine":
			if world.get_block(x, y - 1, z) == 0 and r < 0.1:
				world.set_block(x, y - 1, z, v)
		"budding":
			if r < 0.2:
				var dd := _rng.randi_range(0, 5)
				var p: Vector3i = Vector3i(x, y, z) + Vox.DIR_VEC[dd]
				var cur := world.get_block(p.x, p.y, p.z)
				var stages := ["small_amethyst_bud", "medium_amethyst_bud", "large_amethyst_bud", "amethyst_cluster"]
				if cur == 0:
					world.set_block(p.x, p.y, p.z, Vox.make(BlockDB.id(stages[0]), dd))
				else:
					var cn := D(cur).name
					var si := stages.find(cn)
					if si >= 0 and si < 3 and ((cur >> 12) & 7) == dd:
						world.set_block(p.x, p.y, p.z, Vox.make(BlockDB.id(stages[si + 1]), dd))
		"oxidize":
			if r < 0.02:
				var nm := d.name
				var nxt := ""
				if nm.begins_with("exposed_"):
					nxt = "weathered_" + nm.substr(8)
				elif nm.begins_with("weathered_"):
					nxt = "oxidized_" + nm.substr(10)
				elif not nm.begins_with("oxidized_") and not nm.begins_with("waxed_"):
					nxt = "exposed_" + (nm.replace("copper_block", "copper") if nm == "copper_block" else nm)
				if nxt != "" and BlockDB.has(nxt):
					world.set_block(x, y, z, Vox.make(BlockDB.id(nxt), meta), World.F_NOTIFY)
		"coral":
			if not _touches_water(world, x, y, z) and r < 0.5:
				var dead := "dead_" + d.name
				if BlockDB.has(dead):
					world.set_block(x, y, z, BlockDB.id(dead))
		"redstone_ore":
			if (meta & 1) == 1:
				world.set_block(x, y, z, v & 0xFFF, 0)
		"geyser":
			SulfurCaves.geyser_tick(world, x, y, z)
		"chorus_flower":
			if meta < 5 and world.get_block(x, y + 1, z) == 0 and r < 0.2:
				world.set_block(x, y, z, I["chorus_plant"])
				world.set_block(x, y + 1, z, Vox.make(v & 0xFFF, meta + 1))
	if d.fluid == 2 and world.dim != 2:
		Fire.lava_spread(world, x, y, z)


static func _grow_crop(world: World, x: int, y: int, z: int, v: int, d: BlockDef, steps: int) -> void:
	var meta := _meta(v)
	var max_age: int = d.props.get("max_age", 7)
	var l := world.get_light(x, y + 1, z)
	if maxi(l.x, l.y) < 9 and d.name != "nether_wart":
		return
	if d.name == "nether_wart" and _rng.randf() > 0.1:
		return
	var soil := world.get_block(x, y - 1, z)
	var moist := (_meta(soil) & 7) > 0
	var chance := 0.33 if moist else 0.15
	if _rng.randf() > chance:
		return
	if meta < max_age:
		world.set_block(x, y, z, Vox.make(v & 0xFFF, mini(max_age, meta + steps)), 0)
	elif d.props.get("stem", false):
		var fruit := BlockDB.id(String(d.props.get("fruit", "")))
		var f := _rng.randi_range(0, 3)
		var hv: Vector3i = Vox.H_FACING_VEC[f]
		var tx := x + hv.x
		var tz := z + hv.z
		var below := world.get_id(tx, y - 1, tz)
		if world.get_block(tx, y, tz) == 0 and (below == I["dirt"] or below == I["grass_block"] or below == I["farmland"]):
			world.set_block(tx, y, tz, fruit)


static func _log_near(world: World, x: int, y: int, z: int, r: int) -> bool:
	for dy in range(-r + 2, r - 1):
		for dz in range(-r + 2, r - 1):
			for dx in range(-r + 2, r - 1):
				var id := world.get_id(x + dx, y + dy, z + dz)
				if id != 0 and BlockDB.defs[id].tags.has("logs"):
					return true
	return false


static func _water_near(world: World, x: int, y: int, z: int, r: int) -> bool:
	for dz in range(-r, r + 1):
		for dx in range(-r, r + 1):
			for dy in range(0, 2):
				var id := world.get_id(x + dx, y + dy, z + dz)
				if BlockDB.fluid[id] == 1 or BlockDB.waterlogged[id] == 1:
					return true
	return false


static func _lava_contact(world: World, x: int, y: int, z: int, v: int) -> void:
	# lava next to water -> obsidian (source) / cobblestone (flowing); lava onto water below -> stone
	for dd in 6:
		if dd == Vox.DOWN:
			continue
		var n := world.get_block(x + Vox.DIR_X[dd], y + Vox.DIR_Y[dd], z + Vox.DIR_Z[dd])
		if BlockDB.fluid[n & 0xFFF] == 1:
			var src := _meta(v) == 0
			world.set_block(x, y, z, I["obsidian"] if src else I["cobblestone"])
			Sfx.play_at("lava_pop", Vector3(x, y, z) + Vector3(0.5, 0.5, 0.5))
			if world.session != null:
				world.session.particles.smoke(Vector3(x + 0.5, y + 1.0, z + 0.5), 6, true)
			return


# ------------------------------------------------------------------------------------------------
static func grow_tree(world: World, x: int, y: int, z: int, v: int) -> bool:
	var d := D(v)
	var tree: String = d.props.get("tree", "oak")
	var kind := tree
	match tree:
		"oak":
			kind = "fancy_oak" if _rng.randf() < 0.1 else "oak"
		"spruce":
			kind = "spruce"
			if _is_2x2(world, x, y, z, v & 0xFFF):
				kind = "mega_spruce"
		"jungle":
			kind = "mega_jungle" if _is_2x2(world, x, y, z, v & 0xFFF) else "jungle"
		"dark_oak", "pale_oak":
			if not _is_2x2(world, x, y, z, v & 0xFFF):
				return false
		"crimson_fungus", "warped_fungus":
			kind = tree
	var rng := RandomNumberGenerator.new()
	rng.seed = hash(Vector3i(x, y, z)) ^ Time.get_ticks_usec()
	# write the tree through a temporary per-chunk writer so the generator code can be reused
	var touched := {}
	for dz in range(-2, 3):
		for dx in range(-2, 3):
			var c := world.chunk_at(x + dx * 8, z + dz * 8)
			if c != null:
				touched[c.key()] = c
	world.set_block(x, y, z, 0, 0)
	var changes := []
	for k in touched:
		var c2: Chunk = touched[k]
		var before := c2.blocks.duplicate()
		var w := WorldGen.Writer.new(c2)
		rng.seed = hash(Vector3i(x, y, z))
		Trees.place(w, kind, x, y, z, rng)
		# diff and re-apply through the world so light + meshes update
		var b := c2.blocks
		var nb := before
		for i in b.size():
			if b[i] != nb[i]:
				changes.append([c2.cx * 16 + (i & 15), (i >> 8) + c2.min_y, c2.cz * 16 + ((i >> 4) & 15), b[i]])
		c2.blocks = before
	for ch in changes:
		world.set_block(ch[0], ch[1], ch[2], ch[3], World.F_URGENT)
	return not changes.is_empty()


static func _is_2x2(world: World, x: int, y: int, z: int, id: int) -> bool:
	return world.get_id(x + 1, y, z) == id and world.get_id(x, y, z + 1) == id and world.get_id(x + 1, y, z + 1) == id


# ------------------------------------------------------------------------------------------------
## Removes a block with side effects (doors, beds, containers dropping contents...). drops = spawn loot.
static func remove_block(world: World, pos: Vector3i, v: int, drops: bool, tool: ItemStack = null, player = null) -> void:
	var d := D(v)
	var meta := _meta(v)
	var be := world.remove_be(pos.x, pos.y, pos.z)
	var repl := 0
	if d.name == "ice" and drops and (tool == null or tool.enchant_level("silk_touch") == 0):
		var below := world.get_id(pos.x, pos.y - 1, pos.z)
		if BlockDB.solid[below] == 1 or BlockDB.fluid[below] != 0:
			repl = I["water"] if world.dim != 1 else 0
	if d.waterlogged:
		repl = I["water"]
	world.set_block(pos.x, pos.y, pos.z, repl, World.F_DEFAULT)
	# multi-block parts
	match d.model:
		BlockDB.M_DOOR:
			var other := pos + (Vector3i(0, -1, 0) if (meta & 8) != 0 else Vector3i(0, 1, 0))
			if (world.get_block(other.x, other.y, other.z) & 0xFFF) == (v & 0xFFF):
				world.set_block(other.x, other.y, other.z, 0, World.F_DEFAULT)
		BlockDB.M_BED:
			var fvec: Vector3i = Vox.H_FACING_VEC[meta & 3]
			var op := pos + (-fvec if (meta & 4) != 0 else fvec)
			if (world.get_block(op.x, op.y, op.z) & 0xFFF) == (v & 0xFFF):
				world.set_block(op.x, op.y, op.z, 0, World.F_DEFAULT)
		BlockDB.M_TALL_CROSS:
			var o2 := pos + (Vector3i(0, -1, 0) if (meta & 1) == 1 else Vector3i(0, 1, 0))
			if (world.get_block(o2.x, o2.y, o2.z) & 0xFFF) == (v & 0xFFF):
				world.set_block(o2.x, o2.y, o2.z, 0, World.F_DEFAULT)
		BlockDB.M_PISTON:
			if (meta & 8) != 0:
				var dvec: Vector3i = Vox.DIR_VEC[mini(meta & 7, 5)]
				var hp := pos + dvec
				if D(world.get_block(hp.x, hp.y, hp.z)).model == BlockDB.M_PISTON_HEAD:
					world.set_block(hp.x, hp.y, hp.z, 0, World.F_DEFAULT)
		BlockDB.M_PISTON_HEAD:
			var dvec2: Vector3i = Vox.DIR_VEC[mini(meta & 7, 5)]
			var bp := pos - dvec2
			var bv := world.get_block(bp.x, bp.y, bp.z)
			if D(bv).model == BlockDB.M_PISTON:
				world.set_block(bp.x, bp.y, bp.z, 0, World.F_DEFAULT)
				if drops:
					Loot.drop_block(world, bp, bv, null, player)
	if drops and world.session != null:
		# container contents
		if be.has("items") and not d.props.get("keep_contents", false):
			for it in be["items"]:
				if it is Dictionary and not (it as Dictionary).is_empty():
					var st := ItemStack.from_dict(it)
					if st != null:
						world.session.entities.spawn_item(world, Vector3(pos) + Vector3(0.5, 0.5, 0.5), st)
		if d.props.get("infested", false):
			world.session.entities.spawn_mob(world, "silverfish", Vector3(pos) + Vector3(0.5, 0, 0.5))


## Player breaking in survival: drops according to loot rules + XP.
static func player_break(world: World, pos: Vector3i, v: int, tool: ItemStack, player) -> void:
	var d := D(v)
	var be := world.get_be(pos.x, pos.y, pos.z).duplicate(true)
	remove_block(world, pos, v, true, tool, player)
	Loot.drop_block(world, pos, v, tool, player, be)


## Natural break (support lost, explosion without drops handled separately).
static func break_naturally(world: World, pos: Vector3i, v: int) -> void:
	var be := world.get_be(pos.x, pos.y, pos.z).duplicate(true)
	remove_block(world, pos, v, true)
	Loot.drop_block(world, pos, v, null, null, be)
	if world.session != null:
		world.session.particles.block_break(v, Vector3(pos))


static func on_placed(world: World, pos: Vector3i, v: int, player) -> void:
	var d := D(v)
	if d.model == BlockDB.M_RAIL:
		Rails.on_placed(world, pos)
	if d.tick == "daylight_detector":
		world.schedule_tick(pos.x, pos.y, pos.z, 20)
	if d.props.has("container"):
		var be := world.get_be(pos.x, pos.y, pos.z, true)
		if d.name == "hopper":
			be["type"] = "hopper"
			if world.session != null:
				world.session.register_ticking_be(world, pos)
		if not be.has("items"):
			if d.name != "hopper":
				be["type"] = "container"
			var sz: int = d.props["container"]
			var arr := []
			arr.resize(sz)
			for i in sz:
				arr[i] = {}
			be["items"] = arr
	if d.model == BlockDB.M_LEAVES:
		world.set_block(pos.x, pos.y, pos.z, Vox.make(v & 0xFFF, 1), 0)
	if d.name.ends_with("_head") or d.name.ends_with("_skull"):
		Summons.check_wither(world, pos)
	if d.name == "carved_pumpkin" or d.name == "jack_o_lantern":
		Summons.check_golem(world, pos)
	if d.name == "sponge":
		absorb_water(world, pos.x, pos.y, pos.z)
	if RedstoneSystem.is_component(v):
		RedstoneSystem.on_block_changed(world, pos.x, pos.y, pos.z, 0, v)


static func absorb_water(world: World, x: int, y: int, z: int) -> void:
	var q := [Vector3i(x, y, z)]
	var seen := {}
	var removed := 0
	while not q.is_empty() and removed < 65:
		var p: Vector3i = q.pop_front()
		for dd in 6:
			var n: Vector3i = p + Vox.DIR_VEC[dd]
			if seen.has(n) or n.distance_squared_to(Vector3i(x, y, z)) > 49:
				continue
			seen[n] = true
			var nv := world.get_block(n.x, n.y, n.z)
			if BlockDB.fluid[nv & 0xFFF] == 1 or D(nv).waterlogged:
				world.set_block(n.x, n.y, n.z, 0)
				removed += 1
				q.append(n)
	if removed > 0:
		world.set_block(x, y, z, BlockDB.id("wet_sponge"))
