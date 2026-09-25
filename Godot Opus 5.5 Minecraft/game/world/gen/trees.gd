class_name Trees
extends RefCounted
## Tree and large-plant features. All placement goes through a clipped WorldGen.Writer so a feature
## rooted in a neighbouring chunk contributes exactly the blocks that fall inside the target chunk.

static var B := {}
static var inited := false


static func init() -> void:
	if inited:
		return
	for w in ["oak", "spruce", "birch", "jungle", "acacia", "dark_oak", "mangrove", "cherry", "pale_oak"]:
		B[w + "_log_y"] = BlockDB.id(w + "_log")
		B[w + "_log_x"] = Vox.make(BlockDB.id(w + "_log"), 1)
		B[w + "_log_z"] = Vox.make(BlockDB.id(w + "_log"), 2)
		B[w + "_leaves"] = BlockDB.id(w + "_leaves")
	for n in ["vine", "cocoa", "brown_mushroom_block", "red_mushroom_block", "mushroom_stem", "mangrove_roots",
			"muddy_mangrove_roots", "mud", "moss_block", "rooted_dirt", "azalea_leaves", "flowering_azalea_leaves", "cactus",
			"bamboo", "packed_ice", "snow_block", "ice", "blue_ice", "pale_hanging_moss", "creaking_heart", "pale_moss_carpet",
			"dirt", "podzol", "bee_nest", "crimson_stem", "warped_stem", "nether_wart_block", "warped_wart_block", "shroomlight",
			"weeping_vines", "twisting_vines", "hanging_roots", "cactus_flower", "coarse_dirt"]:
		B[n] = BlockDB.id(n)
	inited = true


static func place(w: WorldGen.Writer, kind: String, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	match kind:
		"oak": oak(w, x, y, z, rng, "oak", rng.randi_range(4, 6))
		"birch": oak(w, x, y, z, rng, "birch", rng.randi_range(5, 7))
		"birch_bee":
			oak(w, x, y, z, rng, "birch", rng.randi_range(5, 7))
			w.put(x + 1, y + 3, z, B["bee_nest"], 1)
		"tall_birch": oak(w, x, y, z, rng, "birch", rng.randi_range(8, 11))
		"swamp_oak": swamp_oak(w, x, y, z, rng)
		"fancy_oak": fancy_oak(w, x, y, z, rng)
		"spruce": spruce(w, x, y, z, rng)
		"pine": pine(w, x, y, z, rng)
		"mega_spruce": mega_spruce(w, x, y, z, rng)
		"jungle": oak(w, x, y, z, rng, "jungle", rng.randi_range(5, 8), true)
		"mega_jungle": mega_jungle(w, x, y, z, rng)
		"jungle_bush": bush(w, x, y, z, rng, "jungle", "oak")
		"acacia": acacia(w, x, y, z, rng)
		"dark_oak": dark_oak(w, x, y, z, rng, "dark_oak")
		"pale_oak": dark_oak(w, x, y, z, rng, "pale_oak")
		"pale_oak_heart": dark_oak(w, x, y, z, rng, "pale_oak", true)
		"mangrove": mangrove(w, x, y, z, rng)
		"cherry": cherry(w, x, y, z, rng)
		"azalea": azalea(w, x, y, z, rng)
		"huge_red_mushroom": huge_mushroom(w, x, y, z, rng, true)
		"huge_brown_mushroom": huge_mushroom(w, x, y, z, rng, false)
		"cactus": cactus(w, x, y, z, rng)
		"bamboo": bamboo(w, x, y, z, rng)
		"ice_spike": ice_spike(w, x, y, z, rng)
		"iceberg": iceberg(w, x, y, z, rng)
		"crimson_fungus": huge_fungus(w, x, y, z, rng, true)
		"warped_fungus": huge_fungus(w, x, y, z, rng, false)


static func _leaf_blob(w: WorldGen.Writer, cx: int, cy: int, cz: int, r: int, leaf: int, rng: RandomNumberGenerator, corner_cut := true) -> void:
	for dy in range(-r, r + 1):
		for dz in range(-r, r + 1):
			for dx in range(-r, r + 1):
				var d := dx * dx + dy * dy + dz * dz
				if d > r * r + 1:
					continue
				if corner_cut and absi(dx) == r and absi(dz) == r and rng.randf() < 0.6:
					continue
				w.put(cx + dx, cy + dy, cz + dz, leaf, 1)


static func oak(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator, wood: String, h: int, vines := false) -> void:
	var log: int = B[wood + "_log_y"]
	var leaf: int = B[wood + "_leaves"]
	# canopy: two wide layers + two narrow layers (Minecraft blob tree shape)
	for ly in range(h - 3, h + 1):
		var rr := 2 if ly < h - 1 else 1
		for dz in range(-rr, rr + 1):
			for dx in range(-rr, rr + 1):
				if absi(dx) == rr and absi(dz) == rr:
					if ly == h or rng.randf() < 0.5:
						continue
				w.put(x + dx, y + ly, z + dz, leaf, 1)
	for i in h:
		w.put(x, y + i, z, log, 2)
	w.put(x, y - 1, z, B["dirt"], 0)
	if vines:
		for i in range(1, h):
			for d in 4:
				if rng.randf() < 0.3:
					var hv: Vector3i = Vox.H_FACING_VEC[d]
					w.put(x + hv.x, y + i, z + hv.z, Vox.make(B["vine"], 1 << ((d + 2) & 3)), 1)


static func swamp_oak(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var h := rng.randi_range(5, 7)
	var leaf: int = B["oak_leaves"]
	for ly in range(h - 3, h + 1):
		var rr := 3 if ly < h - 1 else 2
		for dz in range(-rr, rr + 1):
			for dx in range(-rr, rr + 1):
				if absi(dx) == rr and absi(dz) == rr:
					continue
				w.put(x + dx, y + ly, z + dz, leaf, 1)
				if ly == h - 3 and (absi(dx) == rr or absi(dz) == rr) and rng.randf() < 0.35:
					for k in rng.randi_range(1, 4):
						var face := 0
						if dx == rr: face = 8
						elif dx == -rr: face = 2
						elif dz == rr: face = 1
						else: face = 4
						w.put(x + dx + (1 if dx == rr else (-1 if dx == -rr else 0)), y + ly - k, z + dz + (1 if dz == rr else (-1 if dz == -rr else 0)), Vox.make(B["vine"], face), 1)
	for i in h:
		w.put(x, y + i, z, B["oak_log_y"], 2)


static func fancy_oak(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var h := rng.randi_range(9, 13)
	var log: int = B["oak_log_y"]
	var leaf: int = B["oak_leaves"]
	for i in h:
		w.put(x, y + i, z, log, 2)
	_leaf_blob(w, x, y + h, z, 2, leaf, rng)
	var branches := rng.randi_range(3, 5)
	for b in branches:
		var by := y + rng.randi_range(h / 2, h - 2)
		var ang := rng.randf() * TAU
		var ln := rng.randi_range(3, 5)
		var ex := x + int(round(cos(ang) * ln))
		var ez := z + int(round(sin(ang) * ln))
		var ey := by + rng.randi_range(1, 3)
		for s in ln + 1:
			var t := float(s) / ln
			var bx := int(round(lerpf(x, ex, t)))
			var bz := int(round(lerpf(z, ez, t)))
			var byy := int(round(lerpf(by, ey, t)))
			var axis_v: int = B["oak_log_x"] if absi(ex - x) >= absi(ez - z) else B["oak_log_z"]
			w.put(bx, byy, bz, axis_v, 2)
		_leaf_blob(w, ex, ey, ez, 2, leaf, rng)


static func spruce(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var h := rng.randi_range(6, 9)
	var log: int = B["spruce_log_y"]
	var leaf: int = B["spruce_leaves"]
	var r := 0
	var maxr := rng.randi_range(2, 3)
	var ly := h
	w.put(x, y + h + 1, z, leaf, 1)
	while ly >= 2:
		for dz in range(-r, r + 1):
			for dx in range(-r, r + 1):
				if r > 0 and absi(dx) == r and absi(dz) == r:
					continue
				w.put(x + dx, y + ly, z + dz, leaf, 1)
		r += 1
		if r > maxr:
			r = 1
		ly -= 1
	for i in h:
		w.put(x, y + i, z, log, 2)


static func pine(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var h := rng.randi_range(8, 12)
	var log: int = B["spruce_log_y"]
	var leaf: int = B["spruce_leaves"]
	for ly in range(h - 3, h + 2):
		var r := 1 if ly >= h else 2
		if ly == h + 1:
			r = 0
		for dz in range(-r, r + 1):
			for dx in range(-r, r + 1):
				if r > 0 and absi(dx) == r and absi(dz) == r:
					continue
				w.put(x + dx, y + ly, z + dz, leaf, 1)
	for i in h:
		w.put(x, y + i, z, log, 2)


static func mega_spruce(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var h := rng.randi_range(15, 24)
	var log: int = B["spruce_log_y"]
	var leaf: int = B["spruce_leaves"]
	var top := h - rng.randi_range(8, 12)
	for ly in range(top, h + 2):
		var t := float(ly - top) / float(h + 2 - top)
		var r := int(round(lerpf(4.5, 0.5, t)))
		if (ly - top) % 3 == 1:
			r = maxi(0, r - 1)
		for dz in range(-r, r + 2):
			for dx in range(-r, r + 2):
				var ddx := dx - 0.5
				var ddz := dz - 0.5
				if ddx * ddx + ddz * ddz > (r + 0.6) * (r + 0.6):
					continue
				w.put(x + dx, y + ly, z + dz, leaf, 1)
	for i in h:
		for o in [Vector2i(0, 0), Vector2i(1, 0), Vector2i(0, 1), Vector2i(1, 1)]:
			w.put(x + o.x, y + i, z + o.y, log, 2)
	for dz in range(-2, 4):
		for dx in range(-2, 4):
			if rng.randf() < 0.6:
				w.put_if(x + dx, y - 1, z + dz, B["podzol"], BlockDB.GRASS)


static func mega_jungle(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var h := rng.randi_range(18, 28)
	var log: int = B["jungle_log_y"]
	var leaf: int = B["jungle_leaves"]
	for i in h:
		for o in [Vector2i(0, 0), Vector2i(1, 0), Vector2i(0, 1), Vector2i(1, 1)]:
			w.put(x + o.x, y + i, z + o.y, log, 2)
			if rng.randf() < 0.25:
				pass
	for i in range(2, h - 2):
		for d in 4:
			if rng.randf() < 0.18:
				var hv: Vector3i = Vox.H_FACING_VEC[d]
				var vx := x + (2 if hv.x > 0 else (-1 if hv.x < 0 else rng.randi_range(0, 1)))
				var vz := z + (2 if hv.z > 0 else (-1 if hv.z < 0 else rng.randi_range(0, 1)))
				w.put(vx, y + i, vz, Vox.make(B["vine"], 1 << ((d + 2) & 3)), 1)
	for ly in range(h - 3, h + 2):
		var r := 4 if ly < h else (3 if ly == h else 2)
		for dz in range(-r, r + 2):
			for dx in range(-r, r + 2):
				var ddx := dx - 0.5
				var ddz := dz - 0.5
				if ddx * ddx + ddz * ddz > (r + 0.5) * (r + 0.5):
					continue
				w.put(x + dx, y + ly, z + dz, leaf, 1)
	for b in 3:
		var ang := rng.randf() * TAU
		var by := y + rng.randi_range(h / 2, h - 5)
		var ex := x + int(round(cos(ang) * 4))
		var ez := z + int(round(sin(ang) * 4))
		for s in 5:
			var t := s / 4.0
			w.put(int(round(lerpf(x, ex, t))), by + s / 2, int(round(lerpf(z, ez, t))), log, 2)
		_leaf_blob(w, ex, by + 3, ez, 2, leaf, rng)


static func bush(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator, wood: String, leaves_of: String) -> void:
	var log: int = B[wood + "_log_y"]
	var leaf: int = B[leaves_of + "_leaves"]
	w.put(x, y, z, log, 2)
	for dy in range(0, 3):
		var r := 2 - dy
		for dz in range(-r, r + 1):
			for dx in range(-r, r + 1):
				if absi(dx) == r and absi(dz) == r and r > 0 and rng.randf() < 0.5:
					continue
				w.put(x + dx, y + dy, z + dz, leaf, 1)


static func acacia(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var log: int = B["acacia_log_y"]
	var leaf: int = B["acacia_leaves"]
	var h := rng.randi_range(5, 7)
	var d: Vector3i = Vox.H_FACING_VEC[rng.randi_range(0, 3)]
	var bend := rng.randi_range(2, 3)
	var cx := x
	var cz := z
	for i in h:
		if i >= bend and i < bend + 3:
			cx += d.x
			cz += d.z
		w.put(cx, y + i, cz, log, 2)
	var top := y + h
	for dz in range(-3, 4):
		for dx in range(-3, 4):
			if absi(dx) + absi(dz) > 4:
				continue
			w.put(cx + dx, top - 1, cz + dz, leaf, 1)
	for dz in range(-2, 3):
		for dx in range(-2, 3):
			if absi(dx) + absi(dz) > 2:
				continue
			w.put(cx + dx, top, cz + dz, leaf, 1)
	# second, smaller branch
	if rng.randf() < 0.6:
		var d2: Vector3i = Vox.H_FACING_VEC[rng.randi_range(0, 3)]
		if d2 != d:
			var bx := x
			var bz := z
			var by := y + bend
			for s in 3:
				bx += d2.x
				bz += d2.z
				w.put(bx, by + s, bz, log, 2)
			for dz in range(-2, 3):
				for dx in range(-2, 3):
					if absi(dx) + absi(dz) <= 3:
						w.put(bx + dx, by + 3, bz + dz, leaf, 1)


static func dark_oak(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator, wood: String, heart := false) -> void:
	var log: int = B[wood + "_log_y"]
	var leaf: int = B[wood + "_leaves"]
	var h := rng.randi_range(6, 9)
	for i in h:
		for o in [Vector2i(0, 0), Vector2i(1, 0), Vector2i(0, 1), Vector2i(1, 1)]:
			w.put(x + o.x, y + i, z + o.y, log, 2)
	if heart:
		w.put(x, y + 2 + rng.randi_range(0, 2), z, Vox.make(B["creaking_heart"], 0), 0)
	for ly in range(h - 2, h + 2):
		var r := 3 if ly < h + 1 else 2
		for dz in range(-r, r + 2):
			for dx in range(-r, r + 2):
				var ddx := dx - 0.5
				var ddz := dz - 0.5
				if ddx * ddx + ddz * ddz > (r + 0.7) * (r + 0.7):
					continue
				if ly == h + 1 and rng.randf() < 0.3:
					continue
				w.put(x + dx, y + ly, z + dz, leaf, 1)
	for b in rng.randi_range(1, 3):
		var d: Vector3i = Vox.H_FACING_VEC[rng.randi_range(0, 3)]
		var bx := x + (2 if d.x > 0 else (-1 if d.x < 0 else 0))
		var bz := z + (2 if d.z > 0 else (-1 if d.z < 0 else 0))
		var by := y + h - 3
		w.put(bx, by, bz, log, 2)
		w.put(bx, by + 1, bz, log, 2)
		_leaf_blob(w, bx, by + 2, bz, 2, leaf, rng)
	if wood == "pale_oak":
		for k in 6:
			var hx := x + rng.randi_range(-3, 4)
			var hz := z + rng.randi_range(-3, 4)
			for s in rng.randi_range(1, 3):
				w.put(hx, y + h - 3 - s, hz, B["pale_hanging_moss"], 1)


static func mangrove(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var log: int = B["mangrove_log_y"]
	var leaf: int = B["mangrove_leaves"]
	var roots: int = B["mangrove_roots"]
	var base := y + rng.randi_range(2, 3)
	var h := rng.randi_range(6, 9)
	for d in [Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1), Vector2i(1, 1), Vector2i(-1, -1)]:
		var rx: int = x + d.x
		var rz: int = z + d.y
		for yy in range(base, y - 3, -1):
			if yy < base - 1:
				rx += d.x if rng.randf() < 0.4 else 0
				rz += d.y if rng.randf() < 0.4 else 0
			w.put(rx, yy, rz, roots, 1)
	for i in range(base - 1, base + h):
		w.put(x, i, z, log, 2)
	for ly in range(base + h - 3, base + h + 2):
		var r := 3 if ly < base + h else 2
		for dz in range(-r, r + 1):
			for dx in range(-r, r + 1):
				if dx * dx + dz * dz > r * r + 1:
					continue
				w.put(x + dx, ly, z + dz, leaf, 1)


static func cherry(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var log: int = B["cherry_log_y"]
	var leaf: int = B["cherry_leaves"]
	var h := rng.randi_range(4, 6)
	for i in h:
		w.put(x, y + i, z, log, 2)
	var tops := []
	var nb := rng.randi_range(2, 3)
	for b in nb:
		var ang := b * TAU / nb + rng.randf_range(-0.4, 0.4)
		var ln := rng.randi_range(2, 4)
		var bx := x
		var bz := z
		var by := y + h - 1
		for s in ln:
			bx = x + int(round(cos(ang) * (s + 1)))
			bz = z + int(round(sin(ang) * (s + 1)))
			by += 1 if s > 0 else 0
			w.put(bx, by, bz, B["cherry_log_x"] if absf(cos(ang)) > 0.7 else B["cherry_log_z"], 2)
		tops.append(Vector3i(bx, by + 1, bz))
	for t in tops:
		for dy in range(-1, 3):
			var r := 3 if dy <= 0 else 2
			if dy == 2:
				r = 1
			for dz in range(-r, r + 1):
				for dx in range(-r, r + 1):
					if dx * dx + dz * dz > r * r + 1:
						continue
					w.put(t.x + dx, t.y + dy, t.z + dz, leaf, 1)
			if dy == -1:
				for dz in range(-r, r + 1):
					for dx in range(-r, r + 1):
						if rng.randf() < 0.15:
							w.put(t.x + dx, t.y - 2, t.z + dz, leaf, 1)


static func azalea(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var log: int = B["oak_log_y"]
	var h := rng.randi_range(3, 4)
	for i in h:
		w.put(x, y + i, z, log, 2)
	for dy in range(h - 1, h + 2):
		var r := 2 if dy < h + 1 else 1
		for dz in range(-r, r + 1):
			for dx in range(-r, r + 1):
				if absi(dx) == r and absi(dz) == r:
					continue
				w.put(x + dx, y + dy, z + dz, B["flowering_azalea_leaves"] if rng.randf() < 0.3 else B["azalea_leaves"], 1)
	w.put(x, y - 1, z, B["rooted_dirt"], 0)


static func huge_mushroom(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator, red: bool) -> void:
	var stem: int = B["mushroom_stem"]
	var cap: int = B["red_mushroom_block"] if red else B["brown_mushroom_block"]
	var h := rng.randi_range(5, 7)
	for i in h:
		w.put(x, y + i, z, stem, 2)
	if red:
		for ly in range(h - 3, h + 1):
			var r := 2 if ly < h else 1
			for dz in range(-r, r + 1):
				for dx in range(-r, r + 1):
					if ly < h and absi(dx) < r and absi(dz) < r:
						continue
					if absi(dx) == r and absi(dz) == r and ly < h:
						continue
					w.put(x + dx, y + ly, z + dz, cap, 1)
	else:
		for dz in range(-3, 4):
			for dx in range(-3, 4):
				if absi(dx) == 3 and absi(dz) == 3:
					continue
				w.put(x + dx, y + h, z + dz, cap, 1)


static func cactus(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var h := rng.randi_range(1, 3)
	for i in h:
		w.put(x, y + i, z, B["cactus"], 1)
	if rng.randf() < 0.1:
		w.put(x, y + h, z, B["cactus_flower"], 1)


static func bamboo(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var h := rng.randi_range(6, 14)
	for i in h:
		var leaves := 0
		if i >= h - 3:
			leaves = 2 if i >= h - 2 else 1
		w.put(x, y + i, z, Vox.make(B["bamboo"], leaves), 1)
	for dz in range(-1, 2):
		for dx in range(-1, 2):
			if rng.randf() < 0.5:
				w.put_if(x + dx, y - 1, z + dz, B["podzol"], BlockDB.GRASS)


static func ice_spike(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var h := rng.randi_range(6, 12) if rng.randf() < 0.9 else rng.randi_range(20, 40)
	var r0 := 2.5 if h < 15 else 3.5
	for i in h:
		var r := r0 * (1.0 - float(i) / h) + 0.3
		var ri := int(ceil(r))
		for dz in range(-ri, ri + 1):
			for dx in range(-ri, ri + 1):
				if dx * dx + dz * dz <= r * r:
					w.put(x + dx, y + i, z + dz, B["packed_ice"], 0)


static func iceberg(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator) -> void:
	var h := rng.randi_range(4, 12)
	var r0 := rng.randf_range(4.0, 8.0)
	for i in range(-h, h):
		var t := absf(float(i)) / h
		var r := r0 * (1.0 - t * t) + 0.5
		var ri := int(ceil(r))
		for dz in range(-ri, ri + 1):
			for dx in range(-ri, ri + 1):
				if dx * dx + dz * dz <= r * r:
					var v: int = B["packed_ice"] if rng.randf() < 0.85 else B["blue_ice"]
					if i >= h - 2:
						v = B["snow_block"]
					w.put(x + dx, y + i, z + dz, v, 0)


static func huge_fungus(w: WorldGen.Writer, x: int, y: int, z: int, rng: RandomNumberGenerator, crimson: bool) -> void:
	var stem: int = B["crimson_stem"] if crimson else B["warped_stem"]
	var wart: int = B["nether_wart_block"] if crimson else B["warped_wart_block"]
	var h := rng.randi_range(5, 13)
	for i in h:
		w.put(x, y + i, z, stem, 2)
	var r := 3 if h > 8 else 2
	for ly in range(h - 3, h + 1):
		var rr := r if ly < h else r - 1
		for dz in range(-rr, rr + 1):
			for dx in range(-rr, rr + 1):
				if absi(dx) == rr and absi(dz) == rr and rng.randf() < 0.7:
					continue
				var inner := absi(dx) < rr and absi(dz) < rr and ly < h
				if inner and rng.randf() < 0.7:
					continue
				var v := wart
				if rng.randf() < 0.08:
					v = B["shroomlight"]
				w.put(x + dx, y + ly, z + dz, v, 1)
				if crimson and ly == h - 3 and rng.randf() < 0.2:
					for s in rng.randi_range(1, 4):
						w.put(x + dx, y + ly - s, z + dz, B["weeping_vines"], 1)
