// Smelting, stonecutting, brewing, smithing and composting tables (original data written for this project).
#include "Crafting/MCRecipeBuilder.h"

void MCRegisterProcessingRecipes(FMCRecipeBuilder& B)
{
	constexpr uint8 FurnaceBlast = MCSS_Furnace | MCSS_Blast;
	constexpr uint8 Food = MCSS_Furnace | MCSS_Smoker | MCSS_Campfire;

	// ================================================================ ores (furnace 200 ticks, blast furnace at half time)
	struct FOreSmelt { const TCHAR* In; const TCHAR* Out; float XP; };
	const FOreSmelt Ores[] = {
		{ TEXT("raw_iron"), TEXT("iron_ingot"), 0.7f }, { TEXT("iron_ore"), TEXT("iron_ingot"), 0.7f }, { TEXT("deepslate_iron_ore"), TEXT("iron_ingot"), 0.7f },
		{ TEXT("raw_gold"), TEXT("gold_ingot"), 1.0f }, { TEXT("gold_ore"), TEXT("gold_ingot"), 1.0f }, { TEXT("deepslate_gold_ore"), TEXT("gold_ingot"), 1.0f },
		{ TEXT("nether_gold_ore"), TEXT("gold_ingot"), 1.0f },
		{ TEXT("raw_copper"), TEXT("copper_ingot"), 0.7f }, { TEXT("copper_ore"), TEXT("copper_ingot"), 0.7f }, { TEXT("deepslate_copper_ore"), TEXT("copper_ingot"), 0.7f },
		{ TEXT("coal_ore"), TEXT("coal"), 0.1f }, { TEXT("deepslate_coal_ore"), TEXT("coal"), 0.1f },
		{ TEXT("redstone_ore"), TEXT("redstone"), 0.7f }, { TEXT("deepslate_redstone_ore"), TEXT("redstone"), 0.7f },
		{ TEXT("lapis_ore"), TEXT("lapis_lazuli"), 0.2f }, { TEXT("deepslate_lapis_ore"), TEXT("lapis_lazuli"), 0.2f },
		{ TEXT("diamond_ore"), TEXT("diamond"), 1.0f }, { TEXT("deepslate_diamond_ore"), TEXT("diamond"), 1.0f },
		{ TEXT("emerald_ore"), TEXT("emerald"), 1.0f }, { TEXT("deepslate_emerald_ore"), TEXT("emerald"), 1.0f },
		{ TEXT("nether_quartz_ore"), TEXT("quartz"), 0.2f }, { TEXT("ancient_debris"), TEXT("netherite_scrap"), 2.0f },
		{ TEXT("potent_sulfur"), TEXT("sulfur_shard"), 0.3f },
	};
	for (const FOreSmelt& O : Ores)
	{
		B.Smelt(O.In, O.Out, O.XP, 200, MCSS_Furnace);
		B.Smelt(O.In, O.Out, O.XP, 100, MCSS_Blast);
	}
	// nuggets back from gear
	for (const TCHAR* T : { TEXT("iron_pickaxe"), TEXT("iron_axe"), TEXT("iron_shovel"), TEXT("iron_hoe"), TEXT("iron_sword"), TEXT("iron_spear"), TEXT("iron_helmet"),
		TEXT("iron_chestplate"), TEXT("iron_leggings"), TEXT("iron_boots"), TEXT("iron_horse_armor"), TEXT("chainmail_helmet"), TEXT("chainmail_chestplate"),
		TEXT("chainmail_leggings"), TEXT("chainmail_boots") })
	{ B.Smelt(T, TEXT("iron_nugget"), 0.1f, 200, MCSS_Furnace); B.Smelt(T, TEXT("iron_nugget"), 0.1f, 100, MCSS_Blast); }
	for (const TCHAR* T : { TEXT("golden_pickaxe"), TEXT("golden_axe"), TEXT("golden_shovel"), TEXT("golden_hoe"), TEXT("golden_sword"), TEXT("golden_spear"),
		TEXT("golden_helmet"), TEXT("golden_chestplate"), TEXT("golden_leggings"), TEXT("golden_boots"), TEXT("golden_horse_armor") })
	{ B.Smelt(T, TEXT("gold_nugget"), 0.1f, 200, MCSS_Furnace); B.Smelt(T, TEXT("gold_nugget"), 0.1f, 100, MCSS_Blast); }
	for (const TCHAR* T : { TEXT("copper_pickaxe"), TEXT("copper_axe"), TEXT("copper_shovel"), TEXT("copper_hoe"), TEXT("copper_sword"), TEXT("copper_spear"),
		TEXT("copper_helmet"), TEXT("copper_chestplate"), TEXT("copper_leggings"), TEXT("copper_boots") })
	{ B.Smelt(T, TEXT("copper_nugget"), 0.1f, 200, MCSS_Furnace); B.Smelt(T, TEXT("copper_nugget"), 0.1f, 100, MCSS_Blast); }

	// ================================================================ food (furnace 200, smoker 100, campfire 600)
	struct FFoodSmelt { const TCHAR* In; const TCHAR* Out; };
	const FFoodSmelt Foods[] = {
		{ TEXT("beef"), TEXT("cooked_beef") }, { TEXT("porkchop"), TEXT("cooked_porkchop") }, { TEXT("chicken"), TEXT("cooked_chicken") },
		{ TEXT("mutton"), TEXT("cooked_mutton") }, { TEXT("rabbit"), TEXT("cooked_rabbit") }, { TEXT("cod"), TEXT("cooked_cod") },
		{ TEXT("salmon"), TEXT("cooked_salmon") }, { TEXT("potato"), TEXT("baked_potato") }, { TEXT("kelp"), TEXT("dried_kelp") },
	};
	for (const FFoodSmelt& F : Foods)
	{
		B.Smelt(F.In, F.Out, 0.35f, 200, MCSS_Furnace);
		B.Smelt(F.In, F.Out, 0.35f, 100, MCSS_Smoker);
		B.Smelt(F.In, F.Out, 0.35f, 600, MCSS_Campfire);
	}
	(void)Food;

	// ================================================================ materials
	struct FMatSmelt { const TCHAR* In; const TCHAR* Out; float XP; };
	const FMatSmelt Mats[] = {
		{ TEXT("sand"), TEXT("glass"), 0.1f }, { TEXT("red_sand"), TEXT("glass"), 0.1f }, { TEXT("cobblestone"), TEXT("stone"), 0.1f },
		{ TEXT("stone"), TEXT("smooth_stone"), 0.1f }, { TEXT("cobbled_deepslate"), TEXT("deepslate"), 0.1f }, { TEXT("clay_ball"), TEXT("brick"), 0.3f },
		{ TEXT("clay"), TEXT("terracotta"), 0.35f }, { TEXT("netherrack"), TEXT("nether_brick"), 0.1f }, { TEXT("sandstone"), TEXT("smooth_sandstone"), 0.1f },
		{ TEXT("red_sandstone"), TEXT("smooth_red_sandstone"), 0.1f }, { TEXT("quartz_block"), TEXT("smooth_quartz"), 0.1f }, { TEXT("basalt"), TEXT("smooth_basalt"), 0.1f },
		{ TEXT("stone_bricks"), TEXT("cracked_stone_bricks"), 0.1f }, { TEXT("nether_bricks"), TEXT("cracked_nether_bricks"), 0.1f },
		{ TEXT("deepslate_bricks"), TEXT("cracked_deepslate_bricks"), 0.1f }, { TEXT("deepslate_tiles"), TEXT("cracked_deepslate_tiles"), 0.1f },
		{ TEXT("polished_blackstone_bricks"), TEXT("cracked_polished_blackstone_bricks"), 0.1f }, { TEXT("wet_sponge"), TEXT("sponge"), 0.15f },
		{ TEXT("cactus"), TEXT("green_dye"), 1.0f }, { TEXT("sea_pickle"), TEXT("lime_dye"), 0.1f }, { TEXT("chorus_fruit"), TEXT("popped_chorus_fruit"), 0.1f },
		{ TEXT("resin_clump"), TEXT("resin_brick"), 0.1f }, { TEXT("#logs"), TEXT("charcoal"), 0.15f }, { TEXT("sulfur"), TEXT("polished_sulfur"), 0.1f },
		{ TEXT("cinnabar"), TEXT("polished_cinnabar"), 0.1f },
	};
	for (const FMatSmelt& M : Mats) B.Smelt(M.In, M.Out, M.XP, 200, MCSS_Furnace);
	const TCHAR* Colors[16] = { TEXT("white"), TEXT("orange"), TEXT("magenta"), TEXT("light_blue"), TEXT("yellow"), TEXT("lime"), TEXT("pink"), TEXT("gray"),
		TEXT("light_gray"), TEXT("cyan"), TEXT("purple"), TEXT("blue"), TEXT("brown"), TEXT("green"), TEXT("red"), TEXT("black") };
	for (const TCHAR* C : Colors)
		B.Smelt(*(FString(C) + TEXT("_terracotta")), *(FString(C) + TEXT("_glazed_terracotta")), 0.1f, 200, MCSS_Furnace);
	(void)FurnaceBlast;

	// ================================================================ stonecutter (the crafting table families were added alongside the shapes)
	struct FCut { const TCHAR* In; const TCHAR* Out; int32 N; };
	const FCut Cuts[] = {
		{ TEXT("stone"), TEXT("stone_bricks"), 1 }, { TEXT("stone"), TEXT("stone_brick_stairs"), 1 }, { TEXT("stone"), TEXT("stone_brick_slab"), 2 },
		{ TEXT("stone"), TEXT("stone_brick_wall"), 1 }, { TEXT("stone"), TEXT("chiseled_stone_bricks"), 1 }, { TEXT("stone_bricks"), TEXT("chiseled_stone_bricks"), 1 },
		{ TEXT("granite"), TEXT("polished_granite"), 1 }, { TEXT("granite"), TEXT("polished_granite_stairs"), 1 }, { TEXT("granite"), TEXT("polished_granite_slab"), 2 },
		{ TEXT("diorite"), TEXT("polished_diorite"), 1 }, { TEXT("diorite"), TEXT("polished_diorite_stairs"), 1 }, { TEXT("diorite"), TEXT("polished_diorite_slab"), 2 },
		{ TEXT("andesite"), TEXT("polished_andesite"), 1 }, { TEXT("andesite"), TEXT("polished_andesite_stairs"), 1 }, { TEXT("andesite"), TEXT("polished_andesite_slab"), 2 },
		{ TEXT("cobbled_deepslate"), TEXT("polished_deepslate"), 1 }, { TEXT("cobbled_deepslate"), TEXT("deepslate_bricks"), 1 },
		{ TEXT("cobbled_deepslate"), TEXT("deepslate_tiles"), 1 }, { TEXT("cobbled_deepslate"), TEXT("chiseled_deepslate"), 1 },
		{ TEXT("cobbled_deepslate"), TEXT("polished_deepslate_stairs"), 1 }, { TEXT("cobbled_deepslate"), TEXT("polished_deepslate_slab"), 2 },
		{ TEXT("cobbled_deepslate"), TEXT("polished_deepslate_wall"), 1 }, { TEXT("cobbled_deepslate"), TEXT("deepslate_brick_stairs"), 1 },
		{ TEXT("cobbled_deepslate"), TEXT("deepslate_brick_slab"), 2 }, { TEXT("cobbled_deepslate"), TEXT("deepslate_brick_wall"), 1 },
		{ TEXT("cobbled_deepslate"), TEXT("deepslate_tile_stairs"), 1 }, { TEXT("cobbled_deepslate"), TEXT("deepslate_tile_slab"), 2 },
		{ TEXT("cobbled_deepslate"), TEXT("deepslate_tile_wall"), 1 }, { TEXT("polished_deepslate"), TEXT("deepslate_bricks"), 1 },
		{ TEXT("polished_deepslate"), TEXT("deepslate_tiles"), 1 }, { TEXT("deepslate_bricks"), TEXT("deepslate_tiles"), 1 },
		{ TEXT("tuff"), TEXT("polished_tuff"), 1 }, { TEXT("tuff"), TEXT("tuff_bricks"), 1 }, { TEXT("tuff"), TEXT("chiseled_tuff"), 1 },
		{ TEXT("tuff"), TEXT("chiseled_tuff_bricks"), 1 }, { TEXT("tuff"), TEXT("polished_tuff_stairs"), 1 }, { TEXT("tuff"), TEXT("polished_tuff_slab"), 2 },
		{ TEXT("tuff"), TEXT("tuff_brick_stairs"), 1 }, { TEXT("tuff"), TEXT("tuff_brick_slab"), 2 }, { TEXT("tuff"), TEXT("tuff_brick_wall"), 1 },
		{ TEXT("sandstone"), TEXT("cut_sandstone"), 1 }, { TEXT("sandstone"), TEXT("chiseled_sandstone"), 1 }, { TEXT("sandstone"), TEXT("cut_sandstone_slab"), 2 },
		{ TEXT("red_sandstone"), TEXT("cut_red_sandstone"), 1 }, { TEXT("red_sandstone"), TEXT("chiseled_red_sandstone"), 1 },
		{ TEXT("red_sandstone"), TEXT("cut_red_sandstone_slab"), 2 }, { TEXT("smooth_stone"), TEXT("smooth_stone_slab"), 2 },
		{ TEXT("quartz_block"), TEXT("quartz_pillar"), 1 }, { TEXT("quartz_block"), TEXT("chiseled_quartz_block"), 1 }, { TEXT("quartz_block"), TEXT("quartz_bricks"), 1 },
		{ TEXT("blackstone"), TEXT("polished_blackstone"), 1 }, { TEXT("blackstone"), TEXT("polished_blackstone_bricks"), 1 },
		{ TEXT("blackstone"), TEXT("chiseled_polished_blackstone"), 1 }, { TEXT("polished_blackstone"), TEXT("polished_blackstone_bricks"), 1 },
		{ TEXT("polished_blackstone"), TEXT("chiseled_polished_blackstone"), 1 }, { TEXT("basalt"), TEXT("polished_basalt"), 1 },
		{ TEXT("end_stone"), TEXT("end_stone_bricks"), 1 }, { TEXT("end_stone"), TEXT("end_stone_brick_stairs"), 1 }, { TEXT("end_stone"), TEXT("end_stone_brick_slab"), 2 },
		{ TEXT("end_stone"), TEXT("end_stone_brick_wall"), 1 }, { TEXT("purpur_block"), TEXT("purpur_pillar"), 1 }, { TEXT("prismarine_bricks"), TEXT("prismarine_brick_slab"), 2 },
		{ TEXT("nether_bricks"), TEXT("chiseled_nether_bricks"), 1 }, { TEXT("mud_bricks"), TEXT("mud_brick_wall"), 1 },
		{ TEXT("copper_block"), TEXT("cut_copper"), 4 }, { TEXT("copper_block"), TEXT("cut_copper_stairs"), 4 }, { TEXT("copper_block"), TEXT("cut_copper_slab"), 8 },
		{ TEXT("copper_block"), TEXT("chiseled_copper"), 4 }, { TEXT("copper_block"), TEXT("copper_grate"), 4 }, { TEXT("resin_bricks"), TEXT("chiseled_resin_bricks"), 1 },
		{ TEXT("sulfur"), TEXT("polished_sulfur"), 1 }, { TEXT("sulfur"), TEXT("sulfur_bricks"), 1 }, { TEXT("cinnabar"), TEXT("polished_cinnabar"), 1 },
		{ TEXT("cinnabar"), TEXT("cinnabar_bricks"), 1 },
	};
	for (const FCut& C : Cuts) B.Stonecut(C.In, C.Out, C.N);

	// ================================================================ brewing
	struct FBrew { const TCHAR* Ing; const TCHAR* From; const TCHAR* To; };
	const FBrew Brews[] = {
		{ TEXT("nether_wart"), TEXT("water"), TEXT("awkward") },
		{ TEXT("redstone"), TEXT("water"), TEXT("mundane") }, { TEXT("glowstone_dust"), TEXT("water"), TEXT("thick") },
		{ TEXT("sugar"), TEXT("water"), TEXT("mundane") }, { TEXT("spider_eye"), TEXT("water"), TEXT("mundane") },
		{ TEXT("fermented_spider_eye"), TEXT("water"), TEXT("weakness") },
		{ TEXT("sugar"), TEXT("awkward"), TEXT("swiftness") }, { TEXT("rabbit_foot"), TEXT("awkward"), TEXT("leaping") },
		{ TEXT("glistering_melon_slice"), TEXT("awkward"), TEXT("healing") }, { TEXT("spider_eye"), TEXT("awkward"), TEXT("poison") },
		{ TEXT("ghast_tear"), TEXT("awkward"), TEXT("regeneration") }, { TEXT("magma_cream"), TEXT("awkward"), TEXT("fire_resistance") },
		{ TEXT("pufferfish"), TEXT("awkward"), TEXT("water_breathing") }, { TEXT("golden_carrot"), TEXT("awkward"), TEXT("night_vision") },
		{ TEXT("blaze_powder"), TEXT("awkward"), TEXT("strength") }, { TEXT("turtle_scute"), TEXT("awkward"), TEXT("turtle_master") },
		{ TEXT("phantom_membrane"), TEXT("awkward"), TEXT("slow_falling") },
		// corruption
		{ TEXT("fermented_spider_eye"), TEXT("night_vision"), TEXT("invisibility") }, { TEXT("fermented_spider_eye"), TEXT("long_night_vision"), TEXT("invisibility") },
		{ TEXT("fermented_spider_eye"), TEXT("healing"), TEXT("harming") }, { TEXT("fermented_spider_eye"), TEXT("poison"), TEXT("harming") },
		{ TEXT("fermented_spider_eye"), TEXT("swiftness"), TEXT("slowness") }, { TEXT("fermented_spider_eye"), TEXT("leaping"), TEXT("slowness") },
		{ TEXT("fermented_spider_eye"), TEXT("strength"), TEXT("weakness") },
		// amplify / extend
		{ TEXT("glowstone_dust"), TEXT("healing"), TEXT("strong_healing") }, { TEXT("glowstone_dust"), TEXT("strength"), TEXT("strong_strength") },
		{ TEXT("redstone"), TEXT("regeneration"), TEXT("long_regeneration") }, { TEXT("redstone"), TEXT("swiftness"), TEXT("long_swiftness") },
		{ TEXT("redstone"), TEXT("fire_resistance"), TEXT("long_fire_resistance") }, { TEXT("redstone"), TEXT("night_vision"), TEXT("long_night_vision") },
	};
	for (const FBrew& Br : Brews) B.Brew(Br.Ing, Br.From, Br.To);

	// ================================================================ smithing (netherite upgrade)
	const TCHAR* Gear[] = { TEXT("pickaxe"), TEXT("axe"), TEXT("shovel"), TEXT("hoe"), TEXT("sword"), TEXT("spear"), TEXT("helmet"), TEXT("chestplate"), TEXT("leggings"), TEXT("boots") };
	for (const TCHAR* G : Gear)
		B.Smith(TEXT("netherite_upgrade_smithing_template"), *(FString(TEXT("diamond_")) + G), TEXT("netherite_ingot"), *(FString(TEXT("netherite_")) + G));

	// ================================================================ composter chances (percent)
	struct FComp { const TCHAR* Item; int32 Chance; };
	const FComp Comp[] = {
		{ TEXT("wheat_seeds"), 30 }, { TEXT("beetroot_seeds"), 30 }, { TEXT("melon_seeds"), 30 }, { TEXT("pumpkin_seeds"), 30 }, { TEXT("torchflower_seeds"), 30 },
		{ TEXT("pitcher_pod"), 30 }, { TEXT("short_grass"), 30 }, { TEXT("kelp"), 30 }, { TEXT("dried_kelp"), 30 }, { TEXT("sweet_berries"), 30 },
		{ TEXT("glow_berries"), 30 }, { TEXT("moss_carpet"), 30 }, { TEXT("pale_moss_carpet"), 30 }, { TEXT("hanging_roots"), 30 }, { TEXT("small_dripleaf"), 30 },
		{ TEXT("seagrass"), 30 }, { TEXT("leaf_litter"), 30 }, { TEXT("pink_petals"), 30 }, { TEXT("wildflowers"), 30 },
		{ TEXT("cactus"), 50 }, { TEXT("dried_kelp_block"), 50 }, { TEXT("tall_grass"), 50 }, { TEXT("melon_slice"), 50 }, { TEXT("sugar_cane"), 50 },
		{ TEXT("vine"), 50 }, { TEXT("glow_lichen"), 50 }, { TEXT("nether_sprouts"), 50 }, { TEXT("twisting_vines"), 50 }, { TEXT("weeping_vines"), 50 },
		{ TEXT("apple"), 65 }, { TEXT("beetroot"), 65 }, { TEXT("carrot"), 65 }, { TEXT("cocoa_beans"), 65 }, { TEXT("potato"), 65 }, { TEXT("wheat"), 65 },
		{ TEXT("brown_mushroom"), 65 }, { TEXT("red_mushroom"), 65 }, { TEXT("crimson_fungus"), 65 }, { TEXT("warped_fungus"), 65 }, { TEXT("nether_wart"), 65 },
		{ TEXT("crimson_roots"), 65 }, { TEXT("warped_roots"), 65 }, { TEXT("fern"), 65 }, { TEXT("large_fern"), 65 }, { TEXT("lily_pad"), 65 },
		{ TEXT("pumpkin"), 65 }, { TEXT("carved_pumpkin"), 65 }, { TEXT("melon"), 65 }, { TEXT("moss_block"), 65 }, { TEXT("pale_moss_block"), 65 },
		{ TEXT("sea_pickle"), 65 }, { TEXT("shroomlight"), 65 }, { TEXT("spore_blossom"), 65 }, { TEXT("big_dripleaf"), 65 }, { TEXT("mangrove_roots"), 30 },
		{ TEXT("baked_potato"), 85 }, { TEXT("bread"), 85 }, { TEXT("cookie"), 85 }, { TEXT("hay_block"), 85 }, { TEXT("brown_mushroom_block"), 85 },
		{ TEXT("red_mushroom_block"), 85 }, { TEXT("nether_wart_block"), 85 }, { TEXT("warped_wart_block"), 85 }, { TEXT("flowering_azalea"), 85 },
		{ TEXT("cake"), 100 }, { TEXT("pumpkin_pie"), 100 }, { TEXT("torchflower"), 85 }, { TEXT("pitcher_plant"), 85 },
	};
	for (const FComp& C : Comp) B.Compost(C.Item, C.Chance);
	for (const FMCItem& I : FMCItems::All())
	{
		if (I.Id == 0) continue;
		if (I.HasTag(TEXT("leaves")) || I.HasTag(TEXT("saplings"))) B.Compost(*I.Name.ToString(), 30);
		else if (I.HasTag(TEXT("small_flowers")) || I.HasTag(TEXT("flowers"))) B.Compost(*I.Name.ToString(), 65);
	}
}
