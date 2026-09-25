class_name BlockCatalog
extends RefCounted
## Data catalog for every block. Families (woods, stones, dyes, copper...) are produced by loops so a
## new family member is a one-line change. BlockDB turns the resulting BlockDef list into flat tables.
##
## Drop syntax:  ""  = the block itself,  "-" = nothing,  "item" / "item:min-max",  "@table" = LootDB table.

const DYES := ["white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray",
	"light_gray", "cyan", "purple", "blue", "brown", "green", "red", "black"]

const NON_SOLID := [BlockDB.M_AIR, BlockDB.M_CROSS, BlockDB.M_TALL_CROSS, BlockDB.M_FLUID, BlockDB.M_TORCH,
	BlockDB.M_WIRE, BlockDB.M_RAIL, BlockDB.M_BUTTON, BlockDB.M_PLATE, BlockDB.M_LEVER, BlockDB.M_PORTAL,
	BlockDB.M_END_PORTAL, BlockDB.M_FIRE, BlockDB.M_CROP, BlockDB.M_VINE, BlockDB.M_SHORT, BlockDB.M_SIGN,
	BlockDB.M_TRIPWIRE, BlockDB.M_COBWEB, BlockDB.M_HANGING]

const CUTOUT := [BlockDB.M_CROSS, BlockDB.M_TALL_CROSS, BlockDB.M_TORCH, BlockDB.M_WIRE, BlockDB.M_RAIL,
	BlockDB.M_CROP, BlockDB.M_VINE, BlockDB.M_SHORT, BlockDB.M_LADDER, BlockDB.M_FIRE, BlockDB.M_LILY,
	BlockDB.M_BAMBOO, BlockDB.M_SCAFFOLD, BlockDB.M_COBWEB, BlockDB.M_PANE, BlockDB.M_DOOR, BlockDB.M_TRAPDOOR,
	BlockDB.M_LANTERN, BlockDB.M_CHAIN, BlockDB.M_CAMPFIRE, BlockDB.M_BREWING, BlockDB.M_LEAVES,
	BlockDB.M_REPEATER, BlockDB.M_COMPARATOR, BlockDB.M_CANDLE, BlockDB.M_PICKLE, BlockDB.M_DRIPSTONE,
	BlockDB.M_SPIKE, BlockDB.M_AMETHYST, BlockDB.M_HANGING, BlockDB.M_TRIPWIRE, BlockDB.M_ROD, BlockDB.M_BEACON,
	BlockDB.M_CHORUS, BlockDB.M_POT, BlockDB.M_BELL, BlockDB.M_LEVER, BlockDB.M_CHAIN_H, BlockDB.M_CACTUS,
	BlockDB.M_HOPPER, BlockDB.M_CAULDRON]

const B_NAT := ["natural"]
const B_BUILD := ["building"]
const B_COLOR := ["colored"]
const B_FUNC := ["functional"]
const B_RED := ["redstone"]

var defs: Array = []
var _index := {}
static var families: Array = []      # [prefix, base, stairs, slab, wall] (used by RecipeDB)


static func pretty(n: String) -> String:
	var special := {"tnt": "TNT", "tnt_minecart": "Minecart with TNT"}
	if special.has(n):
		return special[n]
	var parts := n.split("_")
	var out := PackedStringArray()
	for p in parts:
		if p == "":
			continue
		if p in ["of", "and", "on", "a", "the", "with"]:
			out.append(p)
		else:
			out.append(p.substr(0, 1).to_upper() + p.substr(1))
	return " ".join(out)


func _add(bname: String, p: Dictionary = {}) -> BlockDef:
	if _index.has(bname):
		push_error("BlockCatalog: duplicate block '%s'" % bname)
		return _index[bname]
	var d := BlockDef.new()
	d.name = bname
	d.display = pretty(bname)
	d.tex = {"all": bname}
	for k in p:
		if k == "tabs" or k == "tags":
			d.set(k, PackedStringArray(p[k]))
		else:
			d.set(k, p[k])
	var m := d.model
	if not p.has("render") and m in CUTOUT:
		d.render = BlockDB.R_CUTOUT
	if not p.has("full"):
		d.full = m == BlockDB.M_CUBE and d.render == BlockDB.R_OPAQUE
	if not p.has("opacity"):
		d.opacity = 15 if d.full else 0
	if not p.has("solid"):
		d.solid = not (m in NON_SOLID)
	if not p.has("resistance"):
		d.resistance = maxf(d.hardness, 0.0) if d.hardness >= 0.0 else 3600000.0
	if d.hardness < 0.0:
		d.resistance = 3600000.0
	defs.append(d)
	_index[bname] = d
	return d


func has(bname: String) -> bool:
	return _index.has(bname)


func get_def(bname: String) -> BlockDef:
	return _index.get(bname)


func build() -> void:
	families.clear()
	_add("air", {model = BlockDB.M_AIR, render = BlockDB.R_NONE, solid = false, full = false, opacity = 0,
		replaceable = true, hardness = 0.0, has_item = false, tex = {}})
	_fluids()
	_terrain()
	_stones()
	_ores()
	_minerals()
	_woods()
	_plants()
	_colored()
	_nether()
	_end()
	_copper()
	_functional()
	_redstone()
	_misc()
	_sulfur()


# ------------------------------------------------------------------------------------------------
func _fluids() -> void:
	_add("water", {model = BlockDB.M_FLUID, render = BlockDB.R_TRANSLUCENT, tex = {top = "water_still", side = "water_flow"},
		fluid = 1, opacity = 2, hardness = 100.0, resistance = 100.0, solid = false, replaceable = true,
		has_item = false, tint = BlockDB.T_WATER, drop = "-", tick = "fluid"})
	_add("lava", {model = BlockDB.M_FLUID, render = BlockDB.R_OPAQUE, tex = {top = "lava_still", side = "lava_flow"},
		fluid = 2, opacity = 1, light = 15, hardness = 100.0, resistance = 100.0, solid = false, full = false,
		replaceable = true, has_item = false, drop = "-", tick = "fluid", damage = 4.0})


func _terrain() -> void:
	_add("grass_block", {tex = {top = "grass_block_top", bottom = "dirt", side = "grass_block_side"}, hardness = 0.6,
		tool = "shovel", drop = "dirt", sound = "grass", tint = BlockDB.T_GRASS, tabs = B_NAT, tick = "grass",
		props = {tint_top_only = true, tint_side_mark = true, snowy_side = "grass_block_snow"}})
	_add("dirt", {hardness = 0.5, tool = "shovel", sound = "gravel", tabs = B_NAT, tags = ["dirt"]})
	_add("coarse_dirt", {hardness = 0.5, tool = "shovel", sound = "gravel", tabs = B_NAT, tags = ["dirt"]})
	_add("rooted_dirt", {hardness = 0.5, tool = "shovel", sound = "gravel", tabs = B_NAT, tags = ["dirt"]})
	_add("podzol", {tex = {top = "podzol_top", bottom = "dirt", side = "podzol_side"}, hardness = 0.5, tool = "shovel",
		drop = "dirt", sound = "gravel", tabs = B_NAT, tags = ["dirt"], props = {snowy_side = "grass_block_snow"}})
	_add("mycelium", {tex = {top = "mycelium_top", bottom = "dirt", side = "mycelium_side"}, hardness = 0.6,
		tool = "shovel", drop = "dirt", sound = "grass", tabs = B_NAT, tick = "mycelium", tags = ["dirt"],
		props = {snowy_side = "grass_block_snow"}})
	_add("dirt_path", {model = BlockDB.M_PATH, tex = {top = "dirt_path_top", bottom = "dirt", side = "dirt_path_side"},
		hardness = 0.65, tool = "shovel", drop = "dirt", sound = "grass", tabs = B_NAT, full = false, opacity = 15})
	_add("farmland", {model = BlockDB.M_PATH, tex = {top = "farmland", bottom = "dirt", side = "dirt"}, hardness = 0.6,
		tool = "shovel", drop = "dirt", sound = "gravel", tabs = B_NAT, full = false, opacity = 15, tick = "farmland",
		props = {moist_tex = "farmland_moist"}})
	_add("mud", {hardness = 0.5, tool = "shovel", sound = "mud", tabs = B_NAT})
	_add("packed_mud", {hardness = 1.0, tool = "pickaxe", sound = "mud", tabs = B_BUILD})
	_add("mud_bricks", {hardness = 1.5, resistance = 3.0, tool = "pickaxe", sound = "stone", tabs = B_BUILD})
	_add("clay", {hardness = 0.6, tool = "shovel", drop = "clay_ball:4-4", sound = "gravel", tabs = B_NAT})
	_add("gravel", {hardness = 0.6, tool = "shovel", drop = "@gravel", sound = "gravel", gravity = true, tabs = B_NAT})
	_add("sand", {hardness = 0.5, tool = "shovel", sound = "sand", gravity = true, tabs = B_NAT, tags = ["sand"]})
	_add("red_sand", {hardness = 0.5, tool = "shovel", sound = "sand", gravity = true, tabs = B_NAT, tags = ["sand"]})
	_add("suspicious_sand", {hardness = 0.25, tool = "shovel", sound = "sand", gravity = true, tabs = B_NAT, drop = "-"})
	_add("suspicious_gravel", {hardness = 0.25, tool = "shovel", sound = "gravel", gravity = true, tabs = B_NAT, drop = "-"})
	for s in ["sandstone", "red_sandstone"]:
		var st := {top = s + "_top", bottom = s + "_bottom", side = s}
		_add(s, {tex = st, hardness = 0.8, tool = "pickaxe", needs_tool = true, tier = 1, tabs = B_BUILD})
		_add("chiseled_" + s, {tex = {top = s + "_top", bottom = s + "_top", side = "chiseled_" + s}, hardness = 0.8,
			tool = "pickaxe", needs_tool = true, tier = 1, tabs = B_BUILD})
		_add("cut_" + s, {tex = {top = s + "_top", bottom = s + "_top", side = "cut_" + s}, hardness = 0.8,
			tool = "pickaxe", needs_tool = true, tier = 1, tabs = B_BUILD})
		_add("smooth_" + s, {tex = {all = s + "_top"}, hardness = 2.0, resistance = 6.0, tool = "pickaxe",
			needs_tool = true, tier = 1, tabs = B_BUILD})
	_add("snow", {model = BlockDB.M_SNOW, tex = {all = "snow"}, hardness = 0.1, tool = "shovel", needs_tool = true,
		drop = "@snow_layer", sound = "snow", tabs = B_NAT, replaceable = true, opacity = 0, place = "snow_layer",
		tick = "snow_melt"})
	_add("snow_block", {tex = {all = "snow"}, hardness = 0.2, tool = "shovel", needs_tool = true, drop = "snowball:4-4",
		sound = "snow", tabs = B_NAT})
	_add("powder_snow", {tex = {all = "powder_snow"}, hardness = 0.25, tool = "shovel", sound = "snow", solid = false,
		tabs = B_NAT, drop = "-", props = {powder = true}})
	_add("ice", {render = BlockDB.R_TRANSLUCENT, hardness = 0.5, tool = "pickaxe", drop = "-", silk_only = true,
		sound = "glass", slip = 0.98, opacity = 2, cull_same = true, tabs = B_NAT, tick = "ice"})
	_add("packed_ice", {hardness = 0.5, tool = "pickaxe", drop = "-", silk_only = true, sound = "glass", slip = 0.98, tabs = B_NAT})
	_add("blue_ice", {hardness = 2.8, tool = "pickaxe", drop = "-", silk_only = true, sound = "glass", slip = 0.989, tabs = B_NAT})
	_add("bedrock", {hardness = -1.0, drop = "-", tabs = B_NAT})
	_add("obsidian", {hardness = 50.0, resistance = 1200.0, tool = "pickaxe", needs_tool = true, tier = 4, tabs = B_BUILD})
	_add("crying_obsidian", {hardness = 50.0, resistance = 1200.0, tool = "pickaxe", needs_tool = true, tier = 4,
		light = 10, tabs = B_BUILD})
	_add("moss_block", {hardness = 0.1, tool = "hoe", sound = "moss", tabs = B_NAT, tags = ["dirt"]})
	_add("moss_carpet", {model = BlockDB.M_CARPET, tex = {all = "moss_block"}, hardness = 0.1, tool = "hoe", sound = "moss",
		tabs = B_NAT, place = "carpet"})
	_add("pale_moss_block", {hardness = 0.1, tool = "hoe", sound = "moss", tabs = B_NAT, tags = ["dirt"]})
	_add("pale_moss_carpet", {model = BlockDB.M_CARPET, tex = {all = "pale_moss_block"}, hardness = 0.1, tool = "hoe",
		sound = "moss", tabs = B_NAT, place = "carpet"})
	_add("calcite", {hardness = 0.75, tool = "pickaxe", needs_tool = true, tier = 1, sound = "stone", tabs = B_NAT})
	_add("dripstone_block", {hardness = 1.5, tool = "pickaxe", needs_tool = true, tier = 1, tabs = B_NAT})
	_add("pointed_dripstone", {model = BlockDB.M_DRIPSTONE, tex = {all = "pointed_dripstone"}, hardness = 1.5,
		tool = "pickaxe", place = "dripstone", tabs = B_NAT, icon = "texture:pointed_dripstone", damage = 0.0})
	_add("amethyst_block", {hardness = 1.5, tool = "pickaxe", needs_tool = true, tier = 1, sound = "amethyst", tabs = B_NAT})
	_add("budding_amethyst", {hardness = 1.5, tool = "pickaxe", drop = "-", sound = "amethyst", tabs = B_NAT, tick = "budding"})
	for bud in [["small_amethyst_bud", 1, 0.2], ["medium_amethyst_bud", 2, 0.35], ["large_amethyst_bud", 4, 0.45],
			["amethyst_cluster", 5, 0.55]]:
		_add(bud[0], {model = BlockDB.M_AMETHYST, tex = {all = bud[0]}, hardness = 1.5, tool = "pickaxe",
			drop = "amethyst_shard:4-4" if bud[0] == "amethyst_cluster" else "-", light = bud[1], sound = "amethyst",
			place = "facing_all", tabs = B_NAT, icon = "texture:" + bud[0], props = {size = bud[2]}})
	_add("tuff", {hardness = 1.5, resistance = 6.0, tool = "pickaxe", needs_tool = true, tier = 1, tabs = B_NAT})
	_add("smooth_basalt", {hardness = 1.25, resistance = 4.2, tool = "pickaxe", needs_tool = true, tier = 1, tabs = B_BUILD})
	_add("sculk", {hardness = 0.2, tool = "hoe", drop = "-", silk_only = true, sound = "sculk", tabs = B_NAT, xp = Vector2i(1, 1)})
	_add("sculk_vein", {model = BlockDB.M_VINE, tex = {all = "sculk_vein"}, hardness = 0.2, tool = "hoe", sound = "sculk",
		place = "vine", tabs = B_NAT, replaceable = true, icon = "texture:sculk_vein"})
	_add("sculk_catalyst", {tex = {top = "sculk_catalyst_top", bottom = "sculk_catalyst_bottom", side = "sculk_catalyst_side"},
		hardness = 3.0, tool = "hoe", light = 6, sound = "sculk", tabs = B_NAT, xp = Vector2i(5, 5)})
	_add("sculk_sensor", {model = BlockDB.M_SHORT, tex = {top = "sculk_sensor_top", side = "sculk_sensor_side",
		bottom = "sculk_sensor_bottom"}, hardness = 1.5, tool = "hoe", light = 1, sound = "sculk", solid = true,
		tabs = B_RED, props = {height = 0.5}, interact = "", tick = "sculk_sensor"})
	_add("sculk_shrieker", {model = BlockDB.M_SHORT, tex = {top = "sculk_shrieker_top", side = "sculk_shrieker_side",
		bottom = "sculk_shrieker_bottom"}, hardness = 3.0, tool = "hoe", sound = "sculk", solid = true, tabs = B_NAT,
		props = {height = 0.5}, tick = "shrieker"})
	_add("cobweb", {model = BlockDB.M_COBWEB, tex = {all = "cobweb"}, hardness = 4.0, tool = "sword", drop = "string",
		render = BlockDB.R_CUTOUT, tabs = B_NAT, icon = "texture:cobweb", props = {web = true}})


# ------------------------------------------------------------------------------------------------
func _stone_block(bname: String, p: Dictionary) -> BlockDef:
	var q := {hardness = 1.5, resistance = 6.0, tool = "pickaxe", needs_tool = true, tier = 1, tabs = B_BUILD}
	q.merge(p, true)
	return _add(bname, q)


## Creates stairs / slab / wall for a base block. prefix is the Minecraft-style family prefix.
func _family(prefix: String, base: String, stairs := true, slab := true, wall := false) -> void:
	families.append([prefix, base, stairs, slab, wall])
	var bd: BlockDef = _index[base]
	var common := {hardness = bd.hardness, resistance = bd.resistance, tool = bd.tool, needs_tool = bd.needs_tool,
		tier = bd.tier, sound = bd.sound, tabs = B_BUILD, flammable = bd.flammable}
	var t: Dictionary = bd.tex.duplicate()
	if bd.place == "axis":
		t = {top = bd.tex.get("end", bd.tex.get("top", base)), bottom = bd.tex.get("end", base), side = bd.tex.get("side", base)}
	if stairs:
		var q := common.duplicate()
		q.merge({model = BlockDB.M_STAIRS, tex = t, place = "stairs"}, true)
		_add(prefix + "_stairs", q)
	if slab:
		var q2 := common.duplicate()
		q2.merge({model = BlockDB.M_SLAB, tex = t, place = "slab", drop = "@slab"}, true)
		_add(prefix + "_slab", q2)
	if wall:
		var q3 := common.duplicate()
		q3.merge({model = BlockDB.M_WALL, tex = {all = t.get("side", t.get("all", base))}, place = "", tabs = B_BUILD}, true)
		_add(prefix + "_wall", q3)


func _stones() -> void:
	_stone_block("stone", {drop = "cobblestone", tags = ["stone", "base_stone"]})
	_stone_block("cobblestone", {hardness = 2.0, tags = ["stone_tool_materials"]})
	_stone_block("mossy_cobblestone", {hardness = 2.0})
	_stone_block("smooth_stone", {hardness = 2.0, tex = {all = "smooth_stone"}})
	_stone_block("stone_bricks", {})
	_stone_block("mossy_stone_bricks", {})
	_stone_block("cracked_stone_bricks", {})
	_stone_block("chiseled_stone_bricks", {})
	_stone_block("infested_stone", {tex = {all = "stone"}, hardness = 0.75, drop = "-", needs_tool = false, tabs = B_NAT,
		props = {infested = true}, display = "Infested Stone"})
	_stone_block("granite", {tabs = B_NAT, tags = ["base_stone"]})
	_stone_block("polished_granite", {})
	_stone_block("diorite", {tabs = B_NAT, tags = ["base_stone"]})
	_stone_block("polished_diorite", {})
	_stone_block("andesite", {tabs = B_NAT, tags = ["base_stone"]})
	_stone_block("polished_andesite", {})
	_stone_block("deepslate", {tex = {side = "deepslate", end = "deepslate_top"}, place = "axis", hardness = 3.0,
		drop = "cobbled_deepslate", tabs = B_NAT, sound = "deepslate", tags = ["base_stone"]})
	_stone_block("cobbled_deepslate", {hardness = 3.5, sound = "deepslate", tags = ["stone_tool_materials"]})
	_stone_block("polished_deepslate", {hardness = 3.5, sound = "deepslate"})
	_stone_block("deepslate_bricks", {hardness = 3.5, sound = "deepslate"})
	_stone_block("cracked_deepslate_bricks", {hardness = 3.5, sound = "deepslate"})
	_stone_block("deepslate_tiles", {hardness = 3.5, sound = "deepslate"})
	_stone_block("cracked_deepslate_tiles", {hardness = 3.5, sound = "deepslate"})
	_stone_block("chiseled_deepslate", {hardness = 3.5, sound = "deepslate"})
	_stone_block("reinforced_deepslate", {tex = {top = "reinforced_deepslate_top", bottom = "reinforced_deepslate_bottom",
		side = "reinforced_deepslate_side"}, hardness = 55.0, resistance = 1200.0, drop = "-", tabs = B_NAT})
	_stone_block("polished_tuff", {})
	_stone_block("tuff_bricks", {})
	_stone_block("chiseled_tuff", {tex = {top = "chiseled_tuff_top", bottom = "chiseled_tuff_top", side = "chiseled_tuff"}})
	_stone_block("chiseled_tuff_bricks", {tex = {top = "chiseled_tuff_bricks_top", bottom = "chiseled_tuff_bricks_top",
		side = "chiseled_tuff_bricks"}})
	_stone_block("bricks", {hardness = 2.0})
	_family("stone", "stone", true, true, false)
	_family("cobblestone", "cobblestone", true, true, true)
	_family("mossy_cobblestone", "mossy_cobblestone", true, true, true)
	_family("smooth_stone", "smooth_stone", false, true, false)
	_index["smooth_stone_slab"].tex = {top = "smooth_stone", bottom = "smooth_stone", side = "smooth_stone_slab_side"}
	_family("stone_brick", "stone_bricks", true, true, true)
	_family("mossy_stone_brick", "mossy_stone_bricks", true, true, true)
	_family("granite", "granite", true, true, true)
	_family("polished_granite", "polished_granite", true, true, false)
	_family("diorite", "diorite", true, true, true)
	_family("polished_diorite", "polished_diorite", true, true, false)
	_family("andesite", "andesite", true, true, true)
	_family("polished_andesite", "polished_andesite", true, true, false)
	_family("cobbled_deepslate", "cobbled_deepslate", true, true, true)
	_family("polished_deepslate", "polished_deepslate", true, true, true)
	_family("deepslate_brick", "deepslate_bricks", true, true, true)
	_family("deepslate_tile", "deepslate_tiles", true, true, true)
	_family("tuff", "tuff", true, true, true)
	_family("polished_tuff", "polished_tuff", true, true, true)
	_family("tuff_brick", "tuff_bricks", true, true, true)
	_family("brick", "bricks", true, true, true)
	_family("mud_brick", "mud_bricks", true, true, true)
	_family("sandstone", "sandstone", true, true, true)
	_family("smooth_sandstone", "smooth_sandstone", true, true, false)
	_family("cut_sandstone", "cut_sandstone", false, true, false)
	_family("red_sandstone", "red_sandstone", true, true, true)
	_family("smooth_red_sandstone", "smooth_red_sandstone", true, true, false)
	_family("cut_red_sandstone", "cut_red_sandstone", false, true, false)


func _ores() -> void:
	var ores := [
		# name, drop, tier, xp, hardness(normal)
		["coal_ore", "coal", 1, Vector2i(0, 2)],
		["iron_ore", "raw_iron", 2, Vector2i.ZERO],
		["copper_ore", "raw_copper:2-5", 2, Vector2i.ZERO],
		["gold_ore", "raw_gold", 3, Vector2i.ZERO],
		["redstone_ore", "redstone:4-5", 3, Vector2i(1, 5)],
		["emerald_ore", "emerald", 3, Vector2i(3, 7)],
		["lapis_ore", "lapis_lazuli:4-9", 2, Vector2i(2, 5)],
		["diamond_ore", "diamond", 3, Vector2i(3, 7)],
	]
	for o in ores:
		for deep in [false, true]:
			var n: String = ("deepslate_" + o[0]) if deep else o[0]
			var p := {hardness = 4.5 if deep else 3.0, resistance = 3.0, tool = "pickaxe", needs_tool = true, tier = o[2],
				drop = o[1], xp = o[3], tabs = B_NAT, sound = "deepslate" if deep else "stone",
				props = {fortune = true}, tags = ["ore"]}
			if o[0] == "redstone_ore":
				p["light"] = 9
				p["props"] = {fortune = true, lit_bit = 1}
				p["place"] = "lit"
				p["tex"] = {all = n, on = n}
				p["interact"] = "redstone_ore"
				p["tick"] = "redstone_ore"
			_add(n, p)
	_add("nether_gold_ore", {hardness = 3.0, tool = "pickaxe", needs_tool = true, tier = 1, drop = "gold_nugget:2-6",
		xp = Vector2i(0, 1), tabs = B_NAT, sound = "netherrack", props = {fortune = true}, tags = ["ore"]})
	_add("nether_quartz_ore", {hardness = 3.0, tool = "pickaxe", needs_tool = true, tier = 1, drop = "quartz",
		xp = Vector2i(2, 5), tabs = B_NAT, sound = "netherrack", props = {fortune = true}, tags = ["ore"]})
	_add("ancient_debris", {tex = {top = "ancient_debris_top", bottom = "ancient_debris_top", side = "ancient_debris_side"},
		hardness = 30.0, resistance = 1200.0, tool = "pickaxe", needs_tool = true, tier = 4, tabs = B_NAT, sound = "metal"})


func _minerals() -> void:
	var metal := {tool = "pickaxe", needs_tool = true, sound = "metal", tabs = B_BUILD}
	var mk := func(n: String, h: float, tier: int, extra: Dictionary = {}) -> void:
		var p: Dictionary = metal.duplicate()
		p["hardness"] = h
		p["resistance"] = 6.0
		p["tier"] = tier
		p.merge(extra, true)
		_add(n, p)
	mk.call("coal_block", 5.0, 1, {sound = "stone", flammable = true})
	mk.call("iron_block", 5.0, 2)
	mk.call("gold_block", 3.0, 3)
	mk.call("diamond_block", 5.0, 3)
	mk.call("emerald_block", 5.0, 3)
	mk.call("lapis_block", 3.0, 2, {sound = "stone"})
	mk.call("redstone_block", 5.0, 1, {tabs = B_RED, props = {power = 15}})
	mk.call("netherite_block", 50.0, 4, {resistance = 1200.0})
	mk.call("raw_iron_block", 5.0, 2, {sound = "stone"})
	mk.call("raw_copper_block", 5.0, 2, {sound = "stone"})
	mk.call("raw_gold_block", 5.0, 3, {sound = "stone"})
	_stone_block("quartz_block", {tex = {top = "quartz_block_top", bottom = "quartz_block_bottom", side = "quartz_block_side"}, hardness = 0.8})
	_stone_block("chiseled_quartz_block", {tex = {top = "chiseled_quartz_block_top", bottom = "chiseled_quartz_block_top",
		side = "chiseled_quartz_block"}, hardness = 0.8})
	_stone_block("quartz_pillar", {tex = {side = "quartz_pillar", end = "quartz_pillar_top"}, place = "axis", hardness = 0.8})
	_stone_block("quartz_bricks", {hardness = 0.8})
	_stone_block("smooth_quartz", {tex = {all = "quartz_block_bottom"}, hardness = 2.0})
	_family("quartz", "quartz_block", true, true, false)
	_family("smooth_quartz", "smooth_quartz", true, true, false)
	_stone_block("prismarine", {tabs = B_BUILD})
	_stone_block("prismarine_bricks", {})
	_stone_block("dark_prismarine", {})
	_family("prismarine", "prismarine", true, true, true)
	_family("prismarine_brick", "prismarine_bricks", true, true, false)
	_family("dark_prismarine", "dark_prismarine", true, true, false)
	_add("sea_lantern", {hardness = 0.3, light = 15, drop = "prismarine_crystals:2-3", sound = "glass", tabs = B_FUNC})


# ------------------------------------------------------------------------------------------------
const WOODS := [
	{n = "oak", tint = BlockDB.T_FOLIAGE},
	{n = "spruce", tint = BlockDB.T_SPRUCE},
	{n = "birch", tint = BlockDB.T_BIRCH},
	{n = "jungle", tint = BlockDB.T_FOLIAGE},
	{n = "acacia", tint = BlockDB.T_FOLIAGE},
	{n = "dark_oak", tint = BlockDB.T_FOLIAGE},
	{n = "mangrove", tint = BlockDB.T_MANGROVE, sapling = "mangrove_propagule"},
	{n = "cherry", tint = BlockDB.T_NONE},
	{n = "pale_oak", tint = BlockDB.T_NONE},
	{n = "crimson", nether = true},
	{n = "warped", nether = true},
	{n = "bamboo", bamboo = true},
]


func _woods() -> void:
	for w in WOODS:
		var n: String = w.n
		var nether: bool = w.get("nether", false)
		var bamboo: bool = w.get("bamboo", false)
		var wood_p := {hardness = 2.0, resistance = 3.0, tool = "axe", sound = "nether_wood" if nether else "wood",
			flammable = not nether, tabs = B_BUILD}
		var planks := n + "_planks"
		if bamboo:
			_add("bamboo_block", {tex = {side = "bamboo_block", end = "bamboo_block_top"}, place = "axis", hardness = 2.0,
				tool = "axe", sound = "bamboo_wood", flammable = true, tabs = B_BUILD, tags = ["logs"]})
			_add("stripped_bamboo_block", {tex = {side = "stripped_bamboo_block", end = "stripped_bamboo_block_top"},
				place = "axis", hardness = 2.0, tool = "axe", sound = "bamboo_wood", flammable = true, tabs = B_BUILD, tags = ["logs"]})
		else:
			var stem := "stem" if nether else "log"
			var woodn := "hyphae" if nether else "wood"
			_add(n + "_" + stem, {tex = {side = n + "_" + stem, end = n + "_" + stem + "_top"}, place = "axis",
				hardness = 2.0, tool = "axe", sound = wood_p.sound, flammable = not nether, tabs = B_NAT,
				tags = ["logs", n + "_logs"]})
			_add(n + "_" + woodn, {tex = {all = n + "_" + stem}, place = "axis", hardness = 2.0, tool = "axe",
				sound = wood_p.sound, flammable = not nether, tabs = B_BUILD, tags = ["logs", n + "_logs"]})
			_add("stripped_" + n + "_" + stem, {tex = {side = "stripped_" + n + "_" + stem, end = "stripped_" + n + "_" + stem + "_top"},
				place = "axis", hardness = 2.0, tool = "axe", sound = wood_p.sound, flammable = not nether, tabs = B_BUILD,
				tags = ["logs", n + "_logs"]})
			_add("stripped_" + n + "_" + woodn, {tex = {all = "stripped_" + n + "_" + stem}, place = "axis", hardness = 2.0,
				tool = "axe", sound = wood_p.sound, flammable = not nether, tabs = B_BUILD, tags = ["logs", n + "_logs"]})
		var pp: Dictionary = wood_p.duplicate()
		pp["tags"] = ["planks"]
		_add(planks, pp)
		if bamboo:
			var pm: Dictionary = wood_p.duplicate()
			_add("bamboo_mosaic", pm)
		if not nether and not bamboo:
			_add(n + "_leaves", {model = BlockDB.M_LEAVES, render = BlockDB.R_CUTOUT, hardness = 0.2, tool = "hoe",
				sound = "grass", opacity = 1, tint = w.tint, tick = "leaves", drop = "@leaves_" + n, flammable = true,
				tabs = B_NAT, tags = ["leaves"], full = false, solid = true})
			var sap: String = w.get("sapling", n + "_sapling")
			_add(sap, {model = BlockDB.M_CROSS, hardness = 0.0, sound = "grass", tick = "sapling", place = "plant",
				tabs = B_NAT, icon = "texture:" + sap, props = {tree = n}, tags = ["saplings"]})
		var fam := {hardness = 2.0, resistance = 3.0, tool = "axe", sound = wood_p.sound, flammable = not nether}
		for part in [["_stairs", BlockDB.M_STAIRS, "stairs", B_BUILD], ["_slab", BlockDB.M_SLAB, "slab", B_BUILD]]:
			var q := fam.duplicate()
			q.merge({model = part[1], tex = {all = planks}, place = part[2], tabs = part[3]}, true)
			if part[2] == "slab":
				q["drop"] = "@slab"
			_add(n + part[0], q)
		if bamboo:
			for part in [["_stairs", BlockDB.M_STAIRS, "stairs"], ["_slab", BlockDB.M_SLAB, "slab"]]:
				var q := fam.duplicate()
				q.merge({model = part[1], tex = {all = "bamboo_mosaic"}, place = part[2], tabs = B_BUILD}, true)
				if part[2] == "slab":
					q["drop"] = "@slab"
				_add("bamboo_mosaic" + part[0], q)
		var f := fam.duplicate()
		f.merge({model = BlockDB.M_FENCE, tex = {all = planks}, tabs = B_BUILD, tags = ["fences", "wooden_fences"]}, true)
		_add(n + "_fence", f)
		var g := fam.duplicate()
		g.merge({model = BlockDB.M_FENCE_GATE, tex = {all = planks}, place = "facing", interact = "gate", tabs = B_RED}, true)
		_add(n + "_fence_gate", g)
		var dr := fam.duplicate()
		dr.merge({model = BlockDB.M_DOOR, hardness = 3.0, tex = {top = n + "_door_top", bottom = n + "_door_bottom"},
			place = "door", interact = "door", tabs = B_RED, icon = "item:" + n + "_door", drop = "@door"}, true)
		_add(n + "_door", dr)
		var td := fam.duplicate()
		td.merge({model = BlockDB.M_TRAPDOOR, hardness = 3.0, tex = {all = n + "_trapdoor"}, place = "trapdoor",
			interact = "trapdoor", tabs = B_RED}, true)
		_add(n + "_trapdoor", td)
		_add(n + "_button", {model = BlockDB.M_BUTTON, tex = {all = planks}, hardness = 0.5, tool = "axe", sound = wood_p.sound,
			place = "button", interact = "button", tabs = B_RED, props = {wooden = true}, icon = "model"})
		_add(n + "_pressure_plate", {model = BlockDB.M_PLATE, tex = {all = planks}, hardness = 0.5, tool = "axe",
			sound = wood_p.sound, place = "plate", tabs = B_RED, props = {plate = "wood"}, tick = "plate"})
		_add(n + "_shelf", {model = BlockDB.M_SHELF, tex = {all = n + "_shelf", side = planks}, hardness = 2.0, tool = "axe",
			sound = wood_p.sound, place = "facing", interact = "shelf", tabs = B_FUNC, flammable = not nether})
	_add("mangrove_roots", {render = BlockDB.R_CUTOUT, tex = {top = "mangrove_roots_top", bottom = "mangrove_roots_top",
		side = "mangrove_roots_side"}, hardness = 0.7, tool = "axe", sound = "wood", opacity = 1, tabs = B_NAT, full = false})
	_add("muddy_mangrove_roots", {tex = {side = "muddy_mangrove_roots_side", end = "muddy_mangrove_roots_top"},
		place = "axis", hardness = 0.7, tool = "shovel", sound = "mud", tabs = B_NAT})
	_add("bookshelf", {tex = {top = "oak_planks", bottom = "oak_planks", side = "bookshelf"}, hardness = 1.5, tool = "axe",
		drop = "book:3-3", flammable = true, sound = "wood", tabs = B_FUNC})
	_add("chiseled_bookshelf", {tex = {top = "chiseled_bookshelf_top", bottom = "chiseled_bookshelf_top",
		side = "chiseled_bookshelf_side", front = "chiseled_bookshelf_empty", front_on = "chiseled_bookshelf_occupied"},
		place = "facing_lit", hardness = 1.5, tool = "axe", flammable = true, sound = "wood", tabs = B_FUNC,
		interact = "chiseled_bookshelf"})


# ------------------------------------------------------------------------------------------------
func _plant(bname: String, p: Dictionary = {}) -> BlockDef:
	var q := {model = BlockDB.M_CROSS, hardness = 0.0, sound = "grass", place = "plant", tabs = B_NAT,
		icon = "texture:" + bname, replaceable = false, flammable = true}
	q.merge(p, true)
	return _add(bname, q)


func _plants() -> void:
	_plant("short_grass", {tint = BlockDB.T_GRASS, replaceable = true, drop = "@grass", icon = "texture:short_grass",
		tags = ["replaceable_plants"]})
	_plant("fern", {tint = BlockDB.T_GRASS, replaceable = true, drop = "@grass"})
	_plant("tall_grass", {model = BlockDB.M_TALL_CROSS, tex = {top = "tall_grass_top", bottom = "tall_grass_bottom"},
		tint = BlockDB.T_GRASS, replaceable = true, drop = "@grass", place = "tall_plant", icon = "texture:tall_grass_top"})
	_plant("large_fern", {model = BlockDB.M_TALL_CROSS, tex = {top = "large_fern_top", bottom = "large_fern_bottom"},
		tint = BlockDB.T_GRASS, replaceable = true, drop = "@grass", place = "tall_plant", icon = "texture:large_fern_top"})
	_plant("dead_bush", {drop = "stick:0-2", replaceable = true, place = "dry_plant"})
	_plant("bush", {tint = BlockDB.T_GRASS, replaceable = true})
	_plant("firefly_bush", {light = 2, replaceable = false, tick = "firefly"})
	_plant("short_dry_grass", {replaceable = true, place = "dry_plant"})
	_plant("tall_dry_grass", {replaceable = true, place = "dry_plant"})
	var flowers := ["dandelion", "poppy", "blue_orchid", "allium", "azure_bluet", "red_tulip", "orange_tulip",
		"white_tulip", "pink_tulip", "oxeye_daisy", "cornflower", "lily_of_the_valley", "wither_rose", "torchflower",
		"closed_eyeblossom", "open_eyeblossom", "cactus_flower"]
	for fl in flowers:
		var fp := {tags = ["flowers", "small_flowers"]}
		if fl == "open_eyeblossom":
			fp["light"] = 3
		if fl == "cactus_flower":
			fp["place"] = "cactus_flower"
		_plant(fl, fp)
	for tf in ["sunflower", "lilac", "rose_bush", "peony", "pitcher_plant"]:
		_plant(tf, {model = BlockDB.M_TALL_CROSS, tex = {top = tf + "_top", bottom = tf + "_bottom"}, place = "tall_plant",
			icon = "texture:" + tf + "_top", tags = ["flowers", "tall_flowers"], drop = "@tall_plant"})
	_add("pink_petals", {model = BlockDB.M_SHORT, tex = {all = "pink_petals"}, hardness = 0.0, sound = "grass", solid = false,
		place = "plant", tabs = B_NAT, replaceable = true, icon = "texture:pink_petals", props = {flat = true}})
	_add("wildflowers", {model = BlockDB.M_SHORT, tex = {all = "wildflowers"}, hardness = 0.0, sound = "grass", solid = false,
		place = "plant", tabs = B_NAT, replaceable = true, icon = "texture:wildflowers", props = {flat = true}})
	_add("leaf_litter", {model = BlockDB.M_SHORT, tex = {all = "leaf_litter"}, hardness = 0.0, sound = "grass", solid = false,
		place = "plant", tabs = B_NAT, replaceable = true, icon = "texture:leaf_litter", props = {flat = true}})
	_plant("brown_mushroom", {light = 1, place = "mushroom", tick = "mushroom", flammable = false})
	_plant("red_mushroom", {place = "mushroom", tick = "mushroom", flammable = false})
	_add("brown_mushroom_block", {hardness = 0.2, tool = "axe", drop = "brown_mushroom:0-2", sound = "wood", tabs = B_NAT})
	_add("red_mushroom_block", {hardness = 0.2, tool = "axe", drop = "red_mushroom:0-2", sound = "wood", tabs = B_NAT})
	_add("mushroom_stem", {hardness = 0.2, tool = "axe", drop = "-", sound = "wood", tabs = B_NAT})
	_add("sugar_cane", {model = BlockDB.M_CROSS, hardness = 0.0, sound = "grass", tint = BlockDB.T_GRASS, place = "sugar_cane",
		tick = "sugar_cane", tabs = B_NAT, icon = "item:sugar_cane"})
	_add("cactus", {model = BlockDB.M_CACTUS, tex = {top = "cactus_top", bottom = "cactus_bottom", side = "cactus_side"},
		hardness = 0.4, sound = "wool", place = "cactus", tick = "cactus", damage = 1.0, tabs = B_NAT, icon = "model"})
	_add("bamboo", {model = BlockDB.M_BAMBOO, tex = {all = "bamboo_stalk", leaves = "bamboo_leaves"}, hardness = 1.0,
		tool = "sword", sound = "bamboo", place = "bamboo", tick = "bamboo", tabs = B_NAT, icon = "item:bamboo"})
	# crops
	_add("wheat", {model = BlockDB.M_CROP, tex = {all = "wheat_stage7"}, hardness = 0.0, sound = "crop", has_item = false,
		place = "crop", tick = "crop", drop = "@wheat", props = {stages = ["wheat_stage0", "wheat_stage1", "wheat_stage2",
		"wheat_stage3", "wheat_stage4", "wheat_stage5", "wheat_stage6", "wheat_stage7"], item = "wheat_seeds", max_age = 7}})
	_add("carrots", {model = BlockDB.M_CROP, tex = {all = "carrots_stage3"}, hardness = 0.0, sound = "crop", has_item = false,
		place = "crop", tick = "crop", drop = "@carrots", props = {stages = ["carrots_stage0", "carrots_stage0", "carrots_stage1",
		"carrots_stage1", "carrots_stage2", "carrots_stage2", "carrots_stage2", "carrots_stage3"], item = "carrot", max_age = 7}})
	_add("potatoes", {model = BlockDB.M_CROP, tex = {all = "potatoes_stage3"}, hardness = 0.0, sound = "crop", has_item = false,
		place = "crop", tick = "crop", drop = "@potatoes", props = {stages = ["potatoes_stage0", "potatoes_stage0",
		"potatoes_stage1", "potatoes_stage1", "potatoes_stage2", "potatoes_stage2", "potatoes_stage2", "potatoes_stage3"],
		item = "potato", max_age = 7}})
	_add("beetroots", {model = BlockDB.M_CROP, tex = {all = "beetroots_stage3"}, hardness = 0.0, sound = "crop", has_item = false,
		place = "crop", tick = "crop", drop = "@beetroots", props = {stages = ["beetroots_stage0", "beetroots_stage1",
		"beetroots_stage2", "beetroots_stage3"], item = "beetroot_seeds", max_age = 3}})
	_add("melon_stem", {model = BlockDB.M_CROP, tex = {all = "melon_stem"}, hardness = 0.0, sound = "crop", has_item = false,
		place = "crop", tick = "stem", tint = BlockDB.T_STEM, drop = "melon_seeds:0-1", props = {item = "melon_seeds",
		max_age = 7, fruit = "melon", stem = true}})
	_add("pumpkin_stem", {model = BlockDB.M_CROP, tex = {all = "pumpkin_stem"}, hardness = 0.0, sound = "crop", has_item = false,
		place = "crop", tick = "stem", tint = BlockDB.T_STEM, drop = "pumpkin_seeds:0-1", props = {item = "pumpkin_seeds",
		max_age = 7, fruit = "pumpkin", stem = true}})
	_add("sweet_berry_bush", {model = BlockDB.M_CROP, tex = {all = "sweet_berry_bush_stage3"}, hardness = 0.0, sound = "grass",
		has_item = false, place = "plant", tick = "crop", interact = "berry_bush", drop = "@sweet_berry_bush",
		props = {stages = ["sweet_berry_bush_stage0", "sweet_berry_bush_stage1", "sweet_berry_bush_stage2",
		"sweet_berry_bush_stage3"], item = "sweet_berries", max_age = 3, cross = true, thorns = true}})
	_add("pumpkin", {tex = {top = "pumpkin_top", bottom = "pumpkin_top", side = "pumpkin_side"}, hardness = 1.0, tool = "axe",
		sound = "wood", tabs = B_NAT, interact = "pumpkin"})
	_add("carved_pumpkin", {tex = {top = "pumpkin_top", bottom = "pumpkin_top", side = "pumpkin_side", front = "carved_pumpkin"},
		place = "facing", hardness = 1.0, tool = "axe", sound = "wood", tabs = B_NAT})
	_add("jack_o_lantern", {tex = {top = "pumpkin_top", bottom = "pumpkin_top", side = "pumpkin_side", front = "jack_o_lantern"},
		place = "facing", hardness = 1.0, tool = "axe", sound = "wood", light = 15, tabs = B_BUILD})
	_add("melon", {tex = {top = "melon_top", bottom = "melon_top", side = "melon_side"}, hardness = 1.0, tool = "axe",
		drop = "melon_slice:3-7", sound = "wood", tabs = B_NAT})
	_add("hay_block", {tex = {side = "hay_block_side", end = "hay_block_top"}, place = "axis", hardness = 0.5, tool = "hoe",
		sound = "grass", flammable = true, tabs = B_BUILD})
	_add("dried_kelp_block", {tex = {top = "dried_kelp_top", bottom = "dried_kelp_top", side = "dried_kelp_side"},
		hardness = 0.5, tool = "hoe", sound = "grass", tabs = B_BUILD})
	# water plants
	_add("kelp", {model = BlockDB.M_CROSS, tex = {all = "kelp"}, hardness = 0.0, sound = "grass", waterlogged = true,
		place = "kelp", tick = "kelp", tabs = B_NAT, icon = "item:kelp", drop = "kelp", opacity = 1})
	_add("seagrass", {model = BlockDB.M_CROSS, tex = {all = "seagrass"}, hardness = 0.0, sound = "grass", waterlogged = true,
		place = "seagrass", tabs = B_NAT, drop = "-", replaceable = true, opacity = 1, icon = "texture:seagrass"})
	_add("sea_pickle", {model = BlockDB.M_PICKLE, tex = {all = "sea_pickle"}, hardness = 0.0, sound = "slime",
		place = "pickle", tabs = B_NAT, props = {light_meta = [6, 9, 12, 15, 6, 9, 12, 15]},
		icon = "item:sea_pickle", waterlogged = true, opacity = 1})
	_add("lily_pad", {model = BlockDB.M_LILY, tex = {all = "lily_pad"}, hardness = 0.0, sound = "grass", tint = BlockDB.T_LILY,
		place = "lily_pad", tabs = B_NAT, icon = "texture:lily_pad"})
	for coral in ["tube", "brain", "bubble", "fire", "horn"]:
		_add(coral + "_coral_block", {hardness = 1.5, tool = "pickaxe", needs_tool = true, tier = 1, sound = "stone",
			tabs = B_NAT, tick = "coral"})
		_add("dead_" + coral + "_coral_block", {hardness = 1.5, tool = "pickaxe", needs_tool = true, tier = 1, sound = "stone",
			tabs = B_NAT})
		_add(coral + "_coral", {model = BlockDB.M_CROSS, hardness = 0.0, sound = "grass", waterlogged = true, place = "coral",
			tabs = B_NAT, drop = "-", icon = "texture:" + coral + "_coral", opacity = 1})
	_add("sponge", {hardness = 0.6, tool = "hoe", sound = "grass", tabs = B_BUILD, place = "sponge"})
	_add("wet_sponge", {hardness = 0.6, tool = "hoe", sound = "grass", tabs = B_BUILD})
	# vines & cave plants
	_add("vine", {model = BlockDB.M_VINE, tex = {all = "vine"}, hardness = 0.2, tool = "shears", sound = "vine",
		tint = BlockDB.T_FOLIAGE, place = "vine", tick = "vine", climb = true, replaceable = true, drop = "-", flammable = true,
		tabs = B_NAT, icon = "texture:vine"})
	_add("glow_lichen", {model = BlockDB.M_VINE, tex = {all = "glow_lichen"}, hardness = 0.2, tool = "shears", sound = "vine",
		place = "vine", light = 7, replaceable = true, drop = "-", tabs = B_NAT, icon = "texture:glow_lichen"})
	_add("hanging_roots", {model = BlockDB.M_CROSS, hardness = 0.0, sound = "grass", place = "hanging", tabs = B_NAT,
		replaceable = true, drop = "-", icon = "texture:hanging_roots"})
	_add("spore_blossom", {model = BlockDB.M_HANGING, tex = {all = "spore_blossom"}, hardness = 0.0, sound = "grass",
		place = "hanging", tabs = B_NAT, icon = "texture:spore_blossom"})
	_add("cave_vines", {model = BlockDB.M_CROSS, tex = {all = "cave_vines", on = "cave_vines_lit"}, place = "hanging",
		hardness = 0.0, sound = "vine", climb = true, light = 14, has_item = false, interact = "cave_vines",
		drop = "@cave_vines", tabs = B_NAT, tick = "cave_vines", props = {lit_bit = 1, item = "glow_berries"}})
	_add("azalea", {model = BlockDB.M_CROSS, hardness = 0.0, sound = "grass", place = "plant", tabs = B_NAT,
		tick = "sapling", props = {tree = "azalea"}, icon = "texture:azalea"})
	_add("flowering_azalea", {model = BlockDB.M_CROSS, hardness = 0.0, sound = "grass", place = "plant", tabs = B_NAT,
		tick = "sapling", props = {tree = "azalea"}, icon = "texture:flowering_azalea"})
	_add("azalea_leaves", {model = BlockDB.M_LEAVES, render = BlockDB.R_CUTOUT, hardness = 0.2, tool = "hoe", sound = "grass",
		opacity = 1, tick = "leaves", drop = "@leaves_azalea", tabs = B_NAT, full = false, tags = ["leaves"]})
	_add("flowering_azalea_leaves", {model = BlockDB.M_LEAVES, render = BlockDB.R_CUTOUT, hardness = 0.2, tool = "hoe",
		sound = "grass", opacity = 1, tick = "leaves", drop = "@leaves_azalea", tabs = B_NAT, full = false, tags = ["leaves"]})
	_add("pale_hanging_moss", {model = BlockDB.M_CROSS, hardness = 0.0, sound = "moss", place = "hanging", tabs = B_NAT,
		icon = "texture:pale_hanging_moss", replaceable = true})
	_add("creaking_heart", {tex = {side = "creaking_heart", end = "creaking_heart_top", on = "creaking_heart_active"},
		place = "axis", hardness = 10.0, tool = "axe", sound = "wood", tabs = B_NAT, xp = Vector2i(20, 24), tick = "creaking_heart",
		drop = "resin_clump:1-3"})
	_add("resin_clump", {model = BlockDB.M_VINE, tex = {all = "resin_clump"}, hardness = 0.0, sound = "grass", place = "vine",
		replaceable = true, tabs = B_NAT, icon = "item:resin_clump", has_item = false, props = {item = "resin_clump"}})
	_add("resin_block", {hardness = 0.0, sound = "slime", tabs = B_BUILD})
	_stone_block("resin_bricks", {})
	_stone_block("chiseled_resin_bricks", {})
	_family("resin_brick", "resin_bricks", true, true, true)
	_add("bee_nest", {tex = {top = "bee_nest_top", bottom = "bee_nest_bottom", side = "bee_nest_side", front = "bee_nest_front"},
		place = "facing", hardness = 0.3, tool = "axe", sound = "wood", tabs = B_NAT, flammable = true})
	_add("beehive", {tex = {top = "beehive_end", bottom = "beehive_end", side = "beehive_side", front = "beehive_front"},
		place = "facing", hardness = 0.6, tool = "axe", sound = "wood", tabs = B_FUNC, flammable = true})
	_add("honey_block", {render = BlockDB.R_TRANSLUCENT, tex = {top = "honey_block_top", bottom = "honey_block_bottom",
		side = "honey_block_side"}, hardness = 0.0, sound = "slime", opacity = 1, speed = 0.4, jump = 0.5, tabs = B_RED,
		props = {honey = true}})
	_add("honeycomb_block", {hardness = 0.6, sound = "coral", tabs = B_BUILD})
	_add("slime_block", {render = BlockDB.R_TRANSLUCENT, hardness = 0.0, sound = "slime", opacity = 1, slip = 0.8,
		tabs = B_RED, props = {bouncy = true}})
	_add("turtle_egg", {model = BlockDB.M_EGG, tex = {all = "turtle_egg"}, hardness = 0.5, sound = "stone", tabs = B_NAT,
		props = {kind = "turtle"}, icon = "item:turtle_egg"})
	_add("sniffer_egg", {model = BlockDB.M_EGG, tex = {all = "sniffer_egg"}, hardness = 0.5, sound = "stone", tabs = B_NAT,
		props = {kind = "sniffer"}, icon = "model"})


# ------------------------------------------------------------------------------------------------
func _colored() -> void:
	for c in DYES:
		_add(c + "_wool", {hardness = 0.8, tool = "shears", sound = "wool", flammable = true, tabs = B_COLOR, tags = ["wool"]})
		_add(c + "_carpet", {model = BlockDB.M_CARPET, tex = {all = c + "_wool"}, hardness = 0.1, sound = "wool",
			flammable = true, tabs = B_COLOR, place = "carpet", tags = ["carpets"]})
		_add(c + "_terracotta", {hardness = 1.25, resistance = 4.2, tool = "pickaxe", needs_tool = true, tier = 1,
			tabs = B_COLOR, tags = ["terracotta"]})
		_add(c + "_glazed_terracotta", {tex = {all = c + "_glazed_terracotta"}, place = "glazed", hardness = 1.4,
			tool = "pickaxe", needs_tool = true, tier = 1, tabs = B_COLOR})
		_add(c + "_concrete", {hardness = 1.8, tool = "pickaxe", needs_tool = true, tier = 1, tabs = B_COLOR})
		_add(c + "_concrete_powder", {hardness = 0.5, tool = "shovel", sound = "sand", gravity = true, tabs = B_COLOR,
			props = {concrete = c + "_concrete"}})
		_add(c + "_stained_glass", {render = BlockDB.R_TRANSLUCENT, hardness = 0.3, drop = "-", silk_only = true,
			sound = "glass", cull_same = true, tabs = B_COLOR, tags = ["glass"]})
		_add(c + "_stained_glass_pane", {model = BlockDB.M_PANE, render = BlockDB.R_TRANSLUCENT,
			tex = {all = c + "_stained_glass", edge = c + "_stained_glass_pane_top"}, hardness = 0.3, drop = "-",
			silk_only = true, sound = "glass", tabs = B_COLOR, icon = "texture:" + c + "_stained_glass"})
		_add(c + "_bed", {model = BlockDB.M_BED, tex = {top = c + "_bed_head_top", foot = c + "_bed_foot_top",
			side = c + "_bed_side", end = c + "_bed_head_end", foot_end = c + "_bed_foot_end", bottom = "oak_planks"},
			hardness = 0.2, sound = "wood", place = "bed", interact = "bed", tabs = B_FUNC, drop = "@bed", icon = "item:" + c + "_bed",
			tags = ["beds"], props = {color = c}})
		_add(c + "_candle", {model = BlockDB.M_CANDLE, tex = {all = c + "_candle"}, hardness = 0.1, sound = "wool",
			place = "candle", interact = "candle", tabs = B_COLOR, icon = "item:" + c + "_candle",
			light = 3, props = {light_meta = [0, 0, 0, 0, 3, 6, 9, 12, 0, 0, 0, 0, 3, 6, 9, 12]}, tags = ["candles"]})
		_add(c + "_shulker_box", {tex = {top = c + "_shulker_box_top", bottom = c + "_shulker_box_bottom",
			side = c + "_shulker_box_side"}, hardness = 2.0, tool = "pickaxe", place = "facing_all_shulker",
			interact = "shulker_box", tabs = B_COLOR, props = {container = 27, keep_contents = true}, drop = "@shulker_box",
			full = false, opacity = 0})
	_add("terracotta", {hardness = 1.25, resistance = 4.2, tool = "pickaxe", needs_tool = true, tier = 1, tabs = B_COLOR,
		tags = ["terracotta"]})
	_add("candle", {model = BlockDB.M_CANDLE, tex = {all = "candle"}, hardness = 0.1, sound = "wool", place = "candle",
		interact = "candle", tabs = B_COLOR, icon = "item:candle", light = 3,
		props = {light_meta = [0, 0, 0, 0, 3, 6, 9, 12, 0, 0, 0, 0, 3, 6, 9, 12]}, tags = ["candles"]})
	_add("shulker_box", {tex = {top = "shulker_box_top", bottom = "shulker_box_bottom", side = "shulker_box_side"},
		hardness = 2.0, tool = "pickaxe", place = "facing_all_shulker", interact = "shulker_box", tabs = B_COLOR,
		props = {container = 27, keep_contents = true}, drop = "@shulker_box", full = false, opacity = 0})


# ------------------------------------------------------------------------------------------------
func _nether() -> void:
	var nat := B_NAT
	_add("netherrack", {hardness = 0.4, tool = "pickaxe", needs_tool = true, tier = 1, sound = "netherrack", tabs = nat,
		props = {infiniburn = true}})
	_add("crimson_nylium", {tex = {top = "crimson_nylium", bottom = "netherrack", side = "crimson_nylium_side"},
		hardness = 0.4, tool = "pickaxe", needs_tool = true, tier = 1, drop = "netherrack", sound = "nylium", tabs = nat})
	_add("warped_nylium", {tex = {top = "warped_nylium", bottom = "netherrack", side = "warped_nylium_side"},
		hardness = 0.4, tool = "pickaxe", needs_tool = true, tier = 1, drop = "netherrack", sound = "nylium", tabs = nat})
	_add("soul_sand", {hardness = 0.5, tool = "shovel", sound = "soul_sand", speed = 0.4, tabs = nat, tags = ["soul"]})
	_add("soul_soil", {hardness = 0.5, tool = "shovel", sound = "soul_sand", tabs = nat, tags = ["soul"]})
	_add("basalt", {tex = {side = "basalt_side", end = "basalt_top"}, place = "axis", hardness = 1.25, resistance = 4.2,
		tool = "pickaxe", needs_tool = true, tier = 1, sound = "basalt", tabs = nat})
	_add("polished_basalt", {tex = {side = "polished_basalt_side", end = "polished_basalt_top"}, place = "axis",
		hardness = 1.25, resistance = 4.2, tool = "pickaxe", needs_tool = true, tier = 1, sound = "basalt", tabs = B_BUILD})
	_add("blackstone", {tex = {top = "blackstone_top", bottom = "blackstone_top", side = "blackstone"}, hardness = 1.5,
		resistance = 6.0, tool = "pickaxe", needs_tool = true, tier = 1, tabs = nat, tags = ["stone_tool_materials"]})
	_stone_block("gilded_blackstone", {drop = "@gilded_blackstone", tabs = nat})
	_stone_block("polished_blackstone", {hardness = 2.0})
	_stone_block("polished_blackstone_bricks", {})
	_stone_block("cracked_polished_blackstone_bricks", {})
	_stone_block("chiseled_polished_blackstone", {})
	_family("blackstone", "blackstone", true, true, true)
	_family("polished_blackstone", "polished_blackstone", true, true, true)
	_family("polished_blackstone_brick", "polished_blackstone_bricks", true, true, true)
	_add("magma_block", {tex = {all = "magma"}, hardness = 0.5, tool = "pickaxe", needs_tool = true, tier = 1, light = 3,
		damage = 1.0, tabs = nat, props = {magma = true, infiniburn = true}})
	_add("glowstone", {hardness = 0.3, drop = "glowstone_dust:2-4", sound = "glass", light = 15, tabs = B_BUILD,
		props = {fortune = true}})
	_stone_block("nether_bricks", {hardness = 2.0})
	_stone_block("cracked_nether_bricks", {hardness = 2.0})
	_stone_block("chiseled_nether_bricks", {hardness = 2.0})
	_stone_block("red_nether_bricks", {hardness = 2.0})
	_family("nether_brick", "nether_bricks", true, true, true)
	_family("red_nether_brick", "red_nether_bricks", true, true, true)
	_add("nether_brick_fence", {model = BlockDB.M_FENCE, tex = {all = "nether_bricks"}, hardness = 2.0, resistance = 6.0,
		tool = "pickaxe", needs_tool = true, tier = 1, tabs = B_BUILD, tags = ["fences"]})
	_add("nether_wart_block", {hardness = 1.0, tool = "hoe", sound = "wart", tabs = nat})
	_add("warped_wart_block", {hardness = 1.0, tool = "hoe", sound = "wart", tabs = nat})
	_add("shroomlight", {hardness = 1.0, tool = "hoe", light = 15, sound = "wart", tabs = nat})
	_add("nether_wart", {model = BlockDB.M_CROP, tex = {all = "nether_wart_stage2"}, hardness = 0.0, sound = "crop",
		has_item = false, place = "nether_wart", tick = "crop", drop = "@nether_wart", props = {stages = ["nether_wart_stage0",
		"nether_wart_stage1", "nether_wart_stage1", "nether_wart_stage2"], item = "nether_wart", max_age = 3, cross = false}})
	for pl in ["crimson_fungus", "warped_fungus"]:
		_plant(pl, {place = "nether_plant", tick = "sapling", props = {tree = pl}, flammable = false})
	for pl in ["crimson_roots", "warped_roots", "nether_sprouts"]:
		_plant(pl, {place = "nether_plant", replaceable = true, flammable = false})
	_add("weeping_vines", {model = BlockDB.M_CROSS, tex = {all = "weeping_vines"}, hardness = 0.0, sound = "vine",
		place = "hanging", climb = true, tabs = nat, icon = "texture:weeping_vines", tick = "weeping_vines"})
	_add("twisting_vines", {model = BlockDB.M_CROSS, tex = {all = "twisting_vines"}, hardness = 0.0, sound = "vine",
		place = "plant_up", climb = true, tabs = nat, icon = "texture:twisting_vines"})
	_add("nether_portal", {model = BlockDB.M_PORTAL, render = BlockDB.R_TRANSLUCENT, tex = {all = "nether_portal"},
		hardness = -1.0, light = 11, has_item = false, drop = "-", solid = false, tick = "portal"})
	_add("fire", {model = BlockDB.M_FIRE, tex = {all = "fire"}, hardness = 0.0, light = 15, has_item = false, drop = "-",
		replaceable = true, tick = "fire", damage = 1.0, props = {fire = true}})
	_add("soul_fire", {model = BlockDB.M_FIRE, tex = {all = "soul_fire"}, hardness = 0.0, light = 10, has_item = false,
		drop = "-", replaceable = true, damage = 2.0, props = {fire = true}})
	_add("respawn_anchor", {tex = {top = "respawn_anchor_top", bottom = "respawn_anchor_bottom", side = "respawn_anchor_side"},
		hardness = 50.0, resistance = 1200.0, tool = "pickaxe", needs_tool = true, tier = 4, light = 0, tabs = B_FUNC,
		interact = "respawn_anchor", place = "anchor", props = {light_meta = [0, 3, 7, 11, 15]}})
	_add("lodestone", {tex = {top = "lodestone_top", bottom = "lodestone_top", side = "lodestone_side"}, hardness = 3.5,
		tool = "pickaxe", needs_tool = true, tier = 1, sound = "metal", tabs = B_FUNC})


func _end() -> void:
	_add("end_stone", {hardness = 3.0, resistance = 9.0, tool = "pickaxe", needs_tool = true, tier = 1, tabs = B_NAT})
	_stone_block("end_stone_bricks", {hardness = 3.0, resistance = 9.0})
	_family("end_stone_brick", "end_stone_bricks", true, true, true)
	_stone_block("purpur_block", {})
	_stone_block("purpur_pillar", {tex = {side = "purpur_pillar", end = "purpur_pillar_top"}, place = "axis"})
	_family("purpur", "purpur_block", true, true, false)
	_add("end_rod", {model = BlockDB.M_ROD, tex = {all = "end_rod"}, hardness = 0.0, light = 14, place = "facing_all",
		tabs = B_FUNC, icon = "item:end_rod"})
	_add("chorus_plant", {model = BlockDB.M_CHORUS, tex = {all = "chorus_plant"}, hardness = 0.4, tool = "axe", sound = "wood",
		drop = "@chorus_plant", tabs = B_NAT, place = "chorus"})
	_add("chorus_flower", {model = BlockDB.M_CHORUS, tex = {all = "chorus_flower", on = "chorus_flower_dead"}, hardness = 0.4,
		tool = "axe", sound = "wood", tabs = B_NAT, place = "chorus", tick = "chorus_flower", props = {flower = true}})
	_add("end_portal_frame", {model = BlockDB.M_END_FRAME, tex = {top = "end_portal_frame_top", bottom = "end_stone",
		side = "end_portal_frame_side", eye = "end_portal_frame_eye"}, hardness = -1.0, light = 1, place = "facing",
		interact = "end_frame", tabs = B_FUNC, icon = "model", opacity = 0})
	_add("end_portal", {model = BlockDB.M_END_PORTAL, render = BlockDB.R_OPAQUE, tex = {all = "end_portal"}, hardness = -1.0,
		light = 15, has_item = false, drop = "-", solid = false, full = false, opacity = 0})
	_add("end_gateway", {model = BlockDB.M_END_PORTAL, render = BlockDB.R_OPAQUE, tex = {all = "end_portal"}, hardness = -1.0,
		light = 15, has_item = false, drop = "-", solid = false, full = false, opacity = 0, props = {gateway = true}})
	_add("dragon_egg", {model = BlockDB.M_EGG, tex = {all = "dragon_egg"}, hardness = 3.0, resistance = 9.0, light = 1,
		gravity = true, interact = "dragon_egg", tabs = B_NAT, props = {kind = "dragon"}, icon = "model"})


# ------------------------------------------------------------------------------------------------
const OXIDATION := ["", "exposed_", "weathered_", "oxidized_"]


func _copper() -> void:
	var i := 0
	for ox in OXIDATION:
		var blockn: String = "copper_block" if ox == "" else ox + "copper"
		var cp := {hardness = 3.0, resistance = 6.0, tool = "pickaxe", needs_tool = true, tier = 2, sound = "copper",
			tabs = B_BUILD, tick = "oxidize" if i < 3 else "", props = {oxidation = i}}
		for wax in ["", "waxed_"]:
			var q: Dictionary = cp.duplicate(true)
			if wax != "":
				q["tick"] = ""
				q["props"]["waxed"] = true
			var t_block: String = blockn
			_add(wax + blockn, _m(q, {tex = {all = t_block}}))
			_add(wax + ox + "cut_copper", _m(q, {tex = {all = ox + "cut_copper"}}))
			_add(wax + ox + "chiseled_copper", _m(q, {tex = {all = ox + "chiseled_copper"}}))
			_add(wax + ox + "copper_grate", _m(q, {tex = {all = ox + "copper_grate"}, render = BlockDB.R_CUTOUT,
				cull_same = true, full = false, opacity = 0}))
			_add(wax + ox + "copper_bulb", _m(q, {tex = {all = ox + "copper_bulb", on = ox + "copper_bulb_lit"}, place = "lit",
				light = [15, 12, 8, 4][i], tabs = B_RED, props = {oxidation = i, lit_bit = 1, waxed = wax != ""}}))
			_add(wax + ox + "cut_copper_stairs", _m(q, {model = BlockDB.M_STAIRS, tex = {all = ox + "cut_copper"}, place = "stairs"}))
			_add(wax + ox + "cut_copper_slab", _m(q, {model = BlockDB.M_SLAB, tex = {all = ox + "cut_copper"}, place = "slab",
				drop = "@slab"}))
			_add(wax + ox + "copper_door", _m(q, {model = BlockDB.M_DOOR, tex = {top = ox + "copper_door_top",
				bottom = ox + "copper_door_bottom"}, place = "door", interact = "door", tabs = B_RED,
				icon = "item:" + ox + "copper_door", drop = "@door"}))
			_add(wax + ox + "copper_trapdoor", _m(q, {model = BlockDB.M_TRAPDOOR, tex = {all = ox + "copper_trapdoor"},
				place = "trapdoor", interact = "trapdoor", tabs = B_RED}))
			_add(wax + ox + "copper_chest", _m(q, {model = BlockDB.M_CHEST, tex = {all = ox + "copper_chest"}, place = "chest",
				interact = "chest", tabs = B_FUNC, icon = "model", full = false, opacity = 0,
				props = {container = 27, oxidation = i, copper_chest = true, waxed = wax != ""}}))
		_add(ox + "copper_bars", {model = BlockDB.M_PANE, tex = {all = ox + "copper_bars", edge = ox + "copper_bars"},
			hardness = 5.0, tool = "pickaxe", needs_tool = true, tier = 1, sound = "copper", tabs = B_BUILD, render = BlockDB.R_CUTOUT})
		_add(ox + "copper_chain", {model = BlockDB.M_CHAIN, tex = {all = ox + "copper_chain"}, place = "axis", hardness = 5.0,
			tool = "pickaxe", needs_tool = true, tier = 1, sound = "chain", tabs = B_BUILD, icon = "item:" + ox + "copper_chain"})
		_add(ox + "copper_lantern", {model = BlockDB.M_LANTERN, tex = {all = ox + "copper_lantern"}, hardness = 3.5,
			tool = "pickaxe", place = "lantern", light = 15, sound = "lantern", tabs = B_FUNC, icon = "item:" + ox + "copper_lantern"})
		_add(ox + "lightning_rod", {model = BlockDB.M_ROD, tex = {all = ox + "lightning_rod"}, hardness = 3.0, tool = "pickaxe",
			needs_tool = true, tier = 1, sound = "copper", place = "facing_all", tabs = B_RED, icon = "item:" + ox + "lightning_rod"})
		i += 1
	_add("copper_torch", {model = BlockDB.M_TORCH, tex = {all = "copper_torch"}, hardness = 0.0, light = 14, place = "torch",
		tabs = B_FUNC, icon = "texture:copper_torch", sound = "wood", drop = "copper_torch"})


func _m(a: Dictionary, b: Dictionary) -> Dictionary:
	var r := a.duplicate(true)
	r.merge(b, true)
	return r


# ------------------------------------------------------------------------------------------------
func _functional() -> void:
	var F := B_FUNC
	_add("crafting_table", {tex = {top = "crafting_table_top", bottom = "oak_planks", side = "crafting_table_side",
		front = "crafting_table_front"}, hardness = 2.5, tool = "axe", sound = "wood", interact = "crafting_table",
		flammable = true, tabs = F})
	var furn := {tool = "pickaxe", needs_tool = true, tier = 1, place = "facing_lit", tabs = F, light = 13,
		props = {lit_bit = 4, container = 3}}
	_add("furnace", _m(furn, {tex = {top = "furnace_top", bottom = "furnace_top", side = "furnace_side",
		front = "furnace_front", front_on = "furnace_front_on"}, hardness = 3.5, interact = "furnace"}))
	_add("blast_furnace", _m(furn, {tex = {top = "blast_furnace_top", bottom = "blast_furnace_top",
		side = "blast_furnace_side", front = "blast_furnace_front", front_on = "blast_furnace_front_on"}, hardness = 3.5,
		interact = "blast_furnace"}))
	_add("smoker", _m(furn, {tex = {top = "smoker_top", bottom = "smoker_bottom", side = "smoker_side", front = "smoker_front",
		front_on = "smoker_front_on"}, hardness = 3.5, tool = "axe", needs_tool = false, interact = "smoker"}))
	_add("chest", {model = BlockDB.M_CHEST, tex = {all = "chest"}, hardness = 2.5, tool = "axe", sound = "wood", place = "chest",
		interact = "chest", tabs = F, icon = "model", flammable = true, props = {container = 27}})
	_add("trapped_chest", {model = BlockDB.M_CHEST, tex = {all = "trapped_chest"}, hardness = 2.5, tool = "axe", sound = "wood",
		place = "chest", interact = "chest", tabs = B_RED, icon = "model", props = {container = 27, trapped = true}})
	_add("ender_chest", {model = BlockDB.M_CHEST, tex = {all = "ender_chest"}, hardness = 22.5, resistance = 600.0,
		tool = "pickaxe", needs_tool = true, tier = 1, place = "facing", interact = "ender_chest", light = 7, tabs = F,
		icon = "model", drop = "obsidian:8-8", props = {ender = true}})
	_add("barrel", {tex = {front = "barrel_top", front_on = "barrel_top_open", back = "barrel_bottom", side = "barrel_side"},
		place = "facing_all", hardness = 2.5, tool = "axe", sound = "wood", interact = "barrel", tabs = F, flammable = true,
		props = {container = 27}})
	for an in ["anvil", "chipped_anvil", "damaged_anvil"]:
		_add(an, {model = BlockDB.M_ANVIL, tex = {top = an + "_top", side = "anvil"}, hardness = 5.0, resistance = 1200.0,
			tool = "pickaxe", needs_tool = true, tier = 1, sound = "anvil", place = "facing_side", gravity = true,
			interact = "anvil", tabs = F, icon = "model"})
	_add("enchanting_table", {model = BlockDB.M_ENCHANT, tex = {top = "enchanting_table_top", bottom = "enchanting_table_bottom",
		side = "enchanting_table_side"}, hardness = 5.0, resistance = 1200.0, tool = "pickaxe", needs_tool = true, tier = 1,
		light = 7, interact = "enchanting_table", tabs = F, icon = "model", opacity = 0})
	_add("brewing_stand", {model = BlockDB.M_BREWING, tex = {all = "brewing_stand", base = "brewing_stand_base"},
		hardness = 0.5, tool = "pickaxe", needs_tool = true, tier = 1, light = 1, interact = "brewing_stand", tabs = F,
		icon = "item:brewing_stand", sound = "metal"})
	_add("smithing_table", {tex = {top = "smithing_table_top", bottom = "smithing_table_bottom", side = "smithing_table_side",
		front = "smithing_table_front"}, hardness = 2.5, tool = "axe", sound = "wood", interact = "smithing_table", tabs = F})
	_add("stonecutter", {model = BlockDB.M_STONECUTTER, tex = {top = "stonecutter_top", bottom = "stonecutter_bottom",
		side = "stonecutter_side", saw = "stonecutter_saw"}, hardness = 3.5, tool = "pickaxe", needs_tool = true, tier = 1,
		place = "facing", interact = "stonecutter", tabs = F, icon = "model", opacity = 0})
	_add("loom", {tex = {top = "loom_top", bottom = "loom_bottom", side = "loom_side", front = "loom_front"}, place = "facing",
		hardness = 2.5, tool = "axe", sound = "wood", tabs = F})
	_add("grindstone", {model = BlockDB.M_GRINDSTONE, tex = {all = "grindstone_side", round = "grindstone_round",
		pivot = "grindstone_pivot", leg = "dark_oak_log"}, hardness = 2.0, tool = "pickaxe", needs_tool = true, tier = 1,
		place = "facing", interact = "grindstone", tabs = F, icon = "model"})
	_add("composter", {model = BlockDB.M_CAULDRON, tex = {top = "composter_top", bottom = "composter_bottom",
		side = "composter_side", inner = "composter_compost", ready = "composter_ready"}, hardness = 0.6, tool = "axe",
		sound = "wood", interact = "composter", tabs = F, icon = "model", props = {kind = "composter"}})
	_add("lectern", {model = BlockDB.M_LECTERN, tex = {top = "lectern_top", base = "lectern_base", side = "lectern_sides",
		front = "lectern_front"}, hardness = 2.5, tool = "axe", sound = "wood", place = "facing", interact = "lectern", tabs = F,
		icon = "model"})
	_add("cartography_table", {tex = {top = "cartography_table_top", bottom = "dark_oak_planks",
		side = "cartography_table_side1", front = "cartography_table_side3"}, hardness = 2.5, tool = "axe", sound = "wood",
		tabs = F})
	_add("fletching_table", {tex = {top = "fletching_table_top", bottom = "birch_planks", side = "fletching_table_side",
		front = "fletching_table_front"}, hardness = 2.5, tool = "axe", sound = "wood", tabs = F})
	_add("cauldron", {model = BlockDB.M_CAULDRON, tex = {top = "cauldron_top", bottom = "cauldron_bottom", side = "cauldron_side",
		inner = "cauldron_inner"}, hardness = 2.0, tool = "pickaxe", needs_tool = true, tier = 1, sound = "metal",
		interact = "cauldron", tabs = F, icon = "item:cauldron", props = {kind = "cauldron",
		light_meta = [0, 0, 0, 0, 15, 15, 15, 15, 0, 0, 0, 0, 0, 0, 0, 0]}})
	_add("torch", {model = BlockDB.M_TORCH, tex = {all = "torch"}, hardness = 0.0, light = 14, place = "torch", tabs = F,
		icon = "texture:torch", sound = "wood"})
	_add("soul_torch", {model = BlockDB.M_TORCH, tex = {all = "soul_torch"}, hardness = 0.0, light = 10, place = "torch",
		tabs = F, icon = "texture:soul_torch", sound = "wood"})
	_add("lantern", {model = BlockDB.M_LANTERN, tex = {all = "lantern"}, hardness = 3.5, tool = "pickaxe", place = "lantern",
		light = 15, sound = "lantern", tabs = F, icon = "item:lantern"})
	_add("soul_lantern", {model = BlockDB.M_LANTERN, tex = {all = "soul_lantern"}, hardness = 3.5, tool = "pickaxe",
		place = "lantern", light = 10, sound = "lantern", tabs = F, icon = "item:soul_lantern"})
	_add("campfire", {model = BlockDB.M_CAMPFIRE, tex = {all = "campfire_log", fire = "campfire_fire", lit = "campfire_log_lit"},
		hardness = 2.0, tool = "axe", sound = "wood", place = "facing", interact = "campfire", light = 15, damage = 1.0,
		drop = "charcoal:2-2", tabs = F, icon = "item:campfire",
		props = {light_meta = [15, 15, 15, 15, 0, 0, 0, 0, 15, 15, 15, 15, 0, 0, 0, 0], campfire = true}})
	_add("soul_campfire", {model = BlockDB.M_CAMPFIRE, tex = {all = "campfire_log", fire = "soul_campfire_fire",
		lit = "soul_campfire_log_lit"}, hardness = 2.0, tool = "axe", sound = "wood", place = "facing", interact = "campfire",
		light = 10, damage = 2.0, drop = "soul_soil", tabs = F, icon = "item:soul_campfire",
		props = {light_meta = [10, 10, 10, 10, 0, 0, 0, 0, 10, 10, 10, 10, 0, 0, 0, 0], campfire = true}})
	_add("ladder", {model = BlockDB.M_LADDER, tex = {all = "ladder"}, hardness = 0.4, tool = "axe", sound = "ladder",
		place = "ladder", climb = true, tabs = F, icon = "texture:ladder"})
	_add("scaffolding", {model = BlockDB.M_SCAFFOLD, tex = {top = "scaffolding_top", bottom = "scaffolding_bottom",
		side = "scaffolding_side"}, hardness = 0.0, sound = "scaffold", climb = true, tabs = F, place = "scaffolding",
		icon = "model", gravity = false})
	_add("glass", {render = BlockDB.R_CUTOUT, hardness = 0.3, drop = "-", silk_only = true, sound = "glass", cull_same = true,
		tabs = B_BUILD, full = false, opacity = 0, tags = ["glass"]})
	_add("tinted_glass", {render = BlockDB.R_TRANSLUCENT, hardness = 0.3, sound = "glass", cull_same = true, tabs = B_BUILD,
		full = false, opacity = 15})
	_add("glass_pane", {model = BlockDB.M_PANE, tex = {all = "glass", edge = "glass_pane_top"}, hardness = 0.3, drop = "-",
		silk_only = true, sound = "glass", tabs = B_BUILD, icon = "texture:glass"})
	_add("iron_bars", {model = BlockDB.M_PANE, tex = {all = "iron_bars", edge = "iron_bars"}, hardness = 5.0, tool = "pickaxe",
		needs_tool = true, tier = 1, sound = "metal", tabs = B_BUILD, icon = "texture:iron_bars"})
	_add("chain", {model = BlockDB.M_CHAIN, tex = {all = "chain"}, place = "axis", hardness = 5.0, tool = "pickaxe",
		needs_tool = true, tier = 1, sound = "chain", tabs = B_BUILD, icon = "item:chain"})
	_add("jukebox", {tex = {top = "jukebox_top", bottom = "jukebox_side", side = "jukebox_side"}, hardness = 2.0, tool = "axe",
		sound = "wood", interact = "jukebox", tabs = F})
	_add("spawner", {render = BlockDB.R_CUTOUT, hardness = 5.0, tool = "pickaxe", needs_tool = true, tier = 1, drop = "-",
		xp = Vector2i(15, 43), sound = "metal", tabs = ["spawn_eggs"], full = false, opacity = 1, tick = "spawner",
		interact = "spawner"})
	_add("beacon", {model = BlockDB.M_BEACON, tex = {all = "glass", core = "beacon", base = "obsidian"}, hardness = 3.0,
		light = 15, interact = "beacon", sound = "glass", tabs = F, icon = "model", opacity = 0})
	_add("bell", {model = BlockDB.M_BELL, tex = {all = "bell_body", post = "stone", bar = "dark_oak_planks"}, hardness = 5.0,
		tool = "pickaxe", place = "facing", interact = "bell", sound = "metal", tabs = F, icon = "item:bell"})
	_add("flower_pot", {model = BlockDB.M_POT, tex = {all = "flower_pot", dirt = "dirt"}, hardness = 0.0, sound = "stone",
		interact = "flower_pot", tabs = F, icon = "item:flower_pot"})
	_add("decorated_pot", {model = BlockDB.M_POT, tex = {all = "decorated_pot_side", top = "decorated_pot_top"},
		hardness = 0.0, sound = "stone", tabs = F, icon = "model", props = {decorated = true}})
	_add("heavy_core", {model = BlockDB.M_HEAVY, tex = {all = "heavy_core"}, hardness = 10.0, resistance = 1200.0,
		tool = "pickaxe", sound = "metal", tabs = F, icon = "model"})
	_add("vault", {render = BlockDB.R_CUTOUT, tex = {top = "vault_top", bottom = "vault_bottom", side = "vault_side",
		front = "vault_front"}, place = "facing", hardness = 50.0, resistance = 50.0, tool = "pickaxe", drop = "-",
		interact = "vault", tabs = ["spawn_eggs"], full = false, opacity = 1, light = 6})
	_add("trial_spawner", {render = BlockDB.R_CUTOUT, tex = {top = "trial_spawner_top", bottom = "trial_spawner_bottom",
		side = "trial_spawner_side"}, hardness = 50.0, resistance = 50.0, tool = "pickaxe", drop = "-", tabs = ["spawn_eggs"],
		full = false, opacity = 1, light = 4, tick = "trial_spawner"})
	_add("crafter", {tex = {top = "crafter_top", bottom = "crafter_bottom", side = "crafter_side", front = "crafter_north"},
		place = "facing_all", hardness = 1.5, tool = "pickaxe", needs_tool = true, tier = 1, tabs = B_RED, interact = "crafting_table"})


func _redstone() -> void:
	var R := B_RED
	_add("redstone_wire", {model = BlockDB.M_WIRE, tex = {all = "redstone_dust_dot", line = "redstone_dust_line"},
		hardness = 0.0, has_item = false, drop = "redstone", tint = BlockDB.T_REDSTONE, place = "wire",
		props = {item = "redstone"}})
	_add("redstone_torch", {model = BlockDB.M_TORCH, tex = {all = "redstone_torch", off = "redstone_torch_off"}, hardness = 0.0,
		light = 7, place = "torch", tabs = R, icon = "texture:redstone_torch", sound = "wood",
		props = {light_meta = [7, 7, 7, 7, 7, 7, 7, 7, 0, 0, 0, 0, 0, 0, 0, 0]}, tick = "redstone"})
	_add("lever", {model = BlockDB.M_LEVER, tex = {all = "lever", base = "cobblestone"}, hardness = 0.5, sound = "wood",
		place = "button", interact = "lever", tabs = R, icon = "texture:lever"})
	for bt in [["stone_button", "stone"], ["polished_blackstone_button", "polished_blackstone"]]:
		_add(bt[0], {model = BlockDB.M_BUTTON, tex = {all = bt[1]}, hardness = 0.5, tool = "pickaxe", place = "button",
			interact = "button", tabs = R, icon = "model"})
	for pl in [["stone_pressure_plate", "stone", "stone"], ["polished_blackstone_pressure_plate", "polished_blackstone", "stone"],
			["light_weighted_pressure_plate", "gold_block", "light"], ["heavy_weighted_pressure_plate", "iron_block", "heavy"]]:
		_add(pl[0], {model = BlockDB.M_PLATE, tex = {all = pl[1]}, hardness = 0.5, tool = "pickaxe", needs_tool = true, tier = 1,
			place = "plate", tabs = R, props = {plate = pl[2]}, tick = "plate"})
	_add("redstone_lamp", {tex = {all = "redstone_lamp", on = "redstone_lamp_on"}, place = "lit", hardness = 0.3, sound = "glass",
		light = 15, tabs = R, props = {lit_bit = 1}})
	_add("repeater", {model = BlockDB.M_REPEATER, tex = {top = "repeater", top_on = "repeater_on", side = "smooth_stone",
		torch = "redstone_torch", torch_off = "redstone_torch_off"}, hardness = 0.0, place = "diode", interact = "repeater",
		tabs = R, icon = "item:repeater", props = {item = "repeater"}, tick = "redstone"})
	_add("repeater_on", {model = BlockDB.M_REPEATER, tex = {top = "repeater_on", top_on = "repeater_on", side = "smooth_stone",
		torch = "redstone_torch", torch_off = "redstone_torch_off"}, hardness = 0.0, interact = "repeater", has_item = false,
		drop = "repeater", light = 0, props = {item = "repeater", powered = true}, tick = "redstone"})
	_add("comparator", {model = BlockDB.M_COMPARATOR, tex = {top = "comparator", top_on = "comparator_on", side = "smooth_stone",
		torch = "redstone_torch", torch_off = "redstone_torch_off"}, hardness = 0.0, place = "diode", interact = "comparator",
		tabs = R, icon = "item:comparator", tick = "redstone"})
	_add("piston", {model = BlockDB.M_PISTON, tex = {top = "piston_top", side = "piston_side", bottom = "piston_bottom",
		inner = "piston_inner"}, hardness = 1.5, tool = "pickaxe", place = "facing_all_player", tabs = R, icon = "model",
		full = false, opacity = 15, tick = "redstone"})
	_add("sticky_piston", {model = BlockDB.M_PISTON, tex = {top = "piston_top_sticky", side = "piston_side",
		bottom = "piston_bottom", inner = "piston_inner"}, hardness = 1.5, tool = "pickaxe", place = "facing_all_player",
		tabs = R, icon = "model", full = false, opacity = 15, props = {sticky = true}, tick = "redstone"})
	_add("piston_head", {model = BlockDB.M_PISTON_HEAD, tex = {top = "piston_top", sticky = "piston_top_sticky",
		side = "piston_side"}, hardness = 1.5, has_item = false, drop = "-", full = false, opacity = 0})
	_add("moving_piston", {model = BlockDB.M_AIR, render = BlockDB.R_NONE, tex = {}, hardness = -1.0, has_item = false,
		drop = "-", solid = false, full = false, opacity = 0})
	_add("observer", {tex = {front = "observer_front", back = "observer_back", back_on = "observer_back_on",
		side = "observer_side", top = "observer_top"}, place = "facing_all_observer", hardness = 3.0, tool = "pickaxe",
		needs_tool = true, tier = 1, tabs = R, tick = "redstone"})
	_add("dispenser", {tex = {front = "dispenser_front", back = "furnace_top", side = "furnace_side",
		front_vertical = "dispenser_front_vertical"}, place = "facing_all_player", hardness = 3.5, tool = "pickaxe",
		needs_tool = true, tier = 1, interact = "dispenser", tabs = R, props = {container = 9}, tick = "redstone"})
	_add("dropper", {tex = {front = "dropper_front", back = "furnace_top", side = "furnace_side",
		front_vertical = "dropper_front_vertical"}, place = "facing_all_player", hardness = 3.5, tool = "pickaxe",
		needs_tool = true, tier = 1, interact = "dropper", tabs = R, props = {container = 9}, tick = "redstone"})
	_add("hopper", {model = BlockDB.M_HOPPER, tex = {all = "hopper_outside", inside = "hopper_inside", top = "hopper_top"},
		hardness = 3.0, resistance = 4.8, tool = "pickaxe", needs_tool = true, tier = 1, place = "hopper", interact = "hopper",
		sound = "metal", tabs = R, icon = "item:hopper", props = {container = 5}, opacity = 0})
	_add("daylight_detector", {model = BlockDB.M_DAYLIGHT, tex = {top = "daylight_detector_top",
		inverted = "daylight_detector_inverted_top", side = "daylight_detector_side"}, hardness = 0.2, tool = "axe",
		sound = "wood", interact = "daylight_detector", tabs = R, icon = "model", tick = "daylight_detector"})
	_add("target", {tex = {top = "target_top", bottom = "target_top", side = "target_side"}, hardness = 0.5, tool = "hoe",
		sound = "grass", tabs = R})
	_add("note_block", {hardness = 0.8, tool = "axe", sound = "wood", interact = "note_block", tabs = R})
	for rl in [["rail", ""], ["powered_rail", "powered"], ["detector_rail", "detector"], ["activator_rail", "activator"]]:
		var t := {all = rl[0]}
		if rl[0] == "rail":
			t["corner"] = "rail_corner"
		else:
			t["on"] = rl[0] + "_on"
		_add(rl[0], {model = BlockDB.M_RAIL, tex = t, hardness = 0.7, tool = "pickaxe", sound = "metal", place = "rail",
			tabs = R, icon = "texture:" + rl[0], props = {rail = rl[1]}, tick = "redstone" if rl[1] != "" else ""})
	_add("iron_door", {model = BlockDB.M_DOOR, tex = {top = "iron_door_top", bottom = "iron_door_bottom"}, hardness = 5.0,
		tool = "pickaxe", needs_tool = true, tier = 1, sound = "metal", place = "door", tabs = R, icon = "item:iron_door",
		drop = "@door", props = {iron = true}})
	_add("iron_trapdoor", {model = BlockDB.M_TRAPDOOR, tex = {all = "iron_trapdoor"}, hardness = 5.0, tool = "pickaxe",
		needs_tool = true, tier = 1, sound = "metal", place = "trapdoor", tabs = R, props = {iron = true}})
	_add("tnt", {tex = {top = "tnt_top", bottom = "tnt_bottom", side = "tnt_side"}, hardness = 0.0, sound = "grass",
		interact = "tnt", tabs = R, flammable = true, tick = "redstone", props = {tnt = true}})
	_add("tripwire_hook", {model = BlockDB.M_LEVER, tex = {all = "tripwire_hook", base = "oak_planks"}, hardness = 0.0,
		place = "button", tabs = R, icon = "texture:tripwire_hook", props = {hook = true}})


func _misc() -> void:
	for sk in ["skeleton_skull", "wither_skeleton_skull", "zombie_head", "creeper_head", "piglin_head", "player_head",
			"dragon_head"]:
		_add(sk, {model = BlockDB.M_HEAD, tex = {front = sk + "_front", side = sk + "_side", top = sk + "_top",
			back = sk + "_back"}, hardness = 1.0, sound = "stone", place = "head", tabs = B_FUNC, icon = "model",
			props = {head = sk}})
	_add("cake", {model = BlockDB.M_CAKE, tex = {top = "cake_top", bottom = "cake_bottom", side = "cake_side",
		inner = "cake_inner"}, hardness = 0.5, sound = "wool", drop = "-", interact = "cake", tabs = ["food"],
		icon = "item:cake"})
	_add("barrier", {render = BlockDB.R_CUTOUT, tex = {all = "barrier"}, hardness = -1.0, drop = "-", opacity = 0, full = false,
		tabs = B_FUNC, props = {invisible = true}})


func _sulfur() -> void:
	# Minecraft 26.2-era Sulfur Caves content (behaviour designed from the public feature description).
	var sp := {hardness = 1.5, resistance = 6.0, tool = "pickaxe", needs_tool = true, tier = 1, sound = "tuff", tabs = B_NAT}
	_add("sulfur", _m(sp, {display = "Sulfur"}))
	_add("potent_sulfur", _m(sp, {light = 4, tick = "geyser", display = "Potent Sulfur", props = {geyser = true}}))
	_add("cinnabar", _m(sp, {display = "Cinnabar"}))
	_add("polished_sulfur", _m(sp, {tabs = B_BUILD}))
	_add("sulfur_bricks", _m(sp, {tabs = B_BUILD}))
	_add("polished_cinnabar", _m(sp, {tabs = B_BUILD}))
	_add("cinnabar_bricks", _m(sp, {tabs = B_BUILD}))
	_family("sulfur_brick", "sulfur_bricks", true, true, true)
	_family("cinnabar_brick", "cinnabar_bricks", true, true, true)
	_add("sulfur_spike", {model = BlockDB.M_SPIKE, tex = {all = "sulfur_spike"}, hardness = 1.5, tool = "pickaxe",
		place = "dripstone", tabs = B_NAT, icon = "texture:sulfur_spike", damage = 0.0})
