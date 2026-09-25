#include "Items/MCLoot.h"
#include "Blocks/MCBlocks.h"
#include "Blocks/MCBlockBehavior.h"
#include "World/MCWorld.h"
#include "World/MCBlockEntity.h"

namespace
{
	FMCItemStack Stack(const TCHAR* Name, int32 Count) { return FMCItemStack::Of(FName(Name), Count); }

	int32 FortuneBonus(FMCRandom& R, int32 Fortune)
	{
		// Minecraft ore formula: multiplier in [1, fortune + 1], with a 2/(fortune+2) chance of no bonus
		if (Fortune <= 0) return 1;
		int32 Bonus = R.NextInt(Fortune + 2) - 1;
		if (Bonus < 0) Bonus = 0;
		return Bonus + 1;
	}

	struct FLootEntry
	{
		const TCHAR* Item;
		int32 Weight;
		int32 Min, Max;
		int32 EnchantLevel; // > 0: enchant randomly at this level
		bool bTreasure;
	};

	struct FLootPool
	{
		int32 RollsMin, RollsMax;
		TArray<FLootEntry> Entries;
	};

	struct FLootTable
	{
		FName Name;
		TArray<FLootPool> Pools;
	};

	TArray<FLootTable> GTables;

	FLootPool Pool(int32 RMin, int32 RMax, std::initializer_list<FLootEntry> E)
	{
		FLootPool P; P.RollsMin = RMin; P.RollsMax = RMax; P.Entries = E; return P;
	}

	void Table(const TCHAR* Name, std::initializer_list<FLootPool> Pools)
	{
		FLootTable T; T.Name = FName(Name); T.Pools = Pools; GTables.Add(T);
	}

	void InitTables()
	{
		if (GTables.Num()) return;
		Table(TEXT("simple_dungeon"), {
			Pool(1, 3, { { TEXT("saddle"), 20, 1, 1, 0, false }, { TEXT("golden_apple"), 15, 1, 1, 0, false }, { TEXT("enchanted_golden_apple"), 2, 1, 1, 0, false },
				{ TEXT("music_disc_opus55"), 15, 1, 1, 0, false }, { TEXT("music_disc_voxel"), 15, 1, 1, 0, false }, { TEXT("name_tag"), 20, 1, 1, 0, false },
				{ TEXT("golden_horse_armor"), 10, 1, 1, 0, false }, { TEXT("iron_horse_armor"), 15, 1, 1, 0, false }, { TEXT("diamond_horse_armor"), 5, 1, 1, 0, false },
				{ TEXT("enchanted_book"), 10, 1, 1, 30, true } }),
			Pool(1, 4, { { TEXT("iron_ingot"), 10, 1, 4, 0, false }, { TEXT("gold_ingot"), 5, 1, 4, 0, false }, { TEXT("bread"), 20, 1, 1, 0, false },
				{ TEXT("wheat"), 20, 1, 4, 0, false }, { TEXT("bucket"), 10, 1, 1, 0, false }, { TEXT("redstone"), 15, 1, 4, 0, false },
				{ TEXT("coal"), 15, 1, 4, 0, false }, { TEXT("melon_seeds"), 10, 2, 4, 0, false }, { TEXT("pumpkin_seeds"), 10, 2, 4, 0, false },
				{ TEXT("beetroot_seeds"), 10, 2, 4, 0, false } }),
			Pool(3, 3, { { TEXT("bone"), 10, 1, 8, 0, false }, { TEXT("gunpowder"), 10, 1, 8, 0, false }, { TEXT("rotten_flesh"), 10, 1, 8, 0, false },
				{ TEXT("string"), 10, 1, 8, 0, false } }) });
		Table(TEXT("abandoned_mineshaft"), {
			Pool(1, 1, { { TEXT("golden_apple"), 20, 1, 1, 0, false }, { TEXT("enchanted_golden_apple"), 1, 1, 1, 0, false }, { TEXT("name_tag"), 30, 1, 1, 0, false },
				{ TEXT("enchanted_book"), 10, 1, 1, 30, true }, { TEXT("iron_pickaxe"), 5, 1, 1, 0, false } }),
			Pool(2, 4, { { TEXT("iron_ingot"), 10, 1, 5, 0, false }, { TEXT("gold_ingot"), 5, 1, 3, 0, false }, { TEXT("redstone"), 5, 4, 9, 0, false },
				{ TEXT("lapis_lazuli"), 5, 4, 9, 0, false }, { TEXT("diamond"), 3, 1, 2, 0, false }, { TEXT("coal"), 10, 3, 8, 0, false },
				{ TEXT("bread"), 15, 1, 3, 0, false }, { TEXT("glow_berries"), 15, 3, 6, 0, false }, { TEXT("melon_seeds"), 10, 2, 4, 0, false },
				{ TEXT("pumpkin_seeds"), 10, 2, 4, 0, false } }),
			Pool(3, 3, { { TEXT("rail"), 20, 4, 8, 0, false }, { TEXT("powered_rail"), 5, 1, 4, 0, false }, { TEXT("detector_rail"), 5, 1, 4, 0, false },
				{ TEXT("activator_rail"), 5, 1, 4, 0, false }, { TEXT("torch"), 15, 1, 16, 0, false } }) });
		Table(TEXT("village_house"), {
			Pool(3, 8, { { TEXT("bread"), 15, 1, 4, 0, false }, { TEXT("apple"), 10, 1, 5, 0, false }, { TEXT("emerald"), 3, 1, 3, 0, false },
				{ TEXT("wheat"), 10, 1, 7, 0, false }, { TEXT("potato"), 10, 1, 7, 0, false }, { TEXT("carrot"), 10, 1, 7, 0, false },
				{ TEXT("oak_sapling"), 5, 1, 2, 0, false }, { TEXT("feather"), 5, 1, 1, 0, false }, { TEXT("book"), 3, 1, 1, 0, false },
				{ TEXT("torch"), 5, 1, 6, 0, false }, { TEXT("paper"), 5, 1, 4, 0, false }, { TEXT("dandelion"), 3, 1, 1, 0, false } }) });
		Table(TEXT("village_weaponsmith"), {
			Pool(3, 8, { { TEXT("diamond"), 3, 1, 3, 0, false }, { TEXT("iron_ingot"), 10, 1, 5, 0, false }, { TEXT("gold_ingot"), 5, 1, 3, 0, false },
				{ TEXT("bread"), 15, 1, 3, 0, false }, { TEXT("apple"), 15, 1, 3, 0, false }, { TEXT("iron_pickaxe"), 5, 1, 1, 0, false },
				{ TEXT("iron_sword"), 5, 1, 1, 0, false }, { TEXT("iron_chestplate"), 5, 1, 1, 0, false }, { TEXT("iron_helmet"), 5, 1, 1, 0, false },
				{ TEXT("iron_leggings"), 5, 1, 1, 0, false }, { TEXT("iron_boots"), 5, 1, 1, 0, false }, { TEXT("obsidian"), 5, 3, 7, 0, false },
				{ TEXT("oak_sapling"), 5, 3, 7, 0, false }, { TEXT("saddle"), 3, 1, 1, 0, false }, { TEXT("iron_horse_armor"), 1, 1, 1, 0, false },
				{ TEXT("copper_sword"), 4, 1, 1, 0, false }, { TEXT("copper_spear"), 4, 1, 1, 0, false } }) });
		Table(TEXT("stronghold_library"), {
			Pool(2, 10, { { TEXT("book"), 20, 1, 3, 0, false }, { TEXT("paper"), 20, 2, 7, 0, false }, { TEXT("map"), 1, 1, 1, 0, false },
				{ TEXT("compass"), 1, 1, 1, 0, false }, { TEXT("enchanted_book"), 10, 1, 1, 30, true } }) });
		Table(TEXT("stronghold_corridor"), {
			Pool(2, 3, { { TEXT("ender_pearl"), 10, 1, 1, 0, false }, { TEXT("diamond"), 3, 1, 3, 0, false }, { TEXT("iron_ingot"), 10, 1, 5, 0, false },
				{ TEXT("gold_ingot"), 5, 1, 3, 0, false }, { TEXT("redstone"), 5, 4, 9, 0, false }, { TEXT("bread"), 15, 1, 3, 0, false },
				{ TEXT("apple"), 15, 1, 3, 0, false }, { TEXT("iron_pickaxe"), 5, 1, 1, 0, false }, { TEXT("iron_sword"), 5, 1, 1, 0, false },
				{ TEXT("iron_chestplate"), 5, 1, 1, 0, false }, { TEXT("enchanted_book"), 1, 1, 1, 30, true }, { TEXT("saddle"), 1, 1, 1, 0, false } }) });
		Table(TEXT("stronghold_crossing"), {
			Pool(1, 4, { { TEXT("iron_ingot"), 10, 1, 5, 0, false }, { TEXT("gold_ingot"), 5, 1, 3, 0, false }, { TEXT("redstone"), 5, 4, 9, 0, false },
				{ TEXT("coal"), 10, 3, 8, 0, false }, { TEXT("bread"), 15, 1, 3, 0, false }, { TEXT("apple"), 15, 1, 3, 0, false },
				{ TEXT("iron_pickaxe"), 1, 1, 1, 0, false }, { TEXT("enchanted_book"), 1, 1, 1, 30, true } }) });
		Table(TEXT("desert_pyramid"), {
			Pool(2, 4, { { TEXT("diamond"), 5, 1, 3, 0, false }, { TEXT("iron_ingot"), 15, 1, 5, 0, false }, { TEXT("gold_ingot"), 15, 2, 7, 0, false },
				{ TEXT("emerald"), 15, 1, 3, 0, false }, { TEXT("bone"), 25, 4, 6, 0, false }, { TEXT("spider_eye"), 25, 1, 3, 0, false },
				{ TEXT("rotten_flesh"), 25, 3, 7, 0, false }, { TEXT("saddle"), 20, 1, 1, 0, false }, { TEXT("iron_horse_armor"), 15, 1, 1, 0, false },
				{ TEXT("golden_horse_armor"), 10, 1, 1, 0, false }, { TEXT("diamond_horse_armor"), 5, 1, 1, 0, false }, { TEXT("enchanted_book"), 20, 1, 1, 30, true },
				{ TEXT("golden_apple"), 20, 1, 1, 0, false }, { TEXT("enchanted_golden_apple"), 2, 1, 1, 0, false } }),
			Pool(4, 4, { { TEXT("bone"), 10, 1, 8, 0, false }, { TEXT("gunpowder"), 10, 1, 8, 0, false }, { TEXT("rotten_flesh"), 10, 1, 8, 0, false },
				{ TEXT("string"), 10, 1, 8, 0, false }, { TEXT("sand"), 10, 1, 8, 0, false } }) });
		Table(TEXT("jungle_temple"), {
			Pool(2, 6, { { TEXT("diamond"), 3, 1, 3, 0, false }, { TEXT("iron_ingot"), 10, 1, 5, 0, false }, { TEXT("gold_ingot"), 15, 2, 7, 0, false },
				{ TEXT("bamboo"), 15, 1, 3, 0, false }, { TEXT("emerald"), 2, 1, 3, 0, false }, { TEXT("bone"), 20, 4, 6, 0, false },
				{ TEXT("rotten_flesh"), 16, 3, 7, 0, false }, { TEXT("saddle"), 3, 1, 1, 0, false }, { TEXT("enchanted_book"), 1, 1, 1, 30, true } }) });
		Table(TEXT("igloo_chest"), {
			Pool(2, 8, { { TEXT("apple"), 15, 1, 3, 0, false }, { TEXT("coal"), 15, 1, 4, 0, false }, { TEXT("gold_nugget"), 10, 1, 3, 0, false },
				{ TEXT("stone_axe"), 2, 1, 1, 0, false }, { TEXT("rotten_flesh"), 10, 1, 1, 0, false }, { TEXT("emerald"), 1, 1, 1, 0, false },
				{ TEXT("wheat"), 10, 2, 3, 0, false } }),
			Pool(1, 1, { { TEXT("golden_apple"), 1, 1, 1, 0, false } }) });
		Table(TEXT("pillager_outpost"), {
			Pool(0, 1, { { TEXT("crossbow"), 1, 1, 1, 0, false } }),
			Pool(2, 3, { { TEXT("wheat"), 7, 3, 5, 0, false }, { TEXT("potato"), 5, 2, 5, 0, false }, { TEXT("carrot"), 5, 3, 5, 0, false } }),
			Pool(1, 3, { { TEXT("dark_oak_log"), 1, 2, 3, 0, false } }),
			Pool(2, 3, { { TEXT("experience_bottle"), 7, 1, 1, 0, false }, { TEXT("string"), 4, 1, 6, 0, false }, { TEXT("arrow"), 4, 2, 7, 0, false },
				{ TEXT("tripwire_hook"), 3, 1, 3, 0, false }, { TEXT("iron_ingot"), 3, 1, 3, 0, false }, { TEXT("enchanted_book"), 1, 1, 1, 30, true } }) });
		Table(TEXT("shipwreck_map"), {
			Pool(1, 1, { { TEXT("map"), 1, 1, 1, 0, false } }),
			Pool(3, 3, { { TEXT("compass"), 1, 1, 1, 0, false }, { TEXT("map"), 1, 1, 1, 0, false }, { TEXT("clock"), 1, 1, 1, 0, false },
				{ TEXT("paper"), 20, 1, 10, 0, false }, { TEXT("feather"), 10, 1, 5, 0, false }, { TEXT("book"), 5, 1, 5, 0, false } }) });
		Table(TEXT("shipwreck_supply"), {
			Pool(3, 10, { { TEXT("paper"), 8, 1, 12, 0, false }, { TEXT("potato"), 7, 2, 6, 0, false }, { TEXT("poisonous_potato"), 7, 2, 6, 0, false },
				{ TEXT("carrot"), 7, 4, 8, 0, false }, { TEXT("wheat"), 7, 8, 21, 0, false }, { TEXT("coal"), 6, 2, 8, 0, false },
				{ TEXT("rotten_flesh"), 5, 5, 24, 0, false }, { TEXT("pumpkin"), 2, 1, 3, 0, false }, { TEXT("bamboo"), 2, 1, 3, 0, false },
				{ TEXT("gunpowder"), 3, 1, 5, 0, false }, { TEXT("tnt"), 1, 1, 2, 0, false }, { TEXT("leather_helmet"), 3, 1, 1, 0, false },
				{ TEXT("leather_chestplate"), 3, 1, 1, 0, false }, { TEXT("leather_leggings"), 3, 1, 1, 0, false }, { TEXT("leather_boots"), 3, 1, 1, 0, false } }) });
		Table(TEXT("shipwreck_treasure"), {
			Pool(3, 6, { { TEXT("iron_ingot"), 90, 1, 5, 0, false }, { TEXT("gold_ingot"), 10, 1, 5, 0, false }, { TEXT("emerald"), 40, 1, 5, 0, false },
				{ TEXT("diamond"), 5, 1, 1, 0, false }, { TEXT("experience_bottle"), 5, 1, 1, 0, false } }),
			Pool(2, 5, { { TEXT("iron_nugget"), 50, 1, 10, 0, false }, { TEXT("gold_nugget"), 10, 1, 10, 0, false }, { TEXT("lapis_lazuli"), 20, 1, 10, 0, false } }) });
		Table(TEXT("buried_treasure"), {
			Pool(1, 1, { { TEXT("heart_of_the_sea"), 1, 1, 1, 0, false } }),
			Pool(5, 8, { { TEXT("iron_ingot"), 20, 1, 4, 0, false }, { TEXT("gold_ingot"), 10, 1, 4, 0, false }, { TEXT("tnt"), 5, 1, 2, 0, false } }),
			Pool(1, 3, { { TEXT("emerald"), 5, 4, 8, 0, false }, { TEXT("diamond"), 5, 1, 2, 0, false }, { TEXT("prismarine_crystals"), 5, 1, 5, 0, false } }),
			Pool(0, 1, { { TEXT("leather_chestplate"), 1, 1, 1, 0, false }, { TEXT("iron_sword"), 1, 1, 1, 0, false } }),
			Pool(2, 2, { { TEXT("cooked_cod"), 1, 2, 4, 0, false }, { TEXT("cooked_salmon"), 1, 2, 4, 0, false } }) });
		Table(TEXT("underwater_ruin_small"), {
			Pool(2, 8, { { TEXT("coal"), 10, 1, 4, 0, false }, { TEXT("stone_axe"), 2, 1, 1, 0, false }, { TEXT("rotten_flesh"), 5, 1, 1, 0, false },
				{ TEXT("emerald"), 1, 1, 1, 0, false }, { TEXT("wheat"), 10, 2, 3, 0, false } }),
			Pool(1, 1, { { TEXT("leather_chestplate"), 1, 1, 1, 0, false }, { TEXT("golden_helmet"), 1, 1, 1, 0, false }, { TEXT("fishing_rod"), 5, 1, 1, 0, false },
				{ TEXT("map"), 10, 1, 1, 0, false } }) });
		Table(TEXT("underwater_ruin_big"), {
			Pool(2, 8, { { TEXT("coal"), 10, 1, 4, 0, false }, { TEXT("gold_nugget"), 10, 1, 3, 0, false }, { TEXT("emerald"), 1, 1, 1, 0, false },
				{ TEXT("wheat"), 10, 2, 3, 0, false } }),
			Pool(1, 1, { { TEXT("golden_apple"), 1, 1, 1, 0, false }, { TEXT("enchanted_book"), 5, 1, 1, 30, true }, { TEXT("leather_chestplate"), 1, 1, 1, 0, false },
				{ TEXT("golden_helmet"), 1, 1, 1, 0, false }, { TEXT("fishing_rod"), 5, 1, 1, 0, false }, { TEXT("map"), 10, 1, 1, 0, false } }) });
		Table(TEXT("ruined_portal"), {
			Pool(4, 8, { { TEXT("obsidian"), 40, 1, 2, 0, false }, { TEXT("flint"), 40, 1, 4, 0, false }, { TEXT("iron_nugget"), 40, 9, 18, 0, false },
				{ TEXT("flint_and_steel"), 40, 1, 1, 0, false }, { TEXT("fire_charge"), 40, 1, 1, 0, false }, { TEXT("golden_apple"), 15, 1, 1, 0, false },
				{ TEXT("gold_nugget"), 15, 4, 24, 0, false }, { TEXT("golden_sword"), 15, 1, 1, 0, false }, { TEXT("golden_axe"), 15, 1, 1, 0, false },
				{ TEXT("golden_hoe"), 15, 1, 1, 0, false }, { TEXT("golden_shovel"), 15, 1, 1, 0, false }, { TEXT("golden_pickaxe"), 15, 1, 1, 0, false },
				{ TEXT("golden_boots"), 15, 1, 1, 0, false }, { TEXT("golden_chestplate"), 15, 1, 1, 0, false }, { TEXT("golden_helmet"), 15, 1, 1, 0, false },
				{ TEXT("golden_leggings"), 15, 1, 1, 0, false }, { TEXT("glistering_melon_slice"), 5, 4, 12, 0, false }, { TEXT("golden_horse_armor"), 5, 1, 1, 0, false },
				{ TEXT("light_weighted_pressure_plate"), 5, 1, 1, 0, false }, { TEXT("golden_carrot"), 5, 4, 12, 0, false }, { TEXT("clock"), 5, 1, 1, 0, false },
				{ TEXT("gold_ingot"), 5, 2, 8, 0, false }, { TEXT("bell"), 1, 1, 1, 0, false }, { TEXT("enchanted_golden_apple"), 1, 1, 1, 0, false },
				{ TEXT("gold_block"), 1, 1, 2, 0, false } }) });
		Table(TEXT("woodland_mansion"), {
			Pool(1, 3, { { TEXT("lead"), 20, 1, 1, 0, false }, { TEXT("golden_apple"), 15, 1, 1, 0, false }, { TEXT("enchanted_golden_apple"), 2, 1, 1, 0, false },
				{ TEXT("music_disc_opus55"), 15, 1, 1, 0, false }, { TEXT("name_tag"), 20, 1, 1, 0, false }, { TEXT("chainmail_chestplate"), 10, 1, 1, 0, false },
				{ TEXT("diamond_hoe"), 15, 1, 1, 0, false }, { TEXT("diamond_chestplate"), 5, 1, 1, 0, false }, { TEXT("enchanted_book"), 10, 1, 1, 30, true } }),
			Pool(1, 4, { { TEXT("iron_ingot"), 10, 1, 4, 0, false }, { TEXT("gold_ingot"), 5, 1, 4, 0, false }, { TEXT("bread"), 20, 1, 1, 0, false },
				{ TEXT("wheat"), 20, 1, 4, 0, false }, { TEXT("bucket"), 10, 1, 1, 0, false }, { TEXT("redstone"), 15, 1, 4, 0, false },
				{ TEXT("coal"), 15, 1, 4, 0, false }, { TEXT("melon_seeds"), 10, 2, 4, 0, false } }) });
		Table(TEXT("ancient_city"), {
			Pool(5, 10, { { TEXT("enchanted_golden_apple"), 1, 1, 2, 0, false }, { TEXT("music_disc_voxel"), 2, 1, 1, 0, false }, { TEXT("compass"), 2, 1, 1, 0, false },
				{ TEXT("sculk_catalyst"), 2, 1, 2, 0, false }, { TEXT("name_tag"), 2, 1, 1, 0, false }, { TEXT("diamond_hoe"), 2, 1, 1, 30, false },
				{ TEXT("lead"), 2, 1, 1, 0, false }, { TEXT("diamond_leggings"), 2, 1, 1, 30, false }, { TEXT("enchanted_book"), 3, 1, 1, 30, true },
				{ TEXT("disc_fragment_5"), 2, 1, 3, 0, false }, { TEXT("amethyst_shard"), 3, 1, 15, 0, false }, { TEXT("experience_bottle"), 3, 1, 3, 0, false },
				{ TEXT("glow_berries"), 3, 1, 15, 0, false }, { TEXT("iron_leggings"), 3, 1, 1, 20, false }, { TEXT("echo_shard"), 4, 1, 3, 0, false },
				{ TEXT("sculk_sensor"), 4, 1, 3, 0, false }, { TEXT("candle"), 4, 1, 4, 0, false }, { TEXT("book"), 5, 3, 10, 0, false },
				{ TEXT("bone"), 5, 1, 15, 0, false }, { TEXT("soul_torch"), 5, 1, 15, 0, false }, { TEXT("coal"), 7, 6, 15, 0, false } }) });
		Table(TEXT("ancient_city_ice_box"), {
			Pool(4, 10, { { TEXT("suspicious_stew"), 1, 2, 6, 0, false }, { TEXT("golden_carrot"), 1, 1, 10, 0, false }, { TEXT("baked_potato"), 1, 1, 10, 0, false },
				{ TEXT("packed_ice"), 2, 2, 6, 0, false }, { TEXT("snowball"), 4, 2, 6, 0, false } }) });
		Table(TEXT("trail_ruins"), {
			Pool(1, 3, { { TEXT("emerald"), 2, 1, 1, 0, false }, { TEXT("wheat"), 2, 1, 1, 0, false }, { TEXT("wooden_hoe"), 2, 1, 1, 0, false },
				{ TEXT("clay_ball"), 2, 1, 1, 0, false }, { TEXT("brick"), 2, 1, 1, 0, false }, { TEXT("yellow_dye"), 2, 1, 1, 0, false },
				{ TEXT("blue_dye"), 2, 1, 1, 0, false }, { TEXT("coal"), 2, 1, 1, 0, false }, { TEXT("string"), 1, 1, 1, 0, false } }) });
		Table(TEXT("trial_chambers_reward"), {
			Pool(1, 3, { { TEXT("emerald"), 4, 2, 4, 0, false }, { TEXT("arrow"), 4, 3, 8, 0, false }, { TEXT("iron_ingot"), 4, 1, 2, 0, false },
				{ TEXT("honey_bottle"), 3, 1, 1, 0, false }, { TEXT("ominous_trial_key"), 1, 1, 1, 0, false }, { TEXT("wind_charge"), 3, 1, 3, 0, false },
				{ TEXT("diamond"), 2, 1, 2, 0, false }, { TEXT("enchanted_book"), 2, 1, 1, 30, true }, { TEXT("heavy_core"), 1, 1, 1, 0, false },
				{ TEXT("diamond_chestplate"), 1, 1, 1, 30, false }, { TEXT("golden_apple"), 2, 1, 1, 0, false } }) });
		Table(TEXT("nether_bridge"), {
			Pool(2, 4, { { TEXT("diamond"), 5, 1, 3, 0, false }, { TEXT("iron_ingot"), 5, 1, 5, 0, false }, { TEXT("gold_ingot"), 15, 1, 3, 0, false },
				{ TEXT("golden_sword"), 5, 1, 1, 0, false }, { TEXT("golden_chestplate"), 5, 1, 1, 0, false }, { TEXT("flint_and_steel"), 5, 1, 1, 0, false },
				{ TEXT("nether_wart"), 5, 3, 7, 0, false }, { TEXT("saddle"), 10, 1, 1, 0, false }, { TEXT("golden_horse_armor"), 8, 1, 1, 0, false },
				{ TEXT("iron_horse_armor"), 5, 1, 1, 0, false }, { TEXT("diamond_horse_armor"), 3, 1, 1, 0, false }, { TEXT("obsidian"), 2, 2, 4, 0, false } }) });
		Table(TEXT("bastion_treasure"), {
			Pool(3, 3, { { TEXT("netherite_ingot"), 15, 1, 1, 0, false }, { TEXT("ancient_debris"), 10, 1, 1, 0, false }, { TEXT("netherite_scrap"), 8, 1, 1, 0, false },
				{ TEXT("ancient_debris"), 4, 2, 2, 0, false }, { TEXT("diamond_sword"), 6, 1, 1, 30, false }, { TEXT("diamond_chestplate"), 6, 1, 1, 30, false },
				{ TEXT("diamond_helmet"), 6, 1, 1, 30, false }, { TEXT("diamond_leggings"), 6, 1, 1, 30, false }, { TEXT("diamond_boots"), 6, 1, 1, 30, false },
				{ TEXT("diamond"), 5, 2, 6, 0, false }, { TEXT("enchanted_golden_apple"), 2, 1, 1, 0, false } }),
			Pool(3, 4, { { TEXT("spectral_arrow"), 1, 12, 25, 0, false }, { TEXT("gold_block"), 1, 2, 5, 0, false }, { TEXT("iron_block"), 1, 2, 5, 0, false },
				{ TEXT("gold_ingot"), 1, 3, 9, 0, false }, { TEXT("iron_ingot"), 1, 3, 9, 0, false }, { TEXT("crying_obsidian"), 1, 3, 5, 0, false },
				{ TEXT("quartz"), 1, 8, 23, 0, false }, { TEXT("gilded_blackstone"), 1, 5, 15, 0, false }, { TEXT("magma_cream"), 1, 3, 8, 0, false } }) });
		Table(TEXT("bastion_other"), {
			Pool(1, 1, { { TEXT("diamond_pickaxe"), 6, 1, 1, 0, false }, { TEXT("diamond_shovel"), 6, 1, 1, 0, false }, { TEXT("crossbow"), 6, 1, 1, 0, false },
				{ TEXT("ancient_debris"), 12, 1, 1, 0, false }, { TEXT("netherite_scrap"), 4, 1, 1, 0, false }, { TEXT("spectral_arrow"), 10, 10, 22, 0, false },
				{ TEXT("golden_carrot"), 12, 6, 17, 0, false }, { TEXT("golden_apple"), 9, 1, 1, 0, false }, { TEXT("enchanted_book"), 10, 1, 1, 30, true } }),
			Pool(3, 4, { { TEXT("iron_nugget"), 1, 2, 8, 0, false }, { TEXT("gold_nugget"), 1, 2, 8, 0, false }, { TEXT("string"), 1, 4, 6, 0, false },
				{ TEXT("leather"), 1, 1, 3, 0, false }, { TEXT("arrow"), 1, 5, 17, 0, false }, { TEXT("crying_obsidian"), 1, 1, 5, 0, false },
				{ TEXT("gilded_blackstone"), 1, 1, 5, 0, false }, { TEXT("chain"), 1, 2, 10, 0, false }, { TEXT("magma_cream"), 1, 2, 6, 0, false },
				{ TEXT("cooked_porkchop"), 1, 2, 5, 0, false } }) });
		Table(TEXT("bastion_bridge"), {
			Pool(1, 1, { { TEXT("lodestone"), 1, 1, 1, 0, false } }),
			Pool(1, 2, { { TEXT("crossbow"), 1, 1, 1, 0, false }, { TEXT("spectral_arrow"), 1, 10, 28, 0, false }, { TEXT("gilded_blackstone"), 1, 8, 12, 0, false },
				{ TEXT("crying_obsidian"), 1, 3, 8, 0, false }, { TEXT("gold_block"), 1, 1, 1, 0, false }, { TEXT("gold_ingot"), 1, 4, 9, 0, false },
				{ TEXT("iron_ingot"), 1, 4, 9, 0, false }, { TEXT("golden_sword"), 1, 1, 1, 0, false } }) });
		Table(TEXT("end_city_treasure"), {
			Pool(2, 6, { { TEXT("diamond"), 5, 2, 7, 0, false }, { TEXT("iron_ingot"), 10, 4, 8, 0, false }, { TEXT("gold_ingot"), 15, 2, 7, 0, false },
				{ TEXT("emerald"), 2, 2, 6, 0, false }, { TEXT("beetroot_seeds"), 5, 1, 10, 0, false }, { TEXT("saddle"), 3, 1, 1, 0, false },
				{ TEXT("iron_horse_armor"), 1, 1, 1, 0, false }, { TEXT("golden_horse_armor"), 1, 1, 1, 0, false }, { TEXT("diamond_horse_armor"), 1, 1, 1, 0, false },
				{ TEXT("diamond_sword"), 3, 1, 1, 30, false }, { TEXT("diamond_boots"), 3, 1, 1, 30, false }, { TEXT("diamond_chestplate"), 3, 1, 1, 30, false },
				{ TEXT("diamond_leggings"), 3, 1, 1, 30, false }, { TEXT("diamond_helmet"), 3, 1, 1, 30, false }, { TEXT("diamond_pickaxe"), 3, 1, 1, 30, false },
				{ TEXT("diamond_shovel"), 3, 1, 1, 30, false }, { TEXT("iron_sword"), 3, 1, 1, 20, false }, { TEXT("iron_pickaxe"), 3, 1, 1, 20, false } }) });
	}

	const FLootTable* FindTable(FName N)
	{
		InitTables();
		for (const FLootTable& T : GTables) if (T.Name == N) return &T;
		return nullptr;
	}
}

namespace MCLoot
{
	void GetTableNames(TArray<FName>& Out)
	{
		InitTables();
		for (const FLootTable& T : GTables) Out.Add(T.Name);
	}

	void EnchantRandomly(FMCItemStack& S, int32 Level, bool bTreasure, FMCRandom& R)
	{
		if (S.IsEmpty()) return;
		const FMCItem& I = S.Item();
		const bool bBook = I.Name == TEXT("enchanted_book") || I.Name == TEXT("book");
		if (I.Name == TEXT("book")) S = FMCItemStack::Of(TEXT("enchanted_book"), 1);
		TArray<EMCEnchant> Candidates;
		for (int32 e = 1; e < (int32)EMCEnchant::Count; ++e)
		{
			const EMCEnchant E = (EMCEnchant)e;
			const MCEnchants::FInfo& Info = MCEnchants::Info(E);
			if (Info.bTreasure && !bTreasure) continue;
			if (!bBook && !MCEnchants::CanApply(E, I)) continue;
			Candidates.Add(E);
		}
		if (Candidates.Num() == 0) return;
		const int32 N = 1 + (Level > 15 && R.Chance(0.5) ? 1 : 0) + (Level > 25 && R.Chance(0.3) ? 1 : 0);
		for (int32 k = 0; k < N && Candidates.Num(); ++k)
		{
			int32 TotalW = 0;
			for (EMCEnchant E : Candidates) TotalW += FMath::Max(1, MCEnchants::Info(E).Weight);
			int32 Pick = R.NextInt(TotalW);
			EMCEnchant Chosen = Candidates[0];
			for (EMCEnchant E : Candidates) { Pick -= FMath::Max(1, MCEnchants::Info(E).Weight); if (Pick < 0) { Chosen = E; break; } }
			const int32 MaxL = MCEnchants::Info(Chosen).MaxLevel;
			int32 Lvl = FMath::Clamp(1 + (int32)(MaxL * FMath::Min(1.f, Level / 30.f) * R.FRange(0.5f, 1.1f)), 1, MaxL);
			if (bBook) S.MutableExtra().StoredEnchants.Add(FMCEnchantLevel{ Chosen, (uint8)Lvl });
			else S.AddEnchant(Chosen, (uint8)Lvl);
			Candidates.RemoveAll([&](EMCEnchant E) { return E == Chosen || !MCEnchants::Compatible(E, Chosen); });
		}
	}

	void FillContainer(FName TableName, uint64 Seed, FMCContainer& Inv)
	{
		const FLootTable* T = FindTable(TableName);
		if (!T || Inv.Num() == 0) return;
		FMCRandom R(Seed ^ 0x1007ABull);
		TArray<FMCItemStack> Items;
		for (const FLootPool& P : T->Pools)
		{
			int32 TotalW = 0;
			for (const FLootEntry& E : P.Entries) TotalW += E.Weight;
			if (TotalW <= 0) continue;
			const int32 Rolls = R.Range(P.RollsMin, P.RollsMax);
			for (int32 i = 0; i < Rolls; ++i)
			{
				int32 Pick = R.NextInt(TotalW);
				const FLootEntry* Chosen = &P.Entries[0];
				for (const FLootEntry& E : P.Entries) { Pick -= E.Weight; if (Pick < 0) { Chosen = &E; break; } }
				FMCItemStack S = FMCItemStack::Of(FName(Chosen->Item), R.Range(Chosen->Min, Chosen->Max));
				if (S.IsEmpty()) continue;
				if (Chosen->EnchantLevel > 0) EnchantRandomly(S, Chosen->EnchantLevel, Chosen->bTreasure, R);
				if (S.IsDamageable() && S.Item().Kind != EMCItemKind::Book && R.Chance(0.4)) S.Damage = R.NextInt(FMath::Max(1, S.Item().MaxDamage / 2));
				Items.Add(S);
			}
		}
		// Minecraft splits stacks across random slots
		TArray<int32> Free;
		for (int32 i = 0; i < Inv.Num(); ++i) if (Inv[i].IsEmpty()) Free.Add(i);
		for (int32 i = Free.Num() - 1; i > 0; --i) Free.Swap(i, R.NextInt(i + 1));
		for (FMCItemStack& S : Items)
		{
			while (S.Count > 1 && Free.Num() > Items.Num() && R.Chance(0.3))
			{
				FMCItemStack Part = S.Split(FMath::Max(1, S.Count / 2));
				if (Free.Num() == 0) break;
				Inv[Free.Pop()] = Part;
			}
			if (Free.Num() == 0) break;
			Inv[Free.Pop()] = S;
		}
	}

	bool CanHarvest(FMCState S, const FMCItemStack* Tool)
	{
		const FMCBlock& B = FMCBlocks::GetByState(S);
		if (!B.bRequiresTool) return true;
		if (!Tool || Tool->IsEmpty()) return false;
		const FMCItem& I = Tool->Item();
		if (B.Tool == EMCTool::Shears) return I.ToolType == EMCTool::Shears;
		if (B.Tool == EMCTool::Sword) return I.ToolType == EMCTool::Sword || I.ToolType == EMCTool::Shears;
		if (I.ToolType != B.Tool) return false;
		return I.Tier >= B.MinTier;
	}

	void GetBlockDrops(FMCWorld& W, const FMCBlockPos& P, FMCState S, const FMCItemStack* Tool, TArray<FMCItemStack>& Out, FMCRandom& R)
	{
		const FMCBlock& B = FMCBlocks::GetByState(S);
		if (S == 0) return;
		if (!CanHarvest(S, Tool)) return;
		// behaviour-specific drops first (crops, leaves, beds...)
		if (B.Behavior && B.Behavior->GetDrops(W, P, S, Tool, Out, R)) return;
		const bool bSilk = Tool && Tool->GetEnchant(EMCEnchant::SilkTouch) > 0;
		const int32 Fortune = Tool ? Tool->GetEnchant(EMCEnchant::Fortune) : 0;
		const FMCItemId SelfItem = B.ItemName.IsNone() ? FMCItems::ForBlock(B.Id) : FMCItems::FindId(B.ItemName);
		auto AddSelf = [&](int32 Count)
		{
			if (SelfItem) Out.Add(FMCItemStack(SelfItem, Count));
		};
		switch (B.Drop.Type)
		{
		case EMCDropType::None:
			return;
		case EMCDropType::SilkOnly:
			if (bSilk) AddSelf(1);
			return;
		case EMCDropType::Item:
		{
			if (bSilk && B.Drop.bSilkSelf && SelfItem) { AddSelf(1); return; }
			int32 Count = R.Range(B.Drop.Min, B.Drop.Max);
			if (B.Drop.bFortune && Fortune > 0)
			{
				if (B.Drop.Min == B.Drop.Max && B.Drop.Min == 1) Count *= FortuneBonus(R, Fortune); // ores
				else Count = FMath::Min(Count + R.NextInt(Fortune + 1), B.Drop.Max + Fortune * 2);          // glowstone, sea lanterns...
			}
			if (Count > 0) Out.Add(FMCItemStack::Of(B.Drop.Item, Count));
			return;
		}
		case EMCDropType::Special:
			// generic special rules for blocks without dedicated behaviours
			if (B.Name == TEXT("gilded_blackstone")) { if (R.NextFloat() < 0.1f + Fortune * 0.043f) Out.Add(Stack(TEXT("gold_nugget"), R.Range(2, 5))); else AddSelf(1); return; }
			if (B.Name == TEXT("short_grass") || B.Name == TEXT("fern")) return;
			AddSelf(1);
			return;
		default:
			break;
		}
		// Self: slab doubles, snow layers, candles
		int32 Count = 1;
		if (B.Model == EMCModel::Slab && FMCBlocks::MetaOf(S) == 2) Count = 2;
		if (B.Model == EMCModel::SnowLayer) { Out.Add(Stack(TEXT("snowball"), (FMCBlocks::MetaOf(S) & 7) + 1)); return; }
		if (B.Model == EMCModel::SeaPickle) Count = (FMCBlocks::MetaOf(S) & 3) + 1;
		AddSelf(Count);
	}

	void GetMobDrops(FName Mob, int32 Variant, int32 Looting, bool bPlayerKill, bool bOnFire, bool bBaby, FMCRandom& R, TArray<FMCItemStack>& Out)
	{
		if (bBaby) return;
		auto Add = [&](const TCHAR* Item, int32 Min, int32 Max, bool bLooting = true)
		{
			int32 N = R.Range(Min, Max) + (bLooting && Looting > 0 ? R.NextInt(Looting + 1) : 0);
			if (N > 0) Out.Add(Stack(Item, N));
		};
		auto Rare = [&](const TCHAR* Item, float Chance, int32 Count = 1)
		{
			if (bPlayerKill && R.NextFloat() < Chance + Looting * 0.01f) Out.Add(Stack(Item, Count));
		};
		const FString M = Mob.ToString();
		if (M == TEXT("cow") || M == TEXT("mooshroom")) { Add(TEXT("leather"), 0, 2); Add(bOnFire ? TEXT("cooked_beef") : TEXT("beef"), 1, 3); }
		else if (M == TEXT("pig")) Add(bOnFire ? TEXT("cooked_porkchop") : TEXT("porkchop"), 1, 3);
		else if (M == TEXT("sheep"))
		{
			static const TCHAR* Wool[16] = { TEXT("white_wool"), TEXT("orange_wool"), TEXT("magenta_wool"), TEXT("light_blue_wool"), TEXT("yellow_wool"), TEXT("lime_wool"),
				TEXT("pink_wool"), TEXT("gray_wool"), TEXT("light_gray_wool"), TEXT("cyan_wool"), TEXT("purple_wool"), TEXT("blue_wool"), TEXT("brown_wool"),
				TEXT("green_wool"), TEXT("red_wool"), TEXT("black_wool") };
			if (Variant >= 0 && Variant < 16) Out.Add(Stack(Wool[Variant], 1));
			Add(bOnFire ? TEXT("cooked_mutton") : TEXT("mutton"), 1, 2);
		}
		else if (M == TEXT("chicken")) { Add(TEXT("feather"), 0, 2); Add(bOnFire ? TEXT("cooked_chicken") : TEXT("chicken"), 1, 1); }
		else if (M == TEXT("rabbit")) { Add(TEXT("rabbit_hide"), 0, 1); Add(bOnFire ? TEXT("cooked_rabbit") : TEXT("rabbit"), 0, 1); Rare(TEXT("rabbit_foot"), 0.1f); }
		else if (M == TEXT("horse") || M == TEXT("donkey") || M == TEXT("mule") || M == TEXT("llama") || M == TEXT("trader_llama") || M == TEXT("camel")) Add(TEXT("leather"), 0, 2);
		else if (M == TEXT("zombie") || M == TEXT("husk") || M == TEXT("zombie_villager") || M == TEXT("zombie_horse"))
		{
			Add(TEXT("rotten_flesh"), 0, 2);
			Rare(TEXT("iron_ingot"), 0.025f); Rare(TEXT("carrot"), 0.025f); Rare(TEXT("potato"), 0.025f);
		}
		else if (M == TEXT("drowned")) { Add(TEXT("rotten_flesh"), 0, 2); Rare(TEXT("copper_ingot"), 0.11f); }
		else if (M == TEXT("skeleton") || M == TEXT("stray") || M == TEXT("bogged") || M == TEXT("parched") || M == TEXT("skeleton_horse"))
		{
			Add(TEXT("arrow"), 0, 2); Add(TEXT("bone"), 0, 2);
			if (M == TEXT("stray")) Rare(TEXT("tipped_arrow"), 0.5f);
		}
		else if (M == TEXT("wither_skeleton")) { Add(TEXT("coal"), -1, 1); Add(TEXT("bone"), 0, 2); Rare(TEXT("wither_skeleton_skull"), 0.025f); }
		else if (M == TEXT("creeper")) Add(TEXT("gunpowder"), 0, 2);
		else if (M == TEXT("spider") || M == TEXT("cave_spider")) { Add(TEXT("string"), 0, 2); if (bPlayerKill) Add(TEXT("spider_eye"), -1, 1); }
		else if (M == TEXT("enderman")) Add(TEXT("ender_pearl"), 0, 1);
		else if (M == TEXT("slime")) Add(TEXT("slime_ball"), 0, 2);
		else if (M == TEXT("magma_cube")) Add(TEXT("magma_cream"), -2, 1);
		else if (M == TEXT("ghast")) { Add(TEXT("ghast_tear"), 0, 1); Add(TEXT("gunpowder"), 0, 2); }
		else if (M == TEXT("blaze")) { if (bPlayerKill) Add(TEXT("blaze_rod"), 0, 1); }
		else if (M == TEXT("witch"))
		{
			static const TCHAR* Drops[] = { TEXT("glass_bottle"), TEXT("glowstone_dust"), TEXT("gunpowder"), TEXT("redstone"), TEXT("spider_eye"), TEXT("sugar"), TEXT("stick") };
			for (int32 i = 0; i < R.Range(1, 3); ++i) Add(Drops[R.NextInt(UE_ARRAY_COUNT(Drops))], 0, 2);
		}
		else if (M == TEXT("zombified_piglin")) { Add(TEXT("rotten_flesh"), 0, 1); Add(TEXT("gold_nugget"), 0, 1); Rare(TEXT("gold_ingot"), 0.025f); }
		else if (M == TEXT("hoglin")) { Add(bOnFire ? TEXT("cooked_porkchop") : TEXT("porkchop"), 2, 4); Add(TEXT("leather"), 0, 1); }
		else if (M == TEXT("zoglin")) Add(TEXT("rotten_flesh"), 1, 3);
		else if (M == TEXT("guardian")) { Add(TEXT("prismarine_shard"), 0, 2); if (R.Chance(0.4)) Add(TEXT("cod"), 1, 1); else if (R.Chance(0.4)) Add(TEXT("prismarine_crystals"), 1, 1); }
		else if (M == TEXT("elder_guardian")) { Add(TEXT("prismarine_shard"), 0, 2); Out.Add(Stack(TEXT("wet_sponge"), 1)); Add(TEXT("cod"), 0, 1); }
		else if (M == TEXT("shulker")) { if (R.NextFloat() < 0.5f + Looting * 0.0625f) Out.Add(Stack(TEXT("shulker_shell"), 1)); }
		else if (M == TEXT("phantom")) { if (bPlayerKill) Add(TEXT("phantom_membrane"), 0, 1); }
		else if (M == TEXT("cod")) { Out.Add(Stack(bOnFire ? TEXT("cooked_cod") : TEXT("cod"), 1)); if (R.Chance(0.05)) Out.Add(Stack(TEXT("bone_meal"), 1)); }
		else if (M == TEXT("salmon")) { Out.Add(Stack(bOnFire ? TEXT("cooked_salmon") : TEXT("salmon"), 1)); if (R.Chance(0.05)) Out.Add(Stack(TEXT("bone_meal"), 1)); }
		else if (M == TEXT("tropical_fish")) Out.Add(Stack(TEXT("tropical_fish"), 1));
		else if (M == TEXT("pufferfish")) Out.Add(Stack(TEXT("pufferfish"), 1));
		else if (M == TEXT("squid")) Add(TEXT("ink_sac"), 1, 3);
		else if (M == TEXT("glow_squid")) Add(TEXT("glow_ink_sac"), 1, 3);
		else if (M == TEXT("polar_bear")) { Add(R.Chance(0.75) ? TEXT("cod") : TEXT("salmon"), 0, 2); }
		else if (M == TEXT("panda")) Add(TEXT("bamboo"), 0, 2);
		else if (M == TEXT("turtle")) Add(TEXT("seagrass"), 0, 2);
		else if (M == TEXT("dolphin")) Add(bOnFire ? TEXT("cooked_cod") : TEXT("cod"), 0, 1);
		else if (M == TEXT("iron_golem")) { Add(TEXT("iron_ingot"), 3, 5, false); Add(TEXT("poppy"), 0, 2, false); }
		else if (M == TEXT("snow_golem")) Add(TEXT("snowball"), 0, 15, false);
		else if (M == TEXT("copper_golem")) Add(TEXT("copper_ingot"), 1, 3, false);
		else if (M == TEXT("vindicator") || M == TEXT("pillager")) { Add(TEXT("emerald"), 0, 1); if (M == TEXT("pillager") && R.Chance(0.085)) Out.Add(Stack(TEXT("crossbow"), 1)); }
		else if (M == TEXT("evoker")) { Out.Add(Stack(TEXT("totem_of_undying"), 1)); Add(TEXT("emerald"), 0, 1); }
		else if (M == TEXT("ravager")) Out.Add(Stack(TEXT("saddle"), 1));
		else if (M == TEXT("piglin_brute")) Rare(TEXT("golden_axe"), 0.085f);
		else if (M == TEXT("strider")) Add(TEXT("string"), 2, 5);
		else if (M == TEXT("goat")) {}
		else if (M == TEXT("breeze")) Add(TEXT("breeze_rod"), 1, 2);
		else if (M == TEXT("warden")) Out.Add(Stack(TEXT("sculk_catalyst"), 1));
		else if (M == TEXT("armadillo")) {}
		else if (M == TEXT("frog")) {}
		else if (M == TEXT("sulfur_cube")) { Add(TEXT("sulfur_shard"), 1, 3); if (R.Chance(0.2)) Add(TEXT("cinnabar_dust"), 1, 2); }
		else if (M == TEXT("creaking")) Add(TEXT("resin_clump"), 0, 1);
		else if (M == TEXT("wither")) Out.Add(Stack(TEXT("nether_star"), 1));
		else if (M == TEXT("fox")) {}
		else if (M == TEXT("happy_ghast") || M == TEXT("ghastling")) {}
		else if (M == TEXT("zombie_nautilus") || M == TEXT("nautilus")) { if (R.Chance(0.2)) Out.Add(Stack(TEXT("nautilus_shell"), 1)); }
		else if (M == TEXT("camel_husk")) Add(TEXT("rotten_flesh"), 0, 2);
	}

	FMCItemStack Barter(FMCRandom& R)
	{
		struct FB { const TCHAR* Item; int32 W, Min, Max; };
		static const FB Table[] = {
			{ TEXT("ender_pearl"), 10, 2, 4 }, { TEXT("string"), 20, 3, 9 }, { TEXT("quartz"), 20, 5, 12 }, { TEXT("obsidian"), 40, 1, 1 },
			{ TEXT("crying_obsidian"), 40, 1, 3 }, { TEXT("fire_charge"), 40, 1, 1 }, { TEXT("leather"), 40, 2, 4 }, { TEXT("soul_sand"), 40, 2, 8 },
			{ TEXT("nether_brick"), 40, 2, 8 }, { TEXT("spectral_arrow"), 40, 6, 12 }, { TEXT("gravel"), 40, 8, 16 }, { TEXT("blackstone"), 40, 8, 16 },
			{ TEXT("iron_nugget"), 10, 10, 36 }, { TEXT("enchanted_book"), 5, 1, 1 }, { TEXT("iron_boots"), 8, 1, 1 },
		};
		int32 Total = 0;
		for (const FB& B : Table) Total += B.W;
		int32 Pick = R.NextInt(Total);
		for (const FB& B : Table)
		{
			Pick -= B.W;
			if (Pick < 0)
			{
				FMCItemStack S = FMCItemStack::Of(B.Item, R.Range(B.Min, B.Max));
				if (FCString::Strcmp(B.Item, TEXT("enchanted_book")) == 0) S.MutableExtra().StoredEnchants.Add(FMCEnchantLevel{ EMCEnchant::SoulSpeed, (uint8)R.Range(1, 3) });
				if (FCString::Strcmp(B.Item, TEXT("iron_boots")) == 0) S.AddEnchant(EMCEnchant::SoulSpeed, (uint8)R.Range(1, 3));
				return S;
			}
		}
		return FMCItemStack::Of(TEXT("gravel"), 8);
	}

	FMCItemStack Fish(int32 Luck, bool bOpenWater, FMCRandom& R)
	{
		const float Treasure = bOpenWater ? 0.05f + Luck * 0.021f : 0.f;
		const float Junk = FMath::Max(0.f, 0.1f - Luck * 0.021f);
		const float Roll = R.NextFloat();
		if (Roll < Treasure)
		{
			const TCHAR* T[] = { TEXT("bow"), TEXT("enchanted_book"), TEXT("fishing_rod"), TEXT("name_tag"), TEXT("nautilus_shell"), TEXT("saddle") };
			FMCItemStack S = FMCItemStack::Of(T[R.NextInt(UE_ARRAY_COUNT(T))], 1);
			if (S.Item().Name == TEXT("enchanted_book") || S.Item().Name == TEXT("bow") || S.Item().Name == TEXT("fishing_rod")) EnchantRandomly(S, 30, true, R);
			return S;
		}
		if (Roll < Treasure + Junk)
		{
			const TCHAR* J[] = { TEXT("lily_pad"), TEXT("leather_boots"), TEXT("leather"), TEXT("bone"), TEXT("potion"), TEXT("string"), TEXT("stick"), TEXT("bowl"), TEXT("ink_sac"), TEXT("tripwire_hook"), TEXT("rotten_flesh") };
			return FMCItemStack::Of(J[R.NextInt(UE_ARRAY_COUNT(J))], 1);
		}
		const float F = R.NextFloat();
		return FMCItemStack::Of(F < 0.6f ? TEXT("cod") : (F < 0.85f ? TEXT("salmon") : (F < 0.87f ? TEXT("tropical_fish") : TEXT("pufferfish"))), 1);
	}
}
