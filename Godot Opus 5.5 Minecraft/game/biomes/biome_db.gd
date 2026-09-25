class_name BiomeDB
extends RefCounted
## Biome registry: colours, surface rules, vegetation and spawn data for every dimension.

const DIM_OVERWORLD := 0
const DIM_NETHER := 1
const DIM_END := 2

static var defs: Array = []       # Array[Dictionary]
static var by_name: Dictionary = {}
static var grass := PackedColorArray()
static var foliage := PackedColorArray()
static var water := PackedColorArray()
static var inited := false

# Frequently used ids
static var PLAINS := 0
static var OCEAN := 0
static var RIVER := 0
static var BEACH := 0


static func _b(n: String, p: Dictionary) -> void:
	var d := {
		"name": n, "display": BlockCatalog.pretty(n), "dim": DIM_OVERWORLD,
		"grass": "#91bd59", "foliage": "#77ab2f", "water": "#3f76e4",
		"temp": 0.8, "rain": true, "snowy": false,
		"top": "grass_block", "filler": "dirt", "depth": 3, "under": "stone",
		"trees": [], "tree_count": 0.0, "grass_count": 4, "flowers": [], "flower_count": 0,
		"fog": "#c0d8ff", "sky": "#78a7ff",
		"spawns": ["pig", "cow", "sheep", "chicken"], "hostile": ["zombie", "skeleton", "creeper", "spider", "enderman", "witch"],
		"water_spawns": [],
	}
	d.merge(p, true)
	d["id"] = defs.size()
	defs.append(d)
	by_name[n] = d["id"]


static func init() -> void:
	if inited:
		return
	defs.clear()
	by_name.clear()
	var oak := [["oak", 9], ["fancy_oak", 1]]
	_b("plains", {"trees": oak, "tree_count": 0.15, "grass_count": 16, "flowers": ["dandelion", "poppy", "azure_bluet", "oxeye_daisy", "cornflower", "red_tulip", "white_tulip"], "flower_count": 3,
		"spawns": ["pig", "cow", "sheep", "chicken", "horse", "donkey"]})
	_b("sunflower_plains", {"trees": oak, "tree_count": 0.1, "grass_count": 16, "flowers": ["sunflower", "dandelion", "poppy"], "flower_count": 6,
		"spawns": ["pig", "cow", "sheep", "chicken", "horse", "donkey"]})
	_b("forest", {"grass": "#79c05a", "foliage": "#59ae30", "trees": [["oak", 8], ["birch", 2], ["fancy_oak", 1]], "tree_count": 8.0, "grass_count": 6,
		"flowers": ["dandelion", "poppy", "lily_of_the_valley"], "flower_count": 2, "spawns": ["pig", "cow", "sheep", "chicken", "wolf"]})
	_b("flower_forest", {"grass": "#79c05a", "foliage": "#59ae30", "trees": [["oak", 6], ["birch", 3]], "tree_count": 3.0, "grass_count": 4,
		"flowers": ["allium", "azure_bluet", "red_tulip", "orange_tulip", "pink_tulip", "white_tulip", "oxeye_daisy", "cornflower", "lily_of_the_valley", "lilac", "rose_bush", "peony"], "flower_count": 14,
		"spawns": ["pig", "cow", "sheep", "chicken", "rabbit", "bee"]})
	_b("birch_forest", {"grass": "#88bb67", "foliage": "#6ba941", "trees": [["birch", 10], ["tall_birch", 2]], "tree_count": 8.0, "grass_count": 6,
		"flowers": ["dandelion", "poppy", "lily_of_the_valley"], "flower_count": 2})
	_b("dark_forest", {"grass": "#507a32", "foliage": "#59ae30", "trees": [["dark_oak", 12], ["oak", 2], ["huge_red_mushroom", 1], ["huge_brown_mushroom", 1]], "tree_count": 14.0,
		"grass_count": 3, "flowers": ["poppy", "dandelion"], "flower_count": 1})
	_b("pale_garden", {"grass": "#778272", "foliage": "#878d76", "water": "#76889d", "trees": [["pale_oak", 12], ["pale_oak_heart", 1]], "tree_count": 12.0, "grass_count": 2,
		"flowers": ["closed_eyeblossom", "pale_moss_carpet"], "flower_count": 5, "spawns": [], "hostile": ["zombie", "skeleton", "creeper", "spider", "creaking"],
		"fog": "#b9b9b9", "sky": "#b9b9b9"})
	_b("taiga", {"grass": "#86b783", "foliage": "#68a464", "temp": 0.25, "trees": [["spruce", 10], ["pine", 4]], "tree_count": 9.0, "grass_count": 5,
		"flowers": ["fern", "large_fern", "sweet_berry_bush"], "flower_count": 3, "spawns": ["wolf", "rabbit", "fox", "pig", "cow", "sheep"]})
	_b("old_growth_spruce_taiga", {"grass": "#86b783", "foliage": "#68a464", "temp": 0.25, "top": "podzol", "trees": [["mega_spruce", 6], ["spruce", 4]], "tree_count": 10.0,
		"grass_count": 5, "flowers": ["fern", "large_fern", "brown_mushroom"], "flower_count": 3, "spawns": ["wolf", "rabbit", "fox"]})
	_b("snowy_taiga", {"grass": "#80b497", "foliage": "#60a17b", "water": "#3d57d6", "temp": -0.5, "snowy": true, "trees": [["spruce", 10], ["pine", 4]], "tree_count": 7.0,
		"grass_count": 2, "flowers": ["fern"], "flower_count": 2, "spawns": ["wolf", "rabbit", "fox"], "hostile": ["zombie", "stray", "creeper", "spider", "enderman"]})
	_b("snowy_plains", {"grass": "#80b497", "foliage": "#60a17b", "water": "#3d57d6", "temp": 0.0, "snowy": true, "trees": [["spruce", 1]], "tree_count": 0.1, "grass_count": 1,
		"spawns": ["rabbit", "polar_bear"], "hostile": ["zombie", "stray", "creeper", "spider", "enderman"]})
	_b("ice_spikes", {"grass": "#80b497", "foliage": "#60a17b", "water": "#3d57d6", "temp": 0.0, "snowy": true, "top": "snow_block", "filler": "dirt", "trees": [["ice_spike", 1]],
		"tree_count": 1.5, "grass_count": 0, "spawns": ["rabbit", "polar_bear"], "hostile": ["stray", "creeper", "spider"]})
	_b("desert", {"grass": "#bfb755", "foliage": "#aea42a", "water": "#32a598", "temp": 2.0, "rain": false, "top": "sand", "filler": "sand", "depth": 4, "under": "sandstone",
		"trees": [["cactus", 1]], "tree_count": 1.2, "grass_count": 0, "flowers": ["dead_bush"], "flower_count": 2, "spawns": ["rabbit", "camel"],
		"hostile": ["husk", "skeleton", "creeper", "spider", "enderman"], "fog": "#e9dfb8", "sky": "#6ea0e8"})
	_b("savanna", {"grass": "#bfb755", "foliage": "#aea42a", "temp": 2.0, "rain": false, "trees": [["acacia", 8], ["oak", 2]], "tree_count": 1.2, "grass_count": 20,
		"flowers": ["tall_grass", "dandelion"], "flower_count": 4, "spawns": ["horse", "donkey", "llama", "armadillo", "cow", "sheep"]})
	_b("windswept_savanna", {"grass": "#bfb755", "foliage": "#aea42a", "temp": 2.0, "rain": false, "trees": [["acacia", 1]], "tree_count": 0.6, "grass_count": 8,
		"spawns": ["horse", "llama", "armadillo"]})
	_b("jungle", {"grass": "#59c93c", "foliage": "#30bb0b", "water": "#14a2c5", "temp": 0.95, "trees": [["jungle", 6], ["mega_jungle", 3], ["jungle_bush", 5], ["fancy_oak", 1]],
		"tree_count": 16.0, "grass_count": 18, "flowers": ["fern", "melon", "poppy"], "flower_count": 2, "spawns": ["parrot", "ocelot", "panda", "chicken", "pig"]})
	_b("bamboo_jungle", {"grass": "#59c93c", "foliage": "#30bb0b", "water": "#14a2c5", "temp": 0.95, "top": "grass_block", "trees": [["bamboo", 12], ["jungle", 2], ["jungle_bush", 3]],
		"tree_count": 18.0, "grass_count": 12, "flowers": ["fern"], "flower_count": 2, "spawns": ["panda", "parrot", "ocelot"]})
	_b("swamp", {"grass": "#6a7039", "foliage": "#6a7039", "water": "#617b64", "trees": [["swamp_oak", 1]], "tree_count": 2.0, "grass_count": 5,
		"flowers": ["blue_orchid", "brown_mushroom", "red_mushroom"], "flower_count": 2, "spawns": ["frog", "slime"], "hostile": ["zombie", "skeleton", "creeper", "spider", "slime", "witch", "bogged"],
		"fog": "#a8b8a0"})
	_b("mangrove_swamp", {"grass": "#6a7039", "foliage": "#8db127", "water": "#3a7a6a", "top": "mud", "filler": "mud", "trees": [["mangrove", 1]], "tree_count": 6.0, "grass_count": 2,
		"flowers": [], "spawns": ["frog"], "hostile": ["zombie", "skeleton", "creeper", "spider", "slime", "witch", "bogged"]})
	_b("badlands", {"grass": "#90814d", "foliage": "#9e814d", "temp": 2.0, "rain": false, "top": "red_sand", "filler": "orange_terracotta", "depth": 1, "under": "terracotta",
		"trees": [["cactus", 1]], "tree_count": 0.4, "grass_count": 0, "flowers": ["dead_bush"], "flower_count": 3, "spawns": ["armadillo"], "sky": "#7fa1ff"})
	_b("wooded_badlands", {"grass": "#90814d", "foliage": "#9e814d", "temp": 2.0, "rain": false, "top": "red_sand", "filler": "orange_terracotta", "depth": 1, "under": "terracotta",
		"trees": [["oak", 1]], "tree_count": 2.0, "grass_count": 2, "flowers": ["dead_bush"], "flower_count": 2, "spawns": ["armadillo"]})
	_b("meadow", {"grass": "#83bb6d", "foliage": "#63a948", "water": "#0e4ecf", "temp": 0.5, "trees": [["birch_bee", 1], ["oak", 1]], "tree_count": 0.1, "grass_count": 14,
		"flowers": ["dandelion", "poppy", "allium", "azure_bluet", "oxeye_daisy", "cornflower", "wildflowers"], "flower_count": 8, "spawns": ["donkey", "rabbit", "sheep", "bee"]})
	_b("cherry_grove", {"grass": "#b6db61", "foliage": "#b6db61", "water": "#5db7ef", "temp": 0.5, "trees": [["cherry", 1]], "tree_count": 2.5, "grass_count": 10,
		"flowers": ["pink_petals"], "flower_count": 10, "spawns": ["pig", "rabbit", "bee"]})
	_b("grove", {"grass": "#80b497", "foliage": "#60a17b", "temp": -0.2, "snowy": true, "trees": [["spruce", 1]], "tree_count": 5.0, "grass_count": 1,
		"spawns": ["wolf", "rabbit", "fox"]})
	_b("snowy_slopes", {"grass": "#80b497", "foliage": "#60a17b", "temp": -0.3, "snowy": true, "top": "snow_block", "filler": "snow_block", "trees": [], "grass_count": 0,
		"spawns": ["rabbit", "goat"], "hostile": ["stray", "creeper", "spider"]})
	_b("windswept_hills", {"grass": "#8ab689", "foliage": "#6da36b", "water": "#0e4ecf", "temp": 0.2, "trees": [["spruce", 3], ["oak", 2]], "tree_count": 0.5, "grass_count": 4,
		"spawns": ["llama", "sheep", "goat"]})
	_b("jagged_peaks", {"grass": "#80b497", "foliage": "#60a17b", "temp": -0.7, "snowy": true, "top": "snow_block", "filler": "stone", "depth": 1, "trees": [], "grass_count": 0,
		"spawns": ["goat"], "hostile": ["stray", "creeper"]})
	_b("frozen_peaks", {"grass": "#80b497", "foliage": "#60a17b", "temp": -0.7, "snowy": true, "top": "packed_ice", "filler": "stone", "depth": 1, "trees": [], "grass_count": 0,
		"spawns": ["goat"], "hostile": ["stray", "creeper"]})
	_b("stony_peaks", {"grass": "#8ab689", "foliage": "#6da36b", "temp": 1.0, "top": "stone", "filler": "stone", "depth": 1, "trees": [], "grass_count": 0,
		"spawns": ["goat"]})
	_b("beach", {"top": "sand", "filler": "sand", "depth": 3, "under": "sandstone", "trees": [], "grass_count": 0, "spawns": ["turtle"]})
	_b("snowy_beach", {"grass": "#80b497", "foliage": "#60a17b", "water": "#3d57d6", "temp": 0.05, "snowy": true, "top": "sand", "filler": "sand", "trees": [], "grass_count": 0, "spawns": []})
	_b("stony_shore", {"top": "stone", "filler": "stone", "depth": 1, "trees": [], "grass_count": 0, "spawns": []})
	_b("river", {"top": "sand", "filler": "sand", "trees": [], "grass_count": 0, "spawns": [], "water_spawns": ["salmon", "squid"]})
	_b("frozen_river", {"water": "#3938c9", "temp": 0.0, "snowy": true, "top": "sand", "filler": "sand", "trees": [], "grass_count": 0, "spawns": [], "water_spawns": ["salmon"]})
	_b("ocean", {"top": "gravel", "filler": "gravel", "trees": [], "grass_count": 0, "spawns": [], "water_spawns": ["cod", "squid", "dolphin"], "hostile": ["drowned"]})
	_b("deep_ocean", {"top": "gravel", "filler": "gravel", "trees": [], "grass_count": 0, "spawns": [], "water_spawns": ["cod", "squid", "dolphin"], "hostile": ["drowned", "guardian"]})
	_b("warm_ocean", {"water": "#43d5ee", "top": "sand", "filler": "sand", "trees": [], "grass_count": 0, "spawns": [], "water_spawns": ["tropical_fish", "pufferfish", "dolphin"],
		"hostile": ["drowned"]})
	_b("lukewarm_ocean", {"water": "#45adf2", "top": "sand", "filler": "sand", "trees": [], "grass_count": 0, "spawns": [], "water_spawns": ["tropical_fish", "cod", "dolphin"],
		"hostile": ["drowned"]})
	_b("cold_ocean", {"water": "#3d57d6", "top": "gravel", "filler": "gravel", "trees": [], "grass_count": 0, "spawns": [], "water_spawns": ["cod", "salmon", "squid"],
		"hostile": ["drowned"]})
	_b("frozen_ocean", {"water": "#3938c9", "temp": 0.0, "snowy": true, "top": "gravel", "filler": "gravel", "trees": [["iceberg", 1]], "tree_count": 0.05, "grass_count": 0,
		"spawns": ["polar_bear"], "water_spawns": ["salmon"], "hostile": ["drowned", "stray"]})
	_b("mushroom_fields", {"grass": "#55c93f", "foliage": "#2bbb0f", "water": "#8a8997", "top": "mycelium", "trees": [["huge_red_mushroom", 1], ["huge_brown_mushroom", 1]],
		"tree_count": 1.0, "grass_count": 0, "flowers": ["red_mushroom", "brown_mushroom"], "flower_count": 2, "spawns": ["mooshroom"], "hostile": []})
	# underground (cave) biomes
	_b("lush_caves", {"grass": "#91bd59", "foliage": "#77ab2f", "spawns": ["axolotl"], "hostile": ["zombie", "skeleton", "creeper", "spider"]})
	_b("dripstone_caves", {"spawns": [], "hostile": ["drowned", "zombie", "skeleton", "creeper", "spider"]})
	_b("deep_dark", {"spawns": [], "hostile": []})
	_b("sulfur_caves", {"spawns": ["sulfur_cube"], "hostile": ["zombie", "skeleton", "creeper", "spider"]})
	# nether
	var nd := {"dim": DIM_NETHER, "rain": false, "temp": 2.0, "top": "netherrack", "filler": "netherrack", "under": "netherrack", "grass_count": 0}
	var nb := func(n: String, extra: Dictionary) -> void:
		var q: Dictionary = nd.duplicate()
		q.merge(extra, true)
		_b(n, q)
	nb.call("nether_wastes", {"fog": "#330808", "sky": "#330808", "spawns": [], "hostile": ["zombified_piglin", "ghast", "magma_cube", "enderman", "piglin", "strider"]})
	nb.call("crimson_forest", {"fog": "#330303", "sky": "#330303", "top": "crimson_nylium", "spawns": [], "hostile": ["piglin", "hoglin", "zombified_piglin", "strider"]})
	nb.call("warped_forest", {"fog": "#1a051a", "sky": "#1a051a", "top": "warped_nylium", "spawns": [], "hostile": ["enderman", "strider"]})
	nb.call("soul_sand_valley", {"fog": "#1b4745", "sky": "#1b4745", "top": "soul_sand", "filler": "soul_soil", "spawns": [], "hostile": ["skeleton", "ghast", "enderman", "strider"]})
	nb.call("basalt_deltas", {"fog": "#685f70", "sky": "#685f70", "top": "basalt", "filler": "blackstone", "spawns": [], "hostile": ["magma_cube", "ghast", "strider"]})
	# end
	_b("the_end", {"dim": DIM_END, "rain": false, "temp": 0.5, "top": "end_stone", "filler": "end_stone", "under": "end_stone", "grass_count": 0, "fog": "#0b0b14", "sky": "#141020",
		"spawns": [], "hostile": ["enderman"]})
	_b("end_highlands", {"dim": DIM_END, "rain": false, "temp": 0.5, "top": "end_stone", "filler": "end_stone", "under": "end_stone", "grass_count": 0, "fog": "#0b0b14",
		"sky": "#141020", "spawns": [], "hostile": ["enderman"]})
	grass.resize(defs.size())
	foliage.resize(defs.size())
	water.resize(defs.size())
	for d in defs:
		grass[d.id] = Color.html(d.grass)
		foliage[d.id] = Color.html(d.foliage)
		water[d.id] = Color.html(d.water)
	PLAINS = id("plains")
	OCEAN = id("ocean")
	RIVER = id("river")
	BEACH = id("beach")
	inited = true


static func id(n: String) -> int:
	return by_name.get(n, 0)


static func get_def(i: int) -> Dictionary:
	return defs[clampi(i, 0, defs.size() - 1)]


static func name_of(i: int) -> String:
	return defs[clampi(i, 0, defs.size() - 1)].name
