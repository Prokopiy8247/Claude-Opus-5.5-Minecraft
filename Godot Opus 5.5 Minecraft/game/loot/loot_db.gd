class_name LootDB
extends RefCounted
## Data-driven loot tables: block drops (with Silk Touch / Fortune), mob drops (with Looting,
## player-kill-only rare drops) and structure chest tables.
## Table entry format: [item, min, max, chance, flags]   flags: "fortune", "looting", "player", "silk_not"

static var block_tables := {}
static var mob_tables := {}
static var chest_tables := {}
static var inited := false


static func init() -> void:
	if inited:
		return
	_blocks()
	_mobs()
	_chests()
	inited = true


static func _blocks() -> void:
	block_tables["gravel"] = [["flint", 1, 1, 0.1, "fortune_chance"], ["gravel", 1, 1, 1.0, "else"]]
	block_tables["snow_layer"] = [["snowball", 1, 1, 1.0, "layers"]]
	block_tables["grass"] = [["wheat_seeds", 1, 1, 0.125, "fortune_seed"]]
	block_tables["tall_plant"] = [["wheat_seeds", 1, 1, 0.125, "fortune_seed"]]
	block_tables["wheat"] = [["wheat", 1, 1, 1.0, "mature"], ["wheat_seeds", 1, 1, 1.0, ""], ["wheat_seeds", 0, 3, 1.0, "mature_bonus"]]
	block_tables["carrots"] = [["carrot", 1, 1, 1.0, ""], ["carrot", 1, 4, 1.0, "mature_bonus"]]
	block_tables["potatoes"] = [["potato", 1, 1, 1.0, ""], ["potato", 1, 4, 1.0, "mature_bonus"], ["poisonous_potato", 1, 1, 0.02, "mature"]]
	block_tables["beetroots"] = [["beetroot", 1, 1, 1.0, "mature"], ["beetroot_seeds", 1, 1, 1.0, ""], ["beetroot_seeds", 0, 3, 1.0, "mature_bonus"]]
	block_tables["nether_wart"] = [["nether_wart", 1, 1, 1.0, "immature"], ["nether_wart", 2, 4, 1.0, "mature"]]
	block_tables["sweet_berry_bush"] = [["sweet_berries", 2, 3, 1.0, "age3"], ["sweet_berries", 1, 2, 1.0, "age2"]]
	block_tables["cave_vines"] = [["glow_berries", 1, 1, 1.0, "berries"]]
	block_tables["chorus_plant"] = [["chorus_fruit", 0, 1, 1.0, ""]]
	block_tables["gilded_blackstone"] = [["gold_nugget", 2, 5, 0.1, ""], ["gilded_blackstone", 1, 1, 1.0, "else"]]
	block_tables["leaves_azalea"] = [["azalea", 1, 1, 0.05, "sapling"], ["stick", 1, 2, 0.02, ""]]


static func _mobs() -> void:
	var t := mob_tables
	t["pig"] = [["porkchop", 1, 3, 1.0, "looting cook"]]
	t["cow"] = [["beef", 1, 3, 1.0, "looting cook"], ["leather", 0, 2, 1.0, "looting"]]
	t["mooshroom"] = t["cow"]
	t["sheep"] = [["mutton", 1, 2, 1.0, "looting cook"], ["@wool", 1, 1, 1.0, "wool"]]
	t["chicken"] = [["chicken", 1, 1, 1.0, "looting cook"], ["feather", 0, 2, 1.0, "looting"]]
	t["rabbit"] = [["rabbit", 0, 1, 1.0, "looting cook"], ["rabbit_hide", 0, 1, 1.0, "looting"], ["rabbit_foot", 1, 1, 0.1, "player"]]
	t["horse"] = [["leather", 0, 2, 1.0, "looting"]]
	t["donkey"] = t["horse"]
	t["mule"] = t["horse"]
	t["llama"] = t["horse"]
	t["trader_llama"] = t["horse"]
	t["camel"] = []
	t["zombie"] = [["rotten_flesh", 0, 2, 1.0, "looting"], ["iron_ingot", 1, 1, 0.025, "player"], ["carrot", 1, 1, 0.025, "player"], ["potato", 1, 1, 0.025, "player"]]
	t["husk"] = t["zombie"]
	t["zombie_villager"] = t["zombie"]
	t["drowned"] = [["rotten_flesh", 0, 2, 1.0, "looting"], ["copper_ingot", 1, 1, 0.11, "player"]]
	t["skeleton"] = [["bone", 0, 2, 1.0, "looting"], ["arrow", 0, 2, 1.0, "looting"]]
	t["stray"] = [["bone", 0, 2, 1.0, "looting"], ["arrow", 0, 2, 1.0, "looting"], ["tipped_arrow", 0, 1, 0.5, "player"]]
	t["bogged"] = [["bone", 0, 2, 1.0, "looting"], ["arrow", 0, 2, 1.0, "looting"]]
	t["parched"] = [["bone", 0, 2, 1.0, "looting"], ["arrow", 0, 2, 1.0, "looting"]]
	t["wither_skeleton"] = [["coal", 0, 1, 1.0, "looting"], ["bone", 0, 2, 1.0, "looting"], ["wither_skeleton_skull", 1, 1, 0.025, "player"]]
	t["creeper"] = [["gunpowder", 0, 2, 1.0, "looting"]]
	t["spider"] = [["string", 0, 2, 1.0, "looting"], ["spider_eye", 1, 1, 0.33, "player"]]
	t["cave_spider"] = t["spider"]
	t["enderman"] = [["ender_pearl", 0, 1, 1.0, "looting"]]
	t["slime"] = [["slime_ball", 0, 2, 1.0, "looting small"]]
	t["magma_cube"] = [["magma_cream", 0, 1, 0.25, "looting notsmall"]]
	t["witch"] = [["glass_bottle", 0, 2, 0.3, "looting"], ["glowstone_dust", 0, 2, 0.3, "looting"], ["gunpowder", 0, 2, 0.3, "looting"],
		["redstone", 0, 2, 0.3, "looting"], ["spider_eye", 0, 2, 0.3, "looting"], ["sugar", 0, 2, 0.3, "looting"], ["stick", 0, 2, 0.3, "looting"]]
	t["ghast"] = [["ghast_tear", 0, 1, 1.0, "looting"], ["gunpowder", 0, 2, 1.0, "looting"]]
	t["blaze"] = [["blaze_rod", 0, 1, 1.0, "looting player"]]
	t["piglin"] = [["gold_ingot", 0, 1, 0.08, "player"]]
	t["piglin_brute"] = []
	t["zombified_piglin"] = [["rotten_flesh", 0, 1, 1.0, "looting"], ["gold_nugget", 0, 1, 1.0, "looting"], ["gold_ingot", 1, 1, 0.025, "player"]]
	t["hoglin"] = [["porkchop", 2, 4, 1.0, "looting cook"], ["leather", 0, 1, 1.0, "looting"]]
	t["zoglin"] = [["rotten_flesh", 1, 3, 1.0, "looting"]]
	t["strider"] = [["string", 2, 5, 1.0, "looting"]]
	t["shulker"] = [["shulker_shell", 1, 1, 0.5, "looting"]]
	t["guardian"] = [["prismarine_shard", 0, 2, 1.0, "looting"], ["cod", 1, 1, 0.4, "looting"], ["prismarine_crystals", 1, 1, 0.4, "looting"]]
	t["elder_guardian"] = [["prismarine_shard", 0, 2, 1.0, "looting"], ["wet_sponge", 1, 1, 1.0, "player"], ["cod", 1, 1, 0.5, ""]]
	t["squid"] = [["ink_sac", 1, 3, 1.0, "looting"]]
	t["glow_squid"] = [["glow_ink_sac", 1, 3, 1.0, "looting"]]
	t["cod"] = [["cod", 1, 1, 1.0, "cook"], ["bone_meal", 1, 1, 0.05, ""]]
	t["salmon"] = [["salmon", 1, 1, 1.0, "cook"], ["bone_meal", 1, 1, 0.05, ""]]
	t["tropical_fish"] = [["tropical_fish", 1, 1, 1.0, ""], ["bone_meal", 1, 1, 0.05, ""]]
	t["pufferfish"] = [["pufferfish", 1, 1, 1.0, ""], ["bone_meal", 1, 1, 0.05, ""]]
	t["dolphin"] = [["cod", 0, 1, 1.0, "looting cook"]]
	t["turtle"] = [["seagrass", 0, 2, 1.0, "looting"]]
	t["polar_bear"] = [["cod", 0, 2, 0.75, "looting cook"], ["salmon", 0, 2, 0.25, "looting cook"]]
	t["panda"] = [["bamboo", 1, 1, 1.0, ""]]
	t["goat"] = []
	t["fox"] = []
	t["phantom"] = [["phantom_membrane", 0, 1, 1.0, "looting player"]]
	t["pillager"] = [["arrow", 0, 2, 1.0, "looting"]]
	t["vindicator"] = [["emerald", 0, 1, 1.0, "looting player"]]
	t["evoker"] = [["totem_of_undying", 1, 1, 1.0, ""], ["emerald", 0, 1, 1.0, "looting player"]]
	t["ravager"] = [["saddle", 1, 1, 1.0, ""]]
	t["iron_golem"] = [["iron_ingot", 3, 5, 1.0, ""], ["poppy", 0, 2, 1.0, ""]]
	t["snow_golem"] = [["snowball", 0, 15, 1.0, ""]]
	t["copper_golem"] = [["copper_ingot", 1, 3, 1.0, ""]]
	t["breeze"] = [["breeze_rod", 1, 2, 1.0, "looting player"]]
	t["warden"] = [["sculk_catalyst", 1, 1, 1.0, ""]]
	t["wither"] = [["nether_star", 1, 1, 1.0, ""]]
	t["ender_dragon"] = []
	t["armadillo"] = [["armadillo_scute", 0, 1, 0.3, ""]]
	t["sniffer"] = [["moss_block", 0, 1, 1.0, ""]]
	t["bee"] = []
	t["creaking"] = [["resin_clump", 0, 1, 1.0, ""]]
	t["sulfur_cube"] = [["slime_ball", 0, 1, 1.0, "looting small"], ["sulfur", 0, 1, 0.5, ""]]
	t["happy_ghast"] = []
	t["ghastling"] = []
	t["villager"] = []
	t["wandering_trader"] = []
	t["endermite"] = []
	t["silverfish"] = []
	t["vex"] = []
	t["allay"] = []
	t["bat"] = []
	t["frog"] = []
	t["tadpole"] = []
	t["axolotl"] = []
	t["parrot"] = [["feather", 1, 2, 1.0, "looting"]]
	t["ocelot"] = []
	t["cat"] = [["string", 0, 2, 1.0, "looting"]]
	t["wolf"] = []
	t["skeleton_horse"] = [["bone", 0, 2, 1.0, "looting"]]
	t["zombie_horse"] = [["rotten_flesh", 0, 2, 1.0, "looting"]]
	t["nautilus"] = [["nautilus_shell", 0, 1, 0.3, ""]]
	t["zombie_nautilus"] = [["rotten_flesh", 0, 2, 1.0, "looting"]]
	t["camel_husk"] = [["rotten_flesh", 0, 2, 1.0, "looting"]]


static func _chests() -> void:
	var c := chest_tables
	c["village"] = {"rolls": [3, 8], "items": [["bread", 1, 4, 15], ["apple", 1, 5, 15], ["wheat", 1, 8, 10], ["iron_ingot", 1, 5, 10],
		["emerald", 1, 4, 5], ["oak_sapling", 1, 3, 5], ["iron_pickaxe", 1, 1, 3], ["iron_sword", 1, 1, 3], ["iron_helmet", 1, 1, 3],
		["iron_chestplate", 1, 1, 3], ["gold_ingot", 1, 3, 5], ["torch", 1, 8, 8], ["diamond", 1, 3, 1], ["saddle", 1, 1, 2]]}
	c["mineshaft"] = {"rolls": [4, 8], "items": [["rail", 4, 8, 20], ["torch", 1, 16, 15], ["bread", 1, 3, 15], ["iron_ingot", 1, 5, 10],
		["gold_ingot", 1, 3, 5], ["redstone", 4, 9, 5], ["lapis_lazuli", 4, 9, 5], ["diamond", 1, 2, 3], ["coal", 3, 8, 10],
		["melon_seeds", 2, 4, 10], ["pumpkin_seeds", 2, 4, 10], ["iron_pickaxe", 1, 1, 1], ["name_tag", 1, 1, 3],
		["enchanted_book", 1, 1, 2], ["golden_apple", 1, 1, 2], ["powered_rail", 1, 4, 5], ["activator_rail", 1, 4, 5]]}
	c["stronghold"] = {"rolls": [2, 5], "items": [["ender_pearl", 1, 1, 10], ["diamond", 1, 3, 3], ["iron_ingot", 1, 5, 10],
		["gold_ingot", 1, 3, 5], ["redstone", 4, 9, 5], ["bread", 1, 3, 15], ["apple", 1, 3, 15], ["iron_pickaxe", 1, 1, 5],
		["iron_sword", 1, 1, 5], ["iron_chestplate", 1, 1, 5], ["iron_helmet", 1, 1, 5], ["iron_leggings", 1, 1, 5],
		["iron_boots", 1, 1, 5], ["golden_apple", 1, 1, 1], ["saddle", 1, 1, 1], ["enchanted_book", 1, 1, 1], ["book", 1, 3, 10]]}
	c["desert_pyramid"] = {"rolls": [2, 6], "items": [["diamond", 1, 3, 5], ["iron_ingot", 1, 5, 15], ["gold_ingot", 2, 7, 15],
		["emerald", 1, 3, 15], ["bone", 4, 6, 25], ["spider_eye", 1, 3, 25], ["rotten_flesh", 3, 7, 25], ["saddle", 1, 1, 20],
		["iron_horse_armor", 1, 1, 15], ["golden_horse_armor", 1, 1, 10], ["diamond_horse_armor", 1, 1, 5], ["enchanted_book", 1, 1, 20],
		["golden_apple", 1, 1, 20], ["enchanted_golden_apple", 1, 1, 2], ["gunpowder", 1, 8, 10], ["sand", 1, 8, 10], ["string", 1, 8, 10]]}
	c["jungle_temple"] = {"rolls": [2, 6], "items": [["diamond", 1, 3, 3], ["iron_ingot", 1, 5, 10], ["gold_ingot", 2, 7, 15],
		["emerald", 1, 3, 2], ["bone", 4, 6, 20], ["rotten_flesh", 3, 7, 16], ["saddle", 1, 1, 3], ["iron_horse_armor", 1, 1, 1],
		["enchanted_book", 1, 1, 1], ["bamboo", 1, 3, 15]]}
	c["shipwreck"] = {"rolls": [3, 10], "items": [["iron_ingot", 1, 5, 90], ["gold_nugget", 1, 10, 50], ["emerald", 1, 5, 40],
		["diamond", 1, 1, 5], ["experience_bottle", 1, 1, 5], ["paper", 1, 12, 20], ["feather", 1, 5, 10], ["book", 1, 5, 5],
		["carrot", 4, 8, 7], ["potato", 2, 6, 7], ["wheat", 8, 21, 7], ["coal", 2, 8, 6], ["rotten_flesh", 5, 24, 8], ["leather_helmet", 1, 1, 3]]}
	c["buried_treasure"] = {"rolls": [5, 8], "items": [["heart_of_the_sea", 1, 1, 100], ["iron_ingot", 1, 4, 20], ["gold_ingot", 1, 4, 10],
		["tnt", 1, 2, 5], ["emerald", 4, 8, 5], ["diamond", 1, 2, 5], ["prismarine_crystals", 1, 5, 5], ["cooked_cod", 2, 4, 10],
		["cooked_salmon", 2, 4, 10], ["iron_sword", 1, 1, 5], ["leather_chestplate", 1, 1, 5]]}
	c["nether_fortress"] = {"rolls": [2, 4], "items": [["diamond", 1, 3, 5], ["iron_ingot", 1, 5, 5], ["gold_ingot", 1, 3, 15],
		["golden_sword", 1, 1, 5], ["golden_chestplate", 1, 1, 5], ["flint_and_steel", 1, 1, 5], ["nether_wart", 3, 7, 5],
		["saddle", 1, 1, 10], ["golden_horse_armor", 1, 1, 8], ["iron_horse_armor", 1, 1, 5], ["diamond_horse_armor", 1, 1, 3],
		["obsidian", 2, 4, 2]]}
	c["bastion"] = {"rolls": [3, 6], "items": [["gold_ingot", 2, 8, 15], ["gold_block", 1, 2, 5], ["netherite_scrap", 1, 1, 3],
		["ancient_debris", 1, 2, 3], ["diamond", 1, 3, 5], ["crying_obsidian", 1, 5, 8], ["golden_carrot", 3, 8, 10],
		["cooked_porkchop", 2, 6, 10], ["gilded_blackstone", 1, 5, 8], ["golden_apple", 1, 1, 5], ["golden_sword", 1, 1, 5],
		["golden_boots", 1, 1, 5], ["arrow", 5, 17, 5], ["spectral_arrow", 5, 17, 5], ["crossbow", 1, 1, 5], ["enchanted_golden_apple", 1, 1, 1]]}
	c["ruined_portal"] = {"rolls": [4, 8], "items": [["obsidian", 1, 2, 40], ["flint", 1, 4, 40], ["iron_nugget", 9, 18, 40],
		["flint_and_steel", 1, 1, 40], ["fire_charge", 1, 1, 40], ["golden_apple", 1, 1, 15], ["gold_nugget", 4, 24, 15],
		["golden_sword", 1, 1, 15], ["golden_axe", 1, 1, 15], ["golden_boots", 1, 1, 15], ["golden_carrot", 4, 12, 15],
		["gold_ingot", 2, 8, 5], ["clock", 1, 1, 5], ["gold_block", 1, 2, 1], ["enchanted_golden_apple", 1, 1, 1]]}
	c["end_city"] = {"rolls": [2, 6], "items": [["diamond", 2, 7, 5], ["iron_ingot", 4, 8, 10], ["gold_ingot", 2, 7, 15],
		["emerald", 2, 6, 2], ["beetroot_seeds", 1, 10, 5], ["saddle", 1, 1, 3], ["iron_horse_armor", 1, 1, 1],
		["diamond_sword", 1, 1, 3], ["diamond_boots", 1, 1, 3], ["diamond_chestplate", 1, 1, 3], ["diamond_leggings", 1, 1, 3],
		["diamond_helmet", 1, 1, 3], ["diamond_pickaxe", 1, 1, 3], ["iron_sword", 1, 1, 3], ["iron_pickaxe", 1, 1, 3]]}
	c["igloo"] = {"rolls": [2, 8], "items": [["apple", 1, 3, 15], ["coal", 1, 4, 15], ["gold_nugget", 1, 3, 10], ["stone_axe", 1, 1, 2],
		["rotten_flesh", 1, 1, 10], ["emerald", 1, 1, 1], ["wheat", 2, 3, 10], ["golden_apple", 1, 1, 1]]}
	c["pillager_outpost"] = {"rolls": [2, 3], "items": [["crossbow", 1, 1, 5], ["wheat", 3, 5, 7], ["potato", 2, 5, 5], ["carrot", 3, 5, 5],
		["dark_oak_log", 2, 3, 10], ["experience_bottle", 1, 1, 7], ["string", 1, 6, 4], ["arrow", 2, 7, 4], ["tripwire_hook", 1, 3, 3],
		["iron_ingot", 1, 3, 3], ["enchanted_book", 1, 1, 1], ["goat_horn", 1, 1, 1]]}
	c["ancient_city"] = {"rolls": [5, 10], "items": [["enchanted_golden_apple", 1, 2, 1], ["music_disc_otherside", 1, 1, 1],
		["disc_fragment_5", 1, 3, 2], ["echo_shard", 1, 3, 4], ["amethyst_shard", 1, 15, 3], ["sculk_catalyst", 1, 2, 3],
		["experience_bottle", 1, 3, 3], ["glow_berries", 1, 15, 3], ["iron_leggings", 1, 1, 3], ["diamond_hoe", 1, 1, 3],
		["candle", 1, 4, 3], ["book", 3, 10, 3], ["bone", 1, 15, 3], ["soul_torch", 1, 15, 2], ["coal", 6, 15, 7], ["snowball", 1, 7, 2]]}
	c["trial_chambers"] = {"rolls": [3, 6], "items": [["emerald", 2, 4, 5], ["arrow", 4, 14, 5], ["iron_ingot", 1, 2, 5],
		["honey_bottle", 1, 1, 5], ["trial_key", 1, 1, 3], ["wind_charge", 1, 3, 5], ["diamond", 1, 2, 1], ["golden_carrot", 1, 3, 3],
		["baked_potato", 2, 4, 5], ["bread", 1, 3, 5]]}
	c["vault"] = {"rolls": [1, 3], "items": [["emerald", 2, 6, 8], ["diamond", 1, 2, 3], ["enchanted_golden_apple", 1, 1, 1],
		["heavy_core", 1, 1, 1], ["golden_apple", 1, 1, 4], ["wind_charge", 4, 12, 6], ["diamond_axe", 1, 1, 2], ["crossbow", 1, 1, 3]]}
	c["woodland_mansion"] = {"rolls": [1, 3], "items": [["lead", 1, 1, 20], ["golden_apple", 1, 1, 15], ["music_disc_13", 1, 1, 15],
		["music_disc_cat", 1, 1, 15], ["name_tag", 1, 1, 20], ["chainmail_chestplate", 1, 1, 10], ["diamond_hoe", 1, 1, 15],
		["diamond_chestplate", 1, 1, 5], ["enchanted_book", 1, 1, 10], ["iron_ingot", 1, 4, 10], ["gold_ingot", 1, 4, 5],
		["bread", 1, 1, 20], ["wheat", 1, 4, 20], ["redstone", 1, 4, 15], ["coal", 1, 4, 15]]}
	c["swamp_hut"] = {"rolls": [1, 3], "items": [["glass_bottle", 1, 3, 10], ["redstone", 1, 3, 5], ["sugar", 1, 2, 5], ["spider_eye", 1, 2, 5]]}
	c["ocean_ruin"] = {"rolls": [2, 5], "items": [["coal", 1, 4, 10], ["gold_nugget", 1, 3, 10], ["emerald", 1, 1, 5], ["wheat", 2, 3, 10],
		["golden_apple", 1, 1, 1], ["enchanted_book", 1, 1, 1], ["leather_chestplate", 1, 1, 1], ["golden_helmet", 1, 1, 1],
		["fishing_rod", 1, 1, 5], ["map", 1, 1, 10]]}


# ------------------------------------------------------------------------------------------------
## Rolls a chest loot table into a list of item dictionaries for a container of the given size.
static func roll_chest(table: String, rng: RandomNumberGenerator, size := 27) -> Array:
	var slots := []
	slots.resize(size)
	for i in size:
		slots[i] = {}
	var t: Dictionary = chest_tables.get(table, {})
	if t.is_empty():
		return slots
	var rolls: Array = t["rolls"]
	var items: Array = t["items"]
	var total := 0
	for e in items:
		total += int(e[3])
	var n := rng.randi_range(int(rolls[0]), int(rolls[1]))
	for r in n:
		var pick := rng.randi_range(0, total - 1)
		for e in items:
			pick -= int(e[3])
			if pick < 0:
				var iname: String = e[0]
				if not ItemDB.has(iname):
					break
				var cnt := rng.randi_range(int(e[1]), int(e[2]))
				var st := ItemStack.of(iname, mini(cnt, ItemDB.get_by_name(iname).max_stack))
				if iname == "enchanted_book" or (rng.randf() < 0.3 and st.item().enchantability > 0 and st.item().max_stack == 1):
					EnchantDB.enchant_randomly(st, rng, rng.randi_range(5, 30), true)
				var slot := rng.randi_range(0, size - 1)
				for k in size:
					var si := (slot + k) % size
					if (slots[si] as Dictionary).is_empty():
						slots[si] = st.to_dict()
						break
				break
	return slots


## Block drops as ItemStacks.
static func block_drops(v: int, tool: ItemStack, rng: RandomNumberGenerator, be := {}) -> Array:
	var id := v & 0xFFF
	var d: BlockDef = BlockDB.defs[id]
	var meta := (v >> 12) & 15
	var out := []
	var silk := tool != null and tool.enchant_level("silk_touch") > 0
	var fortune := tool.enchant_level("fortune") if tool != null else 0
	var shears := tool != null and tool.item().tool == "shears"
	# tool requirement
	if d.needs_tool:
		if tool == null or tool.item().tool != d.tool or tool.item().tier < d.tier:
			if not (d.tool == "sword" and shears):
				return out
	if d.silk_only and not silk:
		return out
	if silk and d.has_item and (d.silk_only or d.props.get("fortune", false) or d.drop != "" or d.tags.has("ore")):
		var iid := ItemDB.for_block(id)
		if iid >= 0 and not (d.model == BlockDB.M_DOOR and (meta & 8) != 0):
			out.append(ItemStack.new(iid, 1))
			return out
	# shears on leaves / grass / vines / cobweb
	if shears and (d.model == BlockDB.M_LEAVES or d.tags.has("leaves") or d.name in ["short_grass", "tall_grass", "fern", "large_fern",
			"vine", "cobweb", "dead_bush", "seagrass", "glow_lichen", "hanging_roots", "twisting_vines", "weeping_vines",
			"nether_sprouts", "pale_hanging_moss", "bush", "short_dry_grass", "tall_dry_grass"]):
		var sid := ItemDB.for_block(id)
		if d.name == "cobweb":
			sid = ItemDB.id("cobweb")
		if sid >= 0:
			if d.model == BlockDB.M_TALL_CROSS and (meta & 1) == 1:
				return out
			out.append(ItemStack.new(sid, 1))
			return out
	var drop := d.drop
	if drop == "-":
		return out
	# multi-part blocks drop once
	if d.model == BlockDB.M_DOOR and (meta & 8) != 0:
		return out
	if d.model == BlockDB.M_BED and (meta & 4) == 0:
		return out
	if d.model == BlockDB.M_TALL_CROSS and (meta & 1) == 1 and not drop.begins_with("@"):
		return out
	if drop == "":
		var iid2 := ItemDB.for_block(id)
		if iid2 < 0:
			return out
		var cnt := 1
		if d.model == BlockDB.M_SLAB and (meta & 3) == 2:
			cnt = 2
		if d.model == BlockDB.M_CANDLE or d.model == BlockDB.M_PICKLE:
			cnt = (meta & 3) + 1
		if d.model == BlockDB.M_SNOW:
			return [ItemStack.of("snowball", (meta & 7) + 1)]
		var st := ItemStack.new(iid2, cnt)
		if be.has("items") and d.props.get("keep_contents", false):
			st.data["items"] = be["items"]
		if be.has("name"):
			st.data["name"] = be["name"]
		out.append(st)
		return out
	if drop.begins_with("@"):
		var tname := drop.substr(1)
		if tname.begins_with("leaves_") and tname != "leaves_azalea":
			return _leaves(tname.substr(7), rng, fortune)
		match tname:
			"door", "bed", "slab", "shulker_box":
				var iid3 := ItemDB.for_block(id)
				if iid3 >= 0:
					var st2 := ItemStack.new(iid3, 2 if (tname == "slab" and (meta & 3) == 2) else 1)
					if tname == "shulker_box" and be.has("items"):
						st2.data["items"] = be["items"]
					out.append(st2)
				return out
		return _roll_block_table(tname, d, meta, rng, fortune)
	# "item" or "item:min-max"
	var parts := drop.split(":")
	var iname := parts[0]
	var mn := 1
	var mx := 1
	if parts.size() > 1:
		var rr := parts[1].split("-")
		mn = int(rr[0])
		mx = int(rr[1]) if rr.size() > 1 else mn
	var n := rng.randi_range(mn, mx)
	if fortune > 0 and d.props.get("fortune", false):
		if mn == mx and mn == 1:
			# ore-style fortune: multiplier 1..fortune+1 with (2/(f+2)) chance of 1
			var extra := rng.randi_range(0, fortune + 1) - 1
			n *= maxi(1, extra + 1)
		else:
			n += rng.randi_range(0, fortune)
			if iname == "glowstone_dust":
				n = mini(n, 4)
			if iname == "melon_slice":
				n = mini(n, 9)
	if n > 0 and ItemDB.has(iname):
		out.append(ItemStack.of(iname, n))
	return out


static func _leaves(wood: String, rng: RandomNumberGenerator, fortune: int) -> Array:
	var out := []
	var sap_chance: float = [0.05, 0.0625, 0.083, 0.1, 0.125][mini(fortune, 4)]
	if wood == "jungle":
		sap_chance = [0.025, 0.0278, 0.03125, 0.0417, 0.1][mini(fortune, 4)]
	var sap := wood + "_sapling"
	if wood == "mangrove":
		sap = "mangrove_propagule"
	if wood == "cherry":
		sap = "cherry_sapling"
	if rng.randf() < sap_chance and ItemDB.has(sap):
		out.append(ItemStack.of(sap, 1))
	if rng.randf() < 0.02 + 0.0022 * fortune:
		out.append(ItemStack.of("stick", rng.randi_range(1, 2)))
	if (wood == "oak" or wood == "dark_oak") and rng.randf() < 0.005 + 0.0006 * fortune:
		out.append(ItemStack.of("apple", 1))
	return out


static func _roll_block_table(tname: String, d: BlockDef, meta: int, rng: RandomNumberGenerator, fortune: int) -> Array:
	var out := []
	var t: Array = block_tables.get(tname, [])
	var max_age: int = d.props.get("max_age", 7)
	var mature := meta >= max_age
	var gave_primary := false
	for e in t:
		var iname: String = e[0]
		var mn: int = e[1]
		var mx: int = e[2]
		var chance: float = e[3]
		var flag: String = e[4]
		match flag:
			"mature":
				if not mature:
					continue
			"immature":
				if mature:
					continue
			"mature_bonus":
				if not mature:
					continue
				mx += fortune
			"else":
				if gave_primary:
					continue
			"fortune_chance":
				chance = [0.1, 0.14, 0.25, 1.0][mini(fortune, 3)]
			"fortune_seed":
				mx += fortune * 2 if rng.randf() < chance else 0
			"age3":
				if meta < 3:
					continue
			"age2":
				if meta != 2:
					continue
			"berries":
				if (meta & 1) == 0:
					continue
			"layers":
				mn = (meta & 7) + 1
				mx = mn
		if rng.randf() >= chance:
			continue
		var n := rng.randi_range(mn, mx)
		if n > 0 and ItemDB.has(iname):
			out.append(ItemStack.of(iname, n))
			gave_primary = true
	return out


## Mob drops (killed_by_player enables rare drops; looting level adds up to +looting per entry).
static func mob_drops(mob: String, rng: RandomNumberGenerator, looting: int, by_player: bool, on_fire: bool, extra := {}) -> Array:
	var out := []
	var t: Array = mob_tables.get(mob, [])
	for e in t:
		var iname: String = e[0]
		var mn: int = e[1]
		var mx: int = e[2]
		var chance: float = e[3]
		var flags: String = e[4]
		if flags.contains("player") and not by_player:
			continue
		if flags.contains("small") and int(extra.get("size", 1)) != 1:
			continue
		if flags.contains("notsmall") and int(extra.get("size", 1)) == 1:
			continue
		if flags.contains("looting"):
			if chance < 1.0:
				chance += 0.01 * looting
			else:
				mx += looting
		if rng.randf() >= chance:
			continue
		var n := rng.randi_range(mn, mx)
		if n <= 0:
			continue
		if iname == "@wool":
			if extra.get("sheared", false):
				continue
			iname = String(extra.get("color", "white")) + "_wool"
		if flags.contains("cook") and on_fire:
			var cooked := "cooked_" + iname
			if ItemDB.has(cooked):
				iname = cooked
		if ItemDB.has(iname):
			out.append(ItemStack.of(iname, n))
	return out
