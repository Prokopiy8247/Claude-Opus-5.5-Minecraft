class_name ItemCatalog
extends RefCounted
## Data for all non-block items. Tool / armour tiers and dye families are generated from tables.

const TIERS := {
	# name: [harvest tier, mine speed, durability, damage bonus, enchantability, repair item]
	"wooden": [1, 2.0, 59, 0.0, 15, "planks"],
	"stone": [2, 4.0, 131, 1.0, 5, "cobblestone"],
	"copper": [2, 5.0, 190, 1.0, 13, "copper_ingot"],
	"iron": [3, 6.0, 250, 2.0, 14, "iron_ingot"],
	"golden": [1, 12.0, 32, 0.0, 22, "gold_ingot"],
	"diamond": [4, 8.0, 1561, 3.0, 10, "diamond"],
	"netherite": [4, 9.0, 2031, 4.0, 15, "netherite_ingot"],
}
const ARMOR := {
	# material: [durability multiplier, [boots, legs, chest, head] points, toughness, knockback res, enchantability]
	"leather": [5, [1, 2, 3, 1], 0.0, 0.0, 15],
	"copper": [11, [1, 3, 4, 2], 0.0, 0.0, 13],
	"chainmail": [15, [1, 4, 5, 2], 0.0, 0.0, 12],
	"iron": [15, [2, 5, 6, 2], 0.0, 0.0, 9],
	"golden": [7, [1, 3, 5, 2], 0.0, 0.0, 25],
	"diamond": [33, [3, 6, 8, 3], 2.0, 0.0, 10],
	"netherite": [37, [3, 6, 8, 3], 3.0, 0.1, 15],
}
const ARMOR_PIECES := ["boots", "leggings", "chestplate", "helmet"]
const ARMOR_BASE := [13, 15, 16, 11]

var items: Array = []     # Array[Dictionary]
var _names := {}


func _i(n: String, p: Dictionary = {}) -> void:
	if _names.has(n):
		return
	var d := {"name": n, "tabs": ["ingredients"], "icon": "sprite:" + n}
	d.merge(p, true)
	items.append(d)
	_names[n] = true


func build() -> void:
	_tools()
	_armor()
	_combat()
	_food()
	_materials()
	_utility()


func _tools() -> void:
	for t in TIERS:
		var e: Array = TIERS[t]
		var tier: int = e[0]
		var speed: float = e[1]
		var dur: int = e[2]
		var bonus: float = e[3]
		var ench: int = e[4]
		var common := {"tier": tier, "mine_speed": speed, "durability": dur, "enchantability": ench, "material": t,
			"max_stack": 1, "props": {"repair": e[5]}, "rarity": 0}
		var fuel := 200 if t == "wooden" else 0
		var sword_dmg := 4.0 + bonus
		var axe_dmg: float = {"wooden": 7.0, "stone": 9.0, "copper": 9.0, "iron": 9.0, "golden": 7.0, "diamond": 9.0, "netherite": 10.0}[t]
		var axe_spd: float = {"wooden": 0.8, "stone": 0.8, "copper": 0.85, "iron": 0.9, "golden": 1.0, "diamond": 1.0, "netherite": 1.0}[t]
		var hoe_spd: float = {"wooden": 1.0, "stone": 2.0, "copper": 2.5, "iron": 3.0, "golden": 1.0, "diamond": 4.0, "netherite": 4.0}[t]
		var rar := 1 if t == "netherite" else 0
		_i(t + "_sword", _m(common, {"kind": "weapon", "tool": "sword", "damage": sword_dmg, "attack_speed": 1.6, "tabs": ["combat"], "fuel": fuel, "rarity": rar}))
		_i(t + "_shovel", _m(common, {"kind": "tool", "tool": "shovel", "damage": 2.5 + bonus, "attack_speed": 1.0, "tabs": ["tools"], "fuel": fuel, "use": "shovel", "rarity": rar}))
		_i(t + "_pickaxe", _m(common, {"kind": "tool", "tool": "pickaxe", "damage": 2.0 + bonus, "attack_speed": 1.2, "tabs": ["tools"], "fuel": fuel, "rarity": rar}))
		_i(t + "_axe", _m(common, {"kind": "tool", "tool": "axe", "damage": axe_dmg, "attack_speed": axe_spd, "tabs": ["tools", "combat"], "fuel": fuel, "use": "axe", "rarity": rar}))
		_i(t + "_hoe", _m(common, {"kind": "tool", "tool": "hoe", "damage": 1.0, "attack_speed": hoe_spd, "tabs": ["tools"], "fuel": fuel, "use": "hoe", "rarity": rar}))
		_i(t + "_spear", _m(common, {"kind": "weapon", "tool": "spear", "damage": 3.0 + bonus, "attack_speed": 1.1, "reach_bonus": 1.5, "tabs": ["combat"], "fuel": fuel, "use": "spear", "rarity": rar}))


func _armor() -> void:
	for mat in ARMOR:
		var e: Array = ARMOR[mat]
		for slot in 4:
			var piece: String = ARMOR_PIECES[slot]
			var n: String = mat + "_" + piece
			_i(n, {"kind": "armor", "armor_slot": slot, "armor": e[1][slot], "toughness": e[2], "knockback_res": e[3],
				"durability": ARMOR_BASE[slot] * e[0], "enchantability": e[4], "material": mat, "max_stack": 1, "tabs": ["combat"],
				"use": "equip", "rarity": 1 if mat == "netherite" else 0,
				"props": {"repair": {"leather": "leather", "copper": "copper_ingot", "chainmail": "iron_ingot", "iron": "iron_ingot",
					"golden": "gold_ingot", "diamond": "diamond", "netherite": "netherite_ingot"}[mat]}})
	_i("turtle_helmet", {"kind": "armor", "armor_slot": 3, "armor": 2, "durability": 275, "enchantability": 9, "material": "turtle",
		"max_stack": 1, "tabs": ["combat"], "use": "equip", "props": {"repair": "turtle_scute"}})
	_i("elytra", {"kind": "armor", "armor_slot": 2, "armor": 0, "durability": 432, "max_stack": 1, "tabs": ["tools"], "use": "equip",
		"rarity": 1, "props": {"repair": "phantom_membrane", "elytra": true}})
	_i("wolf_armor", {"kind": "armor", "durability": 64, "max_stack": 1, "tabs": ["combat"], "props": {"animal_armor": "wolf"}})
	for hm in ["leather", "iron", "golden", "diamond"]:
		_i(hm + "_horse_armor", {"kind": "armor", "max_stack": 1, "tabs": ["combat"], "props": {"animal_armor": "horse"}})


func _combat() -> void:
	_i("bow", {"kind": "weapon", "durability": 384, "max_stack": 1, "tabs": ["combat"], "use": "bow", "enchantability": 1})
	_i("crossbow", {"kind": "weapon", "durability": 465, "max_stack": 1, "tabs": ["combat"], "use": "crossbow", "enchantability": 1})
	_i("arrow", {"tabs": ["combat"]})
	_i("spectral_arrow", {"tabs": ["combat"]})
	_i("tipped_arrow", {"tabs": ["combat"], "props": {"potion": true}})
	_i("trident", {"kind": "weapon", "damage": 9.0, "attack_speed": 1.1, "durability": 250, "max_stack": 1, "tabs": ["combat"],
		"use": "trident", "rarity": 2, "enchantability": 1})
	_i("shield", {"kind": "weapon", "durability": 336, "max_stack": 1, "tabs": ["combat"], "use": "shield", "fuel": 300})
	_i("mace", {"kind": "weapon", "damage": 6.0, "attack_speed": 0.6, "durability": 500, "max_stack": 1, "tabs": ["combat"], "use": "",
		"rarity": 3, "enchantability": 15, "props": {"mace": true}})
	_i("totem_of_undying", {"max_stack": 1, "tabs": ["combat"], "rarity": 1})
	_i("wind_charge", {"max_stack": 64, "tabs": ["combat"], "use": "throw", "props": {"projectile": "wind_charge"}})
	_i("snowball", {"max_stack": 16, "tabs": ["combat"], "use": "throw", "props": {"projectile": "snowball"}})
	_i("egg", {"max_stack": 16, "tabs": ["ingredients"], "use": "throw", "props": {"projectile": "egg"}})
	_i("ender_pearl", {"max_stack": 16, "tabs": ["combat"], "use": "throw", "props": {"projectile": "ender_pearl"}})
	_i("fire_charge", {"tabs": ["combat"], "use": "fire_charge"})
	_i("end_crystal", {"tabs": ["combat"], "use": "end_crystal", "rarity": 2})
	_i("firework_rocket", {"tabs": ["tools", "combat"], "use": "firework"})
	_i("firework_star", {"tabs": ["ingredients"]})


func _food() -> void:
	var foods := [
		["apple", 4, 2.4], ["golden_apple", 4, 9.6], ["enchanted_golden_apple", 4, 9.6], ["bread", 5, 6.0], ["baked_potato", 5, 6.0],
		["potato", 1, 0.6], ["poisonous_potato", 2, 1.2], ["carrot", 3, 3.6], ["golden_carrot", 6, 14.4], ["beetroot", 1, 1.2],
		["beetroot_soup", 6, 7.2], ["beef", 3, 1.8], ["cooked_beef", 8, 12.8], ["porkchop", 3, 1.8], ["cooked_porkchop", 8, 12.8],
		["chicken", 2, 1.2], ["cooked_chicken", 6, 7.2], ["mutton", 2, 1.2], ["cooked_mutton", 6, 9.6], ["rabbit", 3, 1.8],
		["cooked_rabbit", 5, 6.0], ["rabbit_stew", 10, 12.0], ["cod", 2, 0.4], ["cooked_cod", 5, 6.0], ["salmon", 2, 0.4],
		["cooked_salmon", 6, 9.6], ["tropical_fish", 1, 0.2], ["pufferfish", 1, 0.2], ["cookie", 2, 0.4], ["melon_slice", 2, 1.2],
		["pumpkin_pie", 8, 4.8], ["mushroom_stew", 6, 7.2], ["suspicious_stew", 6, 7.2], ["rotten_flesh", 4, 0.8],
		["spider_eye", 2, 3.2], ["sweet_berries", 2, 0.4], ["glow_berries", 2, 0.4], ["dried_kelp", 1, 0.6], ["honey_bottle", 6, 1.2],
		["chorus_fruit", 4, 2.4],
	]
	for f in foods:
		var p := {"kind": "food", "food": f[1], "saturation": f[2], "tabs": ["food"], "use": "eat"}
		if f[0] in ["mushroom_stew", "beetroot_soup", "rabbit_stew", "suspicious_stew"]:
			p["max_stack"] = 1
			p["props"] = {"returns": "bowl"}
		if f[0] == "honey_bottle":
			p["max_stack"] = 16
			p["props"] = {"returns": "glass_bottle", "drink": true}
		if f[0] == "golden_apple":
			p["rarity"] = 1
			p["props"] = {"effects": [["regeneration", 100, 1], ["absorption", 2400, 0]], "always": true}
		if f[0] == "enchanted_golden_apple":
			p["rarity"] = 2
			p["props"] = {"effects": [["regeneration", 400, 1], ["absorption", 2400, 3], ["resistance", 6000, 0], ["fire_resistance", 6000, 0]], "always": true, "glint": true}
		if f[0] == "rotten_flesh":
			p["props"] = {"effects": [["hunger", 600, 0, 0.8]]}
		if f[0] == "spider_eye" or f[0] == "poisonous_potato":
			p["props"] = {"effects": [["poison", 100, 0, 1.0 if f[0] == "spider_eye" else 0.6]]}
		if f[0] == "pufferfish":
			p["props"] = {"effects": [["poison", 1200, 1], ["hunger", 300, 2], ["nausea", 300, 0]]}
		if f[0] == "chicken":
			p["props"] = {"effects": [["hunger", 600, 0, 0.3]]}
		if f[0] == "chorus_fruit":
			p["props"] = {"teleport": true, "always": true}
		if f[0] in ["potato", "carrot", "beetroot", "sweet_berries", "glow_berries"]:
			p["props"] = {"plants": {"potato": "potatoes", "carrot": "carrots", "beetroot": "", "sweet_berries": "sweet_berry_bush",
				"glow_berries": "cave_vines"}[f[0]]}
		_i(f[0], p)
	_i("milk_bucket", {"kind": "bucket", "max_stack": 1, "tabs": ["food", "tools"], "use": "drink", "props": {"milk": true, "returns": "bucket"}})
	_i("cake", {"max_stack": 1, "tabs": ["food"], "use": "place_block", "block": "cake"})


func _materials() -> void:
	var mats := ["stick", "coal", "charcoal", "diamond", "emerald", "raw_iron", "raw_copper", "raw_gold", "iron_ingot", "copper_ingot",
		"gold_ingot", "netherite_ingot", "netherite_scrap", "iron_nugget", "gold_nugget", "copper_nugget", "lapis_lazuli", "quartz",
		"amethyst_shard", "flint", "feather", "leather", "rabbit_hide", "rabbit_foot", "string", "gunpowder", "bone", "bone_meal",
		"slime_ball", "ender_eye", "blaze_rod", "blaze_powder", "ghast_tear", "magma_cream", "nether_star", "shulker_shell",
		"phantom_membrane", "prismarine_shard", "prismarine_crystals", "nautilus_shell", "heart_of_the_sea", "turtle_scute",
		"armadillo_scute", "echo_shard", "disc_fragment_5", "clay_ball", "brick", "nether_brick", "paper", "book", "sugar",
		"wheat", "wheat_seeds", "pumpkin_seeds", "melon_seeds", "beetroot_seeds", "torchflower_seeds", "pitcher_pod", "cocoa_beans",
		"ink_sac", "glow_ink_sac", "glowstone_dust", "honeycomb", "glass_bottle", "experience_bottle", "breeze_rod", "resin_clump",
		"resin_brick", "fermented_spider_eye", "glistering_melon_slice", "rabbit_foot", "dragon_breath", "trial_key",
		"ominous_trial_key", "bowl", "nether_wart", "sugar_cane", "kelp", "bamboo", "redstone", "popped_chorus_fruit",
		"netherite_upgrade_smithing_template", "scute_placeholder"]
	for m in mats:
		if m == "scute_placeholder":
			continue
		var p := {"tabs": ["ingredients"]}
		match m:
			"coal", "charcoal":
				p["fuel"] = 1600
			"stick", "bamboo":
				p["fuel"] = 100 if m == "stick" else 50
			"blaze_rod":
				p["fuel"] = 2400
			"bone_meal":
				p["use"] = "bone_meal"
			"ender_eye":
				p["use"] = "ender_eye"
				p["tabs"] = ["tools"]
			"experience_bottle":
				p["use"] = "throw"
				p["props"] = {"projectile": "experience_bottle"}
				p["rarity"] = 1
			"glass_bottle":
				p["use"] = "glass_bottle"
				p["tabs"] = ["ingredients", "tools"]
			"nether_star":
				p["rarity"] = 1
				p["props"] = {"glint": true}
			"heart_of_the_sea", "dragon_breath", "echo_shard", "netherite_upgrade_smithing_template":
				p["rarity"] = 1
			"wheat_seeds":
				p["props"] = {"plants": "wheat"}
			"pumpkin_seeds":
				p["props"] = {"plants": "pumpkin_stem"}
			"melon_seeds":
				p["props"] = {"plants": "melon_stem"}
			"beetroot_seeds":
				p["props"] = {"plants": "beetroots"}
			"nether_wart":
				p["props"] = {"plants": "nether_wart"}
			"cocoa_beans":
				p["props"] = {"plants": "cocoa"}
			"sugar_cane":
				p["block"] = "sugar_cane"
				p["tabs"] = ["natural"]
			"kelp":
				p["block"] = "kelp"
				p["tabs"] = ["natural"]
			"bamboo":
				p["block"] = "bamboo"
				p["tabs"] = ["natural"]
			"redstone":
				p["block"] = "redstone_wire"
				p["tabs"] = ["redstone", "ingredients"]
			"string":
				p["tabs"] = ["ingredients"]
			"resin_clump":
				p["block"] = "resin_clump"
		_i(m, p)
	for d in BlockCatalog.DYES:
		_i(d + "_dye", {"tabs": ["ingredients"], "use": "dye", "color": Color.html(TexPalettes.DYE[d]), "props": {"dye": d}})
		_i(d + "_harness", {"max_stack": 1, "tabs": ["tools"], "color": Color.html(TexPalettes.DYE[d]), "props": {"harness": d}})


func _utility() -> void:
	var T := ["tools"]
	_i("bucket", {"kind": "bucket", "max_stack": 16, "tabs": T, "use": "bucket"})
	_i("water_bucket", {"kind": "bucket", "max_stack": 1, "tabs": T, "use": "bucket", "props": {"fluid": "water"}})
	_i("lava_bucket", {"kind": "bucket", "max_stack": 1, "tabs": T, "use": "bucket", "fuel": 20000, "props": {"fluid": "lava"}})
	_i("powder_snow_bucket", {"kind": "bucket", "max_stack": 1, "tabs": T, "use": "bucket", "props": {"fluid": "powder_snow"}})
	for fish in ["cod", "salmon", "tropical_fish", "pufferfish", "axolotl", "tadpole"]:
		_i(fish + "_bucket", {"kind": "bucket", "max_stack": 1, "tabs": T, "use": "bucket", "props": {"fluid": "water", "mob": fish}})
	_i("flint_and_steel", {"kind": "tool", "durability": 64, "max_stack": 1, "tabs": T, "use": "flint_and_steel"})
	_i("shears", {"kind": "tool", "tool": "shears", "durability": 238, "max_stack": 1, "tabs": T, "use": "shears", "mine_speed": 1.5})
	_i("fishing_rod", {"kind": "tool", "durability": 64, "max_stack": 1, "tabs": T, "use": "fishing_rod", "enchantability": 1})
	_i("carrot_on_a_stick", {"kind": "tool", "durability": 25, "max_stack": 1, "tabs": T, "use": "boost_stick"})
	_i("warped_fungus_on_a_stick", {"kind": "tool", "durability": 100, "max_stack": 1, "tabs": T, "use": "boost_stick"})
	_i("brush", {"kind": "tool", "durability": 64, "max_stack": 1, "tabs": T, "use": "brush"})
	_i("compass", {"tabs": T, "use": "", "props": {"compass": true}})
	_i("recovery_compass", {"tabs": T, "rarity": 1, "props": {"compass": true}})
	_i("clock", {"tabs": T, "props": {"clock": true}})
	_i("map", {"tabs": T, "use": "map"})
	_i("filled_map", {"max_stack": 1, "tabs": [], "use": "map"})
	_i("spyglass", {"max_stack": 1, "tabs": T, "use": "spyglass"})
	_i("lead", {"tabs": T, "use": "lead"})
	_i("name_tag", {"tabs": T, "use": "name_tag"})
	_i("saddle", {"max_stack": 1, "tabs": T, "use": "saddle"})
	_i("bundle", {"max_stack": 1, "tabs": T})
	_i("book_and_quill", {"max_stack": 1, "tabs": T})
	_i("written_book", {"max_stack": 16, "tabs": []})
	_i("enchanted_book", {"max_stack": 1, "tabs": ["ingredients"], "rarity": 1, "props": {"glint": true}})
	_i("potion", {"max_stack": 1, "tabs": ["food"], "use": "drink", "props": {"potion": true, "returns": "glass_bottle"}})
	_i("splash_potion", {"max_stack": 1, "tabs": ["food"], "use": "throw", "props": {"potion": true, "projectile": "splash_potion"}})
	_i("lingering_potion", {"max_stack": 1, "tabs": ["food"], "use": "throw", "props": {"potion": true, "projectile": "splash_potion"}})
	_i("ominous_bottle", {"tabs": ["food"], "use": "drink", "rarity": 1})
	_i("goat_horn", {"max_stack": 1, "tabs": T, "use": "horn"})
	for disc in ["13", "cat", "blocks", "chirp", "far", "mall", "mellohi", "stal", "strad", "ward", "11", "wait", "otherside",
			"pigstep", "relic", "creator", "precipice", "tears", "lava_chicken"]:
		_i("music_disc_" + disc, {"max_stack": 1, "tabs": T, "rarity": 1, "use": "", "props": {"disc": disc}})
	_i("minecart", {"max_stack": 1, "tabs": T, "use": "minecart", "props": {"vehicle": "minecart"}})
	_i("chest_minecart", {"max_stack": 1, "tabs": T, "use": "minecart", "props": {"vehicle": "chest_minecart"}})
	_i("furnace_minecart", {"max_stack": 1, "tabs": T, "use": "minecart", "props": {"vehicle": "furnace_minecart"}})
	_i("tnt_minecart", {"max_stack": 1, "tabs": T, "use": "minecart", "props": {"vehicle": "tnt_minecart"}})
	_i("hopper_minecart", {"max_stack": 1, "tabs": T, "use": "minecart", "props": {"vehicle": "hopper_minecart"}})
	for w in ["oak", "spruce", "birch", "jungle", "acacia", "dark_oak", "mangrove", "cherry", "pale_oak"]:
		_i(w + "_boat", {"max_stack": 1, "tabs": T, "use": "boat", "fuel": 1200, "props": {"vehicle": "boat", "wood": w}})
		_i(w + "_chest_boat", {"max_stack": 1, "tabs": T, "use": "boat", "fuel": 1200, "props": {"vehicle": "chest_boat", "wood": w}})
	_i("bamboo_raft", {"max_stack": 1, "tabs": T, "use": "boat", "props": {"vehicle": "boat", "wood": "bamboo"}})
	_i("bamboo_chest_raft", {"max_stack": 1, "tabs": T, "use": "boat", "props": {"vehicle": "chest_boat", "wood": "bamboo"}})
	_i("armor_stand", {"max_stack": 16, "tabs": ["functional"], "use": "armor_stand"})
	_i("item_frame", {"tabs": ["functional"], "use": "item_frame"})
	_i("glow_item_frame", {"tabs": ["functional"], "use": "item_frame", "props": {"glow": true}})
	_i("painting", {"tabs": ["functional"], "use": "painting"})
	for sign_wood in ["oak", "spruce", "birch", "jungle", "acacia", "dark_oak", "mangrove", "cherry", "pale_oak", "crimson", "warped", "bamboo"]:
		_i(sign_wood + "_sign", {"max_stack": 16, "tabs": ["functional"], "use": "", "fuel": 200})


static func _m(a: Dictionary, b: Dictionary) -> Dictionary:
	var r := a.duplicate(true)
	r.merge(b, true)
	return r
