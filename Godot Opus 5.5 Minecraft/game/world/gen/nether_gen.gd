class_name NetherGen
extends WorldGen
## The Nether: y 0..127 with a bedrock floor and ceiling, a lava sea at y=32, cheese/noodle cave
## carving, five biomes (nether wastes, crimson forest, warped forest, soul sand valley, basalt
## deltas), netherrack/gravel/soul sand surface patches, glowstone and quartz, vegetation and the
## fortress / bastion structures.

var n_biome: FastNoiseLite
var n_cheese: FastNoiseLite
var n_noodle: FastNoiseLite
var n_detail: FastNoiseLite
var n_patch: FastNoiseLite
var n_temp: FastNoiseLite

var I := {}
var biome_ids := {}
var LAVA_LEVEL := 32


func setup(world_seed: int) -> void:
	seed_value = world_seed
	min_y = 0
	height = 128
	sea_level = LAVA_LEVEL
	has_sky = false
	has_ceiling = true
	dim_id = 1

	n_biome = mk_noise(seed_value + 11, 0.0035, 3)
	n_cheese = mk_noise(seed_value + 22, 0.028, 3)
	n_noodle = mk_noise(seed_value + 33, 0.055, 1, FastNoiseLite.TYPE_SIMPLEX)
	n_detail = mk_noise(seed_value + 44, 0.09, 2)
	n_patch = mk_noise(seed_value + 55, 0.06, 2)
	n_temp = mk_noise(seed_value + 66, 0.0016, 2)

	I = {
		"netherrack": BlockDB.id("netherrack"), "lava": BlockDB.id("lava"), "bedrock": BlockDB.id("bedrock"),
		"basalt": BlockDB.id("basalt"), "blackstone": BlockDB.id("blackstone"), "soul_sand": BlockDB.id("soul_sand"),
		"soul_soil": BlockDB.id("soul_soil"), "gravel": BlockDB.id("gravel"), "magma_block": BlockDB.id("magma_block"),
		"crimson_nylium": BlockDB.id("crimson_nylium"), "warped_nylium": BlockDB.id("warped_nylium"),
		"nether_wart_block": BlockDB.id("nether_wart_block"), "warped_wart_block": BlockDB.id("warped_wart_block"),
		"glowstone": BlockDB.id("glowstone"), "air": 0, "shroomlight": BlockDB.id("shroomlight"),
		"crimson_stem": BlockDB.id("crimson_stem"), "warped_stem": BlockDB.id("warped_stem"),
		"nether_gold_ore": BlockDB.id("nether_gold_ore"), "nether_quartz_ore": BlockDB.id("nether_quartz_ore"),
		"ancient_debris": BlockDB.id("ancient_debris"), "magma": BlockDB.id("magma_block"),
		"soul_fire": BlockDB.id("soul_fire"), "fire": BlockDB.id("fire"),
	}
	biome_ids = {
		"wastes": BiomeDB.id("nether_wastes"), "crimson": BiomeDB.id("crimson_forest"),
		"warped": BiomeDB.id("warped_forest"), "soul": BiomeDB.id("soul_sand_valley"),
		"basalt": BiomeDB.id("basalt_deltas"),
	}


func biome_at(x: int, z: int) -> int:
	var t := n_temp.get_noise_2d(float(x), float(z))
	var b := n_biome.get_noise_2d(float(x), float(z))
	if t > 0.42:
		return biome_ids.basalt
	if t < -0.45:
		return biome_ids.soul
	if b > 0.22:
		return biome_ids.crimson
	if b < -0.22:
		return biome_ids.warped
	return biome_ids.wastes


func biome_at_3d(x: int, y: int, z: int) -> int:
	return biome_at(x, z)


func surface_height(x: int, z: int) -> int:
	# highest solid ground with air above, scanning down from just under the ceiling
	for y in range(118, 2, -1):
		if n_cheese.get_noise_3d(float(x), float(y) * 2.2, float(z)) > -0.1:
			return y
	return LAVA_LEVEL + 1


func find_spawn() -> Vector3:
	var r := RandomNumberGenerator.new()
	r.seed = seed_value ^ 0x4e17e5
	for i in 400:
		var x := r.randi_range(-160, 160)
		var z := r.randi_range(-160, 160)
		var y := surface_height(x, z)
		if y <= LAVA_LEVEL + 1 or y > 100:
			continue
		if _open_column(x, y, z):
			return Vector3(x + 0.5, float(y) + 1.0, z + 0.5)
	return Vector3(0.5, 70.0, 0.5)


func _open_column(x: int, y: int, z: int) -> bool:
	return user_air_at(x, y + 1, z) and user_air_at(x, y + 2, z)


func user_air_at(_x: int, _y: int, _z: int) -> bool:
	return true


func generate(c: Chunk) -> void:
	var ox := c.cx * 16
	var oz := c.cz * 16
	var blocks := c.blocks
	var MY := min_y
	var H := height
	var netherrack: int = I.netherrack
	var lava: int = I.lava
	var bedrock: int = I.bedrock
	var stone_like := PackedInt32Array([netherrack, I.basalt, I.blackstone, I.soul_soil, I.soul_sand,
		I.nether_wart_block, I.warped_wart_block])
	var rng := chunk_rng(c.cx, c.cz, 7)
	var biome := PackedInt32Array()
	biome.resize(256)
	for lz in 16:
		for lx in 16:
			var bi := (lz << 4) | lx
			var b := biome_at(ox + lx, oz + lz)
			biome[bi] = b
			c.biomes[bi] = b
			# solid netherrack base with 3D cave noise carving
			for y in range(1, H - 1):
				var density := _density(ox + lx, y, oz + lz)
				var v := netherrack
				if density <= 0.0:
					v = lava if y <= LAVA_LEVEL else 0
				blocks[((y - MY) << 8) | bi] = v
			blocks[0] = bedrock
			c.blocks[((H - 1 - MY) << 8) | bi] = bedrock
			for k in range(1, 4):
				if (hash(ox + lx * 31 + (oz + lz) * 17 + k * 13 + seed_value) & 7) < 4 - k:
					blocks[(k << 8) | bi] = bedrock
				if (hash(ox + lx * 29 + (oz + lz) * 23 + k * 11 + seed_value) & 7) < 4 - k:
					blocks[((H - 1 - k - MY) << 8) | bi] = bedrock
	_surface_patches(c, biome, rng, stone_like)
	_ores(c, rng)
	_decor(c, biome, rng)
	if structures != null:
		structures.generate_into(c, self)
	_biome_colors(c)
	c.rebuild_derived()


## Density field: > 0 means solid, <= 0 means carved out. Cheese caves plus noodle tunnels.
func _density(x: int, y: int, z: int) -> float:
	var fy := float(y)
	# vertical falloff so the ceiling and floor stay connected
	var edge := minf(fy - 4.0, float(height - 5) - fy) / 12.0
	var base := clampf(edge, -1.0, 1.0) * 0.8
	if base < -0.7:
		return base
	var cheese := n_cheese.get_noise_3d(float(x), fy * 1.6, float(z))
	var detail := n_detail.get_noise_3d(float(x) * 1.5, fy * 2.0, float(z) * 1.5) * 0.25
	# the lava sea keeps a flat floor
	var floor_bias := 0.0
	if fy < LAVA_LEVEL + 2:
		floor_bias = 0.35
	var noodle := absf(n_noodle.get_noise_3d(float(x) * 0.6, fy * 0.9, float(z) * 0.6))
	var tunnel := 0.06 - noodle
	return base + cheese * 1.15 + detail + floor_bias + tunnel * 2.0


func _surface_patches(c: Chunk, biome: PackedInt32Array, rng: RandomNumberGenerator, stone_like: PackedInt32Array) -> void:
	var ox := c.cx * 16
	var oz := c.cz * 16
	var MY := min_y
	var H := height
	var blocks := c.blocks
	for lz in 16:
		for lx in 16:
			var bi := (lz << 4) | lx
			var x := ox + lx
			var z := oz + lz
			var b := biome[bi]
			var top := -1
			for y in range(H - 2, LAVA_LEVEL, -1):
				if blocks[((y - MY) << 8) | bi] != 0:
					top = y
					break
			if top < 0:
				continue
			var patch := n_patch.get_noise_2d(float(x), float(z))
			var top_block: int = I.netherrack
			var filler: int = I.netherrack
			if b == biome_ids.crimson:
				if patch > -0.2:
					top_block = I.crimson_nylium
			elif b == biome_ids.warped:
				if patch > -0.2:
					top_block = I.warped_nylium
			elif b == biome_ids.soul:
				top_block = I.soul_sand if patch > -0.1 else I.soul_soil
				filler = I.soul_soil
			elif b == biome_ids.basalt:
				top_block = I.basalt if patch > 0.0 else I.blackstone
				filler = I.blackstone
			elif patch > 0.45:
				top_block = I.gravel
			blocks[((top - MY) << 8) | bi] = top_block
			for k in range(1, 4):
				var y2 := top - k
				if y2 <= LAVA_LEVEL:
					break
				var iv := blocks[((y2 - MY) << 8) | bi]
				if stone_like.has(iv):
					blocks[((y2 - MY) << 8) | bi] = filler
			if top <= LAVA_LEVEL and patch > 0.3:
				blocks[((top - MY) << 8) | bi] = I.magma
			# lava that was carved but sits inside solid rock becomes air pockets; lava level keeps lava


func _ores(c: Chunk, rng: RandomNumberGenerator) -> void:
	var w := WorldGen.Writer.new(c)
	var target := PackedByteArray()
	target.resize(BlockDB.count)
	target[I.netherrack] = 1
	target[I.basalt] = 1
	target[I.blackstone] = 1
	var netherrack_deep := PackedByteArray()
	netherrack_deep.resize(BlockDB.count)
	netherrack_deep[I.netherrack] = 2
	var tries := 6
	var ox := c.cx * 16
	var oz := c.cz * 16
	for t in tries:
		var x := ox + rng.randi_range(0, 15)
		var z := oz + rng.randi_range(0, 15)
		var y := rng.randi_range(8, height - 8)
		WorldGen.ore_vein(w, rng, float(x), float(y), float(z), rng.randi_range(6, 14), I.nether_quartz_ore, I.nether_quartz_ore, target)
	for t in 4:
		var x2 := ox + rng.randi_range(0, 15)
		var z2 := oz + rng.randi_range(0, 15)
		var y2 := rng.randi_range(8, height - 8)
		WorldGen.ore_vein(w, rng, float(x2), float(y2), float(z2), rng.randi_range(6, 14), I.nether_gold_ore, I.nether_gold_ore, target)
	# ancient debris: rare, near the bottom, buried in netherrack
	for t in 2:
		if rng.randf() > 0.55:
			continue
		var x3 := ox + rng.randi_range(0, 15)
		var z3 := oz + rng.randi_range(0, 15)
		var y3 := rng.randi_range(8, 22)
		WorldGen.ore_vein(w, rng, float(x3), float(y3), float(z3), rng.randi_range(3, 6), I.ancient_debris, I.ancient_debris, netherrack_deep)


func _decor(c: Chunk, biome: PackedInt32Array, rng: RandomNumberGenerator) -> void:
	var w := WorldGen.Writer.new(c)
	var ox := c.cx * 16
	var oz := c.cz * 16
	for i in 24:
		var lx := rng.randi_range(0, 15)
		var lz := rng.randi_range(0, 15)
		var bi := (lz << 4) | lx
		var x := ox + lx
		var z := oz + lz
		var b := biome[bi]
		# find the surface
		var y := -1
		for yy in range(height - 3, 2, -1):
			if w.get_b(x, yy, z) != 0 and w.get_b(x, yy + 1, z) == 0:
				y = yy
				break
		if y < 0:
			continue
		var ground := w.get_b(x, y, z) & 0xFFF
		if b == biome_ids.crimson:
			if ground == I.crimson_nylium:
				if rng.randf() < 0.5:
					Trees.place(w, "crimson_fungus", x, y + 1, z, rng)
				elif rng.randf() < 0.4:
					w.put(x, y + 1, z, BlockDB.id("crimson_roots"), 1)
		elif b == biome_ids.warped:
			if ground == I.warped_nylium:
				if rng.randf() < 0.45:
					Trees.place(w, "warped_fungus", x, y + 1, z, rng)
				elif rng.randf() < 0.5:
					w.put(x, y + 1, z, BlockDB.id("warped_roots"), 1)
		elif b == biome_ids.soul:
			if rng.randf() < 0.25:
				w.put(x, y + 1, z, BlockDB.id("soul_fire"), 0)
			elif rng.randf() < 0.3:
				w.put(x, y + 1, z, BlockDB.id("nether_sprouts"), 1)
		elif b == biome_ids.basalt:
			if rng.randf() < 0.2:
				w.put(x, y + 1, z, BlockDB.id("basalt"), 0)
		elif rng.randf() < 0.06:
			w.put(x, y + 1, z, BlockDB.id("fire"), 0)
		# glowstone clusters hanging from the ceiling
		for k in 5:
			var gx := rng.randi_range(0, 15)
			var gz := rng.randi_range(0, 15)
			var gy := rng.randi_range(height - 40, height - 3)
			if w.get_b(ox + gx, gy, oz + gz) != 0 and w.get_b(ox + gx, gy - 1, oz + gz) == 0:
				var n := rng.randi_range(2, 5)
				for j in n:
					w.put(ox + gx, gy - 1 - j, oz + gz, I.glowstone, 1)
	# lava springs at the lava level
	for k in 3:
		var lx2 := rng.randi_range(0, 15)
		var lz2 := rng.randi_range(0, 15)
		var x2 := ox + lx2
		var z2 := oz + lz2
		if w.get_b(x2, LAVA_LEVEL, z2) == 0:
			w.put(x2, LAVA_LEVEL, z2, I.lava, 0)


func _biome_colors(c: Chunk) -> void:
	var grass := BiomeDB.grass
	var foliage := BiomeDB.foliage
	var water := BiomeDB.water
	for i in 256:
		var b := c.biomes[i]
		c.colors[i * 3] = grass[b]
		c.colors[i * 3 + 1] = foliage[b]
		c.colors[i * 3 + 2] = water[b]
