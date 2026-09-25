class_name BTexWoodPlants
extends RefCounted
## Wood families (logs, planks, leaves, doors, trapdoors, saplings, shelves) and plant sprites.

const WOOD_NAMES := ["oak", "spruce", "birch", "jungle", "acacia", "dark_oak", "mangrove", "cherry", "pale_oak",
	"crimson", "warped", "bamboo"]
const DOOR_STYLE := {"oak": 0, "spruce": 2, "birch": 1, "jungle": 3, "acacia": 1, "dark_oak": 2, "mangrove": 3,
	"cherry": 0, "pale_oak": 1, "crimson": 2, "warped": 3, "bamboo": 3}


static func C(s: String) -> Color:
	return Color.html(s)


static func cv_for(n: String) -> PixelCanvas:
	return PixelCanvas.new(16, 16, n)


static func _wood_of(n: String) -> String:
	var best := ""
	for w in WOOD_NAMES:
		if n.begins_with(w + "_") or n.begins_with("stripped_" + w + "_"):
			if w.length() > best.length():
				best = w
	return best


static func gen(n: String) -> Array:
	var w := _wood_of(n)
	if w != "":
		var r := _wood(n, w)
		if r != null:
			return [r.img]
	var cv := _plant(n)
	if cv != null:
		return [cv.img]
	return []


static func _wood(n: String, w: String) -> PixelCanvas:
	var pal: Dictionary = TexPalettes.WOOD[w]
	var cv := cv_for(n)
	var rest := n.replace("stripped_", "").substr(w.length() + 1)
	var stripped := n.begins_with("stripped_")
	match rest:
		"log", "stem":
			if stripped:
				BTCommon.stripped_side(cv, pal.ring)
			else:
				BTCommon.log_side(cv, pal.bark)
				if w == "birch":
					cv.palette_noise(TexPalettes.cols(["#dcdbd4", "#cfcec6", "#e8e7e0"]), [4, 3, 2], 0)
					for k in 7:
						var x := cv.rng.randi_range(0, 13)
						var y := cv.rng.randi_range(0, 15)
						cv.hline(x, x + cv.rng.randi_range(1, 3), y, C("#3a342f"))
				elif w == "crimson" or w == "warped":
					var glow := C(pal.bark[3])
					for k in 5:
						var x2 := cv.rng.randi_range(0, 15)
						var y2 := cv.rng.randi_range(0, 11)
						cv.vline(x2, y2, y2 + cv.rng.randi_range(2, 5), glow)
		"log_top", "stem_top":
			if stripped:
				BTCommon.stripped_top(cv, pal.ring)
			else:
				BTCommon.log_top(cv, pal.ring, pal.bark)
		"planks":
			BTCommon.planks(cv, pal.plank)
			if w == "bamboo":
				cv.palette_noise(TexPalettes.cols(pal.plank), [4, 2, 2, 1], 0)
				for x in [3, 7, 11, 15]:
					cv.vline(x, 0, 15, C(pal.plank[3]))
				for y in [7, 15]:
					cv.hline(0, 15, y, C(pal.plank[3]))
		"mosaic":
			pass
		"block":
			var gc := TexPalettes.cols(["#c9c463", "#b8b353", "#d8d372"]) if stripped else TexPalettes.cols(["#7f962e", "#6b8126", "#91a838"])
			for x in 16:
				var col := x % 4
				for y in 16:
					var c: Color = gc[0] if col == 1 or col == 2 else gc[1]
					if col == 3:
						c = PixelCanvas.shade(gc[1], -0.2)
					if (y + (x >> 2) * 5) % 8 == 0:
						c = PixelCanvas.shade(gc[1], -0.12)
					cv.px(x, y, c)
		"block_top":
			var gt := TexPalettes.cols(["#c9c463", "#d8d372", "#a39e3f"]) if stripped else TexPalettes.cols(["#8fa338", "#a3b84a", "#5f7020"])
			cv.fill(gt[2])
			for cc in [[4, 4], [11, 4], [4, 11], [11, 11]]:
				cv.disc(cc[0], cc[1], 3.2, gt[0])
				cv.disc(cc[0], cc[1], 1.5, gt[1])
		"roots_side", "roots_top":
			if w == "mangrove":
				for k in 7:
					var x3 := cv.rng.randi_range(0, 15)
					for y in 16:
						cv.px(x3 + int(round(sin(y * 0.5 + k) * 1.5)), y, C("#4a3a2d") if y % 4 else C("#5d4a3a"))
				for k in 4:
					var y3 := cv.rng.randi_range(0, 15)
					cv.hline(0, 15, y3, Color(0, 0, 0, 0))
					for x in range(0, 16, 3):
						cv.px(x, y3, C("#4a3a2d"))
			else:
				return null
		"leaves":
			match w:
				"cherry":
					BTCommon.leaves(cv, ["#e9a8c2", "#f5c0d4", "#d58daa", "#c77796"], 0.18)
				"pale_oak":
					BTCommon.leaves(cv, ["#9aa093", "#8a9083", "#aab0a3", "#7a8073"], 0.2)
				"spruce":
					BTCommon.leaves(cv, [], 0.25)
				"birch":
					BTCommon.leaves(cv, [], 0.15)
				_:
					BTCommon.leaves(cv, [], 0.2)
		"sapling", "propagule":
			_sapling(cv, w)
		"door_top", "door_bottom":
			BTCommon.door(cv, pal.plank, rest == "door_top", DOOR_STYLE.get(w, 0))
		"trapdoor":
			BTCommon.trapdoor(cv, pal.plank, DOOR_STYLE.get(w, 0))
		"shelf":
			BTCommon.planks(cv, pal.plank)
			var dark := PixelCanvas.shade(C(pal.plank[3]), -0.3)
			cv.rect(1, 1, 14, 6, dark)
			cv.rect(1, 9, 14, 6, dark)
			cv.hline(0, 15, 7, C(pal.plank[2]))
			cv.hline(0, 15, 15, C(pal.plank[2]))
		"fungus":
			_fungus(cv, w)
		"roots":
			_roots(cv, w)
		"nylium", "nylium_side":
			return null
		_:
			return null
	if n == "bamboo_mosaic":
		cv = cv_for(n)
		for y in 16:
			for x in 16:
				var cell := ((x >> 2) + (y >> 3)) % 2
				var c := C(pal.plank[0]) if cell == 0 else C(pal.plank[2])
				if x % 4 == 3 or y % 8 == 7:
					c = C(pal.plank[3])
				cv.px(x, y, c)
	return cv


static func _sapling(cv: PixelCanvas, w: String) -> void:
	var trunk := C(TexPalettes.WOOD[w].bark[0])
	var leaf_cols := {"oak": ["#3f8a24", "#2f6e1a", "#56a534"], "spruce": ["#2f5a32", "#244a28", "#3c6e40"],
		"birch": ["#6fa33c", "#5a8a2e", "#86ba50"], "jungle": ["#3c8f1f", "#2d7417", "#4fae2b"],
		"acacia": ["#6f8f25", "#5a771d", "#86a82f"], "dark_oak": ["#2f6a1a", "#245514", "#3c8222"],
		"mangrove": ["#4f7f2a", "#3f6a22", "#62963a"], "cherry": ["#e59bbf", "#d17fa6", "#f2b8d3"],
		"pale_oak": ["#9aa093", "#858b7e", "#b2b8aa"]}
	var lc: Array = TexPalettes.cols(leaf_cols.get(w, ["#3f8a24", "#2f6e1a", "#56a534"]))
	if w == "mangrove":
		cv.vline(8, 4, 15, C("#6b8f3a"))
		cv.vline(7, 6, 13, C("#5b7a30"))
		cv.rect(6, 1, 4, 3, lc[0])
		cv.px(7, 0, lc[2])
		return
	cv.vline(8, 9, 15, trunk)
	cv.vline(7, 12, 15, PixelCanvas.shade(trunk, -0.15))
	if w == "spruce":
		for y in range(1, 11):
			var half := (y - 1) / 2 + 1
			for x in range(8 - half, 8 + half + 1):
				cv.px(x, y, lc[(x + y) % 3])
	else:
		cv.disc(8, 6, 4.8, lc[0])
		for k in 12:
			var x := cv.rng.randi_range(3, 12)
			var y := cv.rng.randi_range(1, 10)
			if cv.get_px(x, y).a > 0.0:
				cv.px(x, y, lc[cv.rng.randi_range(1, 2)])
		for k in 6:
			cv.px(cv.rng.randi_range(3, 12), cv.rng.randi_range(2, 10), Color(0, 0, 0, 0))


static func _fungus(cv: PixelCanvas, w: String) -> void:
	var cap := C("#b8272b") if w == "crimson" else C("#1f9f8f")
	var spot := C("#ffa04f") if w == "crimson" else C("#ff8f3a")
	cv.vline(8, 8, 15, C("#d9c4a0"))
	cv.vline(7, 11, 15, C("#bda682"))
	for y in range(3, 9):
		var half := 5 - absi(y - 5)
		for x in range(8 - half - 1, 8 + half + 1):
			cv.px(x, y, cap)
	cv.px(6, 4, spot)
	cv.px(10, 5, spot)
	cv.px(8, 3, spot)


static func _roots(cv: PixelCanvas, w: String) -> void:
	var col := C("#8f1d2a") if w == "crimson" else C("#139b8c")
	var hi := PixelCanvas.shade(col, 0.25)
	for k in 6:
		var x := 2 + k * 2 + cv.rng.randi_range(0, 1)
		var top := cv.rng.randi_range(3, 9)
		for y in range(top, 16):
			cv.px(x + (1 if (y + k) % 5 == 0 else 0), y, col if y > top + 1 else hi)


static func _plant(n: String) -> PixelCanvas:
	var cv := cv_for(n)
	var green := C("#4c8e2d")
	var gstem := C("#3f7a25")
	match n:
		"short_grass", "bush":
			_blades(cv, 12 if n == "short_grass" else 16, 4 if n == "short_grass" else 6, [0.55, 0.62, 0.7, 0.48])
			if n == "bush":
				cv.disc(8, 10, 5.5, Color(0.55, 0.55, 0.55))
				_speckle_gray(cv)
		"firefly_bush":
			_blades(cv, 14, 5, [0.55, 0.62, 0.7, 0.48])
			var tint := C("#5f8f33")
			cv.map_colors(func(c, _x, _y): return Color(c.r * tint.r * 1.6, c.g * tint.g * 1.6, c.b * tint.b * 1.6, c.a) if c.a > 0 else c)
			for k in 5:
				cv.px(cv.rng.randi_range(2, 13), cv.rng.randi_range(1, 9), C("#f7ff7a"))
		"fern":
			for side in [-1, 1]:
				for y in range(2, 16):
					var x: int = 8 + side * int((16 - y) / 3.0)
					cv.px(x, y, Color(0.5, 0.5, 0.5))
					if y % 2 == 0:
						cv.px(x + side, y - 1, Color(0.6, 0.6, 0.6))
			cv.vline(8, 4, 15, Color(0.45, 0.45, 0.45))
		"tall_grass_bottom", "large_fern_bottom":
			_blades(cv, 14, 0, [0.55, 0.62, 0.7, 0.48])
		"tall_grass_top", "large_fern_top":
			_blades(cv, 10, 6, [0.55, 0.62, 0.7, 0.48])
		"dead_bush":
			var bc := C("#8a5f30")
			cv.vline(8, 8, 15, bc)
			for br in [[8, 10, -1], [8, 11, 1], [8, 8, -1], [8, 7, 1]]:
				var x2: int = br[0]
				var y2: int = br[1]
				for s in 4:
					x2 += br[2]
					y2 -= 1
					cv.px(x2, y2, bc)
			cv.px(4, 4, C("#6f4a22"))
			cv.px(12, 3, C("#6f4a22"))
		"short_dry_grass", "tall_dry_grass":
			_blades(cv, 10 if n == "short_dry_grass" else 15, 5 if n == "short_dry_grass" else 1, [0.8, 0.72, 0.66, 0.6])
			var dry := C("#c8b26a")
			cv.map_colors(func(c, _x, _y): return Color(c.r * dry.r * 1.2, c.g * dry.g * 1.2, c.b * dry.b * 1.2, c.a) if c.a > 0 else c)
		"dandelion":
			_flower(cv, 9, ["#fce834", "#f2c41a", "#fff38a"], "round", 2)
		"poppy":
			_flower(cv, 8, ["#e0261f", "#b11912", "#ff5a4a"], "cup", 2, "#1a1a1a")
		"blue_orchid":
			_flower(cv, 7, ["#2fb3e8", "#1a8cc4", "#79d6ff"], "star", 2)
		"allium":
			cv.vline(8, 8, 15, gstem)
			cv.disc(8, 5, 3.4, C("#b865e0"))
			cv.speckle(C("#d99cf5"), 5)
			for k in 6:
				cv.px(cv.rng.randi_range(5, 11), cv.rng.randi_range(2, 8), C("#8f3fbd"))
			for y in range(9, 16):
				for x in 16:
					if x != 8:
						cv.px(x, y, Color(0, 0, 0, 0))
			BTCommon.leaf(cv, 8, 13, -1)
		"azure_bluet":
			cv.vline(8, 8, 15, gstem)
			cv.vline(5, 10, 15, gstem)
			cv.vline(11, 9, 15, gstem)
			for p in [[8, 6], [5, 8], [11, 7]]:
				cv.px(p[0], p[1], C("#f2d53b"))
				for d in [[1, 0], [-1, 0], [0, 1], [0, -1]]:
					cv.px(p[0] + d[0], p[1] + d[1], C("#eaf0f5"))
		"red_tulip", "orange_tulip", "white_tulip", "pink_tulip":
			var tc: Array = {"red_tulip": ["#d8261c", "#a3150e", "#ff5b4f"], "orange_tulip": ["#f07d18", "#c45c0a", "#ffac52"],
				"white_tulip": ["#eef0ea", "#c9ccc3", "#ffffff"], "pink_tulip": ["#f2a3c4", "#d67ba0", "#ffd0e2"]}[n]
			_flower(cv, 8, tc, "tulip", 2)
		"oxeye_daisy":
			_flower(cv, 8, ["#f4f6f0", "#d3d6ce", "#ffffff"], "daisy", 2, "#f2c418")
		"cornflower":
			_flower(cv, 8, ["#4568e0", "#2d4bb8", "#7c9aff"], "star", 2)
		"lily_of_the_valley":
			cv.vline(7, 3, 15, gstem)
			for p in [[9, 4], [9, 7], [5, 6], [9, 10]]:
				cv.px(p[0], p[1], C("#f6f7f2"))
				cv.px(p[0], p[1] + 1, C("#d8dbd2"))
				cv.px(p[0] - 1 if p[0] > 7 else p[0] + 1, p[1], gstem)
			BTCommon.leaf(cv, 7, 13, 1, C("#3f8f2a"))
			BTCommon.leaf(cv, 7, 12, -1, C("#3f8f2a"))
		"wither_rose":
			_flower(cv, 8, ["#262320", "#141210", "#3d3833"], "cup", 2, "#0a0a0a", "#3a3a2a")
		"torchflower":
			_flower(cv, 7, ["#f28a2c", "#d55d1a", "#ffd04a"], "torch", 2)
		"closed_eyeblossom":
			_flower(cv, 8, ["#7c7f86", "#5f626a", "#9a9da4"], "tulip", 2, "", "#5b6b52")
		"open_eyeblossom":
			_flower(cv, 8, ["#f4f2ec", "#cfccc4", "#ffffff"], "daisy", 2, "#f28a1c", "#5b6b52")
		"cactus_flower":
			cv.disc(8, 11, 4.0, C("#e86aa0"))
			cv.disc(8, 11, 2.0, C("#f7b3d0"))
			cv.px(8, 11, C("#f6e04a"))
		"sunflower_bottom", "lilac_bottom", "rose_bush_bottom", "peony_bottom", "pitcher_plant_bottom":
			cv.vline(8, 0, 15, gstem)
			for k in 3:
				BTCommon.leaf(cv, 8, 13 - k * 4, -1 if k % 2 == 0 else 1, green)
				BTCommon.leaf(cv, 8, 11 - k * 4, 1 if k % 2 == 0 else -1, green)
		"sunflower_top":
			cv.vline(8, 9, 15, gstem)
			cv.disc(8, 5, 4.5, C("#f7d21e"))
			cv.disc(8, 5, 2.3, C("#6b4413"))
			cv.px(7, 4, C("#8a5a1c"))
			BTCommon.leaf(cv, 8, 13, -1, green)
		"lilac_top":
			cv.vline(8, 10, 15, gstem)
			for k in 26:
				var x3 := 8 + cv.rng.randi_range(-4, 4)
				var y3 := cv.rng.randi_range(1, 10)
				cv.px(x3, y3, C("#c58ad6") if k % 3 else C("#e6b8ef"))
		"rose_bush_top":
			cv.vline(8, 8, 15, gstem)
			_blades(cv, 6, 8, [0.3, 0.35, 0.4, 0.3])
			cv.map_colors(func(c, _x, _y): return Color(c.r * 0.6, c.g * 1.3, c.b * 0.6, c.a) if c.a > 0 else c)
			for p in [[5, 4], [11, 5], [8, 2], [9, 8], [4, 9]]:
				cv.rect(p[0], p[1], 2, 2, C("#d11f24"))
				cv.px(p[0], p[1], C("#ff5a4f"))
		"peony_top":
			cv.vline(8, 9, 15, gstem)
			cv.disc(8, 5, 5.0, C("#e8a6d4"))
			cv.speckle(C("#f7cbe8"), 10)
			cv.speckle(C("#c276ab"), 8)
		"pitcher_plant_top":
			cv.vline(8, 6, 15, gstem)
			cv.rect(5, 1, 6, 7, C("#3f9fa5"))
			cv.rect(6, 0, 4, 2, C("#7ad3c8"))
			cv.rect(6, 2, 4, 5, C("#2b7a86"))
		"pink_petals", "wildflowers", "leaf_litter":
			var pc: Array = {"pink_petals": ["#f5a8cc", "#e582b0", "#ffd0e4"], "wildflowers": ["#f7e04a", "#f2f2ec", "#e8c11c"],
				"leaf_litter": ["#9a6a2c", "#b07c35", "#7c5220"]}[n]
			for k in 9:
				var x4 := cv.rng.randi_range(1, 13)
				var y4 := cv.rng.randi_range(1, 13)
				cv.px(x4, y4, C(pc[0]))
				cv.px(x4 + 1, y4, C(pc[1]))
				cv.px(x4, y4 + 1, C(pc[2]))
				if n != "leaf_litter":
					cv.px(x4 + 1, y4 + 1, C("#5d8f36"))
		"brown_mushroom":
			cv.vline(8, 9, 15, C("#d8c7a8"))
			cv.rect(4, 6, 9, 3, C("#9a6f4f"))
			cv.rect(5, 5, 7, 1, C("#b08566"))
		"red_mushroom":
			cv.vline(8, 9, 15, C("#e0d4be"))
			cv.rect(5, 4, 7, 5, C("#d1231c"))
			cv.rect(6, 3, 5, 1, C("#d1231c"))
			cv.px(6, 5, C("#f6f0e6"))
			cv.px(9, 4, C("#f6f0e6"))
			cv.px(10, 7, C("#f6f0e6"))
		"brown_mushroom_block":
			cv.palette_noise(TexPalettes.cols(["#94704f", "#876447", "#a07b59"]), [4, 3, 2], 1)
		"red_mushroom_block":
			cv.palette_noise(TexPalettes.cols(["#c62b22", "#b3221b", "#d7372c"]), [4, 3, 2], 1)
			cv.blobs(TexPalettes.cols(["#f2ece0", "#e0d8c8"]), 5, 3, 5)
		"mushroom_stem":
			cv.palette_noise(TexPalettes.cols(["#d6ceb8", "#cbc2aa", "#e2dac6"]), [4, 3, 2], 0)
		"sugar_cane":
			for x in [3, 8, 12]:
				for y in 16:
					var g := 0.62 if y % 6 != 0 else 0.45
					cv.px(x, y, Color(g, g, g))
					cv.px(x + 1, y, Color(g - 0.1, g - 0.1, g - 0.1))
				BTCommon.leaf(cv, x, 5 + x % 4, 1, Color(0.6, 0.6, 0.6))
		"cactus_side":
			cv.palette_noise(TexPalettes.cols(["#5c8b2c", "#4f7a25", "#6b9c33"]), [4, 3, 2], 0)
			for x in [1, 14]:
				cv.vline(x, 0, 15, C("#3f6a1d"))
			for x in 16:
				if x == 0 or x == 15:
					for y in 16:
						cv.px(x, y, Color(0, 0, 0, 0))
			for k in 8:
				cv.px(cv.rng.randi_range(2, 13), cv.rng.randi_range(0, 15), C("#1c2b10"))
		"cactus_top", "cactus_bottom":
			cv.palette_noise(TexPalettes.cols(["#6b9c33", "#5c8b2c", "#7cad3c"]), [4, 3, 2], 0)
			cv.rect_outline(1, 1, 14, 14, C("#3f6a1d"))
			for x in 16:
				cv.px(x, 0, Color(0, 0, 0, 0))
				cv.px(x, 15, Color(0, 0, 0, 0))
				cv.px(0, x, Color(0, 0, 0, 0))
				cv.px(15, x, Color(0, 0, 0, 0))
		"bamboo_stalk":
			cv.rect(6, 0, 4, 16, C("#6b9432"))
			cv.vline(6, 0, 15, C("#81ab3e"))
			for y in [3, 10]:
				cv.hline(6, 9, y, C("#4d6e22"))
		"bamboo_leaves":
			BTCommon.leaves(cv, ["#5f9b31", "#4d8327", "#74b03c", "#3f6d20"], 0.45)
		"wheat_stage0", "wheat_stage1", "wheat_stage2", "wheat_stage3", "wheat_stage4", "wheat_stage5", "wheat_stage6", "wheat_stage7":
			var st := int(n.substr(11))
			_crop_rows(cv, 3 + st * 1.6, C("#3f8a24").lerp(C("#c7a53a"), st / 7.0), st == 7, C("#d8b54a"))
		"carrots_stage0", "carrots_stage1", "carrots_stage2", "carrots_stage3":
			var st2 := int(n.substr(13))
			_crop_rows(cv, 3 + st2 * 3, C("#4c9a2a"), false, Color())
			if st2 == 3:
				for x in [3, 8, 12]:
					cv.px(x, 15, C("#f28a1c"))
					cv.px(x, 14, C("#f28a1c"))
		"potatoes_stage0", "potatoes_stage1", "potatoes_stage2", "potatoes_stage3":
			var st3 := int(n.substr(14))
			_crop_rows(cv, 3 + st3 * 3, C("#3f8f2a"), false, Color())
			if st3 == 3:
				for x in [3, 8, 12]:
					cv.rect(x, 13, 2, 2, C("#c9a55a"))
		"beetroots_stage0", "beetroots_stage1", "beetroots_stage2", "beetroots_stage3":
			var st4 := int(n.substr(15))
			_crop_rows(cv, 3 + st4 * 3, C("#3f8a2a"), false, Color())
			for x in [3, 8, 12]:
				cv.px(x, 15 - st4 * 3 + 2, C("#8f1d33"))
			if st4 == 3:
				for x in [3, 8, 12]:
					cv.rect(x, 13, 2, 2, C("#a3213c"))
		"melon_stem", "pumpkin_stem":
			for y in range(4, 16):
				cv.px(8 + (1 if y % 4 == 0 else 0), y, Color(0.6, 0.6, 0.6))
			BTCommon.leaf(cv, 8, 9, -1, Color(0.55, 0.55, 0.55))
			BTCommon.leaf(cv, 8, 6, 1, Color(0.55, 0.55, 0.55))
		"sweet_berry_bush_stage0", "sweet_berry_bush_stage1", "sweet_berry_bush_stage2", "sweet_berry_bush_stage3":
			var st5 := int(n.substr(23))
			_blades(cv, 8 + st5 * 3, 12 - st5 * 3, [0.3, 0.36, 0.42, 0.28])
			cv.map_colors(func(c, _x, _y): return Color(c.r * 0.7, c.g * 1.5, c.b * 0.6, c.a) if c.a > 0 else c)
			if st5 >= 2:
				for k in (3 if st5 == 2 else 7):
					cv.px(cv.rng.randi_range(3, 12), cv.rng.randi_range(5, 14), C("#b8272b") if st5 == 3 else C("#5f8f33"))
		"kelp":
			for y in 16:
				var x5 := 7 + int(round(sin(y * 0.6) * 1.5))
				cv.px(x5, y, C("#56902c"))
				cv.px(x5 + 1, y, C("#467a22"))
				if y % 4 == 1:
					cv.px(x5 + 2, y, C("#6aa83a"))
					cv.px(x5 - 1, y + 1, C("#6aa83a"))
		"seagrass":
			_blades(cv, 10, 1, [0.35, 0.4, 0.45, 0.3])
			cv.map_colors(func(c, _x, _y): return Color(c.r * 0.9, c.g * 1.9, c.b * 0.7, c.a) if c.a > 0 else c)
		"sea_pickle":
			cv.palette_noise(TexPalettes.cols(["#5f8a2a", "#6f9a33", "#4f7a22"]), [4, 3, 2], 1)
			cv.rect(0, 0, 16, 3, C("#8fbf4a"))
			cv.speckle(C("#a8d860"), 6)
		"lily_pad":
			cv.disc(8, 8, 7.0, Color(0.6, 0.6, 0.6))
			for y in range(0, 8):
				cv.px(8, y, Color(0, 0, 0, 0))
				cv.px(9, y, Color(0, 0, 0, 0))
			cv.speckle(Color(0.48, 0.48, 0.48), 12)
		"tube_coral", "brain_coral", "bubble_coral", "fire_coral", "horn_coral":
			var ccol: String = {"tube": "#3257d8", "brain": "#cf5b9e", "bubble": "#a31ca3", "fire": "#d33a2e", "horn": "#d8c742"}[n.replace("_coral", "")]
			var cc := C(ccol)
			for k in 5:
				var x6 := 2 + k * 3
				var top2 := cv.rng.randi_range(2, 7)
				cv.vline(x6, top2, 15, cc)
				cv.px(x6 - 1, top2 + 2, PixelCanvas.shade(cc, 0.2))
				cv.px(x6 + 1, top2 + 1, PixelCanvas.shade(cc, -0.15))
		"tube_coral_block", "brain_coral_block", "bubble_coral_block", "fire_coral_block", "horn_coral_block":
			var ccol2: String = {"tube": "#3257d8", "brain": "#cf5b9e", "bubble": "#a31ca3", "fire": "#d33a2e", "horn": "#d8c742"}[n.replace("_coral_block", "")]
			var bc2 := C(ccol2)
			cv.palette_noise([bc2, PixelCanvas.shade(bc2, -0.15), PixelCanvas.shade(bc2, 0.15)], [4, 3, 2], 1)
			cv.speckle(PixelCanvas.shade(bc2, 0.35), 10)
		"dead_tube_coral_block", "dead_brain_coral_block", "dead_bubble_coral_block", "dead_fire_coral_block", "dead_horn_coral_block":
			cv.palette_noise(TexPalettes.cols(["#8a827d", "#7a736e", "#9a938e"]), [4, 3, 2], 1)
			cv.speckle(C("#aaa39e"), 10)
		"sponge", "wet_sponge":
			var sc := C("#c7c34d") if n == "sponge" else C("#9f9a3b")
			cv.palette_noise([sc, PixelCanvas.shade(sc, -0.12), PixelCanvas.shade(sc, 0.1)], [4, 3, 2], 1)
			for k in 12:
				cv.px(cv.rng.randi_range(0, 15), cv.rng.randi_range(0, 15), PixelCanvas.shade(sc, -0.45))
		"vine":
			for k in 18:
				var x7 := cv.rng.randi_range(0, 15)
				var y7 := cv.rng.randi_range(0, 15)
				cv.px(x7, y7, Color(0.55, 0.55, 0.55))
				cv.px(x7 + 1, y7, Color(0.45, 0.45, 0.45))
				cv.px(x7, y7 + 1, Color(0.62, 0.62, 0.62))
			for x in [3, 10]:
				for y in 16:
					if (y + x) % 3 != 0:
						cv.px(x + int(sin(y * 0.8) * 1.2), y, Color(0.5, 0.5, 0.5))
		"glow_lichen":
			for k in 16:
				var x8 := cv.rng.randi_range(0, 15)
				var y8 := cv.rng.randi_range(0, 15)
				cv.px(x8, y8, C("#6d7d68"))
				cv.px(x8 + 1, y8, C("#86987f"))
			for k in 6:
				cv.px(cv.rng.randi_range(0, 15), cv.rng.randi_range(0, 15), C("#c8f5ea"))
		"hanging_roots":
			for k in 6:
				var x9 := 2 + k * 2 + cv.rng.randi_range(0, 1)
				var ln := cv.rng.randi_range(6, 14)
				for y in ln:
					cv.px(x9 + (1 if y % 5 == 4 else 0), y, C("#a17a55") if y < ln - 1 else C("#c79f75"))
		"spore_blossom":
			cv.disc(8, 8, 6.5, C("#d3659f"))
			cv.disc(8, 8, 3.5, C("#f09ac6"))
			cv.disc(8, 8, 1.4, C("#6fae3c"))
		"cave_vines", "cave_vines_lit":
			for y in 16:
				var x10 := 8 + int(round(sin(y * 0.5) * 1.5))
				cv.px(x10, y, C("#4d7f2a"))
				if y % 3 == 0:
					cv.px(x10 + 1, y, C("#63a036"))
					cv.px(x10 - 1, y + 1, C("#63a036"))
			if n == "cave_vines_lit":
				for p in [[6, 4], [10, 9], [7, 13]]:
					cv.rect(p[0], p[1], 2, 2, C("#f7a93a"))
					cv.px(p[0], p[1], C("#ffe07a"))
		"azalea", "flowering_azalea":
			cv.vline(8, 10, 15, C("#6b5a2f"))
			cv.disc(8, 7, 5.5, C("#5e8f2e"))
			cv.speckle(C("#4b7824"), 12)
			if n == "flowering_azalea":
				for k in 6:
					cv.px(cv.rng.randi_range(4, 12), cv.rng.randi_range(3, 11), C("#d77ec9"))
		"azalea_leaves":
			BTCommon.leaves(cv, ["#5e8f2e", "#4d7a26", "#6fa338", "#3f6a1e"], 0.18)
		"flowering_azalea_leaves":
			BTCommon.leaves(cv, ["#5e8f2e", "#4d7a26", "#6fa338", "#3f6a1e"], 0.18, ["#d77ec9", "#e9a0dd"])
		"pale_hanging_moss":
			for k in 7:
				var x11 := 1 + k * 2
				var ln2 := cv.rng.randi_range(7, 15)
				for y in ln2:
					cv.px(x11, y, C("#9ea396") if y % 2 == 0 else C("#8b9082"))
		"creaking_heart", "creaking_heart_active":
			BTCommon.log_side(cv, TexPalettes.WOOD["pale_oak"].bark)
			var eye := C("#f28a1c") if n == "creaking_heart_active" else C("#5a5046")
			cv.rect(5, 5, 6, 6, C("#3a342d"))
			cv.rect(6, 7, 1, 2, eye)
			cv.rect(9, 7, 1, 2, eye)
		"creaking_heart_top":
			BTCommon.log_top(cv, TexPalettes.WOOD["pale_oak"].ring, TexPalettes.WOOD["pale_oak"].bark)
			cv.rect(6, 6, 4, 4, C("#f28a1c"))
		"resin_clump":
			cv.blobs(TexPalettes.cols(["#e2791f", "#f59a3a", "#c35d12"]), 7, 3, 5)
		"resin_block":
			cv.palette_noise(TexPalettes.cols(["#df7a21", "#cc6a18", "#ee9338", "#b35a12"]), [4, 3, 2, 1], 1)
		"resin_bricks":
			BTCommon.bricks(cv, ["#d97620", "#e5892f", "#c96818"], C("#8a430c"), 4, 8, 4)
		"chiseled_resin_bricks":
			BTCommon.chiseled(cv, ["#d97620", "#e5892f", "#c96818"], C("#8a430c"))
		"bee_nest_top", "bee_nest_bottom", "bee_nest_side", "bee_nest_front":
			cv.palette_noise(TexPalettes.cols(["#c9a24f", "#b58e40", "#dcb45f"]), [4, 3, 2], 0)
			for y in [3, 7, 11, 15]:
				cv.hline(0, 15, y, C("#8f6a2c"))
			if n == "bee_nest_front":
				cv.rect(6, 7, 4, 3, C("#2a1c0c"))
			if n == "bee_nest_top":
				cv.rect_outline(2, 2, 12, 12, C("#8f6a2c"))
		"beehive_end", "beehive_side", "beehive_front":
			BTCommon.planks(cv, TexPalettes.WOOD["oak"].plank)
			if n == "beehive_front":
				cv.rect(3, 4, 10, 3, C("#e2b845"))
				cv.rect(6, 9, 4, 3, C("#2a1c0c"))
			elif n == "beehive_side":
				cv.rect(0, 5, 16, 2, C("#d9ad3c"))
		"honey_block_top", "honey_block_bottom", "honey_block_side":
			for y in 16:
				for x in 16:
					var hc := C("#f5a623").lerp(C("#fbbf3c"), cv.rng.randf() * 0.4)
					hc.a = 0.78
					cv.px(x, y, hc)
			cv.rect_outline(0, 0, 16, 16, Color(0.8, 0.5, 0.08, 0.95))
			cv.rect(4, 4, 8, 8, Color(0.99, 0.72, 0.2, 0.9))
		"honeycomb_block":
			for y in 16:
				for x in 16:
					var hx := (x + (4 if (y >> 2) % 2 == 1 else 0)) % 8
					var edge := hx == 0 or y % 4 == 0
					cv.px(x, y, C("#c9851b") if edge else C("#e8a930").lerp(C("#f5c24a"), cv.rng.randf() * 0.5))
		"slime_block":
			for y in 16:
				for x in 16:
					var s := C("#76be6d")
					s.a = 0.72
					cv.px(x, y, s)
			cv.rect_outline(0, 0, 16, 16, Color(0.33, 0.62, 0.3, 0.9))
			cv.rect(3, 3, 10, 10, Color(0.42, 0.72, 0.38, 0.85))
			cv.rect(5, 5, 6, 6, Color(0.55, 0.82, 0.5, 0.9))
		"turtle_egg":
			cv.fill(C("#e8e3cf"))
			cv.speckle(C("#7fa24f"), 18)
		"sniffer_egg":
			cv.fill(C("#a94c3b"))
			cv.speckle(C("#d37a4f"), 20)
			cv.speckle(C("#6f2d22"), 10)
		"hay_block_side":
			for y in 16:
				for x in 16:
					var hc2 := C("#c9a833").lerp(C("#b18e22"), cv.rng.randf() * 0.7)
					cv.px(x, y, hc2)
			for y in [2, 13]:
				cv.hline(0, 15, y, C("#8f4f1e"))
				cv.hline(0, 15, y + 1, C("#a8612a"))
		"hay_block_top":
			for y in 16:
				for x in 16:
					var d := maxi(absi(x * 2 - 15), absi(y * 2 - 15)) >> 1
					cv.px(x, y, C("#c9a833").lerp(C("#9a7c1c"), float(d % 3) * 0.3 + cv.rng.randf() * 0.2))
		"dried_kelp_side":
			for y in 16:
				for x in 16:
					cv.px(x, y, C("#3b4a26").lerp(C("#2c381b"), cv.rng.randf() * 0.7))
			for y in [2, 13]:
				cv.hline(0, 15, y, C("#566a36"))
		"dried_kelp_top":
			cv.palette_noise(TexPalettes.cols(["#3b4a26", "#2c381b", "#4b5d31"]), [4, 3, 2], 1)
			cv.rect_outline(2, 2, 12, 12, C("#566a36"))
		"pumpkin_side":
			for y in 16:
				for x in 16:
					var rib := x % 4 == 0
					var pcol := C("#e38a1d").lerp(C("#d27a14"), cv.rng.randf() * 0.5)
					if rib:
						pcol = C("#b8620e")
					cv.px(x, y, pcol)
		"pumpkin_top":
			for y in 16:
				for x in 16:
					var d2 := maxi(absi(x * 2 - 15), absi(y * 2 - 15)) >> 1
					cv.px(x, y, C("#e38a1d") if d2 % 3 != 0 else C("#c46f12"))
			cv.rect(7, 7, 2, 2, C("#6b8f2e"))
		"carved_pumpkin", "jack_o_lantern":
			for y in 16:
				for x in 16:
					var pcol2 := C("#e38a1d").lerp(C("#d27a14"), cv.rng.randf() * 0.5)
					if x % 4 == 0:
						pcol2 = C("#b8620e")
					cv.px(x, y, pcol2)
			var hole := C("#ffd35a") if n == "jack_o_lantern" else C("#3d2208")
			# original face design: triangle eyes, wide grin
			for e in [[3, 4], [10, 4]]:
				cv.rect(e[0], e[1] + 1, 3, 2, hole)
				cv.px(e[0] + 1, e[1], hole)
			cv.rect(3, 10, 10, 2, hole)
			cv.px(5, 9, hole)
			cv.px(10, 9, hole)
			cv.px(7, 12, hole)
			cv.px(8, 12, hole)
		"melon_side":
			for y in 16:
				for x in 16:
					var stripe := (x + int(sin(y * 0.4) * 1.5)) % 5 < 2
					cv.px(x, y, C("#6d9a27") if stripe else C("#8fb838").lerp(C("#7aa630"), cv.rng.randf()))
		"melon_top":
			cv.palette_noise(TexPalettes.cols(["#8fb838", "#7aa630", "#6d9a27"]), [4, 3, 2], 1)
			cv.rect(6, 6, 4, 4, C("#5f8a21"))
		"muddy_mangrove_roots_side", "muddy_mangrove_roots_top":
			cv.palette_noise(TexPalettes.cols(["#3c3837", "#45403e", "#353130"]), [4, 3, 2], 1)
			for k in 6:
				var x16 := cv.rng.randi_range(0, 15)
				for y in 16:
					if cv.rng.randf() < 0.8:
						cv.px(x16 + int(round(sin(y * 0.6 + k) * 1.2)), y, C("#5d4a3a"))
		"crimson_nylium", "warped_nylium":
			var nc := TexPalettes.cols(["#8f1d23", "#a8262d", "#7a161c", "#b73a3f"]) if n.begins_with("crimson") else TexPalettes.cols(["#167d74", "#1c9489", "#10675f", "#2aa99c"])
			cv.palette_noise(nc, [4, 3, 2, 1], 1)
		"crimson_nylium_side", "warped_nylium_side":
			cv.palette_noise(TexPalettes.cols(["#6f2e2e", "#612626", "#7d3636", "#521c1c"]), [4, 3, 2, 2], 1)
			var nc2 := TexPalettes.cols(["#8f1d23", "#a8262d", "#b73a3f"]) if n.begins_with("crimson") else TexPalettes.cols(["#167d74", "#1c9489", "#2aa99c"])
			for x in 16:
				var dd := 3 + (1 if cv.rng.randf() < 0.5 else 0)
				for y in dd:
					cv.px(x, y, nc2[cv.rng.randi_range(0, 2)])
		"nether_sprouts":
			for k in 8:
				var x12 := 1 + k * 2
				var ln3 := cv.rng.randi_range(2, 6)
				cv.vline(x12, 15 - ln3, 15, C("#14a08f"))
				cv.px(x12, 15 - ln3, C("#3fd9c3"))
		"weeping_vines":
			for y in 16:
				var x13 := 8 + int(round(sin(y * 0.7) * 2.0))
				cv.px(x13, y, C("#8f1d23"))
				cv.px(x13 + 1, y, C("#b3262d"))
				if y % 3 == 0:
					cv.px(x13 - 1, y, C("#d8434a"))
		"twisting_vines":
			for y in 16:
				var x14 := 8 + int(round(sin(y * 0.7) * 2.0))
				cv.px(x14, y, C("#12877a"))
				cv.px(x14 + 1, y, C("#18a293"))
				if y % 3 == 1:
					cv.px(x14 - 1, y, C("#3fd3bf"))
		"nether_wart_stage0", "nether_wart_stage1", "nether_wart_stage2":
			var ws := int(n.substr(17))
			for k in 3 + ws * 2:
				var x15 := cv.rng.randi_range(2, 12)
				var y15 := cv.rng.randi_range(8 - ws * 3, 14)
				cv.rect(x15, y15, 2, 2, C("#8c1f24"))
				cv.px(x15, y15, C("#b8323a"))
			for x in [4, 8, 11]:
				cv.vline(x, 12, 15, C("#6d151a"))
		_:
			return null
	return cv


static func _speckle_gray(cv: PixelCanvas) -> void:
	for y in 16:
		for x in 16:
			var c := cv.get_px(x, y)
			if c.a > 0.0:
				var g := c.r + cv.rng.randf_range(-0.1, 0.1)
				cv.px(x, y, Color(g, g, g))


static func _blades(cv: PixelCanvas, count: int, top_min: int, shades: Array) -> void:
	for k in count:
		var x := cv.rng.randi_range(0, 15)
		var top := cv.rng.randi_range(top_min, top_min + 5)
		var lean := cv.rng.randi_range(-1, 1)
		var s: float = shades[k % shades.size()]
		for y in range(top, 16):
			var xx := x + (lean if y < top + 3 else 0)
			cv.px(xx, y, Color(s, s, s))


static func _flower(cv: PixelCanvas, head_y: int, pal: Array, shape: String, _unused: int, center := "", stem_col := "#3f7a25") -> void:
	var pc := TexPalettes.cols(pal)
	var sc := C(stem_col)
	cv.vline(8, head_y + 1, 15, sc)
	BTCommon.leaf(cv, 8, 13, -1, PixelCanvas.shade(sc, 0.15))
	BTCommon.leaf(cv, 8, 11, 1, PixelCanvas.shade(sc, 0.15))
	var cy := head_y - 2
	match shape:
		"round":
			cv.disc(8, cy + 0.5, 2.6, pc[0])
			cv.px(7, cy - 1, pc[2])
			cv.px(9, cy + 1, pc[1])
		"cup":
			cv.rect(6, cy - 1, 5, 3, pc[0])
			cv.rect(7, cy - 2, 3, 1, pc[2])
			cv.px(6, cy + 1, pc[1])
			cv.px(10, cy + 1, pc[1])
		"star":
			for d in [[0, 0], [1, 0], [-1, 0], [0, 1], [0, -1], [2, -1], [-2, -1], [1, 2], [-1, 2]]:
				cv.px(8 + d[0], cy + d[1], pc[0] if absi(d[0]) + absi(d[1]) < 2 else pc[1])
			cv.px(8, cy, pc[2])
		"tulip":
			cv.rect(6, cy - 1, 5, 4, pc[0])
			cv.px(6, cy - 2, pc[2])
			cv.px(8, cy - 2, pc[2])
			cv.px(10, cy - 2, pc[2])
			cv.vline(7, cy, cy + 2, pc[1])
		"daisy":
			for a in 8:
				var ang := a * TAU / 8.0
				cv.px(8 + int(round(cos(ang) * 2.2)), cy + int(round(sin(ang) * 2.2)), pc[0])
			cv.px(8, cy, C(center) if center != "" else pc[2])
		"torch":
			cv.rect(7, cy - 2, 3, 4, pc[0])
			cv.px(8, cy - 3, pc[2])
			cv.px(6, cy, pc[1])
			cv.px(10, cy, pc[1])
	if center != "" and shape != "daisy":
		cv.px(8, cy, C(center))


static func _crop_rows(cv: PixelCanvas, height: float, col: Color, ripe: bool, ripe_col: Color) -> void:
	var h := int(height)
	for x in [2, 5, 8, 11, 14]:
		var top := 16 - h + cv.rng.randi_range(0, 1)
		for y in range(maxi(top, 0), 16):
			cv.px(x, y, PixelCanvas.shade(col, cv.rng.randf_range(-0.08, 0.08)))
			if y % 3 == 0:
				cv.px(x + 1, y, PixelCanvas.shade(col, 0.1))
		if ripe:
			for y in range(maxi(top, 0), mini(top + 4, 16)):
				cv.px(x, y, ripe_col)
				cv.px(x + 1, y + 1, PixelCanvas.shade(ripe_col, -0.12))
