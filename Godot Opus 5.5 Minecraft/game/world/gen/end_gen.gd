class_name EndGen
extends WorldGen
## The End: the central island (a ~120 block radius disc of end stone with the exit portal podium
## at the origin), the void around it, and the outer islands (~1000 blocks out) that carry chorus
## plants and, rarely, an End city. Also provides the gateway teleport destinations.

const MAIN_RADIUS := 118.0
const OUTER_START := 720.0
const OUTER_END := 2400.0

var n_island: FastNoiseLite
var n_height: FastNoiseLite
var n_chorus: FastNoiseLite

var I := {}


func setup(world_seed: int) -> void:
	seed_value = world_seed
	min_y = 0
	height = 256
	sea_level = 0
	has_sky = false
	dim_id = 2

	n_island = mk_noise(seed_value + 71, 0.0075, 3)
	n_height = mk_noise(seed_value + 82, 0.02, 2)
	n_chorus = mk_noise(seed_value + 93, 0.09, 1)

	I = {
		"end_stone": BlockDB.id("end_stone"), "air": 0, "obsidian": BlockDB.id("obsidian"),
		"bedrock": BlockDB.id("bedrock"), "end_portal": BlockDB.id("end_portal"),
		"end_portal_frame": BlockDB.id("end_portal_frame"), "chorus_plant": BlockDB.id("chorus_plant"),
		"chorus_flower": BlockDB.id("chorus_flower"), "purpur_block": BlockDB.id("purpur_block"),
		"end_gateway": BlockDB.id("end_gateway"), "end_stone_bricks": BlockDB.id("end_stone_bricks"),
	}


func biome_at(x: int, z: int) -> int:
	var d := Vector2(float(x), float(z)).length()
	if d < 640.0:
		return BiomeDB.id("the_end")
	return BiomeDB.id("end_highlands")


func surface_height(x: int, z: int) -> int:
	var h := _island_top(x, z)
	return h if h >= 0 else 0


func find_spawn() -> Vector3:
	return Vector3(0.5, float(PODIUM_Y + 4), 3.5)


## Height of the solid end stone at (x, z); -1 when it is void.
func _island_top(x: int, z: int) -> int:
	return _column(x, z).x


## (top, bottom) of the end stone column at (x, z); (-1, -1) over the void. The central island is a
## lens: a gently domed surface around y 56..64 (flat around the podium) over a deep rounded belly;
## the outer islands are thinner noise-shaped slabs.
func _column(x: int, z: int) -> Vector2i:
	var fx := float(x)
	var fz := float(z)
	var d := sqrt(fx * fx + fz * fz)
	var best := Vector2i(-1, -1)
	if d < MAIN_RADIUS + 24.0:
		var calm := smoothstep(20.0, 60.0, d)     # no noise near the exit portal
		var f := 1.0 - d / MAIN_RADIUS + n_island.get_noise_2d(fx, fz) * 0.18 * calm
		if f > 0.0:
			var top := 56 + int(round(8.0 * sqrt(minf(f, 1.0)))) + int(round(n_height.get_noise_2d(fx, fz) * 2.0 * calm))
			var depth := 4 + int(52.0 * pow(minf(f, 1.0), 0.8)) + int(absf(n_height.get_noise_2d(fx * 1.7, fz * 1.7)) * 6.0)
			best = Vector2i(top, maxi(1, top - depth))
	var ring := smoothstep(OUTER_START - 260.0, OUTER_START + 120.0, d) * (1.0 - smoothstep(OUTER_END - 300.0, OUTER_END + 200.0, d))
	if ring > 0.0:
		var n := n_island.get_noise_2d(fx * 0.55, fz * 0.55)
		var n2 := n_island.get_noise_2d(fx * 1.7 + 100.0, fz * 1.7 - 60.0) * 0.45
		var isl := clampf(n + n2, 0.0, 1.0) * ring * 1.5
		if isl > 0.0:
			var top2 := 50 + int(isl * 22.0) + int(round(n_height.get_noise_2d(fx, fz) * 2.0))
			var depth2 := 3 + int(isl * 30.0)
			if top2 > best.x:
				best = Vector2i(top2, maxi(1, top2 - depth2))
	return best


func generate(c: Chunk) -> void:
	var ox := c.cx * 16
	var oz := c.cz * 16
	var blocks := c.blocks
	var MY := min_y
	var end_stone: int = I.end_stone
	var rng := chunk_rng(c.cx, c.cz, 3)
	var biome := PackedInt32Array()
	biome.resize(256)
	for lz in 16:
		for lx in 16:
			var bi := (lz << 4) | lx
			var x := ox + lx
			var z := oz + lz
			var b := biome_at(x, z)
			biome[bi] = b
			c.biomes[bi] = b
			var col := _column(x, z)
			if col.x < 0:
				continue
			for y in range(col.y, col.x + 1):
				blocks[((y - MY) << 8) | bi] = end_stone
	_central_podium(c)
	_chorus(c, biome, rng)
	_spikes(c)
	if structures != null:
		structures.generate_into(c, self)
	_biome_colors(c)
	c.rebuild_derived()


## The exit portal at the origin (inactive until the dragon dies).
func _central_podium(c: Chunk) -> void:
	var ox := c.cx * 16
	var oz := c.cz * 16
	if ox > 4 or ox + 16 < -4 or oz > 4 or oz + 16 < -4:
		return
	var w := WorldGen.Writer.new(c)
	for e in podium_blocks(false):
		var p: Vector3i = e[0]
		w.put(p.x, p.y, p.z, int(e[1]), 0)


## Level of the exit portal blocks (one above the island surface at the origin).
const PODIUM_Y := 65


## The exit portal as [Vector3i, block value] pairs: a bedrock bowl (floor disc r < 2.5 one below
## the portal level, rim ring 2.5..3.5 at the portal level), the four-high bedrock pillar in the
## middle with a wall torch on each side, and the portal blocks (or air) inside the rim.
static func podium_blocks(active: bool) -> Array:
	var out := []
	var bedrock := BlockDB.id("bedrock")
	var portal := BlockDB.id("end_portal") if active else 0
	var y0 := PODIUM_Y
	for x in range(-4, 5):
		for z in range(-4, 5):
			var d := sqrt(float(x * x + z * z))
			if d >= 3.5:
				continue
			var inside := d < 2.5
			out.append([Vector3i(x, y0 - 1, z), bedrock if inside else BlockDB.id("end_stone")])
			if x == 0 and z == 0:
				continue
			out.append([Vector3i(x, y0, z), portal if inside else bedrock])
			for y in range(y0 + 1, y0 + 6):
				out.append([Vector3i(x, y, z), 0])
	for dy in 4:
		out.append([Vector3i(0, y0 + dy, 0), bedrock])
	# wall torches: meta 1 + facing (0 S, 1 W, 2 N, 3 E), pointing away from the pillar
	var torch := BlockDB.id("torch")
	out.append([Vector3i(0, y0 + 2, 1), Vox.make(torch, 1)])
	out.append([Vector3i(-1, y0 + 2, 0), Vox.make(torch, 2)])
	out.append([Vector3i(0, y0 + 2, -1), Vox.make(torch, 3)])
	out.append([Vector3i(1, y0 + 2, 0), Vox.make(torch, 4)])
	return out


func _chorus(c: Chunk, biome: PackedInt32Array, rng: RandomNumberGenerator) -> void:
	var w := WorldGen.Writer.new(c)
	var ox := c.cx * 16
	var oz := c.cz * 16
	for i in 26:
		var lx := rng.randi_range(0, 15)
		var lz := rng.randi_range(0, 15)
		var x := ox + lx
		var z := oz + lz
		var d := Vector2(float(x), float(z)).length()
		if d < MAIN_RADIUS + 20.0:
			continue
		if biome[(lz << 4) | lx] != BiomeDB.id("end_highlands"):
			continue
		var top := _island_top(x, z)
		if top < 0:
			continue
		if n_chorus.get_noise_2d(float(x) * 0.5, float(z) * 0.5) < 0.0:
			continue
		_chorus_tree(w, x, top + 1, z, rng)


func _chorus_tree(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var h := rng.randi_range(2, 5)
	var cx := x
	var cz := z
	for i in h:
		w.put(cx, y + i, cz, I.chorus_plant, 1)
		if rng.randf() < 0.55:
			cx += rng.randi_range(-1, 1)
			cz += rng.randi_range(-1, 1)
	w.put(cx, y + h, cz, I.chorus_flower, 1)
	if rng.randf() < 0.4:
		w.put(cx + 1, y + h - 1, cz, I.chorus_plant, 1)
		w.put(cx + 1, y + h, cz, I.chorus_flower, 1)


## The ten obsidian spikes around the exit portal: [x, z, radius, height, caged] per spike. Heights
## 76..103 and radii 2..5 are shuffled per seed; the two cages go on two of the shorter spikes.
static func spikes(world_seed: int) -> Array:
	var order: Array = range(10)
	var rng := RandomNumberGenerator.new()
	rng.seed = world_seed ^ 0x51CE
	for i in range(9, 0, -1):
		var j := rng.randi_range(0, i)
		var t = order[i]
		order[i] = order[j]
		order[j] = t
	var out := []
	for i in 10:
		var ang := TAU * float(i) / 10.0 + PI
		var k: int = order[i]
		out.append([int(floor(42.0 * cos(ang))), int(floor(42.0 * sin(ang))), 2 + k / 3, 76 + k * 2, k == 1 or k == 2])
	return out


## Top of a spike: where its crystal sits (on a bedrock block).
static func spike_crystal_pos(spike: Array) -> Vector3:
	return Vector3(float(spike[0]) + 0.5, float(spike[3]) + 1.0, float(spike[1]) + 0.5)


func _spikes(c: Chunk) -> void:
	var ox := c.cx * 16
	var oz := c.cz * 16
	var w := WorldGen.Writer.new(c)
	var bars := BlockDB.id("iron_bars")
	for sp in spikes(seed_value):
		var sx: int = sp[0]
		var sz: int = sp[1]
		var r: int = sp[2]
		var h: int = sp[3]
		if sx + r + 2 < ox or sx - r - 2 >= ox + 16 or sz + r + 2 < oz or sz - r - 2 >= oz + 16:
			continue
		var r2 := r * r + 1
		var ground := maxi(1, _column(sx, sz).y)
		for dx in range(-r, r + 1):
			for dz in range(-r, r + 1):
				if dx * dx + dz * dz > r2:
					continue
				for y in range(ground, h + 1):
					w.put(sx + dx, y, sz + dz, I.obsidian, 0)
		w.put(sx, h, sz, I.bedrock, 0)
		if bool(sp[4]):
			for dx in range(-2, 3):
				for dz in range(-2, 3):
					for dy in range(1, 4):
						var edge := absi(dx) == 2 or absi(dz) == 2
						if edge or dy == 3:
							w.put(sx + dx, h + dy, sz + dz, bars, 0)


func _biome_colors(c: Chunk) -> void:
	var grass := BiomeDB.grass
	var foliage := BiomeDB.foliage
	var water := BiomeDB.water
	for i in 256:
		var b := c.biomes[i]
		c.colors[i * 3] = grass[b]
		c.colors[i * 3 + 1] = foliage[b]
		c.colors[i * 3 + 2] = water[b]


## Where an End gateway sends the player. Gateways on the main island lead outwards along their
## own bearing to the first outer island (the highest column there); the return gateways built
## on the outer islands lead back to just inside the main island's rim. The y is a hint only -
## the session drops the player onto the real surface once the chunks have streamed in.
static func gateway_destination(world: World, from: Vector3i) -> Vector3:
	var gen := world.gen as EndGen
	var dir := Vector2(float(from.x), float(from.z))
	if dir.length() < 1.0:
		dir = Vector2(1, 0)
	var dist0 := dir.length()
	dir = dir.normalized()
	if dist0 > 500.0:
		var bx := int(round(dir.x * 90.0))
		var bz := int(round(dir.y * 90.0))
		var by := 70
		if gen != null:
			by = maxi(60, gen._island_top(bx, bz) + 2)
		return Vector3(float(bx) + 0.5, float(by), float(bz) + 0.5)
	var best := Vector3i(int(round(dir.x * (OUTER_START + 200.0))), 70, int(round(dir.y * (OUTER_START + 200.0))))
	if gen != null:
		var found := false
		var r := OUTER_START - 100.0
		while r < OUTER_END and not found:
			var cx := int(round(dir.x * r))
			var cz := int(round(dir.y * r))
			if gen._island_top(cx, cz) > 0:
				# climb to the highest column in the neighbourhood
				var top := -1
				for dx in range(-8, 9, 4):
					for dz in range(-8, 9, 4):
						var t := gen._island_top(cx + dx, cz + dz)
						if t > top:
							top = t
							best = Vector3i(cx + dx, t + 1, cz + dz)
				found = true
			r += 8.0
	return Vector3(float(best.x) + 0.5, float(best.y), float(best.z) + 0.5)
