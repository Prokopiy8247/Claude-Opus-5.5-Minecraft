class_name WorldGen
extends RefCounted
## Base class for deterministic, thread-safe chunk generators. A chunk is generated from
## (seed, cx, cz) only, so features that cross chunk borders (trees, ores, structures) are
## re-evaluated from neighbouring source chunks and clipped to the target chunk.

var seed_value := 0
var min_y := 0
var height := 256
var sea_level := 63
var has_sky := true
var has_ceiling := false
var dim_id := 0
var structures: StructureManager = null


func setup(world_seed: int) -> void:
	seed_value = world_seed


func generate(_c: Chunk) -> void:
	pass


func surface_height(_x: int, _z: int) -> int:
	return sea_level


func biome_at(_x: int, _z: int) -> int:
	return 0


func biome_at_3d(x: int, _y: int, z: int) -> int:
	return biome_at(x, z)


func find_spawn() -> Vector3:
	return Vector3(0.5, surface_height(0, 0) + 1, 0.5)


static func mk_noise(seed_v: int, freq: float, octaves: int = 1, type := FastNoiseLite.TYPE_SIMPLEX_SMOOTH) -> FastNoiseLite:
	var n := FastNoiseLite.new()
	n.seed = seed_v
	n.noise_type = type
	n.frequency = freq
	if octaves > 1:
		n.fractal_type = FastNoiseLite.FRACTAL_FBM
		n.fractal_octaves = octaves
		n.fractal_lacunarity = 2.0
		n.fractal_gain = 0.5
	else:
		n.fractal_type = FastNoiseLite.FRACTAL_NONE
	return n


## Deterministic RNG for a source chunk + salt.
func chunk_rng(cx: int, cz: int, salt: int) -> RandomNumberGenerator:
	var r := RandomNumberGenerator.new()
	var h := seed_value ^ (cx * 341873128712) ^ (cz * 132897987541) ^ (salt * 42317861)
	r.seed = h & 0x7FFFFFFFFFFFFFFF
	return r


static func smoothstep(a: float, b: float, x: float) -> float:
	var t := clampf((x - a) / (b - a), 0.0, 1.0)
	return t * t * (3.0 - 2.0 * t)


static func spline(x: float, pts: PackedFloat32Array) -> float:
	# pts = x0, y0, x1, y1, ... sorted by x
	var n := pts.size() >> 1
	if x <= pts[0]:
		return pts[1]
	for i in range(1, n):
		var x1 := pts[i * 2]
		if x <= x1:
			var x0 := pts[i * 2 - 2]
			var t := (x - x0) / (x1 - x0)
			t = t * t * (3.0 - 2.0 * t)
			return lerpf(pts[i * 2 - 1], pts[i * 2 + 1], t)
	return pts[n * 2 - 1]


## Clipped block writer targeting one chunk; features call set() with world coordinates.
class Writer:
	var c: Chunk
	var ox := 0
	var oz := 0
	var min_y := 0
	var max_y := 0
	var replaceable: PackedByteArray
	var solid: PackedByteArray

	func _init(chunk: Chunk) -> void:
		c = chunk
		ox = chunk.cx * 16
		oz = chunk.cz * 16
		min_y = chunk.min_y
		max_y = chunk.min_y + chunk.height - 1
		replaceable = BlockDB.replaceable
		solid = BlockDB.solid

	func inside(x: int, y: int, z: int) -> bool:
		return x >= ox and x < ox + 16 and z >= oz and z < oz + 16 and y >= min_y and y <= max_y

	func get_b(x: int, y: int, z: int) -> int:
		if not inside(x, y, z):
			return -1
		return c.blocks[((y - min_y) << 8) | ((z - oz) << 4) | (x - ox)]

	## mode 0 = always, 1 = only into air/replaceable, 2 = only into air/replaceable/leaves (trunks)
	func put(x: int, y: int, z: int, v: int, mode: int = 0) -> void:
		if x < ox or x >= ox + 16 or z < oz or z >= oz + 16 or y < min_y or y > max_y:
			return
		var i := ((y - min_y) << 8) | ((z - oz) << 4) | (x - ox)
		if mode != 0:
			var cur := c.blocks[i]
			var cid := cur & 0xFFF
			if cur != 0 and replaceable[cid] == 0:
				if mode == 1:
					return
				if mode == 2 and BlockDB.model[cid] != BlockDB.M_LEAVES:
					return
		c.blocks[i] = v

	func put_if(x: int, y: int, z: int, v: int, only: int) -> void:
		if not inside(x, y, z):
			return
		var i := ((y - min_y) << 8) | ((z - oz) << 4) | (x - ox)
		if (c.blocks[i] & 0xFFF) == only:
			c.blocks[i] = v

	func fill(x0: int, y0: int, z0: int, x1: int, y1: int, z1: int, v: int, mode: int = 0) -> void:
		var ax := maxi(mini(x0, x1), ox)
		var bx := mini(maxi(x0, x1), ox + 15)
		var az := maxi(mini(z0, z1), oz)
		var bz := mini(maxi(z0, z1), oz + 15)
		var ay := maxi(mini(y0, y1), min_y)
		var by := mini(maxi(y0, y1), max_y)
		for y in range(ay, by + 1):
			for z in range(az, bz + 1):
				for x in range(ax, bx + 1):
					put(x, y, z, v, mode)

	func overlaps(x0: int, z0: int, x1: int, z1: int) -> bool:
		return x1 >= ox and x0 < ox + 16 and z1 >= oz and z0 < oz + 16

	func add_block_entity(x: int, y: int, z: int, data: Dictionary) -> void:
		if not inside(x, y, z):
			return
		c.block_entities[((y - min_y) << 8) | ((z - oz) << 4) | (x - ox)] = data

	## Queues an entity spawned when the chunk becomes active (EntityManager.spawn_saved format).
	func add_entity(type: String, pos: Vector3, data: Dictionary = {}) -> void:
		if pos.x >= ox and pos.x < ox + 16 and pos.z >= oz and pos.z < oz + 16:
			var e := {"t": type, "p": [pos.x, pos.y, pos.z], "data": data.duplicate(true), "gen": true}
			if data.has("mob"):
				e["mob"] = data["mob"]
			c.pending_entities.append(e)


## Ore vein: a sequence of small spheres along a random segment (Minecraft ore feature style).
## target: PackedByteArray by block id -> 1 = replace with ore, 2 = replace with deep ore.
static func ore_vein(w: Writer, rng: RandomNumberGenerator, x: float, y: float, z: float, size: int, ore: int, deep_ore: int,
		target: PackedByteArray) -> void:
	var ang := rng.randf() * PI
	var len := size / 8.0
	var x0 := x + sin(ang) * len
	var x1 := x - sin(ang) * len
	var z0 := z + cos(ang) * len
	var z1 := z - cos(ang) * len
	var y0 := y + rng.randi_range(-2, 2)
	var y1 := y + rng.randi_range(-2, 2)
	var blocks := w.c.blocks
	var ox := w.ox
	var oz := w.oz
	var my := w.min_y
	var maxy := w.max_y
	var step := 1 if size < 24 else 2
	var i := 0
	var rmax := size / 16.0 + 1.0
	if minf(x0, x1) - rmax > ox + 16 or maxf(x0, x1) + rmax < ox or minf(z0, z1) - rmax > oz + 16 or maxf(z0, z1) + rmax < oz:
		return
	while i < size:
		var t := float(i) / float(size)
		var cx := lerpf(x0, x1, t)
		var cy := lerpf(y0, y1, t)
		var cz := lerpf(z0, z1, t)
		var r := (sin(t * PI) + 1.0) * (rng.randf() * size / 16.0 + 1.0) * 0.5
		i += step
		var r2 := r * r
		var ax := maxi(floori(cx - r), ox)
		var bx := mini(floori(cx + r), ox + 15)
		var az := maxi(floori(cz - r), oz)
		var bz := mini(floori(cz + r), oz + 15)
		if ax > bx or az > bz:
			continue
		var ay := maxi(floori(cy - r), my)
		var by := mini(floori(cy + r), maxy)
		for yy in range(ay, by + 1):
			var dy := yy + 0.5 - cy
			var yb := (yy - my) << 8
			for zz in range(az, bz + 1):
				var dz := zz + 0.5 - cz
				var dyz := dy * dy + dz * dz
				if dyz > r2:
					continue
				var zb := yb | ((zz - oz) << 4)
				for xx in range(ax, bx + 1):
					var dx := xx + 0.5 - cx
					if dx * dx + dyz > r2:
						continue
					var bi := zb | (xx - ox)
					var tg := target[blocks[bi] & 0xFFF]
					if tg == 1:
						blocks[bi] = ore
					elif tg == 2:
						blocks[bi] = deep_ore
