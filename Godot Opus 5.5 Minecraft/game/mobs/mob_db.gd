class_name MobDB
extends RefCounted
## Mob registry: dimensions/biomes they spawn in, stats, AI archetype, model spec path, drops, XP.
## Archetypes drive MobAI: "passive", "neutral", "hostile_melee", "hostile_ranged", "creeper",
## "spider", "enderman", "slime", "flyer", "ghast", "blaze", "swimmer", "fish", "ambient",
## "boss_dragon", "boss_wither", "warden", "creaking", "shulker", "golem", "villager", "copper_golem".

static var defs: Dictionary = {}
static var order: PackedStringArray = PackedStringArray()
static var inited := false


static func init() -> void:
	if inited:
		return
	_ow_passive()
	_ow_hostile()
	_ow_neutral()
	_nether()
	_end()
	_bosses()
	_modern()
	inited = true


static func _m(n: String, display: String, p: Dictionary) -> void:
	var d := {
		"name": n, "display": display, "arch": "passive", "hp": 10.0, "dmg": 0.0, "speed": 0.06, "armor": 0.0,
		"width": 0.6, "height": 1.8, "xp": 1, "dim": 0, "biomes": PackedStringArray(), "light_max": 15, "sky_only": false,
		"water": false, "fly": false, "climb": false, "day_burn": false, "fireproof": false, "despawn": true, "baby": false,
		"model": "", "sounds": n, "hostile": false, "reach": 1.4, "attack_cd": 20, "eye": 1.5, "spawn_cap": 24,
		"tame_item": "", "breed_item": "", "leadable": true, "rideable": false, "group": [1, 3]
	}
	d.merge(p, true)
	# every archetype that attacks players on sight (spiders calm down in daylight in their own AI)
	var attacker: bool = d.arch in ["creeper", "spider", "slime", "flyer", "ghast", "blaze"]
	if d.hostile or attacker or d.arch.begins_with("hostile") or d.arch.begins_with("boss"):
		d["hostile"] = true
	defs[n] = d
	order.append(n)


# ------------------------------------------------------------------------------------------------
static func _ow_passive() -> void:
	_m("pig", "Pig", {hp = 10, xp = 1, model = "pig", speed = 0.06, breed_item = "carrot", rideable = true, biomes = ["plains", "forest",
		"birch_forest", "taiga", "meadow", "cherry_grove", "sunflower_plains"], group = [1, 4], eye = 1.1, width = 0.9, height = 0.9})
	_m("cow", "Cow", {hp = 10, xp = 1, model = "cow", speed = 0.055, breed_item = "wheat", biomes = ["plains", "forest",
		"birch_forest", "taiga", "meadow", "sunflower_plains"], group = [2, 4], eye = 1.4, width = 0.9, height = 1.4})
	_m("sheep", "Sheep", {hp = 8, xp = 1, model = "sheep", speed = 0.055, breed_item = "wheat", biomes = ["plains", "forest",
		"birch_forest", "taiga", "snowy_plains", "meadow", "sunflower_plains"], group = [2, 4], eye = 1.3, width = 0.9, height = 1.3})
	_m("chicken", "Chicken", {hp = 4, xp = 1, model = "chicken", speed = 0.06, breed_item = "wheat_seeds", biomes = ["plains",
		"forest", "birch_forest", "taiga", "jungle", "savanna", "meadow"], group = [2, 4], eye = 0.7, width = 0.5, height = 0.7})
	_m("rabbit", "Rabbit", {hp = 3, xp = 1, model = "rabbit", speed = 0.08, biomes = ["plains", "forest", "taiga", "snowy_plains",
		"desert", "snowy_taiga", "meadow", "sunflower_plains"], group = [1, 3], width = 0.4, height = 0.5, eye = 0.5})
	_m("horse", "Horse", {hp = 22, xp = 2, model = "horse", speed = 0.09, breed_item = "golden_apple", tame_item = "saddle",
		rideable = true, biomes = ["plains", "savanna", "meadow", "sunflower_plains"], group = [2, 6], eye = 1.6, width = 1.0, height = 1.6})
	_m("donkey", "Donkey", {hp = 22, xp = 2, model = "horse", speed = 0.08, rideable = true, biomes = ["plains", "savanna",
		"meadow"], group = [1, 3], eye = 1.6, width = 1.0, height = 1.6})
	_m("mule", "Mule", {hp = 24, xp = 2, model = "horse", speed = 0.08, rideable = true, biomes = [], group = [1, 1], eye = 1.6,
		width = 1.0, height = 1.6})
	_m("camel", "Camel", {hp = 32, xp = 2, model = "camel", speed = 0.075, rideable = true, breed_item = "cactus",
		biomes = ["desert"], group = [1, 2], eye = 2.1, width = 1.2, height = 2.2})
	_m("wolf", "Wolf", {hp = 8, xp = 2, model = "wolf", speed = 0.1, tame_item = "bone", breed_item = "beef", arch = "neutral",
		dmg = 4, biomes = ["taiga", "forest", "snowy_taiga", "pale_garden"], group = [2, 4], eye = 0.7, width = 0.6, height = 0.8})
	_m("cat", "Cat", {hp = 10, xp = 1, model = "cat", speed = 0.09, tame_item = "cod", breed_item = "cod", biomes = ["plains",
		"swamp"], group = [1, 2], width = 0.6, height = 0.7, eye = 0.6})
	_m("ocelot", "Ocelot", {hp = 10, xp = 1, model = "cat", speed = 0.09, tame_item = "cod", biomes = ["jungle"], group = [1, 2],
		width = 0.6, height = 0.7, eye = 0.6})
	_m("parrot", "Parrot", {hp = 6, xp = 1, model = "parrot", speed = 0.07, fly = true, biomes = ["jungle"], group = [1, 2],
		width = 0.5, height = 0.9, eye = 0.8})
	_m("fox", "Fox", {hp = 10, xp = 1, model = "fox", speed = 0.1, biomes = ["taiga", "snowy_taiga", "pale_garden"], group = [2, 4],
		width = 0.6, height = 0.7, eye = 0.6})
	_m("panda", "Panda", {hp = 20, xp = 1, model = "panda", speed = 0.05, breed_item = "bamboo", biomes = ["bamboo_jungle"],
		group = [1, 2], eye = 1.2, width = 1.1, height = 1.2})
	_m("polar_bear", "Polar Bear", {hp = 30, xp = 1, model = "polar_bear", speed = 0.07, arch = "neutral", dmg = 6,
		biomes = ["snowy_plains", "frozen_ocean", "snowy_beach"], group = [1, 2], eye = 1.3, width = 1.2, height = 1.4})
	_m("goat", "Goat", {hp = 10, xp = 1, model = "goat", speed = 0.07, breed_item = "wheat", biomes = ["jagged_peaks",
		"frozen_peaks", "stony_peaks", "windswept_hills"], group = [1, 3], eye = 1.1, width = 0.9, height = 1.1})
	_m("llama", "Llama", {hp = 22, xp = 2, model = "llama", speed = 0.06, breed_item = "hay_block", biomes = ["savanna",
		"windswept_hills"], group = [2, 4], eye = 1.7, width = 0.9, height = 1.8})
	_m("trader_llama", "Trader Llama", {hp = 22, xp = 2, model = "llama", speed = 0.06, biomes = [], group = [1, 1], eye = 1.7,
		width = 0.9, height = 1.8, despawn = false})
	_m("villager", "Villager", {hp = 20, xp = 0, model = "villager", speed = 0.05, biomes = [], group = [1, 1], eye = 1.6,
		width = 0.6, height = 1.9, despawn = false})
	_m("wandering_trader", "Wandering Trader", {hp = 20, xp = 0, model = "villager", speed = 0.05, biomes = [], group = [1, 1],
		eye = 1.6, width = 0.6, height = 1.9})
	_m("iron_golem", "Iron Golem", {hp = 100, xp = 0, model = "iron_golem", speed = 0.05, arch = "golem", dmg = 8,
		biomes = [], group = [1, 1], eye = 2.1, width = 1.2, height = 2.6, attack_cd = 20})
	_m("snow_golem", "Snow Golem", {hp = 4, xp = 0, model = "snow_golem", speed = 0.05, arch = "golem", dmg = 0, biomes = [],
		group = [1, 1], eye = 1.5, width = 0.7, height = 1.7})
	_m("copper_golem", "Copper Golem", {hp = 12, xp = 0, model = "copper_golem", speed = 0.06, arch = "copper_golem", biomes = [],
		group = [1, 1], eye = 1.0, width = 0.7, height = 1.1})
	_m("armadillo", "Armadillo", {hp = 12, xp = 1, model = "armadillo", speed = 0.05, biomes = ["savanna"], group = [1, 2],
		eye = 0.6, width = 0.7, height = 0.7})
	_m("sniffer", "Sniffer", {hp = 14, xp = 1, model = "sniffer", speed = 0.04, biomes = [], group = [1, 1], eye = 1.3,
		width = 1.2, height = 1.4})
	_m("frog", "Frog", {hp = 10, xp = 1, model = "frog", speed = 0.08, biomes = ["swamp", "mangrove_swamp"], group = [1, 3],
		eye = 0.5, width = 0.5, height = 0.5, water = true})
	_m("turtle", "Turtle", {hp = 30, xp = 1, model = "turtle", speed = 0.04, biomes = ["beach"], group = [1, 3], eye = 0.4,
		width = 0.9, height = 0.4, water = true})
	_m("axolotl", "Axolotl", {hp = 14, xp = 1, model = "axolotl", speed = 0.09, arch = "fish", biomes = ["lush_caves"],
		group = [1, 2], water = true, eye = 0.3, width = 0.5, height = 0.4})
	_m("bat", "Bat", {hp = 6, xp = 0, model = "bat", speed = 0.1, arch = "ambient", fly = true, biomes = [], group = [1, 3],
		eye = 0.5, width = 0.5, height = 0.9, light_max = 4})
	_m("bee", "Bee", {hp = 10, xp = 1, model = "bee", speed = 0.09, arch = "ambient", fly = true, biomes = ["plains", "forest",
		"sunflower_plains", "meadow", "cherry_grove"], group = [1, 3], eye = 0.5, width = 0.6, height = 0.6})
	_m("allay", "Allay", {hp = 20, xp = 0, model = "allay", speed = 0.1, arch = "ambient", fly = true, biomes = [], group = [1, 1],
		eye = 0.5, width = 0.5, height = 1.0})
	_m("squid", "Squid", {hp = 10, xp = 1, model = "squid", speed = 0.07, arch = "fish", water = true, biomes = ["ocean",
		"warm_ocean", "cold_ocean", "frozen_ocean", "river"], group = [1, 4], eye = 0.5, width = 0.6, height = 0.6})
	_m("glow_squid", "Glow Squid", {hp = 10, xp = 1, model = "glow_squid", speed = 0.07, arch = "fish", water = true, biomes = [],
		group = [1, 3], eye = 0.5, width = 0.6, height = 0.6, light_max = 15})
	_m("dolphin", "Dolphin", {hp = 10, xp = 1, model = "dolphin", speed = 0.12, arch = "fish", water = true, biomes = ["ocean",
		"warm_ocean", "cold_ocean", "deep_ocean"], group = [3, 5], eye = 0.4, width = 0.6, height = 0.6})
	_m("cod", "Cod", {hp = 3, xp = 1, model = "fish", speed = 0.07, arch = "fish", water = true, biomes = ["ocean", "river",
		"cold_ocean", "frozen_ocean"], group = [3, 6], eye = 0.2, width = 0.3, height = 0.3})
	_m("salmon", "Salmon", {hp = 3, xp = 1, model = "fish", speed = 0.08, arch = "fish", water = true, biomes = ["river",
		"cold_ocean", "frozen_ocean"], group = [1, 5], eye = 0.2, width = 0.4, height = 0.4})
	_m("tropical_fish", "Tropical Fish", {hp = 3, xp = 1, model = "fish", speed = 0.07, arch = "fish", water = true,
		biomes = ["warm_ocean"], group = [1, 4], eye = 0.2, width = 0.3, height = 0.3})
	_m("pufferfish", "Pufferfish", {hp = 3, xp = 1, model = "fish", speed = 0.06, arch = "fish", water = true, biomes = ["warm_ocean"],
		group = [1, 3], eye = 0.2, width = 0.4, height = 0.4})
	_m("tadpole", "Tadpole", {hp = 6, xp = 0, model = "fish", speed = 0.06, arch = "fish", water = true, biomes = ["swamp"],
		group = [1, 3], eye = 0.1, width = 0.2, height = 0.2})
	_m("nautilus", "Nautilus", {hp = 10, xp = 1, model = "nautilus", speed = 0.07, arch = "fish", water = true, biomes = ["ocean",
		"warm_ocean"], group = [1, 3], eye = 0.5, width = 0.6, height = 0.6})


static func _ow_hostile() -> void:
	_m("zombie", "Zombie", {hp = 20, xp = 5, model = "zombie", speed = 0.055, arch = "hostile_melee", dmg = 3, day_burn = true,
		biomes = [], group = [2, 4], eye = 1.7, light_max = 7})
	_m("husk", "Husk", {hp = 20, xp = 5, model = "zombie", speed = 0.055, arch = "hostile_melee", dmg = 3, dmg_fire = true,
		biomes = ["desert"], group = [2, 4], eye = 1.7, light_max = 7})
	_m("drowned", "Drowned", {hp = 20, xp = 5, model = "zombie", speed = 0.055, arch = "hostile_melee", dmg = 3, water = true,
		biomes = ["ocean", "river", "warm_ocean", "cold_ocean"], group = [2, 4], eye = 1.7, light_max = 7})
	_m("zombie_villager", "Zombie Villager", {hp = 20, xp = 5, model = "villager_zombie", speed = 0.055, arch = "hostile_melee",
		dmg = 3, day_burn = true, biomes = [], group = [1, 2], eye = 1.7, light_max = 7})
	_m("skeleton", "Skeleton", {hp = 20, xp = 5, model = "skeleton", speed = 0.055, arch = "hostile_ranged", dmg = 2, day_burn = true,
		biomes = [], group = [1, 3], eye = 1.7, light_max = 7, reach = 16.0})
	_m("stray", "Stray", {hp = 20, xp = 5, model = "skeleton", speed = 0.055, arch = "hostile_ranged", dmg = 2, biomes = ["snowy_plains",
		"snowy_taiga", "frozen_ocean"], group = [1, 3], eye = 1.7, light_max = 7, reach = 16.0})
	_m("bogged", "Bogged", {hp = 16, xp = 5, model = "skeleton", speed = 0.055, arch = "hostile_ranged", dmg = 2, biomes = ["swamp",
		"mangrove_swamp"], group = [1, 3], eye = 1.7, light_max = 7, reach = 16.0})
	_m("parched", "Parched", {hp = 16, xp = 5, model = "skeleton", speed = 0.055, arch = "hostile_ranged", dmg = 2, biomes = ["desert"],
		group = [1, 3], eye = 1.7, light_max = 7, reach = 16.0})
	_m("wither_skeleton", "Wither Skeleton", {hp = 20, xp = 5, model = "wither_skeleton", speed = 0.055, arch = "hostile_melee",
		dmg = 4, dim = 1, fireproof = true, biomes = [], group = [1, 3], eye = 2.2, width = 0.7, height = 2.4})
	_m("creeper", "Creeper", {hp = 20, xp = 5, model = "creeper", speed = 0.06, arch = "creeper", dmg = 0, biomes = [],
		group = [1, 2], eye = 1.5, light_max = 7})
	_m("spider", "Spider", {hp = 16, xp = 5, model = "spider", speed = 0.09, arch = "spider", dmg = 2, climb = true,
		biomes = [], group = [1, 2], eye = 0.8, width = 1.4, height = 0.9, light_max = 7})
	_m("cave_spider", "Cave Spider", {hp = 12, xp = 5, model = "spider", speed = 0.09, arch = "spider", dmg = 2, climb = true,
		biomes = ["lush_caves"], group = [1, 2], eye = 0.5, width = 0.9, height = 0.6, light_max = 15})
	_m("enderman", "Enderman", {hp = 40, xp = 5, model = "enderman", speed = 0.12, arch = "enderman", dmg = 7, biomes = [],
		group = [1, 2], eye = 2.6, width = 0.6, height = 2.9, light_max = 7})
	_m("slime", "Slime", {hp = 16, xp = 4, model = "slime", speed = 0.05, arch = "slime", dmg = 4, biomes = ["swamp"],
		group = [1, 2], eye = 1.0, width = 1.0, height = 1.0, light_max = 7, split = "slime"})
	_m("witch", "Witch", {hp = 26, xp = 5, model = "witch", speed = 0.06, arch = "hostile_ranged", dmg = 3, biomes = [],
		group = [1, 1], eye = 1.8, light_max = 7, reach = 10.0})
	_m("phantom", "Phantom", {hp = 20, xp = 5, model = "phantom", speed = 0.14, arch = "flyer", dmg = 2, fly = true, biomes = [],
		group = [1, 2], eye = 0.6, width = 1.2, height = 0.6, light_max = 7})
	_m("silverfish", "Silverfish", {hp = 8, xp = 5, model = "silverfish", speed = 0.13, arch = "hostile_melee", dmg = 1,
		biomes = [], group = [1, 4], eye = 0.3, width = 0.4, height = 0.3, light_max = 15})
	_m("endermite", "Endermite", {hp = 8, xp = 3, model = "silverfish", speed = 0.13, arch = "hostile_melee", dmg = 2,
		biomes = [], group = [1, 2], eye = 0.3, width = 0.4, height = 0.3, light_max = 15})
	_m("guardian", "Guardian", {hp = 30, xp = 10, model = "guardian", speed = 0.06, arch = "hostile_ranged", dmg = 4, water = true,
		biomes = ["ocean", "deep_ocean"], group = [1, 3], eye = 0.8, width = 0.9, height = 0.9, reach = 12.0})
	_m("elder_guardian", "Elder Guardian", {hp = 80, xp = 20, model = "guardian", speed = 0.05, arch = "hostile_ranged", dmg = 8,
		water = true, biomes = ["deep_ocean"], group = [1, 1], eye = 1.4, width = 1.8, height = 1.8, reach = 16.0, despawn = false})
	_m("pillager", "Pillager", {hp = 24, xp = 5, model = "pillager", speed = 0.06, arch = "hostile_ranged", dmg = 3, biomes = [],
		group = [1, 2], eye = 1.8, light_max = 15, reach = 12.0})
	_m("vindicator", "Vindicator", {hp = 24, xp = 5, model = "vindicator", speed = 0.06, arch = "hostile_melee", dmg = 5,
		biomes = [], group = [1, 1], eye = 1.8, light_max = 15})
	_m("evoker", "Evoker", {hp = 24, xp = 10, model = "evoker", speed = 0.06, arch = "hostile_ranged", dmg = 6, biomes = [],
		group = [1, 1], eye = 1.8, light_max = 15, reach = 12.0})
	_m("vex", "Vex", {hp = 14, xp = 3, model = "vex", speed = 0.14, arch = "flyer", dmg = 4, fly = true, biomes = [], group = [1, 3],
		eye = 0.6, width = 0.4, height = 0.8, light_max = 15})
	_m("ravager", "Ravager", {hp = 100, xp = 20, model = "ravager", speed = 0.08, arch = "hostile_melee", dmg = 12, biomes = [],
		group = [1, 1], eye = 1.8, width = 1.9, height = 2.2, light_max = 15})
	_m("warden", "Warden", {hp = 500, xp = 50, model = "warden", speed = 0.08, arch = "warden", dmg = 30, biomes = ["deep_dark"],
		group = [1, 1], eye = 2.6, width = 0.9, height = 2.9, light_max = 15, despawn = false, attack_cd = 40, fireproof = true})
	_m("creaking", "Creaking", {hp = 1, xp = 0, model = "creaking", speed = 0.1, arch = "creaking", dmg = 3,
		biomes = ["pale_garden"], group = [1, 1], eye = 2.5, width = 0.8, height = 2.7, light_max = 15})
	_m("breeze", "Breeze", {hp = 30, xp = 10, model = "breeze", speed = 0.11, arch = "hostile_ranged", dmg = 3, fly = true,
		biomes = [], group = [1, 1], eye = 1.6, width = 0.6, height = 1.8, light_max = 15, reach = 14.0})
	_m("zombie_nautilus", "Zombie Nautilus", {hp = 20, xp = 5, model = "nautilus_zombie", speed = 0.07, arch = "hostile_melee",
		dmg = 4, water = true, biomes = ["ocean", "deep_ocean"], group = [1, 2], eye = 0.5, width = 0.8, height = 0.8})
	_m("camel_husk", "Camel Husk", {hp = 32, xp = 5, model = "camel", speed = 0.075, arch = "hostile_melee", dmg = 6,
		biomes = ["desert"], group = [1, 1], eye = 2.1, width = 1.2, height = 2.2})


static func _ow_neutral() -> void:
	_m("zombie_horse", "Zombie Horse", {hp = 15, xp = 2, model = "horse", speed = 0.09, rideable = true, biomes = [], group = [1, 1],
		eye = 1.6, width = 1.0, height = 1.6, despawn = false})
	_m("skeleton_horse", "Skeleton Horse", {hp = 15, xp = 2, model = "skeleton_horse", speed = 0.09, rideable = true, biomes = [],
		group = [1, 1], eye = 1.6, width = 1.0, height = 1.6, despawn = false})
	_m("mooshroom", "Mooshroom", {hp = 10, xp = 1, model = "mooshroom", speed = 0.055, breed_item = "wheat",
		biomes = ["mushroom_fields"], group = [2, 4], eye = 1.4, width = 0.9, height = 1.4})


static func _nether() -> void:
	_m("ghast", "Ghast", {hp = 10, xp = 5, model = "ghast", speed = 0.05, arch = "ghast", dmg = 0, dim = 1, fly = true, fireproof = true,
		biomes = ["nether_wastes", "soul_sand_valley", "basalt_deltas"], group = [1, 1], eye = 2.5, width = 4.0, height = 4.0,
		reach = 60.0, despawn = false})
	_m("blaze", "Blaze", {hp = 20, xp = 10, model = "blaze", speed = 0.06, arch = "blaze", dmg = 6, dim = 1, fly = true, fireproof = true,
		biomes = [], group = [1, 2], eye = 1.5, width = 0.6, height = 1.8, reach = 16.0})
	_m("piglin", "Piglin", {hp = 16, xp = 5, model = "piglin", speed = 0.06, arch = "neutral", dmg = 5, dim = 1, fireproof = true,
		biomes = ["crimson_forest", "nether_wastes"], group = [2, 4], eye = 1.8, width = 0.6, height = 1.9})
	_m("piglin_brute", "Piglin Brute", {hp = 50, xp = 20, model = "piglin_brute", speed = 0.06, arch = "hostile_melee", dmg = 13,
		dim = 1, fireproof = true, biomes = [], group = [1, 1], eye = 1.8, width = 0.7, height = 2.0})
	_m("zombified_piglin", "Zombified Piglin", {hp = 20, xp = 5, model = "zombified_piglin", speed = 0.06, arch = "neutral", dmg = 5,
		dim = 1, fireproof = true, biomes = ["nether_wastes", "crimson_forest"], group = [2, 4], eye = 1.8, width = 0.6, height = 1.9})
	_m("hoglin", "Hoglin", {hp = 40, xp = 5, model = "hoglin", speed = 0.08, arch = "hostile_melee", dmg = 6, dim = 1, fireproof = true,
		biomes = ["crimson_forest"], group = [2, 4], eye = 1.0, width = 1.4, height = 1.4})
	_m("zoglin", "Zoglin", {hp = 40, xp = 5, model = "hoglin", speed = 0.08, arch = "hostile_melee", dmg = 6, dim = 1, fireproof = true,
		biomes = [], group = [1, 2], eye = 1.0, width = 1.4, height = 1.4})
	_m("magma_cube", "Magma Cube", {hp = 16, xp = 4, model = "magma_cube", speed = 0.06, arch = "slime", dmg = 6, dim = 1, fireproof = true,
		biomes = ["basalt_deltas", "nether_wastes"], group = [1, 2], eye = 1.0, width = 1.0, height = 1.0, split = "magma_cube"})
	_m("strider", "Strider", {hp = 20, xp = 1, model = "strider", speed = 0.06, arch = "passive", dim = 1, fireproof = true,
		rideable = true, biomes = ["nether_wastes", "soul_sand_valley"], group = [1, 2], eye = 1.2, width = 0.9, height = 1.7,
		water = true, lava_walk = true})
	_m("happy_ghast", "Happy Ghast", {hp = 20, xp = 1, model = "happy_ghast", speed = 0.06, arch = "passive", fly = true,
		rideable = true, fireproof = true, biomes = [], group = [1, 1], eye = 2.2, width = 4.0, height = 4.0, despawn = false})
	_m("ghastling", "Ghastling", {hp = 10, xp = 1, model = "ghast", speed = 0.08, arch = "ambient", fly = true, fireproof = true,
		biomes = [], group = [1, 1], eye = 0.6, width = 1.0, height = 1.0})


static func _end() -> void:
	_m("shulker", "Shulker", {hp = 30, xp = 5, model = "shulker", speed = 0.0, arch = "shulker", dmg = 4, dim = 2, fireproof = true,
		biomes = [], group = [1, 2], eye = 0.8, width = 1.0, height = 1.0, reach = 24.0, despawn = false})
	_m("sulfur_cube", "Sulfur Cube", {hp = 16, xp = 4, model = "sulfur_cube", speed = 0.05, arch = "sulfur_cube", dmg = 3,
		biomes = ["sulfur_caves"], group = [1, 2], eye = 0.9, width = 0.9, height = 0.9, light_max = 15})


static func _bosses() -> void:
	_m("ender_dragon", "Ender Dragon", {hp = 200, xp = 12000, model = "ender_dragon", speed = 0.14, arch = "boss_dragon",
		dmg = 10, dim = 2, fly = true, fireproof = true, biomes = [], group = [1, 1], eye = 3.0, width = 8.0, height = 4.0,
		reach = 100.0, despawn = false, despawn_range = 1000, armor = 0.0, attack_cd = 20, boss = true})
	_m("wither", "Wither", {hp = 300, xp = 50, model = "wither", speed = 0.08, arch = "boss_wither", dmg = 8, fly = true,
		fireproof = true, biomes = [], group = [1, 1], eye = 2.0, width = 1.0, height = 3.5, reach = 40.0, despawn = false,
		boss = true, attack_cd = 40})


static func _modern() -> void:
	_m("sulfur_cube_big", "Sulfur Cube", {"hidden": true, "hp": 32, "xp": 6, "model": "sulfur_cube", "speed": 0.05,
		"arch": "sulfur_cube", "dmg": 5, "biomes": [], "group": [1, 1], "eye": 1.4, "width": 1.6, "height": 1.6, "despawn": false})


static func get_def(n: String) -> Dictionary:
	return defs.get(n, {})


static func has(n: String) -> bool:
	return defs.has(n)


static func hostile(n: String) -> bool:
	return bool(defs.get(n, {}).get("hostile", false))


static func display(n: String) -> String:
	return String(defs.get(n, {}).get("display", n))


## Mob spawn egg colours (original: body + spot colour per mob family).
const EGG_COLORS := {
	"pig": ["#f0a5a2", "#db635f"], "cow": ["#443626", "#a1a1a1"], "sheep": ["#e7e7e7", "#ffb5b5"],
	"chicken": ["#a1a1a1", "#ff0000"], "rabbit": ["#995f40", "#ffb5b5"], "horse": ["#c09e7b", "#945b3f"],
	"donkey": ["#534539", "#a1a1a1"], "mule": ["#1b1a19", "#a1a1a1"], "camel": ["#ffd3a1", "#a1a1a1"],
	"wolf": ["#d7d3d3", "#c5c5c5"], "cat": ["#efc07d", "#a1662f"], "ocelot": ["#efde7d", "#564434"],
	"parrot": ["#0f8b1b", "#ff0000"], "fox": ["#d78c2b", "#ffffff"], "panda": ["#e7e7e7", "#1b1a19"],
	"polar_bear": ["#f0ffff", "#c8d4d4"], "goat": ["#a5937e", "#5c4f3f"], "llama": ["#e3cfb1", "#a3744d"],
	"trader_llama": ["#e3cfb1", "#4b6ba8"], "villager": ["#b6a58e", "#4e6b41"], "wandering_trader": ["#3552a1", "#4e6b41"],
	"iron_golem": ["#dbcfc0", "#a3a3a3"], "snow_golem": ["#f0ffff", "#f0a52a"], "copper_golem": ["#c1714a", "#7ad3a8"],
	"armadillo": ["#a3886b", "#6b543d"], "sniffer": ["#8d6c58", "#4e8f3a"], "frog": ["#d1a167", "#418f41"],
	"turtle": ["#7ac96b", "#35662e"], "axolotl": ["#f7a6e8", "#f7f2b0"], "bat": ["#4c3a2b", "#2b2118"],
	"bee": ["#eec15a", "#5c4a1e"], "allay": ["#5ec9e8", "#a3e0ff"], "squid": ["#2b3b5c", "#4b6ba8"],
	"glow_squid": ["#2b5c5c", "#7ae0e0"], "dolphin": ["#a3b1c4", "#e8e8f0"], "cod": ["#b98b5e", "#e0d0a0"],
	"salmon": ["#9c4a3c", "#d88a7a"], "tropical_fish": ["#f77a3a", "#f0e05a"], "pufferfish": ["#f0d05a", "#3a5ab0"],
	"tadpole": ["#4b3520", "#7a5a3a"], "nautilus": ["#e0c090", "#b08a5a"],
	"zombie": ["#00afaf", "#79a04c"], "husk": ["#7a6a4a", "#5a4a30"], "drowned": ["#3a8f8f", "#2a6a6a"],
	"zombie_villager": ["#4a8a3a", "#4e6b41"], "skeleton": ["#c1c1c1", "#a1a1a1"], "stray": ["#6a7a7a", "#9aa8a8"],
	"bogged": ["#3a5a3a", "#7a9a5a"], "parched": ["#c1b090", "#8a7a5a"], "wither_skeleton": ["#1b1a19", "#4b4b4b"],
	"creeper": ["#4aa02a", "#1b3a10"], "spider": ["#2b2b2b", "#a02020"], "cave_spider": ["#1b3a3a", "#2a8a8a"],
	"enderman": ["#0b0b0b", "#c02a8a"], "slime": ["#6ac04a", "#3a8a2a"], "witch": ["#3a2a4a", "#8a2a2a"],
	"phantom": ["#4a5a6a", "#2a3a4a"], "silverfish": ["#8a8a8a", "#5a5a5a"], "endermite": ["#5a4a6a", "#2a1a3a"],
	"guardian": ["#3a6a7a", "#f0a05a"], "elder_guardian": ["#2a5a6a", "#c0c0c0"], "pillager": ["#4a5a6a", "#2a3a4a"],
	"vindicator": ["#4a5a6a", "#6a7a8a"], "evoker": ["#4a5a6a", "#a0a0a0"], "vex": ["#5a8aa0", "#a0d0e0"],
	"ravager": ["#5a4a3a", "#2a2018"], "warden": ["#0a2a2a", "#3ac0b0"], "creaking": ["#3a2a1a", "#6a4a2a"],
	"breeze": ["#5a5aa0", "#a0e0ff"], "zombie_nautilus": ["#3a7a7a", "#5a9a6a"], "camel_husk": ["#c0a080", "#8a7a5a"],
	"zombie_horse": ["#4a6a3a", "#3a5a2a"], "skeleton_horse": ["#c1c1c1", "#8a8a8a"], "mooshroom": ["#a02020", "#c0c0c0"],
	"ghast": ["#f0f0f0", "#a0a0a0"], "blaze": ["#f0c02a", "#a07020"], "piglin": ["#e8a0a0", "#5a4a3a"],
	"piglin_brute": ["#d09090", "#3a2a1a"], "zombified_piglin": ["#5a8a4a", "#d09090"], "hoglin": ["#e0a0a0", "#6a4a3a"],
	"zoglin": ["#a0a0a0", "#8a5a5a"], "magma_cube": ["#3a1a1a", "#f0a02a"], "strider": ["#a04a4a", "#3a2a2a"],
	"happy_ghast": ["#c0e0ff", "#909090"], "ghastling": ["#e0e0e0", "#b0b0b0"], "shulker": ["#a07aa0", "#6a4a6a"],
	"sulfur_cube": ["#e8d84a", "#a0a020"], "sulfur_cube_big": ["#e8d84a", "#a0a020"],
	"ender_dragon": ["#1b1a19", "#8a5ae8"], "wither": ["#1b1a19", "#4a4a4a"],
}
