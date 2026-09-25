class_name RecipeDB
extends RefCounted
## Recipe registry generated from families + explicit tables: shaped / shapeless crafting (with
## #tags and mirroring), furnace / blast furnace / smoker / campfire smelting, stonecutting and
## smithing. Recipes whose items do not exist are skipped (and counted for validation).

static var shaped: Array = []        # {w, h, grid: Array[String], out, count}
static var shapeless: Array = []     # {ing: Array[String], out, count}
static var smelting: Dictionary = {} # input -> {out, xp, kinds}
static var stonecut: Dictionary = {} # input -> Array[[out, count]]
static var smithing: Array = []      # [template, base, addition, result]
static var tags: Dictionary = {}     # "#tag" -> Dictionary(name -> true)
static var skipped := 0
static var skipped_names: PackedStringArray = PackedStringArray()
static var inited := false
static var _by_output: Dictionary = {}


static func init() -> void:
	if inited:
		return
	_build_tags()
	_woods()
	_stone_families()
	_minerals()
	_colored()
	_tools_armor()
	_functional()
	_redstone()
	_items()
	_food()
	_copper()
	_nether_end()
	_smelting()
	_stonecutting()
	_smithing()
	inited = true
	for r in shaped:
		_by_output.get_or_add(r.out, []).append(r)
	for r in shapeless:
		_by_output.get_or_add(r.out, []).append(r)


# ------------------------------------------------------------------------------------------------
static func _build_tags() -> void:
	var t := {}
	for d in BlockDB.defs:
		var bd: BlockDef = d
		for tag in bd.tags:
			t.get_or_add("#" + tag, {})[bd.name] = true
		if bd.name.ends_with("_wool"):
			t.get_or_add("#wool", {})[bd.name] = true
		if bd.name.ends_with("_slab") and bd.flammable:
			t.get_or_add("#wooden_slabs", {})[bd.name] = true
		if bd.name.ends_with("_carpet") and bd.name != "moss_carpet" and bd.name != "pale_moss_carpet":
			t.get_or_add("#wool_carpets", {})[bd.name] = true
		if bd.name.ends_with("_terracotta") and not bd.name.ends_with("glazed_terracotta"):
			t.get_or_add("#terracotta", {})[bd.name] = true
	t["#stone_tool_materials"] = {"cobblestone": true, "blackstone": true, "cobbled_deepslate": true}
	t["#stone_crafting_materials"] = t["#stone_tool_materials"]
	t["#coals"] = {"coal": true, "charcoal": true}
	t["#soul_fire_base"] = {"soul_sand": true, "soul_soil": true}
	t["#wooden_tool_materials"] = t.get("#planks", {})
	t["#logs_that_burn"] = {}
	for n in t.get("#logs", {}):
		if not (String(n).contains("crimson") or String(n).contains("warped")):
			t["#logs_that_burn"][n] = true
	t["#copper_ingot_like"] = {"copper_ingot": true}
	t["#eggs"] = {"egg": true}
	t["#stone_bricks_any"] = {"stone_bricks": true, "mossy_stone_bricks": true, "cracked_stone_bricks": true, "chiseled_stone_bricks": true}
	tags = t


static func _exists(n: String) -> bool:
	if n == "":
		return true
	if n.begins_with("#"):
		return tags.has(n) and not (tags[n] as Dictionary).is_empty()
	return ItemDB.has(n)


## Shaped recipe. rows: Array of strings, keys: char -> ingredient ("#tag" or item name).
static func s(out: String, count: int, rows: Array, keys: Dictionary) -> void:
	if not _exists(out):
		skipped += 1
		skipped_names.append(out)
		return
	var h := rows.size()
	var w := 0
	for r in rows:
		w = maxi(w, String(r).length())
	var grid: Array[String] = []
	for r in rows:
		var rs: String = r
		for i in w:
			var ch := rs.substr(i, 1) if i < rs.length() else " "
			if ch == " ":
				grid.append("")
			else:
				var ing: String = keys.get(ch, "")
				if not _exists(ing):
					skipped += 1
					skipped_names.append(out + " <- " + ing)
					return
				grid.append(ing)
	shaped.append({"w": w, "h": h, "grid": grid, "out": out, "count": count})


## Shapeless recipe.
static func l(out: String, count: int, ing: Array) -> void:
	if not _exists(out):
		skipped += 1
		skipped_names.append(out)
		return
	for i in ing:
		if not _exists(String(i)):
			skipped += 1
			skipped_names.append(out + " <- " + String(i))
			return
	shapeless.append({"ing": ing, "out": out, "count": count})


static func smelt(input: String, out: String, xp: float, kinds := ["furnace"]) -> void:
	if not ItemDB.has(input) or not ItemDB.has(out):
		skipped += 1
		skipped_names.append("smelt " + input + " -> " + out)
		return
	smelting[input] = {"out": out, "xp": xp, "kinds": kinds}


static func cut(input: String, out: String, count := 1) -> void:
	if not ItemDB.has(input) or not ItemDB.has(out) or input == out:
		return
	var lst: Array = stonecut.get_or_add(input, [])
	for e in lst:
		if e[0] == out:
			return
	lst.append([out, count])


# ------------------------------------------------------------------------------------------------
static func _woods() -> void:
	for w in BlockCatalog.WOODS:
		var n: String = w.n
		var planks := n + "_planks"
		var nether: bool = w.get("nether", false)
		var bamboo: bool = w.get("bamboo", false)
		if bamboo:
			l("bamboo_planks", 2, ["bamboo_block"])
			l("bamboo_planks", 2, ["stripped_bamboo_block"])
			s("bamboo_block", 1, ["###", "###", "###"], {"#": "bamboo"})
			s("bamboo_mosaic", 1, ["#", "#"], {"#": "bamboo_slab"})
			s("bamboo_mosaic_stairs", 4, ["#  ", "## ", "###"], {"#": "bamboo_mosaic"})
			s("bamboo_mosaic_slab", 6, ["###"], {"#": "bamboo_mosaic"})
			s("bamboo_raft", 1, ["# #", "###"], {"#": "bamboo_planks"})
			l("bamboo_chest_raft", 1, ["bamboo_raft", "chest"])
		else:
			var stem := "stem" if nether else "log"
			var woodn := "hyphae" if nether else "wood"
			l(planks, 4, ["#" + n + "_logs"])
			s(n + "_" + woodn, 3, ["##", "##"], {"#": n + "_" + stem})
			s("stripped_" + n + "_" + woodn, 3, ["##", "##"], {"#": "stripped_" + n + "_" + stem})
			if not nether:
				s(n + "_boat", 1, ["# #", "###"], {"#": planks})
				l(n + "_chest_boat", 1, [n + "_boat", "chest"])
			s(n + "_shelf", 6, ["###", "   ", "###"], {"#": "stripped_" + n + "_" + stem})
		s(n + "_stairs", 4, ["#  ", "## ", "###"], {"#": planks})
		s(n + "_slab", 6, ["###"], {"#": planks})
		s(n + "_fence", 3, ["#S#", "#S#"], {"#": planks, "S": "stick"})
		s(n + "_fence_gate", 1, ["S#S", "S#S"], {"#": planks, "S": "stick"})
		s(n + "_door", 3, ["##", "##", "##"], {"#": planks})
		s(n + "_trapdoor", 2, ["###", "###"], {"#": planks})
		l(n + "_button", 1, [planks])
		s(n + "_pressure_plate", 1, ["##"], {"#": planks})
		s(n + "_sign", 3, ["###", "###", " S "], {"#": planks, "S": "stick"})
	s("stick", 4, ["#", "#"], {"#": "#planks"})
	s("stick", 1, ["#", "#"], {"#": "bamboo"})
	s("crafting_table", 1, ["##", "##"], {"#": "#planks"})
	s("chest", 1, ["###", "# #", "###"], {"#": "#planks"})
	s("barrel", 1, ["PSP", "P P", "PSP"], {"P": "#planks", "S": "#wooden_slabs"})
	s("bowl", 4, ["# #", " # "], {"#": "#planks"})
	s("bookshelf", 1, ["###", "BBB", "###"], {"#": "#planks", "B": "book"})
	s("chiseled_bookshelf", 1, ["###", "SSS", "###"], {"#": "#planks", "S": "#wooden_slabs"})
	s("ladder", 3, ["S S", "SSS", "S S"], {"S": "stick"})
	s("composter", 1, ["# #", "# #", "###"], {"#": "#wooden_slabs"})
	s("lectern", 1, ["SSS", " B ", " S "], {"S": "#wooden_slabs", "B": "bookshelf"})
	s("cartography_table", 1, ["PP", "##", "##"], {"P": "paper", "#": "#planks"})
	s("fletching_table", 1, ["FF", "##", "##"], {"F": "flint", "#": "#planks"})
	s("smithing_table", 1, ["II", "##", "##"], {"I": "iron_ingot", "#": "#planks"})
	s("loom", 1, ["SS", "##"], {"S": "string", "#": "#planks"})
	s("jukebox", 1, ["###", "#D#", "###"], {"#": "#planks", "D": "diamond"})
	s("note_block", 1, ["###", "#R#", "###"], {"#": "#planks", "R": "redstone"})
	s("beehive", 1, ["###", "HHH", "###"], {"#": "#planks", "H": "honeycomb"})
	s("scaffolding", 6, ["BSB", "B B", "B B"], {"B": "bamboo", "S": "string"})
	s("campfire", 1, [" S ", "SCS", "LLL"], {"S": "stick", "C": "#coals", "L": "#logs"})
	s("soul_campfire", 1, [" S ", "SCS", "LLL"], {"S": "stick", "C": "#soul_fire_base", "L": "#logs"})
	s("grindstone", 1, ["S#S", "P P"], {"S": "stick", "#": "stone_slab", "P": "#planks"})
	for c in BlockCatalog.DYES:
		s(c + "_bed", 1, ["WWW", "PPP"], {"W": c + "_wool", "P": "#planks"})


static func _stone_families() -> void:
	for f in BlockCatalog.families:
		var prefix: String = f[0]
		var base: String = f[1]
		if prefix.begins_with("sulfur") or prefix.begins_with("cinnabar"):
			pass
		if f[2]:
			s(prefix + "_stairs", 4, ["#  ", "## ", "###"], {"#": base})
		if f[3]:
			s(prefix + "_slab", 6, ["###"], {"#": base})
		if f[4]:
			s(prefix + "_wall", 6, ["###", "###"], {"#": base})
	s("stone_bricks", 4, ["##", "##"], {"#": "stone"})
	l("mossy_stone_bricks", 1, ["stone_bricks", "vine"])
	l("mossy_stone_bricks", 1, ["stone_bricks", "moss_block"])
	l("mossy_cobblestone", 1, ["cobblestone", "vine"])
	l("mossy_cobblestone", 1, ["cobblestone", "moss_block"])
	s("chiseled_stone_bricks", 1, ["#", "#"], {"#": "stone_brick_slab"})
	s("polished_granite", 4, ["##", "##"], {"#": "granite"})
	s("polished_diorite", 4, ["##", "##"], {"#": "diorite"})
	s("polished_andesite", 4, ["##", "##"], {"#": "andesite"})
	l("granite", 1, ["diorite", "quartz"])
	s("diorite", 2, ["CQ", "QC"], {"C": "cobblestone", "Q": "quartz"})
	l("andesite", 2, ["diorite", "cobblestone"])
	s("polished_deepslate", 4, ["##", "##"], {"#": "cobbled_deepslate"})
	s("deepslate_bricks", 4, ["##", "##"], {"#": "polished_deepslate"})
	s("deepslate_tiles", 4, ["##", "##"], {"#": "deepslate_bricks"})
	s("chiseled_deepslate", 1, ["#", "#"], {"#": "cobbled_deepslate_slab"})
	s("polished_tuff", 4, ["##", "##"], {"#": "tuff"})
	s("tuff_bricks", 4, ["##", "##"], {"#": "polished_tuff"})
	s("chiseled_tuff", 1, ["#", "#"], {"#": "tuff_slab"})
	s("chiseled_tuff_bricks", 1, ["#", "#"], {"#": "tuff_brick_slab"})
	s("bricks", 1, ["##", "##"], {"#": "brick"})
	l("packed_mud", 1, ["mud", "wheat"])
	s("mud_bricks", 4, ["##", "##"], {"#": "packed_mud"})
	s("sandstone", 1, ["##", "##"], {"#": "sand"})
	s("red_sandstone", 1, ["##", "##"], {"#": "red_sand"})
	s("cut_sandstone", 4, ["##", "##"], {"#": "sandstone"})
	s("cut_red_sandstone", 4, ["##", "##"], {"#": "red_sandstone"})
	s("chiseled_sandstone", 1, ["#", "#"], {"#": "sandstone_slab"})
	s("chiseled_red_sandstone", 1, ["#", "#"], {"#": "red_sandstone_slab"})
	s("quartz_block", 1, ["##", "##"], {"#": "quartz"})
	s("quartz_pillar", 2, ["#", "#"], {"#": "quartz_block"})
	s("chiseled_quartz_block", 1, ["#", "#"], {"#": "quartz_slab"})
	s("quartz_bricks", 4, ["##", "##"], {"#": "quartz_block"})
	s("prismarine", 1, ["##", "##"], {"#": "prismarine_shard"})
	s("prismarine_bricks", 1, ["###", "###", "###"], {"#": "prismarine_shard"})
	s("dark_prismarine", 1, ["###", "#I#", "###"], {"#": "prismarine_shard", "I": "black_dye"})
	s("sea_lantern", 1, ["SCS", "CCC", "SCS"], {"S": "prismarine_shard", "C": "prismarine_crystals"})
	s("resin_block", 1, ["###", "###", "###"], {"#": "resin_clump"})
	l("resin_clump", 9, ["resin_block"])
	s("resin_bricks", 1, ["##", "##"], {"#": "resin_brick"})
	s("chiseled_resin_bricks", 1, ["#", "#"], {"#": "resin_brick_slab"})
	s("polished_sulfur", 4, ["##", "##"], {"#": "sulfur"})
	s("sulfur_bricks", 4, ["##", "##"], {"#": "polished_sulfur"})
	s("polished_cinnabar", 4, ["##", "##"], {"#": "cinnabar"})
	s("cinnabar_bricks", 4, ["##", "##"], {"#": "polished_cinnabar"})
	s("smooth_stone_slab", 6, ["###"], {"#": "smooth_stone"})
	s("stone_slab", 6, ["###"], {"#": "stone"})
	s("stone_stairs", 4, ["#  ", "## ", "###"], {"#": "stone"})


static func _minerals() -> void:
	var pairs := [["coal_block", "coal"], ["iron_block", "iron_ingot"], ["gold_block", "gold_ingot"], ["diamond_block", "diamond"],
		["emerald_block", "emerald"], ["lapis_block", "lapis_lazuli"], ["redstone_block", "redstone"], ["netherite_block", "netherite_ingot"],
		["raw_iron_block", "raw_iron"], ["raw_copper_block", "raw_copper"], ["raw_gold_block", "raw_gold"], ["copper_block", "copper_ingot"],
		["slime_block", "slime_ball"], ["hay_block", "wheat"], ["dried_kelp_block", "dried_kelp"], ["melon", "melon_slice"]]
	for p in pairs:
		s(p[0], 1, ["###", "###", "###"], {"#": p[1]})
		if p[0] != "melon":
			l(p[1], 9, [p[0]])
	s("bone_block", 1, ["###", "###", "###"], {"#": "bone_meal"})
	l("bone_meal", 9, ["bone_block"])
	l("bone_meal", 3, ["bone"])
	s("iron_ingot", 1, ["###", "###", "###"], {"#": "iron_nugget"})
	l("iron_nugget", 9, ["iron_ingot"])
	s("gold_ingot", 1, ["###", "###", "###"], {"#": "gold_nugget"})
	l("gold_nugget", 9, ["gold_ingot"])
	s("copper_ingot", 1, ["###", "###", "###"], {"#": "copper_nugget"})
	l("copper_nugget", 9, ["copper_ingot"])
	s("amethyst_block", 1, ["##", "##"], {"#": "amethyst_shard"})
	s("honey_block", 1, ["##", "##"], {"#": "honey_bottle"})
	s("honeycomb_block", 1, ["##", "##"], {"#": "honeycomb"})
	s("snow_block", 1, ["##", "##"], {"#": "snowball"})
	s("snow", 6, ["###"], {"#": "snow_block"})
	s("clay", 1, ["##", "##"], {"#": "clay_ball"})
	s("glowstone", 1, ["##", "##"], {"#": "glowstone_dust"})
	s("packed_ice", 1, ["###", "###", "###"], {"#": "ice"})
	s("blue_ice", 1, ["###", "###", "###"], {"#": "packed_ice"})
	s("netherite_ingot", 1, ["SSS", "SGG", "GG "], {"S": "netherite_scrap", "G": "gold_ingot"})
	l("netherite_ingot", 1, ["netherite_scrap", "netherite_scrap", "netherite_scrap", "netherite_scrap", "gold_ingot", "gold_ingot", "gold_ingot", "gold_ingot"])
	s("moss_carpet", 3, ["##"], {"#": "moss_block"})
	s("pale_moss_carpet", 3, ["##"], {"#": "pale_moss_block"})
	s("tinted_glass", 2, [" A ", "AGA", " A "], {"A": "amethyst_shard", "G": "glass"})
	s("lodestone", 1, ["SSS", "SNS", "SSS"], {"S": "chiseled_stone_bricks", "N": "netherite_ingot"})


static func _colored() -> void:
	# dye sources
	var flowers := {"white_dye": ["lily_of_the_valley", "bone_meal"], "red_dye": ["poppy", "red_tulip", "beetroot", "rose_bush"],
		"yellow_dye": ["dandelion", "sunflower", "wildflowers"], "blue_dye": ["cornflower", "lapis_lazuli"],
		"light_blue_dye": ["blue_orchid"], "magenta_dye": ["allium", "lilac"], "light_gray_dye": ["azure_bluet", "oxeye_daisy", "white_tulip"],
		"orange_dye": ["orange_tulip", "torchflower", "open_eyeblossom"], "pink_dye": ["pink_tulip", "peony", "pink_petals", "cactus_flower"],
		"black_dye": ["ink_sac", "wither_rose"], "brown_dye": ["cocoa_beans"], "cyan_dye": ["pitcher_plant"], "gray_dye": ["closed_eyeblossom"]}
	for dye in flowers:
		for f in flowers[dye]:
			var cnt := 2 if String(f) in ["sunflower", "rose_bush", "lilac", "peony", "pitcher_plant"] else 1
			l(dye, cnt, [f])
	l("orange_dye", 2, ["red_dye", "yellow_dye"])
	l("lime_dye", 2, ["green_dye", "white_dye"])
	l("pink_dye", 2, ["red_dye", "white_dye"])
	l("gray_dye", 2, ["black_dye", "white_dye"])
	l("light_gray_dye", 2, ["gray_dye", "white_dye"])
	l("light_gray_dye", 3, ["black_dye", "white_dye", "white_dye"])
	l("cyan_dye", 2, ["blue_dye", "green_dye"])
	l("purple_dye", 2, ["blue_dye", "red_dye"])
	l("magenta_dye", 2, ["purple_dye", "pink_dye"])
	l("magenta_dye", 4, ["blue_dye", "red_dye", "red_dye", "white_dye"])
	l("light_blue_dye", 2, ["blue_dye", "white_dye"])
	s("white_wool", 1, ["##", "##"], {"#": "string"})
	for c in BlockCatalog.DYES:
		var dye: String = c + "_dye"
		l(c + "_wool", 1, [dye, "#wool"])
		s(c + "_carpet", 3, ["##"], {"#": c + "_wool"})
		s(c + "_stained_glass", 8, ["###", "#D#", "###"], {"#": "glass", "D": dye})
		s(c + "_stained_glass_pane", 16, ["###", "###"], {"#": c + "_stained_glass"})
		s(c + "_stained_glass_pane", 8, ["###", "#D#", "###"], {"#": "glass_pane", "D": dye})
		s(c + "_terracotta", 8, ["###", "#D#", "###"], {"#": "terracotta", "D": dye})
		l(c + "_concrete_powder", 8, [dye, "sand", "sand", "sand", "sand", "gravel", "gravel", "gravel", "gravel"])
		l(c + "_candle", 1, ["candle", dye])
		l(c + "_shulker_box", 1, ["shulker_box", dye])
		l(c + "_bed", 1, ["white_bed", dye])
		s(c + "_harness", 1, ["LLL", "GWG"], {"L": "leather", "G": "glass", "W": c + "_wool"})
	s("candle", 1, ["S", "H"], {"S": "string", "H": "honeycomb"})


static func _tools_armor() -> void:
	var mats := {"wooden": "#planks", "stone": "#stone_tool_materials", "copper": "copper_ingot", "iron": "iron_ingot",
		"golden": "gold_ingot", "diamond": "diamond"}
	for t in mats:
		var m: String = mats[t]
		s(t + "_pickaxe", 1, ["XXX", " S ", " S "], {"X": m, "S": "stick"})
		s(t + "_axe", 1, ["XX", "XS", " S"], {"X": m, "S": "stick"})
		s(t + "_shovel", 1, ["X", "S", "S"], {"X": m, "S": "stick"})
		s(t + "_hoe", 1, ["XX", " S", " S"], {"X": m, "S": "stick"})
		s(t + "_sword", 1, ["X", "X", "S"], {"X": m, "S": "stick"})
		s(t + "_spear", 1, ["  X", " S ", "S  "], {"X": m, "S": "stick"})
	var am := {"leather": "leather", "copper": "copper_ingot", "iron": "iron_ingot", "golden": "gold_ingot", "diamond": "diamond"}
	for a in am:
		var m2: String = am[a]
		s(a + "_helmet", 1, ["XXX", "X X"], {"X": m2})
		s(a + "_chestplate", 1, ["X X", "XXX", "XXX"], {"X": m2})
		s(a + "_leggings", 1, ["XXX", "X X", "X X"], {"X": m2})
		s(a + "_boots", 1, ["X X", "X X"], {"X": m2})
	s("turtle_helmet", 1, ["XXX", "X X"], {"X": "turtle_scute"})
	s("wolf_armor", 1, ["X  ", "XXX", "X X"], {"X": "armadillo_scute"})
	s("leather_horse_armor", 1, ["X X", "XXX", "X X"], {"X": "leather"})
	s("bow", 1, [" SX", "S X", " SX"], {"S": "stick", "X": "string"})
	s("crossbow", 1, ["SIS", "XTX", " S "], {"S": "stick", "I": "iron_ingot", "X": "string", "T": "tripwire_hook"})
	s("arrow", 4, ["F", "S", "E"], {"F": "flint", "S": "stick", "E": "feather"})
	s("spectral_arrow", 2, [" G ", "GAG", " G "], {"G": "glowstone_dust", "A": "arrow"})
	s("shield", 1, ["PIP", "PPP", " P "], {"P": "#planks", "I": "iron_ingot"})
	s("mace", 1, ["H", "B"], {"H": "heavy_core", "B": "breeze_rod"})
	s("shears", 1, [" I", "I "], {"I": "iron_ingot"})
	s("flint_and_steel", 1, ["I ", " F"], {"I": "iron_ingot", "F": "flint"})
	s("fishing_rod", 1, ["  S", " SX", "S X"], {"S": "stick", "X": "string"})
	s("carrot_on_a_stick", 1, ["R ", " C"], {"R": "fishing_rod", "C": "carrot"})
	s("warped_fungus_on_a_stick", 1, ["R ", " W"], {"R": "fishing_rod", "W": "warped_fungus"})
	s("brush", 1, ["F", "C", "S"], {"F": "feather", "C": "copper_ingot", "S": "stick"})
	s("spyglass", 1, ["A", "C", "C"], {"A": "amethyst_shard", "C": "copper_ingot"})
	s("saddle", 1, [" L ", "LIL"], {"L": "leather", "I": "iron_ingot"})


static func _functional() -> void:
	s("furnace", 1, ["###", "# #", "###"], {"#": "#stone_crafting_materials"})
	s("blast_furnace", 1, ["III", "IFI", "SSS"], {"I": "iron_ingot", "F": "furnace", "S": "smooth_stone"})
	s("smoker", 1, [" L ", "LFL", " L "], {"L": "#logs", "F": "furnace"})
	l("trapped_chest", 1, ["chest", "tripwire_hook"])
	s("ender_chest", 1, ["OOO", "OEO", "OOO"], {"O": "obsidian", "E": "ender_eye"})
	s("shulker_box", 1, ["S", "C", "S"], {"S": "shulker_shell", "C": "chest"})
	s("anvil", 1, ["BBB", " I ", "III"], {"B": "iron_block", "I": "iron_ingot"})
	s("enchanting_table", 1, [" B ", "DOD", "OOO"], {"B": "book", "D": "diamond", "O": "obsidian"})
	s("brewing_stand", 1, [" B ", "CCC"], {"B": "blaze_rod", "C": "#stone_crafting_materials"})
	s("stonecutter", 1, [" I ", "SSS"], {"I": "iron_ingot", "S": "stone"})
	s("cauldron", 1, ["I I", "I I", "III"], {"I": "iron_ingot"})
	s("torch", 4, ["C", "S"], {"C": "#coals", "S": "stick"})
	s("soul_torch", 4, ["C", "S", "B"], {"C": "#coals", "S": "stick", "B": "#soul_fire_base"})
	s("copper_torch", 4, ["N", "C", "S"], {"N": "copper_nugget", "C": "#coals", "S": "stick"})
	s("lantern", 1, ["NNN", "NTN", "NNN"], {"N": "iron_nugget", "T": "torch"})
	s("soul_lantern", 1, ["NNN", "NTN", "NNN"], {"N": "iron_nugget", "T": "soul_torch"})
	s("copper_lantern", 1, ["NNN", "NTN", "NNN"], {"N": "copper_nugget", "T": "copper_torch"})
	s("glass_pane", 16, ["###", "###"], {"#": "glass"})
	s("iron_bars", 16, ["###", "###"], {"#": "iron_ingot"})
	s("copper_bars", 16, ["###", "###"], {"#": "copper_ingot"})
	s("chain", 1, ["N", "I", "N"], {"N": "iron_nugget", "I": "iron_ingot"})
	s("copper_chain", 1, ["N", "I", "N"], {"N": "copper_nugget", "I": "copper_ingot"})
	s("tnt", 1, ["GSG", "SGS", "GSG"], {"G": "gunpowder", "S": "sand"})
	s("beacon", 1, ["GGG", "GNG", "OOO"], {"G": "glass", "N": "nether_star", "O": "obsidian"})
	s("respawn_anchor", 1, ["OOO", "GGG", "OOO"], {"O": "crying_obsidian", "G": "glowstone"})
	s("flower_pot", 1, ["B B", " B "], {"B": "brick"})
	s("decorated_pot", 1, [" B ", "B B", " B "], {"B": "brick"})
	l("carved_pumpkin", 1, ["pumpkin", "shears"])
	l("jack_o_lantern", 1, ["carved_pumpkin", "torch"])
	s("end_rod", 4, ["B", "P"], {"B": "blaze_rod", "P": "popped_chorus_fruit"})
	s("armor_stand", 1, ["SSS", " S ", "SXS"], {"S": "stick", "X": "smooth_stone_slab"})
	s("item_frame", 1, ["SSS", "SLS", "SSS"], {"S": "stick", "L": "leather"})
	l("glow_item_frame", 1, ["item_frame", "glow_ink_sac"])
	s("painting", 1, ["SSS", "SWS", "SSS"], {"S": "stick", "W": "#wool"})
	s("crafter", 1, ["III", "ICI", "RDR"], {"I": "iron_ingot", "C": "crafting_table", "R": "redstone", "D": "dropper"})
	s("bell", 1, ["SSS", "GGG", "GNG"], {"S": "stick", "G": "gold_ingot", "N": "gold_nugget"})


static func _redstone() -> void:
	s("redstone_torch", 1, ["R", "S"], {"R": "redstone", "S": "stick"})
	s("lever", 1, ["S", "C"], {"S": "stick", "C": "cobblestone"})
	l("stone_button", 1, ["stone"])
	l("polished_blackstone_button", 1, ["polished_blackstone"])
	s("stone_pressure_plate", 1, ["##"], {"#": "stone"})
	s("polished_blackstone_pressure_plate", 1, ["##"], {"#": "polished_blackstone"})
	s("light_weighted_pressure_plate", 1, ["##"], {"#": "gold_ingot"})
	s("heavy_weighted_pressure_plate", 1, ["##"], {"#": "iron_ingot"})
	s("redstone_lamp", 1, [" R ", "RGR", " R "], {"R": "redstone", "G": "glowstone"})
	s("repeater", 1, ["TRT", "SSS"], {"T": "redstone_torch", "R": "redstone", "S": "stone"})
	s("comparator", 1, [" T ", "TQT", "SSS"], {"T": "redstone_torch", "Q": "quartz", "S": "stone"})
	s("piston", 1, ["PPP", "CIC", "CRC"], {"P": "#planks", "C": "cobblestone", "I": "iron_ingot", "R": "redstone"})
	s("sticky_piston", 1, ["S", "P"], {"S": "slime_ball", "P": "piston"})
	s("observer", 1, ["CCC", "RRQ", "CCC"], {"C": "cobblestone", "R": "redstone", "Q": "quartz"})
	s("dispenser", 1, ["CCC", "CBC", "CRC"], {"C": "cobblestone", "B": "bow", "R": "redstone"})
	s("dropper", 1, ["CCC", "C C", "CRC"], {"C": "cobblestone", "R": "redstone"})
	s("hopper", 1, ["I I", "ICI", " I "], {"I": "iron_ingot", "C": "chest"})
	s("daylight_detector", 1, ["GGG", "QQQ", "SSS"], {"G": "glass", "Q": "quartz", "S": "#wooden_slabs"})
	s("target", 1, [" R ", "RHR", " R "], {"R": "redstone", "H": "hay_block"})
	s("rail", 16, ["I I", "ISI", "I I"], {"I": "iron_ingot", "S": "stick"})
	s("powered_rail", 6, ["G G", "GSG", "GRG"], {"G": "gold_ingot", "S": "stick", "R": "redstone"})
	s("detector_rail", 6, ["I I", "IPI", "IRI"], {"I": "iron_ingot", "P": "stone_pressure_plate", "R": "redstone"})
	s("activator_rail", 6, ["ISI", "ITI", "ISI"], {"I": "iron_ingot", "S": "stick", "T": "redstone_torch"})
	s("iron_door", 3, ["##", "##", "##"], {"#": "iron_ingot"})
	s("iron_trapdoor", 1, ["##", "##"], {"#": "iron_ingot"})
	s("tripwire_hook", 2, ["I", "S", "P"], {"I": "iron_ingot", "S": "stick", "P": "#planks"})
	s("lightning_rod", 1, ["C", "C", "C"], {"C": "copper_ingot"})
	s("minecart", 1, ["I I", "III"], {"I": "iron_ingot"})
	l("chest_minecart", 1, ["minecart", "chest"])
	l("furnace_minecart", 1, ["minecart", "furnace"])
	l("tnt_minecart", 1, ["minecart", "tnt"])
	l("hopper_minecart", 1, ["minecart", "hopper"])


static func _items() -> void:
	s("bucket", 1, ["I I", " I "], {"I": "iron_ingot"})
	s("compass", 1, [" I ", "IRI", " I "], {"I": "iron_ingot", "R": "redstone"})
	s("clock", 1, [" G ", "GRG", " G "], {"G": "gold_ingot", "R": "redstone"})
	s("recovery_compass", 1, ["EEE", "ECE", "EEE"], {"E": "echo_shard", "C": "compass"})
	s("map", 1, ["PPP", "PCP", "PPP"], {"P": "paper", "C": "compass"})
	s("paper", 3, ["###"], {"#": "sugar_cane"})
	l("book", 1, ["paper", "paper", "paper", "leather"])
	l("book_and_quill", 1, ["book", "ink_sac", "feather"])
	s("leather", 1, ["##", "##"], {"#": "rabbit_hide"})
	s("lead", 2, ["SS ", "SB ", "  S"], {"S": "string", "B": "slime_ball"})
	s("glass_bottle", 3, ["# #", " # "], {"#": "glass"})
	l("ender_eye", 1, ["ender_pearl", "blaze_powder"])
	l("blaze_powder", 2, ["blaze_rod"])
	l("fire_charge", 3, ["gunpowder", "blaze_powder", "#coals"])
	s("end_crystal", 1, ["GGG", "GEG", "GTG"], {"G": "glass", "E": "ender_eye", "T": "ghast_tear"})
	l("firework_rocket", 3, ["paper", "gunpowder"])
	l("firework_star", 1, ["gunpowder", "white_dye"])
	l("magma_cream", 1, ["slime_ball", "blaze_powder"])
	l("fermented_spider_eye", 1, ["spider_eye", "brown_mushroom", "sugar"])
	s("glistering_melon_slice", 1, ["NNN", "NMN", "NNN"], {"N": "gold_nugget", "M": "melon_slice"})
	s("golden_carrot", 1, ["NNN", "NCN", "NNN"], {"N": "gold_nugget", "C": "carrot"})
	s("golden_apple", 1, ["GGG", "GAG", "GGG"], {"G": "gold_ingot", "A": "apple"})
	l("sugar", 1, ["sugar_cane"])
	l("sugar", 3, ["honey_bottle"])
	l("honey_bottle", 4, ["honey_block", "glass_bottle", "glass_bottle", "glass_bottle", "glass_bottle"])
	l("wind_charge", 4, ["breeze_rod"])
	s("conduit", 1, ["NNN", "NHN", "NNN"], {"N": "nautilus_shell", "H": "heart_of_the_sea"})
	s("bundle", 1, ["S", "L"], {"S": "string", "L": "leather"})
	s("name_tag", 1, [" S", "P "], {"S": "string", "P": "paper"})
	s("netherite_upgrade_smithing_template", 2, ["DTD", "DND", "DDD"], {"D": "diamond", "T": "netherite_upgrade_smithing_template", "N": "netherrack"})
	s("chest", 1, ["###", "# #", "###"], {"#": "#planks"})


static func _food() -> void:
	s("bread", 1, ["WWW"], {"W": "wheat"})
	s("cake", 1, ["MMM", "SES", "WWW"], {"M": "milk_bucket", "S": "sugar", "E": "egg", "W": "wheat"})
	s("cookie", 8, ["WCW"], {"W": "wheat", "C": "cocoa_beans"})
	l("pumpkin_pie", 1, ["pumpkin", "sugar", "egg"])
	l("mushroom_stew", 1, ["brown_mushroom", "red_mushroom", "bowl"])
	l("rabbit_stew", 1, ["cooked_rabbit", "carrot", "baked_potato", "brown_mushroom", "bowl"])
	l("beetroot_soup", 1, ["beetroot", "beetroot", "beetroot", "beetroot", "beetroot", "beetroot", "bowl"])
	l("suspicious_stew", 1, ["brown_mushroom", "red_mushroom", "bowl", "dandelion"])
	l("pumpkin_seeds", 4, ["pumpkin"])
	l("melon_seeds", 1, ["melon_slice"])
	l("wheat", 9, ["hay_block"])
	l("dried_kelp", 9, ["dried_kelp_block"])


static func _copper() -> void:
	var states := ["", "exposed_", "weathered_", "oxidized_"]
	for st in states:
		var base: String = "copper_block" if st == "" else st + "copper"
		s(st + "cut_copper", 4, ["##", "##"], {"#": base})
		s(st + "cut_copper_stairs", 4, ["#  ", "## ", "###"], {"#": st + "cut_copper"})
		s(st + "cut_copper_slab", 6, ["###"], {"#": st + "cut_copper"})
		s(st + "chiseled_copper", 1, ["#", "#"], {"#": st + "cut_copper_slab"})
		s(st + "copper_grate", 4, [" # ", "# #", " # "], {"#": base})
		s(st + "copper_bulb", 4, [" C ", "CBC", " R "], {"C": base, "B": "blaze_rod", "R": "redstone"})
		for w in ["cut_copper", "chiseled_copper", "copper_grate", "copper_bulb", "cut_copper_stairs", "cut_copper_slab", "copper_door",
				"copper_trapdoor", "copper_chest"]:
			l("waxed_" + st + w, 1, [st + w, "honeycomb"])
		l("waxed_" + (st + "copper" if st != "" else "copper_block"), 1, [base, "honeycomb"])
	s("copper_door", 3, ["##", "##", "##"], {"#": "copper_ingot"})
	s("copper_trapdoor", 2, ["###", "###"], {"#": "copper_ingot"})
	s("copper_chest", 1, ["CCC", "CXC", "CCC"], {"C": "copper_ingot", "X": "chest"})


static func _nether_end() -> void:
	s("nether_bricks", 1, ["##", "##"], {"#": "nether_brick"})
	s("red_nether_bricks", 1, ["WB", "BW"], {"W": "nether_wart", "B": "nether_brick"})
	s("nether_brick_fence", 6, ["#B#", "#B#"], {"#": "nether_bricks", "B": "nether_brick"})
	s("chiseled_nether_bricks", 1, ["#", "#"], {"#": "nether_brick_slab"})
	s("nether_wart_block", 1, ["###", "###", "###"], {"#": "nether_wart"})
	s("polished_blackstone", 4, ["##", "##"], {"#": "blackstone"})
	s("polished_blackstone_bricks", 4, ["##", "##"], {"#": "polished_blackstone"})
	s("chiseled_polished_blackstone", 1, ["#", "#"], {"#": "polished_blackstone_slab"})
	s("polished_basalt", 4, ["##", "##"], {"#": "basalt"})
	s("magma_block", 1, ["##", "##"], {"#": "magma_cream"})
	s("end_stone_bricks", 4, ["##", "##"], {"#": "end_stone"})
	s("purpur_block", 4, ["##", "##"], {"#": "popped_chorus_fruit"})
	s("purpur_pillar", 1, ["#", "#"], {"#": "purpur_slab"})


# ------------------------------------------------------------------------------------------------
static func _smelting() -> void:
	var ore_kinds := ["furnace", "blast"]
	var food_kinds := ["furnace", "smoker", "campfire"]
	for o in [["iron_ore", "iron_ingot", 0.7], ["deepslate_iron_ore", "iron_ingot", 0.7], ["raw_iron", "iron_ingot", 0.7],
			["gold_ore", "gold_ingot", 1.0], ["deepslate_gold_ore", "gold_ingot", 1.0], ["raw_gold", "gold_ingot", 1.0],
			["nether_gold_ore", "gold_ingot", 1.0], ["copper_ore", "copper_ingot", 0.7], ["deepslate_copper_ore", "copper_ingot", 0.7],
			["raw_copper", "copper_ingot", 0.7], ["coal_ore", "coal", 0.1], ["deepslate_coal_ore", "coal", 0.1],
			["diamond_ore", "diamond", 1.0], ["deepslate_diamond_ore", "diamond", 1.0], ["emerald_ore", "emerald", 1.0],
			["deepslate_emerald_ore", "emerald", 1.0], ["lapis_ore", "lapis_lazuli", 0.2], ["deepslate_lapis_ore", "lapis_lazuli", 0.2],
			["redstone_ore", "redstone", 0.7], ["deepslate_redstone_ore", "redstone", 0.7], ["nether_quartz_ore", "quartz", 0.2],
			["ancient_debris", "netherite_scrap", 2.0]]:
		smelt(o[0], o[1], o[2], ore_kinds)
	for tool in ["pickaxe", "axe", "shovel", "hoe", "sword", "spear", "helmet", "chestplate", "leggings", "boots"]:
		smelt("iron_" + tool, "iron_nugget", 0.1, ore_kinds)
		smelt("golden_" + tool, "gold_nugget", 0.1, ore_kinds)
		smelt("copper_" + tool, "copper_nugget", 0.1, ore_kinds)
	for f in [["beef", "cooked_beef"], ["porkchop", "cooked_porkchop"], ["chicken", "cooked_chicken"], ["mutton", "cooked_mutton"],
			["rabbit", "cooked_rabbit"], ["cod", "cooked_cod"], ["salmon", "cooked_salmon"], ["potato", "baked_potato"],
			["kelp", "dried_kelp"]]:
		smelt(f[0], f[1], 0.35, food_kinds)
	for b in [["sand", "glass", 0.1], ["red_sand", "glass", 0.1], ["cobblestone", "stone", 0.1], ["stone", "smooth_stone", 0.1],
			["stone_bricks", "cracked_stone_bricks", 0.1], ["clay_ball", "brick", 0.3], ["clay", "terracotta", 0.35],
			["netherrack", "nether_brick", 0.1], ["cactus", "green_dye", 1.0], ["sea_pickle", "lime_dye", 0.1],
			["chorus_fruit", "popped_chorus_fruit", 0.1], ["wet_sponge", "sponge", 0.15], ["sandstone", "smooth_sandstone", 0.1],
			["red_sandstone", "smooth_red_sandstone", 0.1], ["quartz_block", "smooth_quartz", 0.1], ["basalt", "smooth_basalt", 0.1],
			["cobbled_deepslate", "deepslate", 0.1], ["deepslate_bricks", "cracked_deepslate_bricks", 0.1],
			["deepslate_tiles", "cracked_deepslate_tiles", 0.1], ["nether_bricks", "cracked_nether_bricks", 0.1],
			["polished_blackstone_bricks", "cracked_polished_blackstone_bricks", 0.1], ["resin_clump", "resin_brick", 0.1]]:
		smelt(b[0], b[1], b[2])
	for lg in tags.get("#logs_that_burn", {}):
		smelt(String(lg), "charcoal", 0.15)
	for c in BlockCatalog.DYES:
		smelt(c + "_terracotta", c + "_glazed_terracotta", 0.1)


static func _stonecutting() -> void:
	for f in BlockCatalog.families:
		var prefix: String = f[0]
		var base: String = f[1]
		if BlockDB.defs[BlockDB.id(base)].flammable:
			continue
		if f[2]:
			cut(base, prefix + "_stairs")
		if f[3]:
			cut(base, prefix + "_slab", 2)
		if f[4]:
			cut(base, prefix + "_wall")
	var chains := [["stone", ["stone_bricks", "chiseled_stone_bricks", "stone_brick_stairs", "stone_brick_slab", "stone_brick_wall"]],
		["granite", ["polished_granite", "polished_granite_stairs", "polished_granite_slab"]],
		["diorite", ["polished_diorite", "polished_diorite_stairs", "polished_diorite_slab"]],
		["andesite", ["polished_andesite", "polished_andesite_stairs", "polished_andesite_slab"]],
		["cobbled_deepslate", ["polished_deepslate", "deepslate_bricks", "deepslate_tiles", "chiseled_deepslate",
			"polished_deepslate_stairs", "polished_deepslate_slab", "polished_deepslate_wall", "deepslate_brick_stairs",
			"deepslate_brick_slab", "deepslate_brick_wall", "deepslate_tile_stairs", "deepslate_tile_slab", "deepslate_tile_wall"]],
		["tuff", ["polished_tuff", "tuff_bricks", "chiseled_tuff", "chiseled_tuff_bricks", "polished_tuff_stairs", "polished_tuff_slab",
			"polished_tuff_wall", "tuff_brick_stairs", "tuff_brick_slab", "tuff_brick_wall"]],
		["sandstone", ["cut_sandstone", "chiseled_sandstone", "cut_sandstone_slab"]],
		["red_sandstone", ["cut_red_sandstone", "chiseled_red_sandstone", "cut_red_sandstone_slab"]],
		["quartz_block", ["quartz_pillar", "chiseled_quartz_block", "quartz_bricks"]],
		["blackstone", ["polished_blackstone", "polished_blackstone_bricks", "chiseled_polished_blackstone",
			"polished_blackstone_stairs", "polished_blackstone_slab", "polished_blackstone_wall", "polished_blackstone_brick_stairs",
			"polished_blackstone_brick_slab", "polished_blackstone_brick_wall"]],
		["end_stone", ["end_stone_bricks", "end_stone_brick_stairs", "end_stone_brick_slab", "end_stone_brick_wall"]],
		["purpur_block", ["purpur_pillar"]], ["basalt", ["polished_basalt"]],
		["copper_block", ["cut_copper", "chiseled_copper", "cut_copper_stairs", "cut_copper_slab"]],
		["sulfur", ["polished_sulfur", "sulfur_bricks", "sulfur_brick_stairs", "sulfur_brick_slab", "sulfur_brick_wall"]],
		["cinnabar", ["polished_cinnabar", "cinnabar_bricks", "cinnabar_brick_stairs", "cinnabar_brick_slab", "cinnabar_brick_wall"]],
		["mud_bricks", ["mud_brick_stairs", "mud_brick_slab", "mud_brick_wall"]],
		["resin_bricks", ["chiseled_resin_bricks"]]]
	for ch in chains:
		for o in ch[1]:
			var on: String = o
			cut(ch[0], on, 2 if on.ends_with("_slab") else 1)


static func _smithing() -> void:
	for t in ["sword", "pickaxe", "axe", "shovel", "hoe", "spear", "helmet", "chestplate", "leggings", "boots"]:
		if ItemDB.has("diamond_" + t) and ItemDB.has("netherite_" + t):
			smithing.append(["netherite_upgrade_smithing_template", "diamond_" + t, "netherite_ingot", "netherite_" + t])


# ------------------------------------------------------------------------------------------------
# Matching
static func matches(ing: String, st: ItemStack) -> bool:
	if ing == "":
		return st == null
	if st == null:
		return false
	var n := st.item_name()
	if ing.begins_with("#"):
		return (tags.get(ing, {}) as Dictionary).has(n)
	return n == ing


## Finds the crafting result for a square grid (size 2 or 3) of ItemStacks. Returns
## {out: ItemStack, recipe} or {}.
static func find(grid: Array, size: int) -> Dictionary:
	# bounding box of non-empty cells
	var x0 := size
	var y0 := size
	var x1 := -1
	var y1 := -1
	var items := []
	for y in size:
		for x in size:
			var st: ItemStack = grid[y * size + x]
			if st != null and not st.is_empty():
				x0 = mini(x0, x)
				y0 = mini(y0, y)
				x1 = maxi(x1, x)
				y1 = maxi(y1, y)
				items.append(st)
	if x1 < 0:
		return {}
	var w := x1 - x0 + 1
	var h := y1 - y0 + 1
	# special: armor dyeing, repairing two damaged tools, map/book copies
	var sp := _special(items)
	if not sp.is_empty():
		return sp
	for r in shaped:
		if int(r.w) != w or int(r.h) != h:
			continue
		for mirror in [false, true]:
			var ok := true
			for yy in h:
				for xx in w:
					var gx := x0 + xx
					var rx := (w - 1 - xx) if mirror else xx
					var ing: String = r.grid[yy * w + rx]
					var st2: ItemStack = grid[(y0 + yy) * size + gx]
					if st2 != null and st2.is_empty():
						st2 = null
					if not matches(ing, st2):
						ok = false
						break
				if not ok:
					break
			if ok:
				return {"out": ItemStack.of(String(r.out), int(r.count)), "recipe": r}
	for r in shapeless:
		var ing_list: Array = r.ing
		if ing_list.size() != items.size():
			continue
		var used := []
		used.resize(items.size())
		used.fill(false)
		var ok2 := true
		for ing in ing_list:
			var found := false
			for i in items.size():
				if not used[i] and matches(String(ing), items[i]):
					used[i] = true
					found = true
					break
			if not found:
				ok2 = false
				break
		if ok2:
			return {"out": ItemStack.of(String(r.out), int(r.count)), "recipe": r}
	return {}


static func _special(items: Array) -> Dictionary:
	# tool repair: two of the same damageable item
	if items.size() == 2:
		var a: ItemStack = items[0]
		var b: ItemStack = items[1]
		if a.id == b.id and a.item().is_damageable() and a.count == 1 and b.count == 1:
			var mx := a.item().durability
			var rem := (mx - a.damage) + (mx - b.damage) + mx * 5 / 100
			var out := ItemStack.new(a.id, 1, maxi(0, mx - rem))
			return {"out": out, "recipe": {"special": "repair"}}
	# dyeing leather armor
	var armor: ItemStack = null
	var dyes := []
	for st in items:
		var it: ItemStack = st
		if it.item().material == "leather" and it.item().kind == "armor":
			if armor != null:
				return {}
			armor = it
		elif it.item().props.has("dye"):
			dyes.append(it)
		else:
			return {}
	if armor != null and not dyes.is_empty():
		var c := Color(0, 0, 0)
		for dd in dyes:
			c += (dd as ItemStack).item().color
		c /= float(dyes.size())
		var out2 := armor.copy()
		out2.count = 1
		out2.data["color"] = c.to_html(false)
		return {"out": out2, "recipe": {"special": "dye"}}
	return {}


## Recipes producing an item (for the recipe book).
static func recipes_for(item_name: String) -> Array:
	return _by_output.get(item_name, [])


static func smelt_result(input: String, kind := "furnace") -> Dictionary:
	var e: Dictionary = smelting.get(input, {})
	if e.is_empty():
		return {}
	if kind == "blast_furnace":
		kind = "blast"
	if not (e.kinds as Array).has(kind) and kind != "furnace":
		return {}
	return e


static func fuel_ticks(st: ItemStack) -> int:
	if st == null:
		return 0
	return st.item().fuel
