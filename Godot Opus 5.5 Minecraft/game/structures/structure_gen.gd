class_name StructureGen
extends RefCounted
## Builders for every structure kind. Each returns a StructureLayout (block placements bucketed per
## chunk) plus chest block entities carrying a loot table name. Everything is deterministic from
## (seed, start chunk), so a structure is identical no matter which chunk pulls it in.

const CHEST := "chest"
const AIR := 0


# ------------------------------------------------------------------------------------------------
# Overworld
# ------------------------------------------------------------------------------------------------

## Village: a plaza with paths, houses, farms, a well and lamp posts.
static func village(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "village"
	var rng := _rng(seed_value, start, 11)
	var cx := start.x * 16 + 8
	var cz := start.y * 16 + 8
	var cy := g.surface_height(cx, cz)
	if cy < g.sea_level + 1:
		l.empty = true
		return l
	var biome := BiomeDB.name_of(g.biome_at(cx, cz))
	var wood := "oak"
	var plank := "oak_planks"
	var log := "oak_log"
	var wall := "cobblestone"
	if biome == "desert":
		wood = "sandstone"
		plank = "smooth_sandstone"
		log = "cut_sandstone"
		wall = "sandstone"
	elif biome == "savanna":
		wood = "acacia"
		plank = "acacia_planks"
		log = "acacia_log"
	elif biome == "taiga" or biome == "snowy_plains":
		wood = "spruce"
		plank = "spruce_planks"
		log = "spruce_log"
	l.origin = Vector3i(cx, cy, cz)
	# ground: flatten the plaza and lay paths that follow the terrain
	_flat(l, cx - 9, cz - 9, cx + 9, 0, cz + 9, cy, "grass_block" if biome != "desert" else "sand", "dirt")
	for i in 7:
		var ang := TAU * float(i) / 7.0
		var ex := cx + int(round(cos(ang) * 26.0))
		var ez := cz + int(round(sin(ang) * 26.0))
		_line(l, cx, cy, cz, ex, cy, ez, 2, "dirt_path", g)
	# well in the middle
	_well(l, cx, cy, cz, wall)
	# houses around the plaza
	for i in 8:
		var ang2 := TAU * float(i) / 8.0 + rng.randf_range(-0.2, 0.2)
		var hx := cx + int(round(cos(ang2) * rng.randf_range(16.0, 28.0)))
		var hz := cz + int(round(sin(ang2) * rng.randf_range(16.0, 28.0)))
		var hy := g.surface_height(hx, hz)
		if hy < g.sea_level:
			continue
		var kind := i % 3
		match kind:
			0:
				_house(l, hx, hy, hz, plank, log, wall, rng, true)
			1:
				_house(l, hx, hy, hz, plank, log, wall, rng, false)
			_:
				_farm(l, hx, hy, hz, log, rng)
	# lamp posts along the paths
	for i in 7:
		var ang3 := TAU * float(i) / 7.0 + 0.4
		var lx := cx + int(round(cos(ang3) * 14.0))
		var lz := cz + int(round(sin(ang3) * 14.0))
		var ly := g.surface_height(lx, lz)
		l.put_name(lx, ly + 1, lz, log, 0, 2)
		l.put_name(lx, ly + 2, lz, log, 0, 2)
		l.put_name(lx, ly + 3, lz, "torch", 0, 1)
	# villager spawns
	l.entity("structure_mob", Vector3(cx + 0.5, float(cy) + 1.0, cz + 0.5), {"mob": "villager"})
	for i in 4:
		var vx := cx + rng.randi_range(-20, 20)
		var vz := cz + rng.randi_range(-20, 20)
		var vy := g.surface_height(vx, vz)
		if vy >= g.sea_level:
			l.entity("structure_mob", Vector3(vx + 0.5, float(vy) + 1.0, vz + 0.5), {"mob": "villager" if rng.randf() < 0.8 else "iron_golem"})
	return l


static func _well(l: StructureLayout, x: int, y: int, z: int, wall: String) -> void:
	l.fill(x - 2, y - 4, z - 2, x + 2, y, z + 2, wall, 0)
	l.fill(x - 1, y - 4, z - 1, x + 1, y - 1, z + 1, AIR, 0)
	l.put_name(x, y - 4, z, "water", 0, 0)
	l.put_name(x, y - 3, z, "water", 0, 0)
	l.fill(x - 1, y + 1, z - 1, x + 1, y + 1, z + 1, wall, 0)
	l.fill(x, y + 1, z, x, y + 1, z, AIR, 0)
	for i in 4:
		var dx: int = [-2, 2, 2, -2][i]
		var dz: int = [-2, -2, 2, 2][i]
		l.put_name(x + dx, y + 1, z + dz, "oak_fence", 0, 2)
		l.put_name(x + dx, y + 2, z + dz, "oak_fence", 0, 2)
	l.fill(x - 2, y + 3, z - 2, x + 2, y + 3, z + 2, "oak_slab", 0, 1)


static func _house(l: StructureLayout, x: int, y: int, z: int, plank: String, log: String, wall: String,
		rng: RandomNumberGenerator, small: bool) -> void:
	var w := 5 if small else 7
	var d := 5 if small else 7
	var h := 4 if small else 5
	var x0 := x - w / 2
	var z0 := z - d / 2
	var x1 := x0 + w - 1
	var z1 := z0 + d - 1
	# foundation down to the terrain, floor + walls
	l.fill(x0, y - 4, z0, x1, y - 1, z1, wall, 1)
	l.fill(x0, y, z0, x1, y, z1, plank, 0)
	l.hollow(x0, y + 1, z0, x1, y + h, z1, plank, AIR, 0)
	# corner posts
	for dx in [x0, x1]:
		for dz in [z0, z1]:
			l.fill(dx, y + 1, dz, dx, y + h, dz, log, 0)
	# roof: stepped planks
	for i in range(0, h):
		var inset := i / 2
		if x0 + inset > x1 - inset:
			break
		l.fill(x0 + inset, y + h + i, z0 + inset, x1 - inset, y + h + i, z1 - inset, "oak_slab" if i % 2 == 0 else plank, 0, 1)
	# door
	var door_z := z1
	if rng.randf() < 0.5:
		door_z = z0
	l.put_name(x, y + 1, door_z, "oak_door", 0, 0)
	l.put_name(x, y + 2, door_z, "oak_door", 8, 0)
	# windows
	for i in 2:
		l.put_name(x0 + 1 + i, y + 2, z0, "glass", 0, 1)
		l.put_name(x0 + 1 + i, y + 2, z1, "glass", 0, 1)
		l.put_name(x0, y + 2, z0 + 1 + i, "glass", 0, 1)
		l.put_name(x1, y + 2, z0 + 1 + i, "glass", 0, 1)
	# interior: bed, crafting table, chest with village loot
	l.put_name(x0 + 1, y + 1, z0 + 1, "crafting_table", 0, 0)
	l.chest(x1 - 1, y + 1, z0 + 1, "village")
	l.put_name(x0 + 1, y + 1, z1 - 1, "red_bed", 0, 0)
	l.put_name(x0 + 1, y + 1, z1 - 2, "red_bed", 8, 0)
	# torch
	l.put_name(x, y + h - 1, z, "torch", 0, 1)


static func _farm(l: StructureLayout, x: int, y: int, z: int, log: String, rng: RandomNumberGenerator) -> void:
	var w := 7
	var d := 5
	var x0 := x - w / 2
	var z0 := z - d / 2
	var x1 := x0 + w - 1
	var z1 := z0 + d - 1
	l.fill(x0, y, z0, x1, y, z1, "farmland", 0, 0)
	for dz in range(z0, z1 + 1):
		for dx in range(x0, x1 + 1):
			var c := "wheat"
			var r := rng.randf()
			if r < 0.2:
				c = "carrots"
			elif r < 0.35:
				c = "potatoes"
			elif r < 0.45:
				c = "beetroots"
			l.put_name(dx, y + 1, dz, c, 7, 1)
	# water channel down the middle
	for dz in range(z0, z1 + 1):
		l.put_name(x0 + w / 2, y, dz, "water", 0, 0)
	# fence around
	for dx in range(x0 - 1, x1 + 2):
		l.put_name(dx, y + 1, z0 - 1, log, 0, 2)
		l.put_name(dx, y + 1, z1 + 1, log, 0, 2)
	for dz2 in range(z0, z1 + 1):
		l.put_name(x0 - 1, y + 1, dz2, log, 0, 2)
		l.put_name(x1 + 1, y + 1, dz2, log, 0, 2)


## Abandoned mineshaft: a long corridor with supports, rails and side tunnels.
static func mineshaft(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "mineshaft"
	var rng := _rng(seed_value, start, 12)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := 24 + rng.randi_range(-6, 8)
	if g.dim_id == 1:
		y = 40 + rng.randi_range(-8, 8)
	l.origin = Vector3i(x, y, z)
	var len := rng.randi_range(60, 130)
	var dir := rng.randi_range(0, 3)
	var dx: int = [1, 0, -1, 0][dir]
	var dz: int = [0, 1, 0, -1][dir]
	var wood := "oak_fence"
	for i in len:
		var px := x + dx * i
		var pz := z + dz * i
		# corridor 3x3
		for a in range(-1, 2):
			for b in range(-1, 3):
				var qx := px + (dz * a)
				var qz := pz + (dx * a)
				if b < 0:
					l.put_name(qx, y + b, qz, "cobblestone", 0, 0)
				else:
					l.put_name(qx, y + b, qz, AIR, 0, 0)
		# rails on the floor
		if i % 2 == 0:
			l.put_name(px, y - 1, pz, "oak_planks", 0, 0)
			if i % 8 == 0:
				l.put_name(px, y, pz, "rail", 0, 0)
			else:
				l.put_name(px, y, pz, "rail", 0, 1)
		# supports every 8 blocks
		if i % 8 == 0:
			for a2: int in [-1, 1]:
				var sx: int = px + dz * a2
				var sz: int = pz + dx * a2
				l.put_name(sx, y, sz, wood, 0, 2)
				l.put_name(sx, y + 1, sz, wood, 0, 2)
				l.put_name(sx, y + 2, sz, wood, 0, 2)
			for a3 in range(-1, 2):
				l.put_name(px + dz * a3, y + 2, pz + dx * a3, "oak_planks", 0, 0)
			if rng.randf() < 0.3:
				l.put_name(px + dz, y + 2, pz + dx, "torch", 0, 1)
		# side branches
		if i % 20 == 10:
			var side := 1 if rng.randf() < 0.5 else -1
			var blen := rng.randi_range(8, 22)
			var bx := px
			var bz := pz
			for j in blen:
				bx += dz * side
				bz += dx * side
				l.fill(bx, y - 1, bz, bx, y + 1, bz, AIR, 0)
				l.put_name(bx, y - 2, bz, "oak_planks", 0, 0)
			l.chest(bx, y - 1, bz, "mineshaft")
	# cave spider spawner at the far end
	var ex := x + dx * (len - 2)
	var ez := z + dz * (len - 2)
	l.put_name(ex, y, ez, "spawner", 0, 0)
	l.block_entity(ex, y, ez, {"type": "spawner", "mob": "cave_spider"})
	l.block_entity(x + dx * 10, y, z + dz * 10, {"type": "spawner", "mob": "cave_spider"})
	return l


## Ruined portal: a broken obsidian frame with a loot chest and netherrack spill.
static func ruined_portal(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "ruined_portal"
	var rng := _rng(seed_value, start, 13)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := g.surface_height(x, z)
	if y < g.sea_level:
		y = g.sea_level + 1
	l.origin = Vector3i(x, y, z)
	var w := 3 if rng.randf() < 0.5 else 4
	var h := 4
	# frame with random gaps
	for i in range(-1, w + 1):
		for j in range(-1, h + 1):
			var edge := i == -1 or i == w or j == -1 or j == h
			if not edge:
				continue
			if rng.randf() < 0.22:
				continue
			l.put_name(x + i, y + j, z, "obsidian", 0, 0)
	# rubble
	for i in 30:
		var rx := x + rng.randi_range(-5, 7)
		var rz := z + rng.randi_range(-5, 5)
		var ry := g.surface_height(rx, rz)
		if rng.randf() < 0.6:
			l.put_name(rx, ry + 1, rz, "netherrack", 0, 0)
		else:
			l.put_name(rx, ry + 1, rz, "magma_block", 0, 0)
	l.chest(x + w + 1, y + 1, z, "ruined_portal")
	# gold blocks under the chest like the original
	if rng.randf() < 0.4:
		l.put_name(x + w + 1, y, z, "gold_block", 0, 0)
	# a portal block when the roll wants a live one
	if rng.randf() < 0.25:
		l.put_name(x + 1, y + 1, z, "nether_portal", 0, 0)
		l.put_name(x + 1, y + 2, z, "nether_portal", 0, 0)
	return l


## Desert pyramid with the hidden treasure chamber.
static func desert_pyramid(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "desert_pyramid"
	var rng := _rng(seed_value, start, 14)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := g.surface_height(x, z)
	l.origin = Vector3i(x, y, z)
	var s := "sandstone"
	var ss := "smooth_sandstone"
	# stepped pyramid 21x21 base
	for level in 7:
		var r := 10 - level
		var yy := y + level
		for dx in range(-r, r + 1):
			for dz in range(-r, r + 1):
				if absi(dx) != r and absi(dz) != r:
					continue
				var b := s if level % 2 == 0 else ss
				l.put_name(x + dx, yy, z + dz, b, 0, 0)
		l.fill(x - r + 1, y + level, z - r + 1, x + r - 1, y + level, z + r - 1, s if level < 6 else "sandstone", 0, 0)
	# entrance corridor and treasure room
	l.fill(x, y + 1, z + 8, x, y + 3, z + 10, AIR, 0)
	for dz2 in range(0, 9):
		l.fill(x - 1, y, z + dz2, x + 1, y + 3, z + dz2, AIR, 0)
		l.put_name(x - 2, y + 1, z + dz2, "sandstone", 0, 0)
		l.put_name(x + 2, y + 1, z + dz2, "sandstone", 0, 0)
	l.fill(x - 3, y - 4, z - 3, x + 3, y, z + 3, AIR, 0)
	l.fill(x - 4, y - 4, z - 4, x + 4, y - 2, z + 4, "sandstone", 1)
	l.fill(x - 3, y - 5, z - 3, x + 3, y - 4, z + 3, "sandstone", 0)
	# four chests in the middle
	for i in 4:
		l.chest(x + [1, -1, 1, -1][i], y - 1, z + [1, 1, -1, -1][i], "desert_pyramid")
	l.put_name(x, y - 3, z, "stone_pressure_plate", 0, 0)
	l.put_name(x, y - 4, z, "tnt", 0, 0)
	l.put_name(x, y - 4, z - 1, "tnt", 0, 0)
	l.put_name(x, y - 4, z + 1, "tnt", 0, 0)
	l.put_name(x - 1, y - 4, z, "tnt", 0, 0)
	l.put_name(x, y - 4, z - 2, "tnt", 0, 0)
	return l


## Jungle temple with a stairway into a hidden chamber.
static func jungle_temple(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "jungle_temple"
	var rng := _rng(seed_value, start, 15)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := g.surface_height(x, z)
	l.origin = Vector3i(x, y, z)
	var base := "mossy_cobblestone"
	var b2 := "chiseled_stone_bricks"
	# stepped base
	for level in 3:
		var r := 6 - level
		for dx in range(-r, r + 1):
			for dz in range(-r, r + 1):
				if absi(dx) != r and absi(dz) != r:
					continue
				l.put_name(x + dx, y + level, z + dz, base if (dx + dz + level) % 3 != 0 else b2, 0, 0)
	l.fill(x - 4, y + 1, z - 4, x + 4, y + 4, z + 4, base, 0)
	l.fill(x - 3, y + 1, z - 3, x + 3, y + 3, z + 3, AIR, 0)
	l.fill(x - 5, y + 4, z - 5, x + 5, y + 4, z + 5, base, 0)
	# tower
	for h in range(0, 7):
		l.fill(x - 2, y + 5 + h, z - 2, x + 2, y + 5 + h, z + 2, base, 1)
	l.fill(x - 1, y + 5, z - 1, x + 1, y + 10, z + 1, AIR, 0)
	# hidden chamber below
	l.fill(x - 4, y - 6, z - 4, x + 4, y - 2, z + 4, AIR, 0)
	l.fill(x - 5, y - 6, z - 5, x + 5, y - 2, z + 5, base, 1)
	for i in 4:
		l.fill(x - 4, y - 6 + i, z - 4 + i, x - 4, y - 6 + i, z + 4 - i, base, 0)
	l.chest(x + 2, y - 5, z, "jungle_temple")
	l.chest(x - 1, y - 5, z - 2, "jungle_temple")
	l.put_name(x, y - 5, z, "chiseled_stone_bricks", 0, 0)
	# vine decor
	for i in 20:
		var vx := x + rng.randi_range(-6, 6)
		var vz := z + rng.randi_range(-6, 6)
		l.put_name(vx, y + 5 + rng.randi_range(0, 3), vz, "vine", 0, 1)
	return l


## Witch hut on stilts over swamp water.
static func witch_hut(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "witch_hut"
	var rng := _rng(seed_value, start, 16)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := g.surface_height(x, z)
	if y < g.sea_level:
		y = g.sea_level
	l.origin = Vector3i(x, y, z)
	var wood := "spruce_log"
	var plank := "spruce_planks"
	# stilts
	for i in 4:
		var dx: int = [-3, 3, 3, -3][i]
		var dz: int = [-3, -3, 3, 3][i]
		l.fill(x + dx, y - 3, z + dz, x + dx, y + 2, z + dz, wood, 0)
	l.fill(x - 4, y + 3, z - 4, x + 4, y + 3, z + 4, plank, 0)
	l.hollow(x - 3, y + 4, z - 3, x + 3, y + 6, z + 3, plank, AIR, 0)
	for i in 3:
		l.fill(x - 4 + i, y + 7 + i, z - 4 + i, x + 4 - i, y + 7 + i, z + 4 - i, wood, 0)
	# cauldron + crafting table + chest
	l.put_name(x + 2, y + 4, z + 2, "cauldron", 0, 0)
	l.put_name(x - 2, y + 4, z - 2, "crafting_table", 0, 0)
	l.chest(x - 2, y + 4, z + 2, "swamp_hut")
	l.put_name(x, y + 6, z, "redstone_torch", 0, 1)
	l.entity("structure_mob", Vector3(x + 0.5, float(y) + 4.0, z + 0.5), {"mob": "witch"})
	l.entity("structure_mob", Vector3(x + 2.5, float(y) + 1.0, z + 0.5), {"mob": "cat"})
	return l


## Igloo: a small snow dome with a hidden basement (zombie villager + loot).
static func igloo(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "igloo"
	var rng := _rng(seed_value, start, 17)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := g.surface_height(x, z)
	l.origin = Vector3i(x, y, z)
	var snow := "snow_block"
	var ice := "packed_ice"
	for layer in 4:
		var r := 3 - layer
		if r < 0:
			break
		for dx in range(-r, r + 1):
			for dz in range(-r, r + 1):
				if maxi(absi(dx), absi(dz)) <= r:
					l.put_name(x + dx, y + layer, z + dz, snow, 0, 0 if layer == 0 else 1)
	l.fill(x - 1, y, z - 1, x + 1, y + 2, z + 1, AIR, 0)
	l.fill(x - 2, y + 3, z - 2, x + 2, y + 3, z + 2, snow, 0)
	l.put_name(x, y, z + 2, "snow", 0, 1)
	l.put_name(x - 1, y + 1, z - 2, ice, 0, 0)
	l.put_name(x + 1, y + 1, z - 2, ice, 0, 0)
	l.put_name(x, y + 2, z, "torch", 0, 1)
	l.put_name(x + 1, y + 1, z + 1, "red_bed", 0, 0)
	l.put_name(x + 1, y + 1, z, "red_bed", 8, 0)
	l.put_name(x - 1, y + 1, z + 1, "crafting_table", 0, 0)
	l.put_name(x - 2, y + 1, z + 1, "furnace", 0, 0)
	# basement with a ladder shaft
	l.fill(x + 1, y - 6, z - 4, x + 5, y - 2, z, AIR, 0)
	l.hollow(x + 1, y - 7, z - 4, x + 5, y - 2, z, "cobblestone", AIR, 0)
	l.fill(x + 3, y - 2, z - 3, x + 3, y, z - 3, AIR, 0)
	for i in 3:
		l.put_name(x + 3, y - 1 - i, z - 3, "ladder", 0, 1)
	l.chest(x + 2, y - 6, z - 1, "igloo")
	l.entity("structure_mob", Vector3(x + 3.5, float(y) - 6.0, z - 2.5), {"mob": "zombie_villager", "baby": false})
	return l


## Pillager outpost: a tall watchtower with a cage and a loot chest.
static func pillager_outpost(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "pillager_outpost"
	var rng := _rng(seed_value, start, 18)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := g.surface_height(x, z)
	l.origin = Vector3i(x, y, z)
	var log := "dark_oak_log"
	var plank := "dark_oak_planks"
	l.fill(x - 4, y, z - 4, x + 4, y, z + 4, plank, 0)
	for h in range(0, 14):
		l.hollow(x - 3, y + 1 + h, z - 3, x + 3, y + 1 + h, z + 3, log if h % 4 == 3 else plank, AIR, 0)
	# the walkway boxes
	l.fill(x - 5, y + 11, z - 5, x + 5, y + 11, z + 5, plank, 0)
	l.fill(x - 5, y + 11, z - 5, x + 5, y + 11, z - 5, "dark_oak_fence", 1)
	l.fill(x - 5, y + 11, z + 5, x + 5, y + 11, z + 5, "dark_oak_fence", 1)
	l.fill(x - 5, y + 11, z - 5, x - 5, y + 11, z + 5, "dark_oak_fence", 1)
	l.fill(x + 5, y + 11, z - 5, x + 5, y + 11, z + 5, "dark_oak_fence", 1)
	l.fill(x - 1, y + 12, z - 1, x + 1, y + 12, z + 1, plank, 0)
	# ladder up the middle
	for h2 in range(1, 12):
		l.put_name(x, y + h2, z - 2, "ladder", 0, 1)
	# cage with an iron golem
	l.fill(x + 6, y + 1, z, x + 8, y + 4, z + 2, AIR, 0)
	l.hollow(x + 6, y, z, x + 8, y + 4, z + 2, "dark_oak_log", AIR, 0)
	l.entity("structure_mob", Vector3(x + 7.5, float(y) + 1.0, z + 1.5), {"mob": "iron_golem"})
	l.chest(x, y + 12, z, "pillager_outpost")
	l.chest(x - 4, y + 1, z - 4, "pillager_outpost")
	for i in 3:
		l.entity("structure_mob", Vector3(x + rng.randi_range(-3, 3) + 0.5, float(y) + 12.0, z + rng.randi_range(-3, 3) + 0.5),
			{"mob": "pillager"})
	# tents outside
	for i in 3:
		var tx := x + rng.randi_range(-12, 12)
		var tz := z + rng.randi_range(-12, 12)
		var ty := g.surface_height(tx, tz)
		if absi(tx - x) < 6 and absi(tz - z) < 6:
			continue
		l.fill(tx - 2, ty, tz - 2, tx + 2, ty, tz + 2, "white_wool", 0)
		l.fill(tx - 1, ty + 1, tz - 2, tx + 1, ty + 3, tz + 1, AIR, 0)
		l.fill(tx - 2, ty + 3, tz - 2, tx + 2, ty + 3, tz + 2, "white_wool", 0)
	return l


## Stronghold: stone brick corridors leading to the End portal room.
static func stronghold(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "stronghold"
	var rng := _rng(seed_value, start, 19)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := 30 + rng.randi_range(-6, 6)
	l.origin = Vector3i(x, y, z)
	var brick := "stone_bricks"
	var cracked := "cracked_stone_bricks"
	var mossy := "mossy_stone_bricks"
	var pick := func() -> String:
		var r := rng.randf()
		if r < 0.1:
			return mossy
		if r < 0.22:
			return cracked
		return brick
	# main corridor ring: four long halls
	var len := 34
	for dir in 4:
		var dx: int = [1, 0, -1, 0][dir]
		var dz: int = [0, 1, 0, -1][dir]
		var px := x + dx * len
		var pz := z + dz * len
		# room at the end
		l.hollow(px - 5, y - 1, pz - 5, px + 5, y + 4, pz + 5, "stone_bricks", AIR, 0)
		l.chest(px + 3, y, pz + 3, "stronghold")
		l.put_name(px, y, pz, "torch", 0, 1)
		for i in len:
			var cx := x + dx * i
			var cz := z + dz * i
			# 3 wide, 4 high corridor
			for a in range(-1, 2):
				for h in range(0, 4):
					if h == 0:
						l.put_name(cx + dz * a, y - 1, cz + dx * a, pick.call(), 0, 0)
					else:
						l.put_name(cx + dz * a, y + h - 1, cz + dx * a, AIR, 0, 0)
			# walls and ceiling
			for h2 in range(-1, 3):
				l.put_name(cx + dz * 2, y + h2, cz + dx * 2, pick.call(), 0, 0)
				l.put_name(cx - dz * 2, y + h2, cz - dx * 2, pick.call(), 0, 0)
			l.put_name(cx + dz * 2, y + 3, cz + dx * 2, pick.call(), 0, 0)
			l.put_name(cx - dz * 2, y + 3, cz - dx * 2, pick.call(), 0, 0)
			l.fill(cx - 1, y + 3, cz - 1, cx + 1, y + 3, cz + 1, pick.call(), 0, 0)
			if i % 9 == 4:
				l.put_name(cx + dz * 1, y + 2, cz + dx * 1, "torch", 0, 1)
			# library
			if i == 12:
				l.hollow(cx + dz * 4, y - 1, cz + dx * 4, cx + dz * 9, y + 4, cz + dx * 9, "oak_planks", AIR, 0)
				for b in range(0, 5):
					l.put_name(cx + dz * 5 + dx * b, y, cz + dx * 5 + dz * b, "bookshelf", 0, 0)
					l.put_name(cx + dz * 5 + dx * b, y + 1, cz + dx * 5 + dz * b, "bookshelf", 0, 0)
				l.chest(cx + dz * 6, y, cz + dx * 6, "stronghold")
			# fountain room
			if i == 24:
				l.hollow(cx - 6, y - 1, cz - 6, cx + 6, y + 5, cz + 6, "stone_bricks", AIR, 0)
				l.fill(cx - 2, y - 1, cz - 2, cx + 2, y - 1, cz + 2, "stone_bricks", 0, 0)
				l.fill(cx - 1, y, cz - 1, cx + 1, y, cz + 1, "water", 0, 0)
				l.fill(cx - 2, y, cz - 2, cx + 2, y, cz + 2, "stone_brick_slab", 0, 0)
				l.put_name(cx, y + 1, cz, "stone_brick_slab", 5, 1)
				l.chest(cx + 4, y, cz + 4, "stronghold")
				l.chest(cx - 4, y, cz - 4, "stronghold")
	# the portal room inside the ring
	var pr := 11
	l.hollow(x - pr, y - 1, z - pr, x + pr, y + 5, z + pr, "stone_bricks", AIR, 0)
	for dx2 in range(-pr, pr + 1):
		for dz2 in range(-pr, pr + 1):
			var edge := dx2 == -pr or dx2 == pr or dz2 == -pr or dz2 == pr
			if edge:
				l.fill(x + dx2, y - 1, z + dz2, x + dx2, y + 5, z + dz2, "stone_bricks", 0, 0)
				continue
	# lava pool in front of the portal
	l.fill(x - 2, y - 1, z + 5, x + 2, y - 1, z + 8, "lava", 0, 0)
	l.fill(x - 3, y - 1, z + 4, x + 3, y - 1, z + 9, "stone_bricks", 0, 0)
	l.fill(x - 2, y - 1, z + 5, x + 2, y - 1, z + 8, "lava", 0, 0)
	# the ring of 12 end portal frames around a 3x3 hole
	for i in range(-1, 2):
		l.put_name(x + i, y, z - 2, "end_portal_frame", 0, 0)
		l.put_name(x + i, y, z + 2, "end_portal_frame", 0, 0)
		l.put_name(x - 2, y, z + i, "end_portal_frame", 0, 0)
		l.put_name(x + 2, y, z + i, "end_portal_frame", 0, 0)
	for dx3 in range(-1, 2):
		for dz3 in range(-1, 2):
			l.put_name(x + dx3, y - 1, z + dz3, "lava", 0, 0)
	# stairs / doorway
	l.fill(x, y, z + pr, x, y + 2, z + pr, AIR, 0)
	l.put_name(x - 1, y + 4, z - pr + 1, "chiseled_stone_bricks", 0, 1)
	# silverfish spawner
	l.put_name(x + 6, y, z - 6, "spawner", 0, 0)
	l.block_entity(x + 6, y, z - 6, {"type": "spawner", "mob": "silverfish"})
	return l


## Ocean monument (a compact prismarine/sea lantern pyramid).
static func ocean_monument(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "ocean_monument"
	var rng := _rng(seed_value, start, 20)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := g.surface_height(x, z)
	if y > g.sea_level - 8:
		l.empty = true
		return l
	l.origin = Vector3i(x, y, z)
	var prism := "prismarine"
	var dark := "dark_prismarine"
	var bricks := "prismarine_bricks"
	for level in 6:
		var r := 12 - level * 2
		if r < 2:
			break
		l.fill(x - r, y + level * 3, z - r, x + r, y + level * 3, z + r, dark if level % 2 == 0 else prism, 0, 0)
		l.hollow(x - r, y + level * 3 + 1, z - r, x + r, y + level * 3 + 3, z + r, bricks, AIR, 0)
		# sea lanterns in the corners
		for i in 4:
			var dx: int = [-r, r, r, -r][i]
			var dz: int = [-r, -r, r, r][i]
			l.put_name(x + dx, y + level * 3 + 1, z + dz, "sea_lantern", 0, 0)
	# inner chamber with gold blocks and a sponge room
	l.fill(x - 4, y + 4, z - 4, x + 4, y + 8, z + 4, AIR, 0)
	for i in 4:
		l.put_name(x + [3, -3, 3, -3][i], y + 4, z + [3, 3, -3, -3][i], "gold_block", 0, 0)
	l.put_name(x + 3, y + 5, z + 3, "gold_block", 0, 0)
	l.put_name(x - 3, y + 5, z - 3, "gold_block", 0, 0)
	l.put_name(x, y + 8, z, "sea_lantern", 0, 0)
	l.chest(x + 5, y + 6, z + 5, "ocean_monument")
	# guardian spawns
	for i in 6:
		l.entity("structure_mob", Vector3(x + rng.randi_range(-10, 10) + 0.5, float(y + rng.randi_range(4, 16)), z + rng.randi_range(-10, 10) + 0.5),
			{"mob": "guardian"})
	l.entity("structure_mob", Vector3(x + 0.5, float(y + 8), z + 0.5), {"mob": "elder_guardian"})
	return l


## Shipwreck: a tilted hull with loot chests.
static func shipwreck(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "shipwreck"
	var rng := _rng(seed_value, start, 21)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := maxi(g.surface_height(x, z), g.sea_level - 6)
	l.origin = Vector3i(x, y, z)
	var wood := "spruce_planks"
	var log := "spruce_log"
	var len := rng.randi_range(14, 22)
	var dir := rng.randi_range(0, 3)
	var dx: int = [1, 0, -1, 0][dir]
	var dz: int = [0, 1, 0, -1][dir]
	for i in len:
		var px := x + dx * i
		var pz := z + dz * i
		var sink := i / 5
		l.fill(px - dz * 2, y - sink, pz - dx * 2, px + dz * 2, y - sink, pz + dx * 2, wood, 0, 0)
		l.fill(px - dz * 2, y - sink + 1, pz - dx * 2, px + dz * 2, y - sink + 4, pz + dx * 2, AIR, 0, 0)
		l.fill(px - dz * 3, y - sink, pz - dx * 3, px - dz * 3, y - sink + 2, pz - dx * 3, log, 0, 0)
		l.fill(px + dz * 3, y - sink, pz + dx * 3, px + dz * 3, y - sink + 2, pz + dx * 3, log, 0, 0)
	for i in 3:
		l.chest(x + dx * (4 + i * 5), y - 1, z + dz * (4 + i * 5), "shipwreck")
	if rng.randf() < 0.4:
		l.chest(x + dx * 8 + dz, y - 1, z + dz * 8 + dx, "buried_treasure")
	return l


static func buried_treasure(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "buried_treasure"
	var rng := _rng(seed_value, start, 22)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := g.surface_height(x, z) - rng.randi_range(4, 8)
	l.origin = Vector3i(x, y, z)
	l.fill(x - 1, y, z - 1, x + 1, y, z + 1, "sand", 0, 0)
	l.chest(x, y + 1, z, "buried_treasure")
	return l


static func ocean_ruin(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "ocean_ruin"
	var rng := _rng(seed_value, start, 23)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := mini(g.surface_height(x, z), g.sea_level - 2)
	l.origin = Vector3i(x, y, z)
	var mat := "sandstone" if rng.randf() < 0.5 else "stone_bricks"
	for i in 4:
		var wx := x + rng.randi_range(-5, 5)
		var wz := z + rng.randi_range(-5, 5)
		var h := rng.randi_range(2, 5)
		var w := rng.randi_range(3, 6)
		l.hollow(wx, y - 1, wz, wx + w, y + h, wz + 4, mat, AIR, 0)
		l.fill(wx, y, wz, wx + w, y, wz + 4, mat, 0, 0)
		if rng.randf() < 0.6:
			l.chest(wx + 1, y + 1, wz + 1, "ocean_ruin")
	l.put_name(x, y + 1, z, "sea_lantern", 0, 0)
	return l


## Woodland mansion: a big dark oak box with a few rooms.
static func woodland_mansion(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "woodland_mansion"
	var rng := _rng(seed_value, start, 24)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := g.surface_height(x, z)
	l.origin = Vector3i(x, y, z)
	var plank := "dark_oak_planks"
	var log := "dark_oak_log"
	# ground floor / first floor / roof
	for level in 3:
		var yy := y + level * 7
		l.fill(x - 20, yy, z - 14, x + 20, yy, z + 14, plank, 0)
		l.hollow(x - 20, yy + 1, z - 14, x + 20, yy + 6, z + 14, plank, AIR, 0)
		# divider walls
		for i in range(-16, 17, 8):
			l.fill(x + i, yy + 1, z - 14, x + i, yy + 6, z + 14, plank, 2)
		for j in range(-10, 11, 8):
			l.fill(x - 20, yy + 1, z + j, x + 20, yy + 6, z + j, plank, 2)
	l.fill(x - 21, y + 21, z - 15, x + 21, y + 21, z + 15, "dark_oak_slab", 0, 0)
	# windows and torches
	for i in range(-16, 17, 4):
		l.put_name(x + i, y + 3, z - 14, "glass_pane", 0, 1)
		l.put_name(x + i, y + 3, z + 14, "glass_pane", 0, 1)
	for j2 in range(-10, 11, 4):
		l.put_name(x - 20, y + 3, z + j2, "glass_pane", 0, 1)
		l.put_name(x + 20, y + 3, z + j2, "glass_pane", 0, 1)
	# entrance
	l.fill(x, y + 1, z - 14, x, y + 2, z - 14, "dark_oak_door", 0, 0)
	# loot
	l.chest(x + 6, y + 1, z + 6, "woodland_mansion")
	l.chest(x - 12, y + 8, z - 8, "woodland_mansion")
	l.chest(x + 12, y + 8, z + 10, "woodland_mansion")
	l.chest(x - 6, y + 15, z + 2, "woodland_mansion")
	# pillagers and a couple of evokers
	for i in 5:
		l.entity("structure_mob", Vector3(x + rng.randi_range(-16, 16) + 0.5, float(y + 1), z + rng.randi_range(-10, 10) + 0.5),
			{"mob": ["vindicator", "pillager", "evoker"][i % 3]})
	return l


## Ancient city: a sculk palace deep underground.
static func ancient_city(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "ancient_city"
	var rng := _rng(seed_value, start, 25)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := -50 + rng.randi_range(-4, 4)
	l.origin = Vector3i(x, y, z)
	var deep := "deepslate_bricks"
	var tiles := "deepslate_tiles"
	var sculk := "sculk"
	# main hall
	l.fill(x - 26, y, z - 20, x + 26, y, z + 20, tiles, 0)
	l.fill(x - 26, y + 1, z - 20, x + 26, y + 12, z + 20, sculk, 2)
	l.hollow(x - 26, y + 1, z - 20, x + 26, y + 12, z + 20, deep, AIR, 1)
	l.fill(x - 26, y + 13, z - 20, x + 26, y + 13, z + 20, tiles, 0)
	# pillars
	for i in range(-20, 21, 10):
		for j in range(-16, 17, 8):
			l.fill(x + i, y + 1, z + j, x + i, y + 12, z + j, tiles, 0)
	# carpet of sculk and a couple of catalysts
	for i in 40:
		var sx := x + rng.randi_range(-24, 24)
		var sz := z + rng.randi_range(-18, 18)
		l.put_name(sx, y + 1, sz, "sculk_vein", 0, 1)
	l.put_name(x, y + 1, z, "sculk_catalyst", 0, 0)
	l.put_name(x + 12, y + 1, z - 8, "sculk_catalyst", 0, 0)
	l.put_name(x - 14, y + 1, z + 10, "sculk_catalyst", 0, 0)
	l.put_name(x + 20, y + 1, z + 14, "sculk_shrieker", 0, 0)
	# chests with the city loot
	l.chest(x + 22, y + 1, z + 18, "ancient_city")
	l.chest(x - 22, y + 1, z - 18, "ancient_city")
	l.chest(x + 8, y + 1, z - 16, "ancient_city")
	# the portal frame in the middle (decorative, structure block style)
	l.fill(x - 3, y + 1, z + 4, x + 3, y + 1, z + 4, "reinforced_deepslate", 0, 0)
	l.entity("structure_mob", Vector3(x + 20.5, float(y + 1), z + 14.5), {"mob": "warden"})
	return l


## A pillager watchtower variant (small tower with two pillagers).
static func pillager_tower(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "pillager_tower"
	var rng := _rng(seed_value, start, 26)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := g.surface_height(x, z)
	l.origin = Vector3i(x, y, z)
	var log := "dark_oak_log"
	var plank := "dark_oak_planks"
	for h in range(0, 9):
		l.hollow(x - 2, y + 1 + h, z - 2, x + 2, y + 1 + h, z + 2, log, AIR, 0)
		if h % 3 == 0:
			l.fill(x - 2, y + 1 + h, z - 2, x + 2, y + 1 + h, z + 2, plank, 1)
	l.fill(x - 3, y + 9, z - 3, x + 3, y + 9, z + 3, plank, 0)
	l.fill(x - 1, y + 10, z - 1, x + 1, y + 10, z + 1, plank, 0)
	for h2 in range(1, 9):
		l.put_name(x, y + h2, z - 1, "ladder", 0, 1)
	l.chest(x + 1, y + 9, z + 1, "pillager_outpost")
	for i in 3:
		l.entity("structure_mob", Vector3(x + rng.randi_range(-1, 1) + 0.5, float(y + 9), z + rng.randi_range(-1, 1) + 0.5),
			{"mob": "pillager"})
	return l


# ------------------------------------------------------------------------------------------------
# The Nether
# ------------------------------------------------------------------------------------------------

## Nether fortress: brick bridges with blaze spawners and wither skeleton spawns.
static func nether_fortress(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "nether_fortress"
	var rng := _rng(seed_value, start, 31)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := 62 + rng.randi_range(-8, 8)
	l.origin = Vector3i(x, y, z)
	var brick := "nether_bricks"
	var fence := "nether_brick_fence"
	# two crossing bridges
	for axis in 2:
		var dx := 1 if axis == 0 else 0
		var dz := 0 if axis == 0 else 1
		var len := rng.randi_range(60, 110)
		var half := len / 2
		for i in range(-half, half + 1):
			var px := x + dx * i
			var pz := z + dz * i
			var sag := absi(i) / 12
			var yy := y - sag
			l.fill(px - dz * 2, yy, pz - dx * 2, px + dz * 2, yy, pz + dx * 2, brick, 0, 0)
			# railings
			l.fill(px - dz * 3, yy + 1, pz - dx * 3, px - dz * 3, yy + 2, pz - dx * 3, fence, 0, 1)
			l.fill(px + dz * 3, yy + 1, pz + dx * 3, px + dz * 3, yy + 2, pz + dx * 3, fence, 0, 1)
			# ceiling supports
			if i % 5 == 0:
				l.fill(px - dz * 2, yy + 3, pz - dx * 2, px + dz * 2, yy + 3, pz + dx * 2, brick, 1)
				l.fill(px - dz * 2, yy + 1, pz - dx * 2, px - dz * 2, yy + 3, pz - dx * 2, brick, 0, 0)
				l.fill(px + dz * 2, yy + 1, pz + dx * 2, px + dz * 2, yy + 3, pz + dx * 2, brick, 0, 0)
			if i % 16 == 8:
				l.put_name(px + dz * 2, yy + 2, pz + dx * 2, "torch", 0, 1)
		# towers along the bridge
		for t: int in [-half + 8, 0, half - 8]:
			var tx: int = x + dx * t
			var tz: int = z + dz * t
			var ty: int = y - absi(t) / 12
			l.hollow(tx - 3, ty, tz - 3, tx + 3, ty + 5, tz + 3, brick, AIR, 0)
			l.fill(tx - 4, ty + 6, tz - 4, tx + 4, ty + 6, tz + 4, brick, 0, 0)
			l.put_name(tx, ty + 5, tz, "torch", 0, 1)
			if rng.randf() < 0.5:
				l.put_name(tx + 1, ty + 1, tz + 1, "spawner", 0, 0)
				l.block_entity(tx + 1, ty + 1, tz + 1, {"type": "spawner", "mob": "blaze"})
			# nether wart farm room
			if rng.randf() < 0.4:
				l.fill(tx - 2, ty + 1, tz + 4, tx + 2, ty + 1, tz + 6, "soul_sand", 0, 0)
				for wx in range(-2, 3):
					l.put_name(tx + wx, ty + 2, tz + 5, "nether_wart", 3, 1)
	l.chest(x + 2, y + 1, z + 2, "nether_fortress")
	l.chest(x - 2, y + 1, z - 2, "nether_fortress")
	# blaze and wither skeleton population
	for i in 4:
		l.entity("structure_mob", Vector3(x + rng.randi_range(-14, 14) + 0.5, float(y + 1), z + rng.randi_range(-14, 14) + 0.5),
			{"mob": "wither_skeleton"})
	for i in 2:
		l.entity("structure_mob", Vector3(x + rng.randi_range(-10, 10) + 0.5, float(y + 2), z + rng.randi_range(-10, 10) + 0.5),
			{"mob": "blaze"})
	return l


## Bastion remnant: blackstone halls with gold blocks, hoglins and piglins.
static func bastion(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "bastion"
	var rng := _rng(seed_value, start, 32)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := 48 + rng.randi_range(-6, 10)
	l.origin = Vector3i(x, y, z)
	var bs := "blackstone"
	var pb := "polished_blackstone"
	var pbb := "polished_blackstone_bricks"
	var gold := "gilded_blackstone"
	# main platform
	l.fill(x - 22, y, z - 22, x + 22, y, z + 22, bs, 0, 0)
	l.fill(x - 20, y + 1, z - 20, x + 20, y + 12, z + 20, AIR, 0)
	l.hollow(x - 22, y - 1, z - 22, x + 22, y + 12, z + 22, pbb, AIR, 1)
	l.fill(x - 24, y + 13, z - 24, x + 24, y + 13, z + 24, pb, 0, 0)
	# inner walls
	for i in range(-14, 15, 14):
		l.fill(x + i, y + 1, z - 20, x + i, y + 12, z + 20, pbb, 2)
	for j in range(-14, 15, 14):
		l.fill(x - 20, y + 1, z + j, x + 20, y + 12, z + j, pbb, 2)
	# gold hoard
	for i in 24:
		var gx := x + rng.randi_range(-8, 8)
		var gz := z + rng.randi_range(-8, 8)
		var gy := y + 1 + rng.randi_range(0, 2)
		l.put_name(gx, gy, gz, gold if rng.randf() < 0.5 else "gold_block", 0, 0)
	# lava moat
	for dx in range(-24, 25):
		l.put_name(x + dx, y - 1, z - 23, "lava", 0, 0)
		l.put_name(x + dx, y - 1, z + 23, "lava", 0, 0)
	l.chest(x + 6, y + 1, z + 6, "bastion")
	l.chest(x - 8, y + 1, z - 8, "bastion")
	l.chest(x, y + 1, z + 12, "bastion")
	# piglins + hoglins
	for i in 6:
		l.entity("structure_mob", Vector3(x + rng.randi_range(-16, 16) + 0.5, float(y + 1), z + rng.randi_range(-16, 16) + 0.5),
			{"mob": ["piglin", "piglin_brute", "hoglin"][i % 3]})
	return l


# ------------------------------------------------------------------------------------------------
# The End
# --------------------------------------------------------------------------------------------------

## End city: purpur towers on the outer islands, with shulkers and an elytra ship.
static func end_city(seed_value: int, start: Vector2i, g: WorldGen) -> StructureLayout:
	var l := StructureLayout.new()
	l.name = "end_city"
	var rng := _rng(seed_value, start, 41)
	var x := start.x * 16 + 8
	var z := start.y * 16 + 8
	var y := g.surface_height(x, z)
	if y < 40:
		l.empty = true
		return l
	l.origin = Vector3i(x, y, z)
	var purpur := "purpur_block"
	var pillar := "purpur_pillar"
	# base tower
	var tower_h := rng.randi_range(20, 34)
	l.fill(x - 4, y, z - 4, x + 4, y, z + 4, purpur, 0, 0)
	l.hollow(x - 3, y + 1, z - 3, x + 3, y + tower_h, z + 3, purpur, AIR, 0)
	l.fill(x - 4, y + tower_h + 1, z - 4, x + 4, y + tower_h + 1, z + 4, purpur, 0, 0)
	# decorative pillars on the corners
	for i in 4:
		var dx: int = [-4, 4, 4, -4][i]
		var dz: int = [-4, -4, 4, 4][i]
		l.fill(x + dx, y + 1, z + dz, x + dx, y + tower_h, z + dz, pillar, 0, 0)
	# rooms every 8 blocks with a floor
	for h in range(6, tower_h, 8):
		l.fill(x - 3, y + h, z - 3, x + 3, y + h, z + 3, purpur, 1)
		l.put_name(x + 2, y + h + 1, z + 2, "end_rod", 0, 0)
		# windows
		for k in 3:
			l.put_name(x - 3 + k, y + h - 2, z - 3, "purpur_block", 0, 1)
	# the loot at the top
	l.chest(x, y + tower_h, z, "end_city")
	l.chest(x + 2, y + tower_h - 7, z - 2, "end_city")
	# side bridge to a smaller tower
	var bx := x + rng.randi_range(-20, 20)
	var bz := z + rng.randi_range(-20, 20)
	var by := y
	var bh := rng.randi_range(8, 16)
	l.fill(bx - 3, by, bz - 3, bx + 3, by + bh + 1, bz + 3, purpur, 1)
	l.hollow(bx - 2, by, bz - 2, bx + 2, by + bh, bz + 2, purpur, AIR, 0)
	l.put_name(bx, by + bh, bz, "end_rod", 0, 0)
	# the elytra ship
	var sx := x + rng.randi_range(-30, 30)
	var sz := z + rng.randi_range(-30, 30)
	var sy := g.surface_height(sx, sz) + 12
	if sy > 40:
		_end_ship(l, sx, sy, sz, rng)
	# shulkers
	for i in 5:
		l.entity("structure_mob", Vector3(x + rng.randi_range(-3, 3) + 0.5, float(y + rng.randi_range(2, tower_h - 2)),
			z + rng.randi_range(-3, 3) + 0.5), {"mob": "shulker"})
	return l


static func _end_ship(l: StructureLayout, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var purpur := "purpur_block"
	var pillar := "purpur_pillar"
	# hull ~15 long
	for i in range(-7, 8):
		var w := 3 - absi(i) / 3
		l.fill(x + i, y, z - w, x + i, y, z + w, purpur, 0, 0)
		l.fill(x + i, y + 1, z - w, x + i, y + 3, z + w, AIR, 0, 0)
		l.fill(x + i, y + 4, z - w, x + i, y + 4, z + w, purpur, 0, 1 if absi(i) > 1 else 0)
		l.fill(x + i, y + 1, z - w, x + i, y + 1, z - w, purpur, 0, 1)
		l.fill(x + i, y + 1, z + w, x + i, y + 1, z + w, purpur, 0, 1)
	# mast and bow
	l.fill(x - 1, y + 5, z, x - 1, y + 10, z, pillar, 0, 0)
	l.put_name(x - 1, y + 11, z, "end_rod", 0, 0)
	l.fill(x + 6, y + 1, z, x + 9, y + 2, z, purpur, 0, 0)
	# treasure room at the stern
	l.fill(x - 7, y + 1, z - 2, x - 4, y + 3, z + 2, purpur, 1)
	l.put_name(x - 6, y + 1, z, "end_rod", 0, 0)
	# the elytra is carried by an item frame in the original; here it is a chest with the elytra inside
	l.chest(x - 6, y + 1, z + 1, "end_city")


# ------------------------------------------------------------------------------------------------
# helpers
# ------------------------------------------------------------------------------------------------
static func _rng(seed_value: int, start: Vector2i, salt: int) -> RandomNumberGenerator:
	var r := RandomNumberGenerator.new()
	r.seed = (seed_value ^ (start.x * 341873128712) ^ (start.y * 132897987541) ^ (salt * 7919)) & 0x7FFFFFFFFFFFFFFF
	return r


## Flattens an area onto the surface height of its centre, with a top and filler layer.
static func _flat(l: StructureLayout, x0: int, z0: int, x1: int, _unused: int, z1: int, y: int, top: String, filler: String) -> void:
	for dx in range(x0, x1 + 1):
		for dz in range(z0, z1 + 1):
			l.put_name(dx, y, dz, top, 0, 0)
			l.put_name(dx, y - 1, dz, filler, 0, 0)
			for h in range(1, 5):
				l.put_name(dx, y + h, dz, AIR, 0, 0)


## Lays a path of the given half width between two points (Bresenham-ish).
static func _line(l: StructureLayout, x0: int, y0: int, z0: int, x1: int, y1: int, z1: int, half: int, block: String,
		g: WorldGen) -> void:
	var steps := maxi(absi(x1 - x0), absi(z1 - z0))
	if steps == 0:
		return
	for i in range(steps + 1):
		var t := float(i) / float(steps)
		var x := int(round(lerpf(float(x0), float(x1), t)))
		var z := int(round(lerpf(float(z0), float(z1), t)))
		var y := g.surface_height(x, z)
		for dx in range(-half, half + 1):
			for dz in range(-half, half + 1):
				if absi(dx) + absi(dz) > half:
					continue
				l.put_name(x + dx, y, z + dz, block, 0, 0)
				l.put_name(x + dx, y + 1, z + dz, AIR, 0, 0)
				l.put_name(x + dx, y + 2, z + dz, AIR, 0, 0)
