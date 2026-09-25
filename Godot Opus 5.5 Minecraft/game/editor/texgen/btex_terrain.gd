class_name BTexTerrain
extends RefCounted
## Terrain, stone, ore, mineral, sulfur and utility textures.



static func C(s: String) -> Color:
	return Color.html(s)


static func cv_for(n: String) -> PixelCanvas:
	return PixelCanvas.new(16, 16, n)


const STONE := ["#7f7f7f", "#747474", "#8b8b8b", "#6a6a6a", "#969696"]
const DEEPSLATE := ["#4f4f55", "#46464c", "#5a5a60", "#3c3c42", "#626268"]
const NETHERRACK := ["#6f2e2e", "#612626", "#7d3636", "#521c1c", "#8a4040"]
const DIRT := ["#866043", "#6f4d34", "#9b7351", "#5c3f29", "#79573c"]
const SAND := ["#dbd3a0", "#d1c68e", "#e5ddb0", "#c6ba83"]
const RED_SAND := ["#be6621", "#ab5a1c", "#cb7431", "#96501a"]
const END_STONE := ["#dbde9e", "#d0d38f", "#e5e8ad", "#c4c784", "#eef0bf"]
const TUFF := ["#6c6d66", "#61625b", "#797a72", "#575850", "#84857c"]
const CALCITE := ["#dfe0dc", "#d4d5d0", "#e9eae6", "#c9cac4"]
const BLACKSTONE := ["#2d272c", "#252025", "#373036", "#1c181c", "#433b42"]
const SULFUR := ["#d8c43a", "#c7b22c", "#e6d556", "#b39e22", "#f0e27a"]
const CINNABAR := ["#b2393a", "#9f2f31", "#c54a48", "#8a2527", "#d6645c"]


static func base_stone(n: String, pal: Array, weights: Array = [5, 3, 3, 2, 1], cluster := 2) -> PixelCanvas:
	var cv := cv_for(n)
	cv.palette_noise(TexPalettes.cols(pal), weights, cluster)
	return cv


static func gen(n: String) -> Array:
	var cv: PixelCanvas = null
	match n:
		"missing":
			cv = cv_for(n)
			for y in 16:
				for x in 16:
					cv.px(x, y, C("#f800f8") if ((x >> 3) + (y >> 3)) % 2 == 0 else C("#000000"))
		"dirt":
			cv = base_stone(n, DIRT, [5, 3, 2, 1, 2], 1)
			cv.speckle(C("#a67d58"), 6)
			cv.speckle(C("#4d3322"), 5)
		"coarse_dirt":
			cv = base_stone(n, DIRT, [5, 3, 2, 1, 2], 1)
			cv.blobs(TexPalettes.cols(["#8a8480", "#6f6a66", "#a09a95"]), 9, 2, 3, 0)
		"rooted_dirt":
			cv = base_stone(n, DIRT, [5, 3, 2, 1, 2], 1)
			for k in 4:
				var x := cv.rng.randi_range(1, 14)
				var y := cv.rng.randi_range(0, 8)
				for s in cv.rng.randi_range(4, 8):
					cv.px(x, y, C("#b58d5e"))
					y += 1
					x += cv.rng.randi_range(-1, 1)
		"podzol_top":
			cv = base_stone(n, ["#5b3d1d", "#7a5427", "#8e6431", "#3f2a13", "#6a4820"], [4, 3, 2, 2, 2], 1)
			cv.speckle(C("#9c7a3e"), 8)
		"podzol_side":
			cv = _side_with_top(n, DIRT, ["#5b3d1d", "#7a5427", "#8e6431", "#3f2a13"], 3)
		"mycelium_top":
			cv = base_stone(n, ["#6f6268", "#857680", "#5d5157", "#9a8b95", "#776b72"], [4, 3, 3, 1, 2], 1)
			cv.speckle(C("#b3a6ae"), 6)
		"mycelium_side":
			cv = _side_with_top(n, DIRT, ["#6f6268", "#857680", "#5d5157", "#9a8b95"], 3)
		"grass_block_top":
			cv = cv_for(n)
			cv.palette_noise([Color(0.62, 0.62, 0.62), Color(0.55, 0.55, 0.55), Color(0.7, 0.7, 0.7), Color(0.48, 0.48, 0.48), Color(0.76, 0.76, 0.76)], [4, 3, 3, 2, 1], 1)
		"grass_block_side":
			cv = base_stone(n, DIRT, [5, 3, 2, 1, 2], 1)
			cv.speckle(C("#a67d58"), 4)
			for x in 16:
				var depth := 3 + (1 if cv.rng.randf() < 0.5 else 0) + (1 if cv.rng.randf() < 0.25 else 0)
				for y in depth:
					var g := 0.58 + cv.rng.randf_range(-0.08, 0.1)
					cv.px(x, y, Color(g, g, g, 0.9))
		"grass_block_snow":
			cv = base_stone(n, DIRT, [5, 3, 2, 1, 2], 1)
			for x in 16:
				var depth := 2 + (1 if cv.rng.randf() < 0.6 else 0) + (1 if cv.rng.randf() < 0.3 else 0)
				for y in depth:
					cv.px(x, y, C("#f4fbfb").lerp(C("#d8e8ea"), cv.rng.randf() * 0.6))
		"dirt_path_top":
			cv = base_stone(n, ["#947549", "#a58657", "#826540", "#b39562"], [4, 3, 2, 1], 1)
		"dirt_path_side":
			cv = base_stone(n, DIRT, [5, 3, 2, 1, 2], 1)
			for x in 16:
				cv.px(x, 0, Color(0, 0, 0, 0))
				cv.px(x, 1, C("#947549").lerp(C("#a58657"), cv.rng.randf()))
				if cv.rng.randf() < 0.5:
					cv.px(x, 2, C("#826540"))
		"farmland", "farmland_moist":
			var wet := n == "farmland_moist"
			var pal := ["#5a3d25", "#4b3220", "#66482c", "#3d2817"] if wet else ["#8a6341", "#76543a", "#9a7450", "#644630"]
			cv = base_stone(n, pal, [4, 3, 2, 1], 1)
			for y in [1, 5, 9, 13]:
				for x in 16:
					cv.px(x, y, PixelCanvas.shade(C(pal[3]), -0.15))
					cv.px(x, y + 1, PixelCanvas.shade(C(pal[2]), 0.05))
		"mud":
			cv = base_stone(n, ["#3c3837", "#45403e", "#353130", "#4e4846"], [4, 3, 2, 1], 2)
		"packed_mud":
			cv = base_stone(n, ["#8e6b50", "#7f5f46", "#9b785b", "#70523c"], [4, 3, 2, 1], 1)
			cv.speckle(C("#b0916f"), 7)
			cv.speckle(C("#5f4532"), 5)
		"mud_bricks":
			cv = cv_for(n)
			BTCommon.bricks(cv, ["#8b6a50", "#997759", "#7f5f47"], C("#6a4f3b"), 4, 8)
		"clay":
			cv = base_stone(n, ["#a0a6b3", "#959ba8", "#abb1be", "#8a8f9b"], [4, 3, 2, 1], 2)
			cv.speckle(C("#b8bdc9"), 5)
		"gravel":
			cv = base_stone(n, ["#838080", "#6f6b6b", "#9b9797", "#5c5757", "#8b7f78"], [3, 3, 2, 2, 1], 1)
			for k in 10:
				var x := cv.rng.randi_range(0, 14)
				var y := cv.rng.randi_range(0, 14)
				cv.px(x, y, C("#a9a5a5"))
				cv.px(x + 1, y + 1, C("#4d4848"))
		"sand":
			cv = base_stone(n, SAND, [4, 3, 2, 1], 1)
		"red_sand":
			cv = base_stone(n, RED_SAND, [4, 3, 2, 1], 1)
		"suspicious_sand":
			cv = base_stone(n, SAND, [4, 3, 2, 1], 1)
			cv.blobs(TexPalettes.cols(["#a99f6c", "#b5ab77"]), 5, 2, 3)
		"suspicious_gravel":
			cv = base_stone(n, ["#838080", "#6f6b6b", "#9b9797", "#5c5757"], [3, 3, 2, 2], 1)
			cv.blobs(TexPalettes.cols(["#6b5a4c", "#7c6a5a"]), 5, 2, 3)
		"sandstone", "red_sandstone":
			var pal: Array = SAND if n == "sandstone" else RED_SAND
			cv = base_stone(n, pal, [4, 3, 2, 1], 1)
			var light := PixelCanvas.shade(C(pal[2]), 0.05)
			var dark := PixelCanvas.shade(C(pal[3]), -0.08)
			for x in 16:
				for y in 3:
					cv.px(x, y, light.lerp(C(pal[0]), cv.rng.randf() * 0.4))
				cv.px(x, 3, dark)
				cv.px(x, 10, PixelCanvas.shade(C(pal[1]), -0.04))
				cv.px(x, 13, PixelCanvas.shade(C(pal[1]), -0.06))
				cv.px(x, 15, dark)
		"sandstone_top", "red_sandstone_top":
			cv = base_stone(n, SAND if n.begins_with("sandstone") else RED_SAND, [5, 3, 2, 1], 0)
		"sandstone_bottom", "red_sandstone_bottom":
			var pal2: Array = SAND if n.begins_with("sandstone") else RED_SAND
			cv = base_stone(n, pal2, [3, 4, 1, 2], 1)
			cv.speckle(PixelCanvas.shade(C(pal2[3]), -0.1), 10)
		"cut_sandstone", "cut_red_sandstone":
			var pal3: Array = SAND if n == "cut_sandstone" else RED_SAND
			cv = base_stone(n, pal3, [5, 3, 2, 1], 0)
			var d3 := PixelCanvas.shade(C(pal3[3]), -0.12)
			cv.hline(0, 15, 7, d3)
			cv.hline(0, 15, 15, d3)
			cv.hline(0, 15, 0, PixelCanvas.shade(C(pal3[2]), 0.08))
			cv.hline(0, 15, 8, PixelCanvas.shade(C(pal3[2]), 0.08))
		"chiseled_sandstone", "chiseled_red_sandstone":
			var pal4: Array = SAND if n == "chiseled_sandstone" else RED_SAND
			cv = base_stone(n, pal4, [5, 3, 2, 1], 0)
			var d4 := PixelCanvas.shade(C(pal4[3]), -0.2)
			cv.hline(0, 15, 1, d4)
			cv.hline(0, 15, 14, d4)
			# original sun glyph
			cv.disc(8, 8, 3.2, d4)
			cv.disc(8, 8, 2.0, C(pal4[2]))
			for a in 8:
				var ang := a * TAU / 8.0
				cv.px(8 + int(round(cos(ang) * 5.0)), 8 + int(round(sin(ang) * 5.0)), d4)
		"snow":
			cv = base_stone(n, ["#f0fbfb", "#e3f1f2", "#ffffff", "#d5e6e8"], [4, 3, 2, 1], 1)
		"powder_snow":
			cv = base_stone(n, ["#f4fdfd", "#e6f3f4", "#ffffff", "#d0e1e3"], [4, 3, 3, 2], 0)
			cv.speckle(C("#c4d8db"), 8)
		"ice":
			cv = cv_for(n)
			for y in 16:
				for x in 16:
					var c := C("#8fb3f5").lerp(C("#a9c7fb"), cv.rng.randf() * 0.5)
					c.a = 0.7
					cv.px(x, y, c)
			for k in 4:
				var x0 := cv.rng.randi_range(0, 15)
				var y0 := cv.rng.randi_range(0, 15)
				for s in 5:
					var c2 := C("#d8e6ff")
					c2.a = 0.8
					cv.wrap_px(x0 + s, y0 - s, c2)
		"packed_ice":
			cv = base_stone(n, ["#8cb3f8", "#7fa6f0", "#9bbffc", "#739aea"], [4, 3, 2, 1], 2)
			for k in 3:
				var x1 := cv.rng.randi_range(0, 15)
				var y1 := cv.rng.randi_range(0, 15)
				for s in 6:
					cv.wrap_px(x1 + s, y1 + (s >> 1), C("#c3d8ff"))
		"blue_ice":
			cv = base_stone(n, ["#74a8fc", "#6598f2", "#84b5ff", "#5689e8"], [4, 3, 2, 1], 2)
			cv.speckle(C("#a8c9ff"), 6)
		"bedrock":
			cv = base_stone(n, ["#565656", "#3b3b3b", "#6d6d6d", "#232323", "#838383"], [3, 3, 2, 2, 1], 2)
		"obsidian":
			cv = base_stone(n, ["#15121e", "#1f1930", "#0d0b13", "#2b2143", "#3d2e5e"], [4, 3, 3, 1, 1], 1)
		"crying_obsidian":
			cv = base_stone(n, ["#15121e", "#1f1930", "#0d0b13", "#2b2143"], [4, 3, 3, 1], 1)
			cv.blobs(TexPalettes.cols(["#8b2df0", "#b467ff", "#6a19c4"]), 6, 2, 4, 0)
		"moss_block":
			cv = base_stone(n, ["#5a6f2c", "#4c6026", "#6a8134", "#3f521f", "#778f3c"], [4, 3, 2, 2, 1], 1)
		"pale_moss_block":
			cv = base_stone(n, ["#a5a99f", "#959b8f", "#b4b8ad", "#878d81"], [4, 3, 2, 2], 1)
		"calcite":
			cv = base_stone(n, CALCITE, [4, 3, 2, 1], 1)
			cv.speckle(C("#bfc0ba"), 5)
		"dripstone_block":
			cv = cv_for(n)
			var dp := TexPalettes.cols(["#866c5a", "#7a6150", "#957966", "#6c5446", "#a38672"])
			for x in 16:
				var cs := cv.rng.randf_range(-0.06, 0.06)
				for y in 16:
					var c3: Color = dp[cv.rng.randi_range(0, 3)] if cv.rng.randf() < 0.5 else dp[x % 3]
					cv.px(x, y, PixelCanvas.shade(c3, cs))
		"pointed_dripstone", "sulfur_spike":
			cv = cv_for(n)
			var sp_pal := ["#8c715e", "#7a6150", "#a08470"] if n == "pointed_dripstone" else ["#d8c43a", "#b39e22", "#efe07a"]
			for y in 16:
				var half := maxf(0.5, 3.2 - y * 0.2)
				for x in 16:
					if absf(x + 0.5 - 8.0) <= half:
						cv.px(x, y, C(sp_pal[cv.rng.randi_range(0, 2)]))
		"amethyst_block":
			cv = base_stone(n, ["#8561c8", "#6f4fb1", "#a07fe0", "#553a92", "#c7a6ff"], [4, 3, 2, 2, 1], 0)
			for k in 5:
				var x2 := cv.rng.randi_range(0, 13)
				var y2 := cv.rng.randi_range(0, 13)
				cv.px(x2, y2, C("#dcc4ff"))
				cv.px(x2 + 1, y2 + 1, C("#b597f0"))
		"budding_amethyst":
			cv = base_stone(n, ["#8561c8", "#6f4fb1", "#a07fe0", "#553a92"], [4, 3, 2, 2], 0)
			cv.blobs(TexPalettes.cols(["#3d2670", "#4a2f86"]), 6, 2, 3)
		"small_amethyst_bud", "medium_amethyst_bud", "large_amethyst_bud", "amethyst_cluster":
			cv = cv_for(n)
			var hgt: int = {"small_amethyst_bud": 5, "medium_amethyst_bud": 8, "large_amethyst_bud": 11, "amethyst_cluster": 14}[n]
			for sp in [[8, hgt, 2], [4, hgt - 3, 1], [12, hgt - 2, 1]]:
				for y in sp[1]:
					var yy: int = 15 - y
					var wd: int = sp[2] if y < sp[1] - 2 else 0
					for x in range(sp[0] - wd, sp[0] + wd + 1):
						cv.px(x, yy, C("#c9a8ff") if x == sp[0] else C("#8b62d6"))
		"tuff":
			cv = base_stone(n, TUFF, [4, 3, 2, 2, 1], 1)
			cv.speckle(C("#8f9087"), 6)
		"polished_tuff":
			cv = cv_for(n)
			BTCommon.polished(cv, ["#707169", "#676860", "#7b7c74"])
		"tuff_bricks":
			cv = cv_for(n)
			BTCommon.bricks(cv, ["#6f7068", "#7a7b72", "#666760"], C("#4f504a"), 4, 8, 4)
		"chiseled_tuff", "chiseled_tuff_bricks":
			cv = cv_for(n)
			BTCommon.bricks(cv, ["#6f7068", "#7a7b72", "#666760"], C("#4f504a"), 2, 16, 0)
			cv.rect(3, 3, 10, 3, C("#5a5b54"))
			cv.rect(3, 10, 10, 3, C("#5a5b54"))
			cv.rect(4, 4, 8, 1, C("#8a8b82"))
			cv.rect(4, 11, 8, 1, C("#8a8b82"))
		"chiseled_tuff_top", "chiseled_tuff_bricks_top":
			cv = cv_for(n)
			BTCommon.polished(cv, ["#707169", "#676860", "#7b7c74"])
			cv.rect_outline(3, 3, 10, 10, C("#55564f"))
			cv.rect(6, 6, 4, 4, C("#8a8b82"))
		"smooth_basalt":
			cv = base_stone(n, ["#4a4a50", "#424248", "#535359", "#3a3a40"], [4, 3, 2, 1], 2)
		"sculk":
			cv = base_stone(n, ["#0c1d25", "#062f37", "#0f3c47", "#041419"], [4, 3, 2, 2], 1)
			cv.speckle(C("#1fb8c6"), 5)
			cv.speckle(C("#0d6f7a"), 8)
		"sculk_vein":
			cv = cv_for(n)
			for k in 9:
				var x3 := cv.rng.randi_range(0, 15)
				var y3 := cv.rng.randi_range(0, 15)
				for s in cv.rng.randi_range(2, 5):
					cv.wrap_px(x3, y3, C("#0b3842") if cv.rng.randf() < 0.7 else C("#29dfee"))
					x3 += cv.rng.randi_range(-1, 1)
					y3 += cv.rng.randi_range(-1, 1)
		"sculk_catalyst_top", "sculk_catalyst_side", "sculk_catalyst_bottom", "sculk_sensor_top", "sculk_sensor_side",\
				"sculk_sensor_bottom", "sculk_shrieker_top", "sculk_shrieker_side", "sculk_shrieker_bottom":
			cv = _sculk_part(n)
		"cobweb":
			cv = cv_for(n)
			var wc := Color(0.93, 0.93, 0.95, 0.9)
			for a in 8:
				var ang := a * TAU / 8.0 + 0.2
				for r in 9:
					cv.px(8 + int(round(cos(ang) * r)), 8 + int(round(sin(ang) * r)), wc)
			for r in [3.0, 5.5]:
				for a2 in 32:
					var ang2 := a2 * TAU / 32.0
					cv.px(8 + int(round(cos(ang2) * r)), 8 + int(round(sin(ang2) * r)), wc)
		# ---------------- stones
		"stone", "infested_stone":
			cv = base_stone("stone", STONE, [5, 3, 3, 2, 1], 2)
		"cobblestone":
			cv = cv_for(n)
			BTCommon.cobble(cv, ["#8a8a8a", "#7a7a7a", "#9a9a9a", "#6f6f6f", "#838383"], C("#4c4c4c"), 10)
		"mossy_cobblestone":
			cv = cv_for("cobblestone")
			BTCommon.cobble(cv, ["#8a8a8a", "#7a7a7a", "#9a9a9a", "#6f6f6f", "#838383"], C("#4c4c4c"), 10)
			_moss_overlay(cv, 0.4)
		"cobbled_deepslate":
			cv = cv_for(n)
			BTCommon.cobble(cv, ["#55555b", "#4b4b51", "#606066", "#434349"], C("#2b2b30"), 11)
		"smooth_stone":
			cv = base_stone(n, ["#9f9f9f", "#999999", "#a6a6a6"], [4, 3, 2], 0)
			cv.rect_outline(0, 0, 16, 16, C("#8a8a8a"))
		"smooth_stone_slab_side":
			cv = base_stone(n, ["#9f9f9f", "#999999", "#a6a6a6"], [4, 3, 2], 0)
			cv.rect_outline(0, 0, 16, 8, C("#8a8a8a"))
			cv.rect_outline(0, 8, 16, 8, C("#8a8a8a"))
		"stone_bricks":
			cv = cv_for(n)
			BTCommon.big_bricks(cv, ["#7b7b7b", "#767676", "#828282", "#727272"], C("#595959"))
		"mossy_stone_bricks":
			cv = cv_for("stone_bricks")
			BTCommon.big_bricks(cv, ["#7b7b7b", "#767676", "#828282", "#727272"], C("#595959"))
			_moss_overlay(cv, 0.3)
		"cracked_stone_bricks":
			cv = cv_for("stone_bricks")
			BTCommon.big_bricks(cv, ["#7b7b7b", "#767676", "#828282", "#727272"], C("#595959"))
			_cracks(cv, C("#4a4a4a"), 3)
		"chiseled_stone_bricks":
			cv = cv_for(n)
			BTCommon.chiseled(cv, ["#7b7b7b", "#767676", "#828282"], C("#555555"))
		"granite":
			cv = base_stone(n, ["#9a6a55", "#8b5e4a", "#a97762", "#7d5240", "#b8876f"], [4, 3, 3, 2, 1], 1)
			cv.speckle(C("#c89c85"), 5)
		"polished_granite":
			cv = cv_for(n)
			BTCommon.polished(cv, ["#9c6b56", "#925f4b", "#a67662"])
		"diorite":
			cv = base_stone(n, ["#bdbdbd", "#a9a9a9", "#d2d2d2", "#8e8e8e", "#e2e2e2"], [4, 3, 3, 2, 1], 0)
		"polished_diorite":
			cv = cv_for(n)
			BTCommon.polished(cv, ["#c1c1c3", "#b5b5b7", "#cdcdcf"])
		"andesite":
			cv = base_stone(n, ["#888889", "#7c7c7d", "#959596", "#6e6e6f", "#a0a0a1"], [4, 3, 3, 2, 1], 0)
		"polished_andesite":
			cv = cv_for(n)
			BTCommon.polished(cv, ["#848685", "#7b7d7c", "#8e908f"])
		"deepslate":
			cv = cv_for(n)
			var dp2 := TexPalettes.cols(DEEPSLATE)
			for y in 16:
				var rs := cv.rng.randf_range(-0.05, 0.05)
				for x in 16:
					var c4: Color = dp2[cv.rng.randi_range(0, 3)]
					cv.px(x, y, PixelCanvas.shade(c4, rs))
			for k in 6:
				var yy2 := cv.rng.randi_range(0, 15)
				var xx2 := cv.rng.randi_range(0, 10)
				cv.hline(xx2, xx2 + cv.rng.randi_range(2, 5), yy2, dp2[3])
		"deepslate_top":
			cv = cv_for(n)
			BTCommon.tiles(cv, ["#57575d", "#4e4e54", "#5f5f65"], C("#3a3a40"), 8)
			cv.rect(3, 3, 2, 2, C("#48484e"))
			cv.rect(11, 11, 2, 2, C("#48484e"))
		"polished_deepslate":
			cv = cv_for(n)
			BTCommon.polished(cv, ["#4d4d52", "#47474c", "#56565b"])
		"deepslate_bricks", "cracked_deepslate_bricks":
			cv = cv_for("deepslate_bricks")
			BTCommon.bricks(cv, ["#515157", "#4a4a50", "#58585e"], C("#2e2e33"), 4, 8, 4)
			if n.begins_with("cracked"):
				_cracks(cv, C("#28282c"), 4)
		"deepslate_tiles", "cracked_deepslate_tiles":
			cv = cv_for("deepslate_tiles")
			BTCommon.tiles(cv, ["#3f3f44", "#39393e", "#46464b"], C("#242428"), 4)
			if n.begins_with("cracked"):
				_cracks(cv, C("#1f1f23"), 4)
		"chiseled_deepslate":
			cv = cv_for(n)
			BTCommon.chiseled(cv, ["#48484d", "#424247", "#505055"], C("#2a2a2e"))
		"reinforced_deepslate_top", "reinforced_deepslate_bottom", "reinforced_deepslate_side":
			cv = cv_for(n)
			BTCommon.tiles(cv, ["#4a4a50", "#43434a", "#525258"], C("#2e2e33"), 8)
			var metal := C("#6d7b74")
			cv.rect_outline(0, 0, 16, 16, metal)
			cv.rect_outline(1, 1, 14, 14, C("#3f4a45"))
			if n.ends_with("side"):
				cv.rect(6, 0, 4, 16, C("#56625c"))
				cv.vline(7, 0, 15, C("#8a9a92"))
			else:
				cv.rect(5, 5, 6, 6, C("#1b3a3e"))
				cv.rect(7, 7, 2, 2, C("#2fb8c0"))
		"bricks":
			cv = cv_for(n)
			BTCommon.bricks(cv, ["#97493a", "#a3533f", "#8a4234", "#b0604b"], C("#a09389"), 4, 8, 4)
		# ---------------- ores
		"coal_ore", "iron_ore", "copper_ore", "gold_ore", "redstone_ore", "emerald_ore", "lapis_ore", "diamond_ore":
			cv = base_stone(n, STONE, [5, 3, 3, 2, 1], 2)
			var key := n.replace("_ore", "")
			BTCommon.ore(cv, TexPalettes.ORE[key], 3 if key == "emerald" else 5)
		"deepslate_coal_ore", "deepslate_iron_ore", "deepslate_copper_ore", "deepslate_gold_ore", "deepslate_redstone_ore",\
				"deepslate_emerald_ore", "deepslate_lapis_ore", "deepslate_diamond_ore":
			var cvd := cv_for(n)
			cvd.img = (gen("deepslate")[0] as Image).duplicate()
			cvd.reseed(n)
			var key2 := n.replace("deepslate_", "").replace("_ore", "")
			BTCommon.ore(cvd, TexPalettes.ORE[key2], 3 if key2 == "emerald" else 5)
			cv = cvd
		"nether_gold_ore":
			cv = base_stone(n, NETHERRACK, [4, 3, 2, 2, 1], 1)
			BTCommon.ore(cv, TexPalettes.ORE["nether_gold"], 7)
		"nether_quartz_ore":
			cv = base_stone(n, NETHERRACK, [4, 3, 2, 2, 1], 1)
			BTCommon.ore(cv, TexPalettes.ORE["quartz"], 6)
		"ancient_debris_side":
			cv = cv_for(n)
			for y in 16:
				var band := (y % 4) == 0
				for x in 16:
					var c5 := C("#5e4640").lerp(C("#4a3530"), cv.rng.randf())
					if band:
						c5 = PixelCanvas.shade(c5, -0.2)
					cv.px(x, y, c5)
			cv.blobs(TexPalettes.cols(["#7a5e55", "#8c6d63"]), 5, 2, 3)
		"ancient_debris_top":
			cv = cv_for(n)
			for y in 16:
				for x in 16:
					var d := maxi(absi(x * 2 - 15), absi(y * 2 - 15)) >> 1
					var c6 := C("#5e4640") if d % 3 != 0 else C("#3e2c28")
					cv.px(x, y, PixelCanvas.shade(c6, cv.rng.randf_range(-0.06, 0.06)))
		# ---------------- minerals
		"coal_block":
			cv = base_stone(n, ["#1f1f1f", "#282828", "#151515", "#333333"], [4, 3, 2, 1], 1)
			_facets(cv, C("#3a3a3a"), C("#0e0e0e"))
		"iron_block":
			cv = _metal(n, ["#dcdcdc", "#d0d0d0", "#e8e8e8"], C("#a8a8a8"), C("#f5f5f5"))
		"gold_block":
			cv = _metal(n, ["#f6d53c", "#ebc52c", "#fbe36c"], C("#c49b17"), C("#fff6a8"))
		"diamond_block":
			cv = _metal(n, ["#63e7e1", "#4fd4ce", "#8cf2ee"], C("#2aa9a3"), C("#d6fffd"))
		"emerald_block":
			cv = _metal(n, ["#44d670", "#31b75b", "#6fe792"], C("#1b8c40"), C("#b8ffcd"))
		"lapis_block":
			cv = base_stone(n, ["#1f4a9e", "#1a3f88", "#2d5cbd", "#12306d", "#4a78d0"], [4, 3, 2, 2, 1], 1)
		"redstone_block":
			cv = base_stone(n, ["#b31a0e", "#9b150a", "#cc271a", "#7b0f06"], [4, 3, 2, 1], 1)
			_facets(cv, C("#e8483a"), C("#5e0a04"))
		"netherite_block":
			cv = _metal(n, ["#433d40", "#3b3538", "#4c4649"], C("#2a2527"), C("#5d5659"))
		"raw_iron_block":
			cv = base_stone(n, ["#a6876b", "#957660", "#b99a7e", "#7e6150", "#cfb399"], [4, 3, 2, 2, 1], 2)
		"raw_copper_block":
			cv = base_stone(n, ["#9a5a40", "#8a4f38", "#b06a4c", "#6f3f2c", "#c98a64"], [4, 3, 2, 2, 1], 2)
		"raw_gold_block":
			cv = base_stone(n, ["#dca533", "#c99229", "#edbd48", "#a87a1f", "#f7d97a"], [4, 3, 2, 2, 1], 2)
		"quartz_block_top", "quartz_block_bottom", "quartz_block_side", "quartz_bricks":
			cv = base_stone(n, ["#ebe5de", "#e1dad1", "#f2eee9", "#d6cec4"], [5, 3, 2, 1], 0)
			if n == "quartz_block_side" or n == "quartz_block_top":
				cv.rect_outline(0, 0, 16, 16, C("#d9d1c7"))
			if n == "quartz_bricks":
				BTCommon.bricks(cv, ["#ebe5de", "#e3ddd5", "#f1ece6"], C("#cfc5ba"), 4, 8)
		"chiseled_quartz_block", "chiseled_quartz_block_top":
			cv = base_stone(n, ["#ebe5de", "#e1dad1", "#f2eee9"], [5, 3, 2], 0)
			cv.rect_outline(1, 1, 14, 14, C("#cfc5ba"))
			cv.rect_outline(4, 4, 8, 8, C("#cfc5ba"))
			if n == "chiseled_quartz_block":
				cv.hline(1, 14, 7, C("#d8cfc5"))
		"quartz_pillar", "quartz_pillar_top":
			cv = base_stone(n, ["#ebe5de", "#e1dad1", "#f2eee9"], [5, 3, 2], 0)
			if n == "quartz_pillar":
				for x in [1, 5, 10, 14]:
					cv.vline(x, 0, 15, C("#d4cbc0"))
			else:
				cv.rect_outline(1, 1, 14, 14, C("#d4cbc0"))
				cv.disc(8, 8, 3.5, C("#e2dbd2"))
		"prismarine":
			cv = base_stone(n, ["#63a795", "#57968a", "#71b6a3", "#4a8679", "#8ec6b6", "#5c8f9a"], [4, 3, 2, 2, 1, 1], 2)
		"prismarine_bricks":
			cv = cv_for(n)
			BTCommon.bricks(cv, ["#63a996", "#6db5a1", "#5a9c8a"], C("#3f7466"), 4, 8, 4)
		"dark_prismarine":
			cv = cv_for(n)
			BTCommon.tiles(cv, ["#335a4e", "#2d5045", "#3a6456"], C("#1f3a32"), 8)
		"sea_lantern":
			cv = base_stone(n, ["#d8e8e2", "#c5dbd4", "#ecf6f3"], [4, 3, 2], 0)
			cv.rect_outline(0, 0, 16, 16, C("#9fbfb8"))
			cv.rect_outline(3, 3, 10, 10, C("#aac9c2"))
			cv.rect(6, 6, 4, 4, C("#ffffff"))
		# ---------------- sulfur caves (26.2)
		"sulfur":
			cv = base_stone(n, SULFUR, [4, 3, 2, 2, 1], 2)
		"potent_sulfur":
			cv = base_stone(n, SULFUR, [4, 3, 2, 2, 1], 2)
			_cracks(cv, C("#fff4a0"), 4)
			cv.speckle(C("#9fe05a"), 6)
		"cinnabar":
			cv = base_stone(n, CINNABAR, [4, 3, 2, 2, 1], 2)
		"polished_sulfur":
			cv = cv_for(n)
			BTCommon.polished(cv, ["#d6c23c", "#cbb632", "#e0cf52"])
		"polished_cinnabar":
			cv = cv_for(n)
			BTCommon.polished(cv, ["#b33b3c", "#a73435", "#c14a49"])
		"sulfur_bricks":
			cv = cv_for(n)
			BTCommon.bricks(cv, ["#d4bf38", "#dfcc4d", "#c7b02c"], C("#8e7d19"), 4, 8, 4)
		"cinnabar_bricks":
			cv = cv_for(n)
			BTCommon.bricks(cv, ["#b2393a", "#c04746", "#a33133"], C("#6d1d1f"), 4, 8, 4)
		"barrier":
			cv = cv_for(n)
			var rc := Color(0.9, 0.1, 0.1, 1.0)
			for i in 16:
				cv.px(i, i, rc)
				cv.px(15 - i, i, rc)
			cv.rect_outline(0, 0, 16, 16, rc)
		_:
			if n.begins_with("destroy_stage_"):
				cv = _destroy(n, int(n.substr(14)))
	if cv == null:
		return []
	return [cv.img]


static func _side_with_top(n: String, base: Array, top: Array, depth: int) -> PixelCanvas:
	var cv := base_stone(n, base, [5, 3, 2, 1, 2], 1)
	var tc := TexPalettes.cols(top)
	for x in 16:
		var d := depth + (1 if cv.rng.randf() < 0.45 else 0)
		for y in d:
			cv.px(x, y, tc[cv.rng.randi_range(0, tc.size() - 1)])
	return cv


static func _moss_overlay(cv: PixelCanvas, amount: float) -> void:
	var mc := TexPalettes.cols(["#5f7a33", "#4f692a", "#6f8c3c"])
	for y in 16:
		for x in 16:
			var v := sin(x * 0.9 + cv.rng.randf()) + cos(y * 0.7) + cv.rng.randf_range(-0.8, 0.8)
			if v > 1.4 - amount * 2.0:
				cv.px(x, y, mc[cv.rng.randi_range(0, 2)])


static func _cracks(cv: PixelCanvas, col: Color, count: int) -> void:
	for k in count:
		var x := cv.rng.randi_range(1, 14)
		var y := cv.rng.randi_range(1, 14)
		for s in cv.rng.randi_range(3, 6):
			cv.px(x, y, col)
			x += cv.rng.randi_range(-1, 1)
			y += cv.rng.randi_range(0, 1)


static func _facets(cv: PixelCanvas, hi: Color, lo: Color) -> void:
	for k in 6:
		var x := cv.rng.randi_range(0, 13)
		var y := cv.rng.randi_range(0, 13)
		cv.px(x, y, hi)
		cv.px(x + 1, y, hi)
		cv.px(x + 1, y + 1, lo)


static func _metal(n: String, pal: Array, edge: Color, hi: Color) -> PixelCanvas:
	var cv := base_stone(n, pal, [5, 3, 2], 0)
	for i in 16:
		cv.px(i, 0, hi)
		cv.px(0, i, hi)
		cv.px(i, 15, edge)
		cv.px(15, i, edge)
	cv.px(2, 2, edge)
	cv.px(13, 2, edge)
	cv.px(2, 13, edge)
	cv.px(13, 13, edge)
	for i in range(3, 13):
		cv.px(i, 3, PixelCanvas.shade(TexPalettes.c(pal[0]), 0.06))
	return cv


static func _sculk_part(n: String) -> PixelCanvas:
	var cv := base_stone(n, ["#0c1d25", "#062f37", "#0f3c47", "#041419"], [4, 3, 2, 2], 1)
	var bone := TexPalettes.cols(["#d8d3bf", "#c4bea7", "#e6e2d2"])
	if n.ends_with("_top"):
		if n.begins_with("sculk_catalyst"):
			cv.rect(3, 3, 10, 10, bone[0])
			cv.rect(5, 5, 6, 6, C("#1fb8c6"))
		elif n.begins_with("sculk_sensor"):
			cv.rect(2, 2, 12, 12, C("#0e4b55"))
			cv.rect(5, 5, 6, 6, C("#29dfee"))
		else:
			cv.rect(2, 2, 12, 12, bone[1])
			cv.rect(4, 4, 8, 8, C("#0a2a30"))
			cv.rect(6, 6, 4, 4, C("#1b8f99"))
	elif n.ends_with("_side"):
		if n.begins_with("sculk_catalyst"):
			for x in 16:
				cv.px(x, 0, bone[0])
				cv.px(x, 1, bone[1])
			cv.rect(6, 2, 4, 5, bone[2])
		else:
			for x in 16:
				for y in range(0, 8):
					cv.px(x, y, Color(0, 0, 0, 0))
			for x in range(3, 13, 3):
				cv.vline(x, 2, 7, C("#29dfee") if n.begins_with("sculk_sensor") else bone[0])
	return cv


static func _destroy(n: String, stage: int) -> PixelCanvas:
	var cv := PixelCanvas.new(16, 16, "destroy")
	cv.fill(Color(0, 0, 0, 0))
	var rng := RandomNumberGenerator.new()
	rng.seed = 424242
	var cracks := 2 + stage * 2
	for k in cracks:
		var x := 8 + rng.randi_range(-2, 2)
		var y := 8 + rng.randi_range(-2, 2)
		var len := 2 + stage + rng.randi_range(0, 2)
		var dx := rng.randi_range(-1, 1)
		var dy := rng.randi_range(-1, 1)
		if dx == 0 and dy == 0:
			dx = 1
		for s in len:
			cv.px(x, y, Color(0.08, 0.08, 0.08, 0.85))
			x += dx if rng.randf() < 0.75 else rng.randi_range(-1, 1)
			y += dy if rng.randf() < 0.75 else rng.randi_range(-1, 1)
	return cv
