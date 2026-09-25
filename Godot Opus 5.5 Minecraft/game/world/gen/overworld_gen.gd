class_name OverworldGen
extends WorldGen
## Seeded multi-noise Overworld generator: continentalness / erosion / weirdness splines, rivers,
## biome surface rules, noise caves (cheese + spaghetti), worm carvers and ravines, cave biomes,
## Minecraft 1.18+ style ore distribution, trees, vegetation and structures.

const CONT_SPLINE := [-1.0, 26.0, -0.55, 31.0, -0.35, 43.0, -0.2, 53.0, -0.12, 59.0, -0.05, 63.5, 0.05, 66.0,
	0.2, 70.0, 0.4, 77.0, 0.7, 88.0, 1.0, 98.0]
const PV_MOUNT := [-1.0, 0.0, -0.4, 6.0, 0.0, 30.0, 0.4, 72.0, 0.75, 118.0, 1.0, 150.0]
const BAND_COLORS := ["orange", "white", "yellow", "", "brown", "red", "light_gray", "", "orange", "white", "", "yellow",
	"red", "", "brown", "white", "orange", "", "light_gray", "yellow"]

var n_cont: FastNoiseLite
var n_ero: FastNoiseLite
var n_weird: FastNoiseLite
var n_temp: FastNoiseLite
var n_hum: FastNoiseLite
var n_detail: FastNoiseLite
var n_river: FastNoiseLite
var n_mush: FastNoiseLite
var n_patch: FastNoiseLite
var n_band: FastNoiseLite
var n_sulfur: FastNoiseLite
var n_cave_region: FastNoiseLite
var n_cheese: FastNoiseLite
var n_spag_a: FastNoiseLite
var n_spag_b: FastNoiseLite
var n_noodle: FastNoiseLite

var cont_spline := PackedFloat32Array()
var pv_spline := PackedFloat32Array()
var I := {}               # block ids by name
var biome_ids := {}
var stone_like := PackedInt32Array()
var deep_like := PackedInt32Array()
var ore_target := PackedByteArray()     # stone-like -> 1, deepslate-like -> 2
var blob_target := PackedByteArray()    # stone and deepslate -> 1
var band_ids := PackedInt32Array()


func setup(world_seed: int) -> void:
	seed_value = world_seed
	min_y = -64
	height = 384
	sea_level = 63
	has_sky = true
	dim_id = 0
	n_cont = mk_noise(world_seed + 101, 0.0011, 5)
	n_ero = mk_noise(world_seed + 202, 0.0013, 4)
	n_weird = mk_noise(world_seed + 303, 0.0019, 3)
	n_temp = mk_noise(world_seed + 404, 0.00085, 3)
	n_hum = mk_noise(world_seed + 505, 0.00095, 3)
	n_detail = mk_noise(world_seed + 606, 0.018, 3)
	n_river = mk_noise(world_seed + 707, 0.0016, 2, FastNoiseLite.TYPE_PERLIN)
	n_mush = mk_noise(world_seed + 808, 0.0025, 2)
	n_patch = mk_noise(world_seed + 909, 0.06, 2)
	n_band = mk_noise(world_seed + 1010, 0.01, 1)
	n_sulfur = mk_noise(world_seed + 1111, 0.004, 2)
	n_cave_region = mk_noise(world_seed + 1212, 0.006, 2)
	n_cheese = mk_noise(world_seed + 2001, 0.011, 2)
	n_spag_a = mk_noise(world_seed + 2002, 0.018, 1)
	n_spag_b = mk_noise(world_seed + 2003, 0.018, 1)
	n_noodle = mk_noise(world_seed + 2004, 0.04, 1)
	cont_spline = PackedFloat32Array(CONT_SPLINE)
	pv_spline = PackedFloat32Array(PV_MOUNT)
	for n in ["stone", "deepslate", "bedrock", "water", "lava", "ice", "grass_block", "dirt", "sand", "sandstone", "gravel",
			"clay", "snow", "snow_block", "powder_snow", "packed_ice", "podzol", "coarse_dirt", "mycelium", "mud", "red_sand",
			"terracotta", "calcite", "tuff", "granite", "diorite", "andesite", "moss_block", "moss_carpet", "cave_vines",
			"spore_blossom", "azalea", "flowering_azalea", "short_grass", "tall_grass", "fern", "large_fern", "dripstone_block",
			"pointed_dripstone", "sculk", "sculk_vein", "sculk_sensor", "sculk_shrieker", "sculk_catalyst", "glow_lichen",
			"sulfur", "potent_sulfur", "cinnabar", "sulfur_spike", "coal_ore", "deepslate_coal_ore", "iron_ore",
			"deepslate_iron_ore", "copper_ore", "deepslate_copper_ore", "gold_ore", "deepslate_gold_ore", "redstone_ore",
			"deepslate_redstone_ore", "lapis_ore", "deepslate_lapis_ore", "diamond_ore", "deepslate_diamond_ore", "emerald_ore",
			"deepslate_emerald_ore", "kelp", "seagrass", "sugar_cane", "pumpkin", "melon", "sweet_berry_bush", "lily_pad",
			"dead_bush", "cactus", "brown_mushroom", "red_mushroom", "hanging_roots", "rooted_dirt", "clay", "amethyst_block",
			"budding_amethyst", "calcite", "smooth_basalt", "amethyst_cluster", "sea_pickle", "tube_coral_block",
			"brain_coral_block", "bubble_coral_block", "fire_coral_block", "horn_coral_block", "tube_coral", "brain_coral",
			"bubble_coral", "fire_coral", "horn_coral", "pale_moss_block", "pale_moss_carpet", "firefly_bush", "bush",
			"short_dry_grass", "tall_dry_grass", "leaf_litter", "wildflowers", "pink_petals", "big_dripleaf", "obsidian",
			"magma_block", "blue_ice", "infested_stone"]:
		I[n] = BlockDB.id(n)
	stone_like = PackedInt32Array([I["stone"], I["granite"], I["diorite"], I["andesite"], I["tuff"], I["calcite"]])
	deep_like = PackedInt32Array([I["deepslate"], I["tuff"]])
	ore_target = PackedByteArray()
	ore_target.resize(BlockDB.count)
	blob_target = PackedByteArray()
	blob_target.resize(BlockDB.count)
	for sid in stone_like:
		ore_target[sid] = 1
	ore_target[I["deepslate"]] = 2
	ore_target[I["tuff"]] = 2
	blob_target[I["stone"]] = 1
	blob_target[I["deepslate"]] = 1
	band_ids = PackedInt32Array()
	for bc in BAND_COLORS:
		band_ids.append(BlockDB.id(bc + "_terracotta") if bc != "" else I["terracotta"])
	for b in BiomeDB.defs:
		biome_ids[b.name] = b.id
	Trees.init()


# ------------------------------------------------------------------------------------------------
## Returns [height, cont, ero, weird, temp, hum, river, mountain, mushroom]
func column(x: int, z: int) -> PackedFloat32Array:
	var fx := float(x)
	var fz := float(z)
	var c := clampf(n_cont.get_noise_2d(fx, fz) * 1.75 + 0.08, -1.0, 1.0)
	var e := clampf(n_ero.get_noise_2d(fx, fz) * 1.7, -1.0, 1.0)
	var w := clampf(n_weird.get_noise_2d(fx, fz) * 1.7, -1.0, 1.0)
	var t := n_temp.get_noise_2d(fx, fz) * 1.6
	var hum := n_hum.get_noise_2d(fx, fz) * 1.6
	var pv := 1.0 - absf(3.0 * absf(w) - 2.0)
	var base := spline(c, cont_spline)
	var inland := smoothstep(-0.12, 0.06, c)
	var m := smoothstep(0.0, 0.5, c) * (1.0 - smoothstep(-0.55, 0.2, e))
	var mountain := spline(pv, pv_spline) * m
	var hill_amp := lerpf(20.0, 3.0, smoothstep(-0.5, 0.7, e))
	var hills := (pv * 0.5 + 0.35) * hill_amp * inland * (1.0 - m)
	var det := n_detail.get_noise_2d(fx, fz) * lerpf(5.0, 1.6, smoothstep(-0.2, 0.8, e))
	var h := base + mountain + hills + det
	# swamps flatten toward the water line
	var sw := smoothstep(0.25, 0.55, hum) * smoothstep(0.35, 0.7, e) * inland * (1.0 - m) * smoothstep(-0.3, 0.05, t) * (1.0 - smoothstep(0.5, 0.8, t))
	if sw > 0.0:
		h = lerpf(h, 62.6 + det * 0.35, sw * 0.9)
	# rivers
	var rv := absf(n_river.get_noise_2d(fx, fz))
	var rf := (1.0 - smoothstep(0.0, 0.05, rv)) * inland * (1.0 - smoothstep(0.35, 0.7, m))
	if rf > 0.0:
		h = lerpf(h, 57.5 + det * 0.3, smoothstep(0.0, 1.0, rf))
	# mushroom islands
	var mush := 0.0
	if c < -0.4:
		mush = smoothstep(0.55, 0.68, n_mush.get_noise_2d(fx, fz) * 1.6)
		if mush > 0.0:
			h = lerpf(h, 67.0 + det, mush)
	return PackedFloat32Array([h, c, e, w, t, hum, rf, m, mush])


func pick_biome(col: PackedFloat32Array) -> int:
	var h := col[0]
	var c := col[1]
	var e := col[2]
	var w := col[3]
	var t := col[4]
	var hum := col[5]
	var rf := col[6]
	var m := col[7]
	var ti := 0 if t < -0.45 else (1 if t < -0.15 else (2 if t < 0.2 else (3 if t < 0.55 else 4)))
	var hi := 0 if hum < -0.35 else (1 if hum < -0.1 else (2 if hum < 0.1 else (3 if hum < 0.3 else 4)))
	if col[8] > 0.5:
		return biome_ids["mushroom_fields"]
	if h < sea_level - 1.5 and c < -0.08:
		if c < -0.42:
			match ti:
				0: return biome_ids["frozen_ocean"]
				1: return biome_ids["cold_ocean"]
				4: return biome_ids["warm_ocean"]
				3: return biome_ids["lukewarm_ocean"]
			return biome_ids["deep_ocean"]
		match ti:
			0: return biome_ids["frozen_ocean"]
			1: return biome_ids["cold_ocean"]
			3: return biome_ids["lukewarm_ocean"]
			4: return biome_ids["warm_ocean"]
		return biome_ids["ocean"]
	if rf > 0.45 and h < sea_level + 0.5:
		return biome_ids["frozen_river"] if ti == 0 else biome_ids["river"]
	if c < 0.03 and h < sea_level + 2.5 and h >= sea_level - 2.0 and m < 0.25:
		if ti == 0:
			return biome_ids["snowy_beach"]
		if e < -0.25:
			return biome_ids["stony_shore"]
		return biome_ids["beach"]
	if h > 172.0 and m > 0.35:
		if ti <= 1:
			return biome_ids["frozen_peaks"] if w < 0.0 else biome_ids["jagged_peaks"]
		if ti == 2:
			return biome_ids["jagged_peaks"]
		return biome_ids["stony_peaks"]
	if h > 128.0 and m > 0.28:
		if ti <= 1:
			return biome_ids["snowy_slopes"]
		if ti == 2:
			return biome_ids["grove"] if hi >= 2 else biome_ids["snowy_slopes"]
		if ti == 3:
			return biome_ids["cherry_grove"] if w > 0.1 else biome_ids["meadow"]
		return biome_ids["windswept_savanna"]
	if m > 0.2 and h > 96.0:
		if ti == 2 or ti == 3:
			return biome_ids["cherry_grove"] if w > 0.35 else biome_ids["meadow"]
		if ti <= 1:
			return biome_ids["windswept_hills"]
		return biome_ids["windswept_savanna"]
	if hi >= 3 and e > 0.35 and h < 67.0 and (ti == 2 or ti == 3):
		if ti == 3 and hi == 4:
			return biome_ids["mangrove_swamp"]
		return biome_ids["swamp"]
	match ti:
		0:
			if hi <= 1:
				return biome_ids["ice_spikes"] if w > 0.55 else biome_ids["snowy_plains"]
			if hi == 2:
				return biome_ids["snowy_plains"]
			return biome_ids["snowy_taiga"]
		1:
			if hi <= 1:
				return biome_ids["plains"]
			if hi == 2:
				return biome_ids["forest"]
			if hi == 3:
				return biome_ids["taiga"]
			return biome_ids["old_growth_spruce_taiga"]
		2:
			if hi == 0:
				return biome_ids["flower_forest"] if w > 0.35 else biome_ids["plains"]
			if hi == 1:
				return biome_ids["sunflower_plains"] if w > 0.55 else biome_ids["plains"]
			if hi == 2:
				return biome_ids["forest"]
			if hi == 3:
				return biome_ids["birch_forest"]
			return biome_ids["pale_garden"] if w > 0.4 else biome_ids["dark_forest"]
		3:
			if hi <= 1:
				return biome_ids["savanna"]
			if hi == 2:
				return biome_ids["forest"]
			if hi == 3:
				return biome_ids["jungle"]
			return biome_ids["bamboo_jungle"] if w > 0.25 else biome_ids["jungle"]
	# hot
	if hi <= 2:
		return biome_ids["desert"]
	if c > 0.15:
		return biome_ids["wooded_badlands"] if hi == 4 else biome_ids["badlands"]
	return biome_ids["savanna"]


func surface_height(x: int, z: int) -> int:
	return int(floor(column(x, z)[0]))


func biome_at(x: int, z: int) -> int:
	return pick_biome(column(x, z))


func cave_biome(x: int, y: int, z: int, col: PackedFloat32Array) -> int:
	var h := col[0]
	if y > h - 12.0:
		return -1
	var fx := float(x)
	var fz := float(z)
	if y >= -40 and y <= 32 and n_sulfur.get_noise_2d(fx, fz) > 0.42:
		return biome_ids["sulfur_caves"]
	if y < -18 and col[2] < -0.3 and n_cave_region.get_noise_2d(fx, fz) > 0.05:
		return biome_ids["deep_dark"]
	if col[5] > 0.33 and y < 55:
		return biome_ids["lush_caves"]
	if col[1] > 0.45 and y < 80:
		return biome_ids["dripstone_caves"]
	return -1


func biome_at_3d(x: int, y: int, z: int) -> int:
	var col := column(x, z)
	var cb := cave_biome(x, y, z, col)
	return cb if cb >= 0 else pick_biome(col)


func find_spawn() -> Vector3:
	var best := Vector3(0.5, 80, 0.5)
	var bad := ["ocean", "deep_ocean", "warm_ocean", "lukewarm_ocean", "cold_ocean", "frozen_ocean", "river", "frozen_river",
		"jagged_peaks", "frozen_peaks", "stony_peaks", "snowy_slopes", "mushroom_fields", "beach", "stony_shore", "swamp",
		"mangrove_swamp"]
	var good := ["plains", "forest", "flower_forest", "birch_forest", "meadow", "sunflower_plains", "cherry_grove", "savanna", "taiga"]
	var fallback := Vector3.INF
	for r in range(0, 1200, 24):
		var steps := maxi(1, r / 12)
		for s in steps:
			var ang := TAU * s / steps
			var x := int(round(cos(ang) * r))
			var z := int(round(sin(ang) * r))
			var col := column(x, z)
			var b: String = BiomeDB.name_of(pick_biome(col))
			if col[0] < sea_level + 1 or b in bad:
				continue
			if b in good:
				return Vector3(x + 0.5, floor(col[0]) + 1.0, z + 0.5)
			if fallback == Vector3.INF:
				fallback = Vector3(x + 0.5, floor(col[0]) + 1.0, z + 0.5)
	return fallback if fallback != Vector3.INF else best


# ------------------------------------------------------------------------------------------------
func generate(c: Chunk) -> void:
	var ox := c.cx * 16
	var oz := c.cz * 16
	var blocks := c.blocks
	var MY := min_y
	# padded 18x18 column data for slope detection
	var cols: Array = []
	cols.resize(324)
	var hgrid := PackedFloat32Array()
	hgrid.resize(324)
	for pz in 18:
		for px in 18:
			var col := column(ox + px - 1, oz + pz - 1)
			cols[pz * 18 + px] = col
			hgrid[pz * 18 + px] = col[0]
	var stone: int = I["stone"]
	var deep: int = I["deepslate"]
	var water: int = I["water"]
	var ice: int = I["ice"]
	var bedrock: int = I["bedrock"]
	var hmap := PackedInt32Array()
	hmap.resize(256)
	var bio := PackedInt32Array()
	bio.resize(256)
	var rng := chunk_rng(c.cx, c.cz, 1)
	for lz in 16:
		for lx in 16:
			var gi := (lz + 1) * 18 + lx + 1
			var col: PackedFloat32Array = cols[gi]
			var h := int(floor(col[0]))
			var b := pick_biome(col)
			var bi := (lz << 4) | lx
			hmap[bi] = h
			bio[bi] = b
			c.biomes[bi] = b
			var x := ox + lx
			var z := oz + lz
			# base stone column
			var top_yr := mini(h - MY, height - 1)
			for yr in range(0, top_yr + 1):
				var y := yr + MY
				var v := stone
				if y < 0:
					if y < -8 or (MY + yr) < -((hash(x * 31 + z * 17 + y) & 7) + 1):
						v = deep
				blocks[(yr << 8) | bi] = v
			# bedrock floor
			blocks[bi] = bedrock
			for k in range(1, 5):
				if (hash(x * 73 + z * 19 + k * 7 + seed_value) & 7) < 5 - k:
					blocks[(k << 8) | bi] = bedrock
			# water
			if h < sea_level - 1:
				var bd: Dictionary = BiomeDB.defs[b]
				for y in range(h + 1, sea_level):
					blocks[((y - MY) << 8) | bi] = water
				if bd.snowy:
					blocks[((sea_level - 1 - MY) << 8) | bi] = ice
			_surface(c, lx, lz, x, z, h, b, col, hgrid, gi, rng)
	_carve_noise_caves(c, hmap)
	_carve_worms(c, hmap)
	_ores(c)
	_cave_decor(c, hmap, cols)
	_vegetation(c, hmap, bio, cols)
	_trees(c, hmap)
	if structures != null:
		structures.generate_into(c, self)
	_biome_colors(c, cols)
	c.rebuild_derived()


func _surface(c: Chunk, lx: int, lz: int, x: int, z: int, h: int, b: int, col: PackedFloat32Array, hgrid: PackedFloat32Array,
		gi: int, rng: RandomNumberGenerator) -> void:
	var bd: Dictionary = BiomeDB.defs[b]
	var blocks := c.blocks
	var MY := min_y
	var bi := (lz << 4) | lx
	var stone: int = I["stone"]
	var bname: String = bd.name
	var slope := maxf(absf(hgrid[gi + 1] - hgrid[gi - 1]), absf(hgrid[gi + 18] - hgrid[gi - 18]))
	var patch := n_patch.get_noise_2d(float(x), float(z))
	var top_n: String = bd.top
	var fill_n: String = bd.filler
	var depth: int = bd.depth + int(patch * 1.5 + 1.0)
	var under_water := h < sea_level - 1
	# surface overrides
	if under_water:
		match bname:
			"ocean", "deep_ocean", "cold_ocean", "frozen_ocean":
				top_n = "gravel" if patch < 0.2 else ("sand" if patch < 0.45 else "clay")
				fill_n = top_n
			"river", "frozen_river":
				top_n = "sand" if patch > -0.3 else ("clay" if patch < -0.5 else "gravel")
				fill_n = "sand"
			"swamp", "mangrove_swamp":
				top_n = "mud" if bname == "mangrove_swamp" else ("clay" if patch > 0.3 else "dirt")
				fill_n = "dirt"
			"warm_ocean", "lukewarm_ocean", "beach":
				top_n = "sand"
				fill_n = "sand"
			_:
				if top_n == "grass_block" or top_n == "podzol" or top_n == "mycelium":
					top_n = "dirt" if patch > 0.0 else "sand"
				if top_n == "snow_block":
					top_n = "gravel"
	else:
		match bname:
			"old_growth_spruce_taiga":
				top_n = "podzol" if patch > -0.2 else ("coarse_dirt" if patch < -0.5 else "grass_block")
			"savanna", "windswept_savanna":
				if patch > 0.45:
					top_n = "coarse_dirt"
			"windswept_hills":
				if slope > 4.0 or patch > 0.5:
					top_n = "stone" if patch < 0.6 else "gravel"
					fill_n = top_n
			"stony_peaks":
				top_n = "calcite" if patch > 0.35 else "stone"
				fill_n = "stone"
			"jagged_peaks", "frozen_peaks", "snowy_slopes":
				if slope > 7.0:
					top_n = "stone"
					fill_n = "stone"
				elif bname == "frozen_peaks" and patch > 0.3:
					top_n = "packed_ice"
			"pale_garden":
				if patch > 0.25:
					top_n = "pale_moss_block"
			"badlands", "wooded_badlands":
				if bname == "wooded_badlands" and h > 90:
					top_n = "coarse_dirt" if patch > 0.0 else "grass_block"
					fill_n = "dirt"
				if slope > 3.0:
					top_n = "orange_terracotta"
		if slope > 9.0 and top_n in ["grass_block", "dirt", "podzol", "snow_block"] and h > 90:
			top_n = "stone"
			fill_n = "stone"
	var top_id := BlockDB.id(top_n)
	var fill_id := BlockDB.id(fill_n)
	var under_id := BlockDB.id(bd.under)
	if top_id == 0:
		top_id = I["grass_block"]
	var badlands := bname == "badlands" or bname == "wooded_badlands"
	for d in range(0, depth + 4):
		var y := h - d
		if y < MY + 5:
			break
		var i := ((y - MY) << 8) | bi
		var cur := blocks[i] & 0xFFF
		if cur != I["stone"] and cur != I["deepslate"]:
			continue
		var v := stone
		if d == 0:
			v = top_id
		elif d <= depth:
			v = fill_id
		elif d <= depth + 3 and under_id != I["stone"]:
			v = under_id
		else:
			continue
		if badlands and d >= 1 and y >= sea_level - 4:
			var band := posmod(y + int(n_band.get_noise_2d(float(x), float(z)) * 4.0), band_ids.size())
			v = band_ids[band]
		blocks[i] = v
	if badlands:
		# exposed terracotta strata on slopes/mesas
		var hb := h - depth - 1
		while hb > sea_level - 4 and hb > h - 40:
			var i2 := ((hb - MY) << 8) | bi
			if (blocks[i2] & 0xFFF) == stone:
				var band2 := posmod(hb + int(n_band.get_noise_2d(float(x), float(z)) * 4.0), band_ids.size())
				blocks[i2] = band_ids[band2]
			hb -= 1
	# snow cover
	var snowy: bool = bd.snowy or h > 165 + int(patch * 8.0)
	if snowy and not under_water and h + 1 < min_y + height:
		var ti := ((h + 1 - MY) << 8) | bi
		if blocks[ti] == 0:
			var tb := blocks[((h - MY) << 8) | bi] & 0xFFF
			if tb != I["packed_ice"] and tb != I["water"] and tb != I["ice"]:
				blocks[ti] = I["snow"]
				if h > 180 and (bname == "jagged_peaks" or bname == "snowy_slopes" or bname == "frozen_peaks"):
					blocks[((h - MY) << 8) | bi] = I["snow_block"] if patch < 0.4 else I["powder_snow"]


# ------------------------------------------------------------------------------------------------
func _carve_noise_caves(c: Chunk, hmap: PackedInt32Array) -> void:
	var MY := min_y
	var ox := c.cx * 16
	var oz := c.cz * 16
	var blocks := c.blocks
	var lava: int = I["lava"]
	var water: int = I["water"]
	var bedrock: int = I["bedrock"]
	var nc := n_cheese
	var na := n_spag_a
	var nb := n_spag_b
	var nn := n_noodle
	var y0 := MY + 5
	for lz in 16:
		var z := float(oz + lz)
		for lx in 16:
			var x := float(ox + lx)
			var bi := (lz << 4) | lx
			var h := hmap[bi]
			var wet := h < sea_level
			var top_allowed := h - 7 if wet else h + 1
			var entrance := n_patch.get_noise_2d(x * 0.12, z * 0.12) > 0.45
			if not entrance and not wet:
				top_allowed = h - 3
			var y := y0
			while y <= top_allowed:
				var fy := float(y)
				var carve := false
				var cheese_thr := 0.36 + clampf((fy + 10.0) / 280.0, 0.0, 0.2)
				if fy < h - 8 and nc.get_noise_3d(x, fy * 1.7, z) > cheese_thr:
					carve = true
				else:
					var a := na.get_noise_3d(x, fy * 1.4, z)
					if a > -0.075 and a < 0.075:
						var bb := nb.get_noise_3d(x, fy * 1.4, z)
						if bb > -0.075 and bb < 0.075:
							carve = true
					if not carve and fy < h - 5 and a > -0.25 and a < 0.25:
						var nv := nn.get_noise_3d(x, fy, z)
						if nv > -0.03 and nv < 0.03:
							carve = true
				if carve:
					var i := ((y - MY) << 8) | bi
					var cur := blocks[i]
					var cid := cur & 0xFFF
					if cur != 0 and cid != bedrock and cid != water and (blocks[i + 256] & 0xFFF) != water:
						blocks[i] = lava if y <= -55 else 0
				y += 1


func _carve_worms(c: Chunk, hmap: PackedInt32Array) -> void:
	var R := 5
	var ox := float(c.cx * 16)
	var oz := float(c.cz * 16)
	for sz in range(c.cz - R, c.cz + R + 1):
		for sx in range(c.cx - R, c.cx + R + 1):
			var rng := chunk_rng(sx, sz, 7)
			if rng.randi_range(0, 6) == 0:
				var n := rng.randi_range(1, 3)
				for k in n:
					var x := sx * 16 + rng.randf() * 16.0
					var y := rng.randf_range(-52.0, 92.0)
					if rng.randf() < 0.4:
						y = rng.randf_range(-52.0, 30.0)
					var z := sz * 16 + rng.randf() * 16.0
					var yaw := rng.randf() * TAU
					var pitch := rng.randf_range(-0.4, 0.4)
					var rad := rng.randf_range(1.2, 3.2)
					if rng.randf() < 0.08:
						rad = rng.randf_range(4.0, 6.5)
					var steps := rng.randi_range(60, 130)
					_worm(c, hmap, rng, x, y, z, yaw, pitch, rad, steps, ox, oz, true)
			if rng.randi_range(0, 90) == 0:
				# ravine
				var rx := sx * 16 + rng.randf() * 16.0
				var ry := rng.randf_range(10.0, 60.0)
				var rz := sz * 16 + rng.randf() * 16.0
				_ravine(c, hmap, rng, rx, ry, rz, ox, oz)


func _worm(c: Chunk, hmap: PackedInt32Array, rng: RandomNumberGenerator, x: float, y: float, z: float, yaw: float,
		pitch: float, rad: float, steps: int, ox: float, oz: float, branch: bool) -> void:
	var dyaw := 0.0
	var dpitch := 0.0
	var branch_at := steps / 2 + rng.randi_range(-steps / 5, steps / 5)
	for s in steps:
		var r := rad * (0.6 + sin(float(s) / steps * PI) * 0.6)
		x += cos(yaw) * cos(pitch)
		z += sin(yaw) * cos(pitch)
		y += sin(pitch)
		pitch *= 0.7
		pitch += dpitch * 0.1
		yaw += dyaw * 0.1
		dpitch = dpitch * 0.9 + rng.randf_range(-1.0, 1.0) * 1.2
		dyaw = dyaw * 0.75 + rng.randf_range(-1.0, 1.0) * 2.0
		if branch and s == branch_at and rad > 1.0:
			_worm(c, hmap, rng, x, y, z, yaw + PI * 0.5, pitch / 3.0, rad * rng.randf_range(0.5, 0.9), steps - s, ox, oz, false)
			_worm(c, hmap, rng, x, y, z, yaw - PI * 0.5, pitch / 3.0, rad * rng.randf_range(0.5, 0.9), steps - s, ox, oz, false)
			return
		if x + r < ox or x - r > ox + 16.0 or z + r < oz or z - r > oz + 16.0:
			continue
		_carve_sphere(c, hmap, x, y, z, r, r * 0.75)


func _ravine(c: Chunk, hmap: PackedInt32Array, rng: RandomNumberGenerator, x: float, y: float, z: float, ox: float, oz: float) -> void:
	var yaw := rng.randf() * TAU
	var steps := rng.randi_range(70, 110)
	var width := rng.randf_range(1.8, 3.2)
	var depth := rng.randf_range(10.0, 18.0)
	var dyaw := 0.0
	for s in steps:
		var t := sin(float(s) / steps * PI)
		x += cos(yaw)
		z += sin(yaw)
		y += rng.randf_range(-0.15, 0.15)
		dyaw = dyaw * 0.8 + rng.randf_range(-1.0, 1.0) * 0.5
		yaw += dyaw * 0.05
		var r := width * t + 0.6
		if x + r < ox or x - r > ox + 16.0 or z + r < oz or z - r > oz + 16.0:
			continue
		_carve_sphere(c, hmap, x, y, z, r, depth * t + 2.0)


func _carve_sphere(c: Chunk, hmap: PackedInt32Array, cx: float, cy: float, cz: float, rh: float, rv: float) -> void:
	var ox := c.cx * 16
	var oz := c.cz * 16
	var MY := min_y
	var blocks := c.blocks
	var water: int = I["water"]
	var lava: int = I["lava"]
	var bedrock: int = I["bedrock"]
	var ax := maxi(int(floor(cx - rh)), ox)
	var bx := mini(int(floor(cx + rh)), ox + 15)
	var az := maxi(int(floor(cz - rh)), oz)
	var bz := mini(int(floor(cz + rh)), oz + 15)
	var ay := maxi(int(floor(cy - rv)), MY + 5)
	var by := mini(int(floor(cy + rv)), MY + height - 2)
	for x in range(ax, bx + 1):
		var dx := (x + 0.5 - cx) / rh
		for z in range(az, bz + 1):
			var dz := (z + 0.5 - cz) / rh
			var d2 := dx * dx + dz * dz
			if d2 >= 1.0:
				continue
			var bi := ((z - oz) << 4) | (x - ox)
			var h := hmap[bi]
			var wet := h < sea_level
			for y in range(ay, by + 1):
				var dy := (y + 0.5 - cy) / rv
				if d2 + dy * dy >= 1.0:
					continue
				if wet and y > h - 7:
					continue
				var i := ((y - MY) << 8) | bi
				var cur := blocks[i] & 0xFFF
				if cur == 0 or cur == bedrock or cur == water:
					continue
				if (blocks[i + 256] & 0xFFF) == water:
					continue
				blocks[i] = lava if y <= -55 else 0


# ------------------------------------------------------------------------------------------------
func _ores(c: Chunk) -> void:
	var w := WorldGen.Writer.new(c)
	var mountain_emerald := false
	var center_b: String = BiomeDB.name_of(c.biomes[136])
	if center_b in ["windswept_hills", "jagged_peaks", "frozen_peaks", "stony_peaks", "snowy_slopes", "grove", "meadow", "cherry_grove"]:
		mountain_emerald = true
	var badlands := center_b.contains("badlands")
	for sz in range(c.cz - 1, c.cz + 2):
		for sx in range(c.cx - 1, c.cx + 2):
			var rng := chunk_rng(sx, sz, 3)
			var bx := sx * 16
			var bz := sz * 16
			# [ore, deep, attempts, size, ymin, ymax, triangle]
			var table := [
				["coal_ore", "deepslate_coal_ore", 20, 17, 0, 192, true],
				["coal_ore", "deepslate_coal_ore", 12, 17, 136, 320, false],
				["iron_ore", "deepslate_iron_ore", 10, 9, -24, 56, true],
				["iron_ore", "deepslate_iron_ore", 12, 9, 80, 256, true],
				["iron_ore", "deepslate_iron_ore", 10, 4, -64, 72, false],
				["copper_ore", "deepslate_copper_ore", 16, 10, -16, 112, true],
				["gold_ore", "deepslate_gold_ore", 4, 9, -64, 32, true],
				["redstone_ore", "deepslate_redstone_ore", 4, 8, -64, 15, false],
				["redstone_ore", "deepslate_redstone_ore", 8, 8, -64, -32, true],
				["lapis_ore", "deepslate_lapis_ore", 2, 7, -32, 32, true],
				["lapis_ore", "deepslate_lapis_ore", 4, 7, -64, 64, false],
				["diamond_ore", "deepslate_diamond_ore", 7, 5, -64, 16, true],
				["granite", "granite", 2, 40, 0, 60, false],
				["diorite", "diorite", 2, 40, 0, 60, false],
				["andesite", "andesite", 2, 40, 0, 60, false],
				["tuff", "tuff", 2, 40, -64, 0, false],
				["dirt", "dirt", 5, 24, 0, 160, false],
				["gravel", "gravel", 6, 24, -64, 200, false],
			]
			if badlands:
				table.append(["gold_ore", "deepslate_gold_ore", 30, 9, 32, 256, false])
			for e in table:
				var ore: int = BlockDB.id(e[0])
				var dore: int = BlockDB.id(e[1])
				var attempts: int = e[2]
				var size: int = e[3]
				var y0: int = e[4]
				var y1: int = e[5]
				var tri: bool = e[6]
				for a in attempts:
					var x := bx + rng.randf() * 16.0
					var z := bz + rng.randf() * 16.0
					var y: float
					if tri:
						y = (rng.randf_range(y0, y1) + rng.randf_range(y0, y1)) * 0.5
					else:
						y = rng.randf_range(y0, y1)
					var reach := size / 8.0 + 3.0
					if x + reach < w.ox or x - reach > w.ox + 16 or z + reach < w.oz or z - reach > w.oz + 16:
						continue
					if e[0] == "granite" or e[0] == "diorite" or e[0] == "andesite" or e[0] == "tuff" or e[0] == "dirt" or e[0] == "gravel":
						ore_vein(w, rng, x, y, z, size, ore, dore, blob_target)
					else:
						ore_vein(w, rng, x, y, z, size, ore, dore, ore_target)
			if rng.randf() < 0.12:
				# rare large diamond vein
				ore_vein(w, rng, bx + rng.randf() * 16.0, rng.randf_range(-60.0, -20.0), bz + rng.randf() * 16.0, 11,
					I["diamond_ore"], I["deepslate_diamond_ore"], ore_target)
			if mountain_emerald and sx == c.cx and sz == c.cz:
				for a in 40:
					var ex := bx + rng.randi_range(0, 15)
					var ey := rng.randi_range(-16, 256)
					var ez := bz + rng.randi_range(0, 15)
					var cur := w.get_b(ex, ey, ez)
					if cur >= 0 and (cur & 0xFFF) == I["stone"]:
						w.put(ex, ey, ez, I["emerald_ore"])
			# amethyst geode (rare)
			if rng.randf() < 0.02 and sx == c.cx and sz == c.cz:
				_geode(w, rng, bx + 8, rng.randi_range(-50, 20), bz + 8)


func _geode(w: WorldGen.Writer, rng: RandomNumberGenerator, cx: int, cy: int, cz: int) -> void:
	var r := rng.randf_range(4.0, 6.0)
	for y in range(cy - 7, cy + 8):
		for z in range(cz - 7, cz + 8):
			for x in range(cx - 7, cx + 8):
				var d := Vector3(x - cx, y - cy, z - cz).length()
				if d > r + 1.5:
					continue
				var cur := w.get_b(x, y, z)
				if cur < 0 or cur == 0:
					continue
				if d > r + 0.8:
					w.put(x, y, z, I["smooth_basalt"])
				elif d > r:
					w.put(x, y, z, I["calcite"])
				elif d > r - 1.0:
					w.put(x, y, z, I["budding_amethyst"] if rng.randf() < 0.1 else I["amethyst_block"])
				else:
					w.put(x, y, z, 0)
					if d > r - 2.0 and rng.randf() < 0.1:
						w.put(x, y, z, Vox.make(I["amethyst_cluster"], Vox.UP))


# ------------------------------------------------------------------------------------------------
func _cave_decor(c: Chunk, hmap: PackedInt32Array, cols: Array) -> void:
	var blocks := c.blocks
	var MY := min_y
	var ox := c.cx * 16
	var oz := c.cz * 16
	var rng := chunk_rng(c.cx, c.cz, 21)
	var solid := BlockDB.solid
	var full := BlockDB.full
	var stone: int = I["stone"]
	var deep: int = I["deepslate"]
	for lz in 16:
		for lx in 16:
			var bi := (lz << 4) | lx
			var col: PackedFloat32Array = cols[(lz + 1) * 18 + lx + 1]
			var h := hmap[bi]
			var x := ox + lx
			var z := oz + lz
			var y := MY + 6
			var ytop := h - 4
			while y < ytop:
				var i := ((y - MY) << 8) | bi
				var v := blocks[i]
				if v != 0:
					y += 1
					continue
				# found air cell inside a cave: classify floor/ceiling around it
				var below := blocks[i - 256] & 0xFFF
				var above := blocks[i + 256] & 0xFFF
				var floor_cell := full[below] == 1
				var ceil_cell := full[above] == 1
				if not floor_cell and not ceil_cell:
					y += 1
					continue
				var cb := cave_biome(x, y, z, col)
				var r := rng.randf()
				if cb == biome_ids.get("lush_caves", -2):
					if floor_cell:
						blocks[i - 256] = I["moss_block"]
						if r < 0.3:
							blocks[i] = I["moss_carpet"]
						elif r < 0.45:
							blocks[i] = I["short_grass"]
						elif r < 0.5:
							blocks[i] = I["azalea"] if r < 0.48 else I["flowering_azalea"]
					if ceil_cell:
						if r < 0.12:
							var ln := rng.randi_range(1, 6)
							for k in ln:
								var j := i - k * 256
								if j < 0 or blocks[j] != 0:
									break
								blocks[j] = Vox.make(I["cave_vines"], 1 if rng.randf() < 0.25 else 0)
						elif r > 0.985:
							blocks[i] = I["spore_blossom"]
						else:
							blocks[i + 256] = I["moss_block"] if rng.randf() < 0.6 else blocks[i + 256]
				elif cb == biome_ids.get("dripstone_caves", -2):
					if floor_cell and r < 0.12:
						blocks[i - 256] = I["dripstone_block"]
						blocks[i] = Vox.make(I["pointed_dripstone"], 1)
					elif ceil_cell and r > 0.85:
						blocks[i + 256] = I["dripstone_block"]
						blocks[i] = I["pointed_dripstone"]
				elif cb == biome_ids.get("deep_dark", -2):
					if floor_cell:
						if r < 0.55:
							blocks[i - 256] = I["sculk"]
						if r < 0.012:
							blocks[i] = I["sculk_sensor"]
						elif r < 0.016:
							blocks[i] = I["sculk_shrieker"]
						elif r < 0.018:
							blocks[i - 256] = I["sculk_catalyst"]
						elif r < 0.1:
							blocks[i] = Vox.make(I["sculk_vein"], 0)
				elif cb == biome_ids.get("sulfur_caves", -2):
					if floor_cell:
						blocks[i - 256] = I["sulfur"] if r < 0.7 else I["cinnabar"]
						if r < 0.035:
							blocks[i - 256] = I["potent_sulfur"]
						elif r < 0.09:
							blocks[i] = Vox.make(I["sulfur_spike"], 1)
					elif ceil_cell and r > 0.9:
						blocks[i] = I["sulfur_spike"]
				else:
					if ceil_cell and r < 0.02 and y > 0:
						blocks[i] = Vox.make(I["glow_lichen"], 0)
					elif floor_cell and r < 0.004:
						blocks[i] = I["brown_mushroom"] if r < 0.002 else I["red_mushroom"]
				y += 1
			# sulfur caves: banded walls
			var cbw := cave_biome(x, -5, z, col)
			if cbw == biome_ids.get("sulfur_caves", -2):
				for yy in range(-40, mini(33, h - 12)):
					var ii := ((yy - MY) << 8) | bi
					var cur := blocks[ii] & 0xFFF
					if cur == stone or cur == deep or cur == I["tuff"]:
						var band := posmod(yy + int(n_band.get_noise_2d(float(x) * 3.0, float(z) * 3.0) * 3.0), 9)
						if band < 3:
							blocks[ii] = I["sulfur"]
						elif band == 5:
							blocks[ii] = I["cinnabar"]
				# sulfur pools: fill low floor dips with water above potent sulfur
			if cbw == biome_ids.get("sulfur_caves", -2) and rng.randf() < 0.2:
				for yy2 in range(-38, mini(30, h - 14)):
					var i2 := ((yy2 - MY) << 8) | bi
					if blocks[i2] == 0 and full[blocks[i2 - 256] & 0xFFF] == 1 and blocks[i2 + 256] == 0:
						blocks[i2 - 256] = I["potent_sulfur"]
						blocks[i2] = I["water"]
						break


# ------------------------------------------------------------------------------------------------
func _vegetation(c: Chunk, hmap: PackedInt32Array, bio: PackedInt32Array, cols: Array) -> void:
	var blocks := c.blocks
	var MY := min_y
	var rng := chunk_rng(c.cx, c.cz, 5)
	var grass_id: int = I["grass_block"]
	var sand_id: int = I["sand"]
	var water: int = I["water"]
	for lz in 16:
		for lx in 16:
			var bi := (lz << 4) | lx
			var h := hmap[bi]
			var b := bio[bi]
			var bd: Dictionary = BiomeDB.defs[b]
			var bname: String = bd.name
			var ti := ((h + 1 - MY) << 8) | bi
			if h + 2 >= MY + height:
				continue
			var top := blocks[((h - MY) << 8) | bi] & 0xFFF
			var above := blocks[ti]
			var r := rng.randf()
			if above == water:
				# underwater plants
				var floor_y := h
				var depth := sea_level - 1 - floor_y
				if depth >= 2:
					if bname.contains("ocean") or bname.contains("river"):
						if bname == "warm_ocean" and r < 0.08:
							var corals := ["tube", "brain", "bubble", "fire", "horn"]
							var ck: String = corals[rng.randi_range(0, 4)]
							blocks[((h - MY) << 8) | bi] = BlockDB.id(ck + "_coral_block")
							blocks[ti] = BlockDB.id(ck + "_coral")
						elif bname == "warm_ocean" and r < 0.1:
							blocks[ti] = Vox.make(I["sea_pickle"], rng.randi_range(0, 3))
						elif r < 0.25 and not bname.contains("frozen"):
							blocks[ti] = I["seagrass"]
						elif r < 0.33 and (bname == "ocean" or bname == "lukewarm_ocean" or bname == "cold_ocean" or bname == "deep_ocean"):
							var kl := rng.randi_range(2, mini(depth - 1, 18))
							for k in kl:
								var j := ti + k * 256
								if blocks[j] != water:
									break
								blocks[j] = I["kelp"]
				continue
			if above != 0:
				continue
			var grass_count: int = bd.grass_count
			var flower_count: int = bd.flower_count
			if top == grass_id or top == I["podzol"] or top == I["coarse_dirt"] or top == I["pale_moss_block"] or top == I["dirt"]:
				if r < grass_count / 256.0 * 3.0:
					var g: int = I["short_grass"]
					if bname.contains("taiga") or bname.contains("jungle") or bname == "grove":
						if rng.randf() < 0.4:
							g = I["fern"]
					if bname == "savanna" or bname.contains("windswept_savanna"):
						if rng.randf() < 0.1:
							g = I["short_dry_grass"]
					if rng.randf() < 0.1 and grass_count > 8:
						blocks[ti] = Vox.make(I["tall_grass"] if g != I["fern"] else I["large_fern"], 0)
						blocks[ti + 256] = Vox.make(I["tall_grass"] if g != I["fern"] else I["large_fern"], 1)
					else:
						blocks[ti] = g
				elif r < (grass_count * 3.0 + flower_count * 2.0) / 256.0 and flower_count > 0:
					var fl: Array = bd.flowers
					var fname: String = fl[rng.randi_range(0, fl.size() - 1)]
					var fid := BlockDB.id(fname)
					if fid != 0:
						var fdef: BlockDef = BlockDB.defs[fid]
						if fdef.model == BlockDB.M_TALL_CROSS:
							blocks[ti] = Vox.make(fid, 0)
							blocks[ti + 256] = Vox.make(fid, 1)
						elif fname == "sweet_berry_bush":
							blocks[ti] = Vox.make(fid, rng.randi_range(1, 3))
						elif fname == "melon" or fname == "pumpkin":
							if rng.randf() < 0.3:
								blocks[ti] = fid
						else:
							blocks[ti] = fid
				elif r > 0.9985 and (bname == "plains" or bname.contains("taiga") or bname == "forest"):
					blocks[ti] = I["pumpkin"]
				elif r > 0.997 and bname == "forest":
					blocks[ti] = I["bush"]
				elif r > 0.994 and (bname == "birch_forest" or bname == "dark_forest"):
					blocks[ti] = I["leaf_litter"]
				elif r > 0.992 and bname == "swamp":
					blocks[ti] = I["firefly_bush"]
			elif top == sand_id or top == I["red_sand"]:
				if (bname == "desert" or bname.contains("badlands")) and r < flower_count / 256.0:
					blocks[ti] = I["dead_bush"] if rng.randf() < 0.7 else I["short_dry_grass"]
			# sugar cane next to water at sea level
			if (top == grass_id or top == sand_id or top == I["dirt"]) and h == sea_level - 1 and r > 0.9 and blocks[ti] == 0:
				var near_water := false
				for d in 4:
					var nx: int = lx + Vox.H_FACING_VEC[d].x
					var nz: int = lz + Vox.H_FACING_VEC[d].z
					if nx >= 0 and nx < 16 and nz >= 0 and nz < 16:
						if (blocks[((h - MY) << 8) | (nz << 4) | nx] & 0xFFF) == water:
							near_water = true
				if near_water and not bd.snowy:
					for k in rng.randi_range(1, 3):
						blocks[ti + k * 256] = I["sugar_cane"]
			# lily pads on swamp water
			if (bname == "swamp" or bname == "mangrove_swamp") and top == water and r < 0.06:
				blocks[ti] = I["lily_pad"]
	# lily pads for swamp water columns (surface water is at sea_level - 1)
	for lz in 16:
		for lx in 16:
			var bi2 := (lz << 4) | lx
			var bd2: Dictionary = BiomeDB.defs[bio[bi2]]
			if bd2.name == "swamp" and hmap[bi2] < sea_level - 1 and rng.randf() < 0.05:
				var sy := ((sea_level - MY) << 8) | bi2
				if blocks[sy] == 0 and (blocks[sy - 256] & 0xFFF) == water:
					blocks[sy] = I["lily_pad"]


func _trees(c: Chunk, hmap: PackedInt32Array) -> void:
	var w := WorldGen.Writer.new(c)
	var MY := min_y
	for sz in range(c.cz - 1, c.cz + 2):
		for sx in range(c.cx - 1, c.cx + 2):
			var rng := chunk_rng(sx, sz, 11)
			var ccol := column(sx * 16 + 8, sz * 16 + 8)
			var cb := pick_biome(ccol)
			var bd: Dictionary = BiomeDB.defs[cb]
			var tc: float = bd.tree_count
			var trees: Array = bd.trees
			if trees.is_empty() or tc <= 0.0:
				continue
			var n := int(tc)
			if rng.randf() < tc - n:
				n += 1
			for k in n:
				var x := sx * 16 + rng.randi_range(0, 15)
				var z := sz * 16 + rng.randi_range(0, 15)
				var h: int
				var inside := sx == c.cx and sz == c.cz
				var col := ccol
				if inside:
					h = hmap[((z - w.oz) << 4) | (x - w.ox)]
				else:
					col = column(x, z)
					h = int(floor(col[0]))
				if h < sea_level:
					if not (bd.name == "mangrove_swamp" and h >= sea_level - 4):
						continue
				var tb := pick_biome(col) if not inside else c.biomes[((z - w.oz) << 4) | (x - w.ox)]
				var tbd: Dictionary = BiomeDB.defs[tb]
				if tbd.name != bd.name and (tbd.tree_count as float) <= 0.0:
					continue
				if inside:
					var top := w.get_b(x, h, z) & 0xFFF
					if not (top in [I["grass_block"], I["dirt"], I["podzol"], I["coarse_dirt"], I["sand"], I["mud"], I["mycelium"],
							I["snow_block"], I["pale_moss_block"], I["red_sand"]]):
						if top != I["snow"]:
							continue
					var above := w.get_b(x, h + 1, z)
					if above != 0 and (above & 0xFFF) != I["snow"] and (above & 0xFFF) != I["water"] and BlockDB.replaceable[above & 0xFFF] == 0:
						continue
				var kind := _pick_weighted(trees, rng)
				var ty := h + 1
				if kind == "iceberg":
					ty = sea_level - 1
				Trees.place(w, kind, x, ty, z, rng)


func _pick_weighted(list: Array, rng: RandomNumberGenerator) -> String:
	var total := 0
	for e in list:
		total += int(e[1])
	var r := rng.randi_range(0, maxi(total - 1, 0))
	for e in list:
		r -= int(e[1])
		if r < 0:
			return e[0]
	return list[0][0]


func _biome_colors(c: Chunk, cols: Array) -> void:
	# sample biomes on the padded 18x18 grid, then blur 3x3 for smooth transitions
	var bg := PackedInt32Array()
	bg.resize(324)
	for i in 324:
		bg[i] = pick_biome(cols[i])
	var grass := BiomeDB.grass
	var foliage := BiomeDB.foliage
	var water := BiomeDB.water
	for lz in 16:
		for lx in 16:
			var g := Color(0, 0, 0)
			var f := Color(0, 0, 0)
			var wc := Color(0, 0, 0)
			for dz in 3:
				for dx in 3:
					var b := bg[(lz + dz) * 18 + lx + dx]
					g += grass[b]
					f += foliage[b]
					wc += water[b]
			var bi := ((lz << 4) | lx) * 3
			c.colors[bi] = g / 9.0
			c.colors[bi + 1] = f / 9.0
			c.colors[bi + 2] = wc / 9.0
