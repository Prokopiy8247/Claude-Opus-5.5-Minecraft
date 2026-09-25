// Crafting recipe definitions (original data tables written for this project).
#include "Crafting/MCRecipeBuilder.h"

namespace
{
	using RB = EMCRecipeBook;
	using KP = TPair<TCHAR, const TCHAR*>;

	FString DerivedBase(const FString& Base)
	{
		auto Strip = [](const FString& S, const TCHAR* Suffix) { return S.EndsWith(Suffix) ? S.LeftChop(FCString::Strlen(Suffix)) : S; };
		if (Base.EndsWith(TEXT("_planks"))) return Strip(Base, TEXT("_planks"));
		if (Base.EndsWith(TEXT("_bricks"))) return Strip(Base, TEXT("_bricks")) + TEXT("_brick");
		if (Base.EndsWith(TEXT("_tiles"))) return Strip(Base, TEXT("_tiles")) + TEXT("_tile");
		if (Base == TEXT("bricks")) return TEXT("brick");
		if (Base == TEXT("purpur_block")) return TEXT("purpur");
		if (Base == TEXT("quartz_block")) return TEXT("quartz");
		if (Base.EndsWith(TEXT("_block"))) return Strip(Base, TEXT("_block"));
		return Base;
	}

	/** Stairs / slabs / walls for a building block (crafting + stonecutter). */
	void StoneFamily(FMCRecipeBuilder& B, const TCHAR* BaseName, bool bStonecut = true)
	{
		const FString Base(BaseName);
		const FString D = DerivedBase(Base);
		const FString Stairs = D + TEXT("_stairs"), Slab = D + TEXT("_slab"), Wall = D + TEXT("_wall");
		if (FMCRecipeBuilder::Exists(*Stairs)) B.Shaped(*Stairs, 4, { TEXT("#  "), TEXT("## "), TEXT("###") }, { KP(TEXT('#'), BaseName) }, RB::Building);
		if (FMCRecipeBuilder::Exists(*Slab)) B.Shaped(*Slab, 6, { TEXT("###") }, { KP(TEXT('#'), BaseName) }, RB::Building);
		if (FMCRecipeBuilder::Exists(*Wall)) B.Shaped(*Wall, 6, { TEXT("###"), TEXT("###") }, { KP(TEXT('#'), BaseName) }, RB::Building);
		if (bStonecut)
		{
			if (FMCRecipeBuilder::Exists(*Stairs)) B.Stonecut(BaseName, *Stairs, 1);
			if (FMCRecipeBuilder::Exists(*Slab)) B.Stonecut(BaseName, *Slab, 2);
			if (FMCRecipeBuilder::Exists(*Wall)) B.Stonecut(BaseName, *Wall, 1);
		}
	}

	void Compact(FMCRecipeBuilder& B, const TCHAR* Item, const TCHAR* Block, bool bReverse = true)
	{
		B.Shaped(Block, 1, { TEXT("###"), TEXT("###"), TEXT("###") }, { KP(TEXT('#'), Item) }, RB::Building);
		if (bReverse) B.Shapeless(Item, 9, { Block }, RB::Misc);
	}

	void Square(FMCRecipeBuilder& B, const TCHAR* From, const TCHAR* To, int32 Count = 4)
	{
		B.Shaped(To, Count, { TEXT("##"), TEXT("##") }, { KP(TEXT('#'), From) }, RB::Building);
	}
}

void MCRegisterCraftingRecipes(FMCRecipeBuilder& B)
{
	// ================================================================ wood families
	const TCHAR* Woods[] = { TEXT("oak"), TEXT("spruce"), TEXT("birch"), TEXT("jungle"), TEXT("acacia"), TEXT("dark_oak"), TEXT("mangrove"), TEXT("cherry"), TEXT("pale_oak"), TEXT("bamboo"), TEXT("crimson"), TEXT("warped") };
	for (const TCHAR* Wd : Woods)
	{
		const FString N = Wd;
		const bool bNether = N == TEXT("crimson") || N == TEXT("warped");
		const bool bBamboo = N == TEXT("bamboo");
		const FString Planks = N + TEXT("_planks");
		if (bBamboo)
		{
			B.Shapeless(*Planks, 2, { TEXT("bamboo_block") }, RB::Building);
			B.Shapeless(*Planks, 2, { TEXT("stripped_bamboo_block") }, RB::Building);
			B.Shaped(TEXT("bamboo_block"), 1, { TEXT("###"), TEXT("###"), TEXT("###") }, { KP(TEXT('#'), TEXT("bamboo")) }, RB::Building);
			B.Shaped(TEXT("bamboo_mosaic"), 1, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("bamboo_slab")) }, RB::Building);
			StoneFamily(B, TEXT("bamboo_mosaic"), false);
			B.Shaped(TEXT("bamboo_raft"), 1, { TEXT("# #"), TEXT("###") }, { KP(TEXT('#'), TEXT("bamboo_planks")) }, RB::Misc);
		}
		else
		{
			const FString Log = bNether ? N + TEXT("_stem") : N + TEXT("_log");
			const FString Wood = bNether ? N + TEXT("_hyphae") : N + TEXT("_wood");
			const FString SLog = TEXT("stripped_") + Log, SWood = TEXT("stripped_") + Wood;
			for (const FString& L : { Log, Wood, SLog, SWood }) B.Shapeless(*Planks, 4, { *L }, RB::Building);
			B.Shaped(*Wood, 3, { TEXT("##"), TEXT("##") }, { KP(TEXT('#'), *Log) }, RB::Building);
			B.Shaped(*SWood, 3, { TEXT("##"), TEXT("##") }, { KP(TEXT('#'), *SLog) }, RB::Building);
			if (!bNether)
			{
				B.Shaped(*(N + TEXT("_boat")), 1, { TEXT("# #"), TEXT("###") }, { KP(TEXT('#'), *Planks) }, RB::Misc);
				B.Shapeless(*(N + TEXT("_chest_boat")), 1, { *(N + TEXT("_boat")), TEXT("chest") }, RB::Misc);
			}
		}
		const FString Stairs = N + TEXT("_stairs"), Slab = N + TEXT("_slab");
		B.Shaped(*Stairs, 4, { TEXT("#  "), TEXT("## "), TEXT("###") }, { KP(TEXT('#'), *Planks) }, RB::Building);
		B.Shaped(*Slab, 6, { TEXT("###") }, { KP(TEXT('#'), *Planks) }, RB::Building);
		B.Shaped(*(N + TEXT("_fence")), 3, { TEXT("#|#"), TEXT("#|#") }, { KP(TEXT('#'), *Planks), KP(TEXT('|'), TEXT("stick")) }, RB::Misc);
		B.Shaped(*(N + TEXT("_fence_gate")), 1, { TEXT("|#|"), TEXT("|#|") }, { KP(TEXT('#'), *Planks), KP(TEXT('|'), TEXT("stick")) }, RB::Redstone);
		B.Shaped(*(N + TEXT("_door")), 3, { TEXT("##"), TEXT("##"), TEXT("##") }, { KP(TEXT('#'), *Planks) }, RB::Redstone);
		B.Shaped(*(N + TEXT("_trapdoor")), 2, { TEXT("###"), TEXT("###") }, { KP(TEXT('#'), *Planks) }, RB::Redstone);
		B.Shapeless(*(N + TEXT("_button")), 1, { *Planks }, RB::Redstone);
		B.Shaped(*(N + TEXT("_pressure_plate")), 1, { TEXT("##") }, { KP(TEXT('#'), *Planks) }, RB::Redstone);
	}
	B.Shaped(TEXT("stick"), 4, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("#planks")) });
	B.Shaped(TEXT("stick"), 1, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("bamboo")) });
	B.Shaped(TEXT("crafting_table"), 1, { TEXT("##"), TEXT("##") }, { KP(TEXT('#'), TEXT("#planks")) });
	B.Shaped(TEXT("chest"), 1, { TEXT("###"), TEXT("# #"), TEXT("###") }, { KP(TEXT('#'), TEXT("#planks")) });
	B.Shaped(TEXT("barrel"), 1, { TEXT("#_#"), TEXT("# #"), TEXT("#_#") }, { KP(TEXT('#'), TEXT("#planks")), KP(TEXT('_'), TEXT("#wooden_slabs")) });
	B.Shaped(TEXT("bowl"), 4, { TEXT("# #"), TEXT(" # ") }, { KP(TEXT('#'), TEXT("#planks")) });
	B.Shaped(TEXT("ladder"), 3, { TEXT("| |"), TEXT("|||"), TEXT("| |") }, { KP(TEXT('|'), TEXT("stick")) });
	B.Shaped(TEXT("bookshelf"), 1, { TEXT("###"), TEXT("BBB"), TEXT("###") }, { KP(TEXT('#'), TEXT("#planks")), KP(TEXT('B'), TEXT("book")) });
	B.Shaped(TEXT("chiseled_bookshelf"), 1, { TEXT("###"), TEXT("___"), TEXT("###") }, { KP(TEXT('#'), TEXT("#planks")), KP(TEXT('_'), TEXT("#wooden_slabs")) });
	B.Shaped(TEXT("composter"), 1, { TEXT("_ _"), TEXT("_ _"), TEXT("___") }, { KP(TEXT('_'), TEXT("#wooden_slabs")) });
	B.Shaped(TEXT("lectern"), 1, { TEXT("___"), TEXT(" B "), TEXT(" _ ") }, { KP(TEXT('_'), TEXT("#wooden_slabs")), KP(TEXT('B'), TEXT("bookshelf")) });
	B.Shaped(TEXT("jukebox"), 1, { TEXT("###"), TEXT("#D#"), TEXT("###") }, { KP(TEXT('#'), TEXT("#planks")), KP(TEXT('D'), TEXT("diamond")) });
	B.Shaped(TEXT("note_block"), 1, { TEXT("###"), TEXT("#R#"), TEXT("###") }, { KP(TEXT('#'), TEXT("#planks")), KP(TEXT('R'), TEXT("redstone")) }, RB::Redstone);
	B.Shaped(TEXT("loom"), 1, { TEXT("SS"), TEXT("##") }, { KP(TEXT('S'), TEXT("string")), KP(TEXT('#'), TEXT("#planks")) });
	B.Shaped(TEXT("fletching_table"), 1, { TEXT("FF"), TEXT("##"), TEXT("##") }, { KP(TEXT('F'), TEXT("flint")), KP(TEXT('#'), TEXT("#planks")) });
	B.Shaped(TEXT("cartography_table"), 1, { TEXT("PP"), TEXT("##"), TEXT("##") }, { KP(TEXT('P'), TEXT("paper")), KP(TEXT('#'), TEXT("#planks")) });
	B.Shaped(TEXT("smithing_table"), 1, { TEXT("II"), TEXT("##"), TEXT("##") }, { KP(TEXT('I'), TEXT("iron_ingot")), KP(TEXT('#'), TEXT("#planks")) });
	B.Shaped(TEXT("scaffolding"), 6, { TEXT("I~I"), TEXT("I I"), TEXT("I I") }, { KP(TEXT('I'), TEXT("bamboo")), KP(TEXT('~'), TEXT("string")) });
	B.Shaped(TEXT("shelf"), 3, { TEXT("###"), TEXT("   "), TEXT("###") }, { KP(TEXT('#'), TEXT("#planks")) });
	B.Shaped(TEXT("campfire"), 1, { TEXT(" | "), TEXT("|C|"), TEXT("LLL") }, { KP(TEXT('|'), TEXT("stick")), KP(TEXT('C'), TEXT("#coals")), KP(TEXT('L'), TEXT("#logs")) });
	B.Shaped(TEXT("soul_campfire"), 1, { TEXT(" | "), TEXT("|S|"), TEXT("LLL") }, { KP(TEXT('|'), TEXT("stick")), KP(TEXT('S'), TEXT("#soul_fire_base_blocks")), KP(TEXT('L'), TEXT("#logs")) });

	// ================================================================ stone & building
	const TCHAR* StoneBases[] = {
		TEXT("stone"), TEXT("cobblestone"), TEXT("mossy_cobblestone"), TEXT("stone_bricks"), TEXT("mossy_stone_bricks"), TEXT("granite"), TEXT("polished_granite"),
		TEXT("diorite"), TEXT("polished_diorite"), TEXT("andesite"), TEXT("polished_andesite"), TEXT("cobbled_deepslate"), TEXT("polished_deepslate"),
		TEXT("deepslate_bricks"), TEXT("deepslate_tiles"), TEXT("tuff"), TEXT("polished_tuff"), TEXT("tuff_bricks"), TEXT("bricks"), TEXT("mud_bricks"),
		TEXT("sandstone"), TEXT("smooth_sandstone"), TEXT("red_sandstone"), TEXT("smooth_red_sandstone"), TEXT("prismarine"), TEXT("prismarine_bricks"),
		TEXT("dark_prismarine"), TEXT("smooth_quartz"), TEXT("sulfur"), TEXT("polished_sulfur"), TEXT("sulfur_bricks"), TEXT("cinnabar"),
		TEXT("polished_cinnabar"), TEXT("cinnabar_bricks"), TEXT("nether_bricks"), TEXT("red_nether_bricks"), TEXT("blackstone"), TEXT("polished_blackstone"),
		TEXT("polished_blackstone_bricks"), TEXT("end_stone_bricks"), TEXT("purpur_block"), TEXT("cut_copper"), TEXT("exposed_cut_copper"),
		TEXT("weathered_cut_copper"), TEXT("oxidized_cut_copper") };
	for (const TCHAR* S : StoneBases) StoneFamily(B, S);
	B.Shaped(TEXT("quartz_stairs"), 4, { TEXT("#  "), TEXT("## "), TEXT("###") }, { KP(TEXT('#'), TEXT("quartz_block")) }, RB::Building);
	B.Shaped(TEXT("quartz_slab"), 6, { TEXT("###") }, { KP(TEXT('#'), TEXT("quartz_block")) }, RB::Building);
	B.Stonecut(TEXT("quartz_block"), TEXT("quartz_stairs")); B.Stonecut(TEXT("quartz_block"), TEXT("quartz_slab"), 2);
	B.Shaped(TEXT("smooth_stone_slab"), 6, { TEXT("###") }, { KP(TEXT('#'), TEXT("smooth_stone")) }, RB::Building);
	B.Shaped(TEXT("cut_sandstone_slab"), 6, { TEXT("###") }, { KP(TEXT('#'), TEXT("cut_sandstone")) }, RB::Building);
	B.Shaped(TEXT("cut_red_sandstone_slab"), 6, { TEXT("###") }, { KP(TEXT('#'), TEXT("cut_red_sandstone")) }, RB::Building);
	B.Shaped(TEXT("resin_brick_stairs"), 4, { TEXT("#  "), TEXT("## "), TEXT("###") }, { KP(TEXT('#'), TEXT("resin_bricks")) }, RB::Building);
	B.Shaped(TEXT("resin_brick_slab"), 6, { TEXT("###") }, { KP(TEXT('#'), TEXT("resin_bricks")) }, RB::Building);
	B.Shaped(TEXT("resin_brick_wall"), 6, { TEXT("###"), TEXT("###") }, { KP(TEXT('#'), TEXT("resin_bricks")) }, RB::Building);
	B.Shaped(TEXT("nether_brick_fence"), 6, { TEXT("#N#"), TEXT("#N#") }, { KP(TEXT('#'), TEXT("nether_bricks")), KP(TEXT('N'), TEXT("nether_brick")) }, RB::Building);

	Square(B, TEXT("stone"), TEXT("stone_bricks"));
	Square(B, TEXT("granite"), TEXT("polished_granite"));
	Square(B, TEXT("diorite"), TEXT("polished_diorite"));
	Square(B, TEXT("andesite"), TEXT("polished_andesite"));
	Square(B, TEXT("cobbled_deepslate"), TEXT("polished_deepslate"));
	Square(B, TEXT("polished_deepslate"), TEXT("deepslate_bricks"));
	Square(B, TEXT("deepslate_bricks"), TEXT("deepslate_tiles"));
	Square(B, TEXT("tuff"), TEXT("polished_tuff"));
	Square(B, TEXT("polished_tuff"), TEXT("tuff_bricks"));
	Square(B, TEXT("basalt"), TEXT("polished_basalt"));
	Square(B, TEXT("blackstone"), TEXT("polished_blackstone"));
	Square(B, TEXT("polished_blackstone"), TEXT("polished_blackstone_bricks"));
	Square(B, TEXT("end_stone"), TEXT("end_stone_bricks"));
	Square(B, TEXT("quartz_block"), TEXT("quartz_bricks"));
	Square(B, TEXT("sulfur"), TEXT("polished_sulfur"));
	Square(B, TEXT("polished_sulfur"), TEXT("sulfur_bricks"));
	Square(B, TEXT("cinnabar"), TEXT("polished_cinnabar"));
	Square(B, TEXT("polished_cinnabar"), TEXT("cinnabar_bricks"));
	Square(B, TEXT("packed_mud"), TEXT("mud_bricks"));
	Square(B, TEXT("sand"), TEXT("sandstone"), 1);
	Square(B, TEXT("red_sand"), TEXT("red_sandstone"), 1);
	Square(B, TEXT("sandstone"), TEXT("cut_sandstone"));
	Square(B, TEXT("red_sandstone"), TEXT("cut_red_sandstone"));
	Square(B, TEXT("copper_block"), TEXT("cut_copper"));
	Square(B, TEXT("exposed_copper"), TEXT("exposed_cut_copper"));
	Square(B, TEXT("weathered_copper"), TEXT("weathered_cut_copper"));
	Square(B, TEXT("oxidized_copper"), TEXT("oxidized_cut_copper"));
	Square(B, TEXT("brick"), TEXT("bricks"), 1);
	Square(B, TEXT("nether_brick"), TEXT("nether_bricks"), 1);
	Square(B, TEXT("resin_brick"), TEXT("resin_bricks"), 1);
	Square(B, TEXT("clay_ball"), TEXT("clay"), 1);
	Square(B, TEXT("snowball"), TEXT("snow_block"), 1);
	Square(B, TEXT("quartz"), TEXT("quartz_block"), 1);
	Square(B, TEXT("amethyst_shard"), TEXT("amethyst_block"), 1);
	Square(B, TEXT("glowstone_dust"), TEXT("glowstone"), 1);
	Square(B, TEXT("prismarine_shard"), TEXT("prismarine"), 1);
	Square(B, TEXT("honey_bottle"), TEXT("honey_block"), 1);
	Square(B, TEXT("honeycomb"), TEXT("honeycomb_block"), 1);
	Square(B, TEXT("magma_cream"), TEXT("magma_block"), 1);
	Square(B, TEXT("dripstone_block"), TEXT("dripstone_block"), 1);
	B.Shaped(TEXT("pointed_dripstone"), 1, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("dripstone_block")) }); // not vanilla-exact, convenience
	B.Shaped(TEXT("dripstone_block"), 1, { TEXT("##"), TEXT("##") }, { KP(TEXT('#'), TEXT("pointed_dripstone")) });
	B.Shaped(TEXT("prismarine_bricks"), 1, { TEXT("###"), TEXT("###"), TEXT("###") }, { KP(TEXT('#'), TEXT("prismarine_shard")) });
	B.Shaped(TEXT("dark_prismarine"), 1, { TEXT("###"), TEXT("#I#"), TEXT("###") }, { KP(TEXT('#'), TEXT("prismarine_shard")), KP(TEXT('I'), TEXT("black_dye")) });
	B.Shaped(TEXT("sea_lantern"), 1, { TEXT("#C#"), TEXT("CCC"), TEXT("#C#") }, { KP(TEXT('#'), TEXT("prismarine_shard")), KP(TEXT('C'), TEXT("prismarine_crystals")) });
	B.Shaped(TEXT("red_nether_bricks"), 1, { TEXT("NW"), TEXT("WN") }, { KP(TEXT('N'), TEXT("nether_brick")), KP(TEXT('W'), TEXT("nether_wart")) });
	B.Shaped(TEXT("nether_wart_block"), 1, { TEXT("###"), TEXT("###"), TEXT("###") }, { KP(TEXT('#'), TEXT("nether_wart")) });
	B.Shaped(TEXT("purpur_block"), 4, { TEXT("##"), TEXT("##") }, { KP(TEXT('#'), TEXT("popped_chorus_fruit")) });
	B.Shaped(TEXT("purpur_pillar"), 1, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("purpur_slab")) });
	B.Shaped(TEXT("quartz_pillar"), 2, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("quartz_block")) });
	B.Shaped(TEXT("chiseled_quartz_block"), 1, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("quartz_slab")) });
	B.Shaped(TEXT("chiseled_stone_bricks"), 1, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("stone_brick_slab")) });
	B.Shaped(TEXT("chiseled_sandstone"), 1, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("sandstone_slab")) });
	B.Shaped(TEXT("chiseled_red_sandstone"), 1, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("red_sandstone_slab")) });
	B.Shaped(TEXT("chiseled_polished_blackstone"), 1, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("polished_blackstone_slab")) });
	B.Shaped(TEXT("chiseled_nether_bricks"), 1, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("nether_brick_slab")) });
	B.Shaped(TEXT("chiseled_deepslate"), 1, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("cobbled_deepslate_slab")) });
	B.Shaped(TEXT("chiseled_tuff"), 1, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("tuff_slab")) });
	B.Shaped(TEXT("chiseled_copper"), 1, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("cut_copper_slab")) });
	B.Shaped(TEXT("chiseled_resin_bricks"), 1, { TEXT("#"), TEXT("#") }, { KP(TEXT('#'), TEXT("resin_brick_slab")) });
	B.Shapeless(TEXT("mossy_cobblestone"), 1, { TEXT("cobblestone"), TEXT("vine") });
	B.Shapeless(TEXT("mossy_cobblestone"), 1, { TEXT("cobblestone"), TEXT("moss_block") });
	B.Shapeless(TEXT("mossy_stone_bricks"), 1, { TEXT("stone_bricks"), TEXT("vine") });
	B.Shapeless(TEXT("mossy_stone_bricks"), 1, { TEXT("stone_bricks"), TEXT("moss_block") });
	B.Shapeless(TEXT("packed_mud"), 1, { TEXT("mud"), TEXT("wheat") });
	B.Shapeless(TEXT("mud"), 1, { TEXT("dirt"), TEXT("potion") });
	B.Shaped(TEXT("coarse_dirt"), 4, { TEXT("DG"), TEXT("GD") }, { KP(TEXT('D'), TEXT("dirt")), KP(TEXT('G'), TEXT("gravel")) });
	B.Shaped(TEXT("moss_carpet"), 3, { TEXT("##") }, { KP(TEXT('#'), TEXT("moss_block")) });
	B.Shaped(TEXT("pale_moss_carpet"), 3, { TEXT("##") }, { KP(TEXT('#'), TEXT("pale_moss_block")) });
	B.Shapeless(TEXT("resin_clump"), 9, { TEXT("resin_block") });
	B.Shaped(TEXT("resin_block"), 1, { TEXT("###"), TEXT("###"), TEXT("###") }, { KP(TEXT('#'), TEXT("resin_clump")) });
	B.Shaped(TEXT("glass_pane"), 16, { TEXT("###"), TEXT("###") }, { KP(TEXT('#'), TEXT("glass")) });
	B.Shaped(TEXT("tinted_glass"), 2, { TEXT(" A "), TEXT("AGA"), TEXT(" A ") }, { KP(TEXT('A'), TEXT("amethyst_shard")), KP(TEXT('G'), TEXT("glass")) });
	B.Shaped(TEXT("iron_bars"), 16, { TEXT("###"), TEXT("###") }, { KP(TEXT('#'), TEXT("iron_ingot")) });
	B.Shaped(TEXT("copper_bars"), 16, { TEXT("###"), TEXT("###") }, { KP(TEXT('#'), TEXT("copper_ingot")) });
	B.Shaped(TEXT("chain"), 1, { TEXT("N"), TEXT("I"), TEXT("N") }, { KP(TEXT('N'), TEXT("iron_nugget")), KP(TEXT('I'), TEXT("iron_ingot")) });
	B.Shaped(TEXT("copper_chain"), 1, { TEXT("N"), TEXT("I"), TEXT("N") }, { KP(TEXT('N'), TEXT("copper_nugget")), KP(TEXT('I'), TEXT("copper_ingot")) });
	B.Shaped(TEXT("hay_block"), 1, { TEXT("###"), TEXT("###"), TEXT("###") }, { KP(TEXT('#'), TEXT("wheat")) });
	B.Shapeless(TEXT("wheat"), 9, { TEXT("hay_block") });
	Compact(B, TEXT("dried_kelp"), TEXT("dried_kelp_block"));
	Compact(B, TEXT("bone_meal"), TEXT("bone_block"));
	Compact(B, TEXT("slime_ball"), TEXT("slime_block"));
	B.Shapeless(TEXT("honey_bottle"), 4, { TEXT("honey_block"), TEXT("glass_bottle"), TEXT("glass_bottle"), TEXT("glass_bottle"), TEXT("glass_bottle") });
	B.Shaped(TEXT("snow"), 6, { TEXT("###") }, { KP(TEXT('#'), TEXT("snow_block")) });
	B.Shaped(TEXT("packed_ice"), 1, { TEXT("###"), TEXT("###"), TEXT("###") }, { KP(TEXT('#'), TEXT("ice")) });
	B.Shaped(TEXT("blue_ice"), 1, { TEXT("###"), TEXT("###"), TEXT("###") }, { KP(TEXT('#'), TEXT("packed_ice")) });
	B.Shaped(TEXT("melon"), 1, { TEXT("###"), TEXT("###"), TEXT("###") }, { KP(TEXT('#'), TEXT("melon_slice")) });

	// ================================================================ minerals
	Compact(B, TEXT("iron_ingot"), TEXT("iron_block"));
	Compact(B, TEXT("gold_ingot"), TEXT("gold_block"));
	Compact(B, TEXT("copper_ingot"), TEXT("copper_block"));
	Compact(B, TEXT("diamond"), TEXT("diamond_block"));
	Compact(B, TEXT("emerald"), TEXT("emerald_block"));
	Compact(B, TEXT("lapis_lazuli"), TEXT("lapis_block"));
	Compact(B, TEXT("redstone"), TEXT("redstone_block"));
	Compact(B, TEXT("coal"), TEXT("coal_block"));
	Compact(B, TEXT("netherite_ingot"), TEXT("netherite_block"));
	Compact(B, TEXT("raw_iron"), TEXT("raw_iron_block"));
	Compact(B, TEXT("raw_gold"), TEXT("raw_gold_block"));
	Compact(B, TEXT("raw_copper"), TEXT("raw_copper_block"));
	Compact(B, TEXT("iron_nugget"), TEXT("iron_ingot"));
	Compact(B, TEXT("gold_nugget"), TEXT("gold_ingot"));
	Compact(B, TEXT("copper_nugget"), TEXT("copper_ingot"));
	B.Shapeless(TEXT("netherite_ingot"), 1, { TEXT("netherite_scrap"), TEXT("netherite_scrap"), TEXT("netherite_scrap"), TEXT("netherite_scrap"),
		TEXT("gold_ingot"), TEXT("gold_ingot"), TEXT("gold_ingot"), TEXT("gold_ingot") });
	B.Shapeless(TEXT("bone_meal"), 3, { TEXT("bone") });
	B.Shaped(TEXT("netherite_upgrade_smithing_template"), 2, { TEXT("DTD"), TEXT("DND"), TEXT("DDD") }, { KP(TEXT('D'), TEXT("diamond")), KP(TEXT('T'), TEXT("netherite_upgrade_smithing_template")), KP(TEXT('N'), TEXT("netherrack")) });

	// ================================================================ tools, weapons, armour
	struct FMat { const TCHAR* Prefix; const TCHAR* Material; };
	const FMat ToolMats[] = { { TEXT("wooden"), TEXT("#planks") }, { TEXT("stone"), TEXT("#stone_tool_materials") }, { TEXT("copper"), TEXT("copper_ingot") },
		{ TEXT("iron"), TEXT("iron_ingot") }, { TEXT("golden"), TEXT("gold_ingot") }, { TEXT("diamond"), TEXT("diamond") } };
	for (const FMat& M : ToolMats)
	{
		const FString P = M.Prefix;
		B.Shaped(*(P + TEXT("_pickaxe")), 1, { TEXT("###"), TEXT(" | "), TEXT(" | ") }, { KP(TEXT('#'), M.Material), KP(TEXT('|'), TEXT("stick")) }, RB::Equipment);
		B.Shaped(*(P + TEXT("_axe")), 1, { TEXT("##"), TEXT("#|"), TEXT(" |") }, { KP(TEXT('#'), M.Material), KP(TEXT('|'), TEXT("stick")) }, RB::Equipment);
		B.Shaped(*(P + TEXT("_shovel")), 1, { TEXT("#"), TEXT("|"), TEXT("|") }, { KP(TEXT('#'), M.Material), KP(TEXT('|'), TEXT("stick")) }, RB::Equipment);
		B.Shaped(*(P + TEXT("_hoe")), 1, { TEXT("##"), TEXT(" |"), TEXT(" |") }, { KP(TEXT('#'), M.Material), KP(TEXT('|'), TEXT("stick")) }, RB::Equipment);
		B.Shaped(*(P + TEXT("_sword")), 1, { TEXT("#"), TEXT("#"), TEXT("|") }, { KP(TEXT('#'), M.Material), KP(TEXT('|'), TEXT("stick")) }, RB::Equipment);
		B.Shaped(*(P + TEXT("_spear")), 1, { TEXT("  #"), TEXT(" | "), TEXT("|  ") }, { KP(TEXT('#'), M.Material), KP(TEXT('|'), TEXT("stick")) }, RB::Equipment);
	}
	const FMat ArmorMats[] = { { TEXT("leather"), TEXT("leather") }, { TEXT("copper"), TEXT("copper_ingot") }, { TEXT("iron"), TEXT("iron_ingot") },
		{ TEXT("golden"), TEXT("gold_ingot") }, { TEXT("diamond"), TEXT("diamond") } };
	for (const FMat& M : ArmorMats)
	{
		const FString P = M.Prefix;
		B.Shaped(*(P + TEXT("_helmet")), 1, { TEXT("###"), TEXT("# #") }, { KP(TEXT('#'), M.Material) }, RB::Equipment);
		B.Shaped(*(P + TEXT("_chestplate")), 1, { TEXT("# #"), TEXT("###"), TEXT("###") }, { KP(TEXT('#'), M.Material) }, RB::Equipment);
		B.Shaped(*(P + TEXT("_leggings")), 1, { TEXT("###"), TEXT("# #"), TEXT("# #") }, { KP(TEXT('#'), M.Material) }, RB::Equipment);
		B.Shaped(*(P + TEXT("_boots")), 1, { TEXT("# #"), TEXT("# #") }, { KP(TEXT('#'), M.Material) }, RB::Equipment);
	}
	B.Shaped(TEXT("turtle_helmet"), 1, { TEXT("###"), TEXT("# #") }, { KP(TEXT('#'), TEXT("turtle_scute")) }, RB::Equipment);
	B.Shaped(TEXT("wolf_armor"), 1, { TEXT("#  "), TEXT("###"), TEXT("# #") }, { KP(TEXT('#'), TEXT("armadillo_scute")) }, RB::Equipment);
	B.Shaped(TEXT("leather_horse_armor"), 1, { TEXT("# #"), TEXT("###"), TEXT("# #") }, { KP(TEXT('#'), TEXT("leather")) }, RB::Equipment);
	B.Shaped(TEXT("bow"), 1, { TEXT(" |S"), TEXT("| S"), TEXT(" |S") }, { KP(TEXT('|'), TEXT("stick")), KP(TEXT('S'), TEXT("string")) }, RB::Equipment);
	B.Shaped(TEXT("crossbow"), 1, { TEXT("|I|"), TEXT("S~S"), TEXT(" | ") }, { KP(TEXT('|'), TEXT("stick")), KP(TEXT('I'), TEXT("iron_ingot")), KP(TEXT('S'), TEXT("string")), KP(TEXT('~'), TEXT("tripwire_hook")) }, RB::Equipment);
	B.Shaped(TEXT("crossbow"), 1, { TEXT("|I|"), TEXT("S S"), TEXT(" | ") }, { KP(TEXT('|'), TEXT("stick")), KP(TEXT('I'), TEXT("iron_ingot")), KP(TEXT('S'), TEXT("string")) }, RB::Equipment);
	B.Shaped(TEXT("arrow"), 4, { TEXT("F"), TEXT("|"), TEXT("E") }, { KP(TEXT('F'), TEXT("flint")), KP(TEXT('|'), TEXT("stick")), KP(TEXT('E'), TEXT("feather")) }, RB::Equipment);
	B.Shaped(TEXT("spectral_arrow"), 2, { TEXT(" G "), TEXT("GAG"), TEXT(" G ") }, { KP(TEXT('G'), TEXT("glowstone_dust")), KP(TEXT('A'), TEXT("arrow")) }, RB::Equipment);
	B.Shaped(TEXT("shield"), 1, { TEXT("#I#"), TEXT("###"), TEXT(" # ") }, { KP(TEXT('#'), TEXT("#planks")), KP(TEXT('I'), TEXT("iron_ingot")) }, RB::Equipment);
	B.Shaped(TEXT("mace"), 1, { TEXT("H"), TEXT("R") }, { KP(TEXT('H'), TEXT("heavy_core")), KP(TEXT('R'), TEXT("breeze_rod")) }, RB::Equipment);
	B.Shaped(TEXT("fishing_rod"), 1, { TEXT("  |"), TEXT(" |S"), TEXT("| S") }, { KP(TEXT('|'), TEXT("stick")), KP(TEXT('S'), TEXT("string")) }, RB::Equipment);
	B.Shaped(TEXT("carrot_on_a_stick"), 1, { TEXT("R "), TEXT(" C") }, { KP(TEXT('R'), TEXT("fishing_rod")), KP(TEXT('C'), TEXT("carrot")) }, RB::Equipment);
	B.Shaped(TEXT("warped_fungus_on_a_stick"), 1, { TEXT("R "), TEXT(" F") }, { KP(TEXT('R'), TEXT("fishing_rod")), KP(TEXT('F'), TEXT("warped_fungus")) }, RB::Equipment);
	B.Shaped(TEXT("shears"), 1, { TEXT(" I"), TEXT("I ") }, { KP(TEXT('I'), TEXT("iron_ingot")) }, RB::Equipment);
	B.Shapeless(TEXT("flint_and_steel"), 1, { TEXT("iron_ingot"), TEXT("flint") }, RB::Equipment);
	B.Shaped(TEXT("brush"), 1, { TEXT("F"), TEXT("C"), TEXT("|") }, { KP(TEXT('F'), TEXT("feather")), KP(TEXT('C'), TEXT("copper_ingot")), KP(TEXT('|'), TEXT("stick")) }, RB::Equipment);
	B.Shaped(TEXT("spyglass"), 1, { TEXT("A"), TEXT("C"), TEXT("C") }, { KP(TEXT('A'), TEXT("amethyst_shard")), KP(TEXT('C'), TEXT("copper_ingot")) }, RB::Equipment);
	B.Shaped(TEXT("compass"), 1, { TEXT(" I "), TEXT("IRI"), TEXT(" I ") }, { KP(TEXT('I'), TEXT("iron_ingot")), KP(TEXT('R'), TEXT("redstone")) }, RB::Equipment);
	B.Shaped(TEXT("recovery_compass"), 1, { TEXT("EEE"), TEXT("ECE"), TEXT("EEE") }, { KP(TEXT('E'), TEXT("echo_shard")), KP(TEXT('C'), TEXT("compass")) }, RB::Equipment);
	B.Shaped(TEXT("clock"), 1, { TEXT(" G "), TEXT("GRG"), TEXT(" G ") }, { KP(TEXT('G'), TEXT("gold_ingot")), KP(TEXT('R'), TEXT("redstone")) }, RB::Equipment);
	B.Shaped(TEXT("map"), 1, { TEXT("PPP"), TEXT("PCP"), TEXT("PPP") }, { KP(TEXT('P'), TEXT("paper")), KP(TEXT('C'), TEXT("compass")) }, RB::Equipment);
	B.Shaped(TEXT("bucket"), 1, { TEXT("I I"), TEXT(" I ") }, { KP(TEXT('I'), TEXT("iron_ingot")) }, RB::Equipment);
	B.Shaped(TEXT("lead"), 2, { TEXT("SS "), TEXT("SB "), TEXT("  S") }, { KP(TEXT('S'), TEXT("string")), KP(TEXT('B'), TEXT("slime_ball")) }, RB::Equipment);
	B.Shaped(TEXT("saddle"), 1, { TEXT(" L "), TEXT("LIL") }, { KP(TEXT('L'), TEXT("leather")), KP(TEXT('I'), TEXT("iron_ingot")) }, RB::Equipment);
	B.Shaped(TEXT("bundle"), 1, { TEXT("S"), TEXT("L") }, { KP(TEXT('S'), TEXT("string")), KP(TEXT('L'), TEXT("leather")) }, RB::Equipment);
	B.Shaped(TEXT("armor_stand"), 1, { TEXT("|||"), TEXT(" | "), TEXT("|_|") }, { KP(TEXT('|'), TEXT("stick")), KP(TEXT('_'), TEXT("smooth_stone_slab")) });
	B.Shaped(TEXT("item_frame"), 1, { TEXT("|||"), TEXT("|L|"), TEXT("|||") }, { KP(TEXT('|'), TEXT("stick")), KP(TEXT('L'), TEXT("leather")) });
	B.Shaped(TEXT("painting"), 1, { TEXT("|||"), TEXT("|W|"), TEXT("|||") }, { KP(TEXT('|'), TEXT("stick")), KP(TEXT('W'), TEXT("#wool")) });
	B.Shaped(TEXT("flower_pot"), 1, { TEXT("B B"), TEXT(" B ") }, { KP(TEXT('B'), TEXT("brick")) });
	B.Shaped(TEXT("decorated_pot"), 1, { TEXT(" B "), TEXT("B B"), TEXT(" B ") }, { KP(TEXT('B'), TEXT("brick")) });
	B.Shaped(TEXT("glass_bottle"), 3, { TEXT("# #"), TEXT(" # ") }, { KP(TEXT('#'), TEXT("glass")) });
	B.Shaped(TEXT("paper"), 3, { TEXT("###") }, { KP(TEXT('#'), TEXT("sugar_cane")) });
	B.Shapeless(TEXT("book"), 1, { TEXT("paper"), TEXT("paper"), TEXT("paper"), TEXT("leather") });
	B.Shapeless(TEXT("writable_book"), 1, { TEXT("book"), TEXT("ink_sac"), TEXT("feather") });
	B.Shapeless(TEXT("sugar"), 1, { TEXT("sugar_cane") });
	B.Shapeless(TEXT("sugar"), 1, { TEXT("honey_bottle") });
	B.Shapeless(TEXT("fire_charge"), 3, { TEXT("gunpowder"), TEXT("blaze_powder"), TEXT("#coals") });
	B.Shapeless(TEXT("blaze_powder"), 2, { TEXT("blaze_rod") });
	B.Shapeless(TEXT("wind_charge"), 4, { TEXT("breeze_rod") });
	B.Shapeless(TEXT("magma_cream"), 1, { TEXT("blaze_powder"), TEXT("slime_ball") });
	B.Shapeless(TEXT("ender_eye"), 1, { TEXT("ender_pearl"), TEXT("blaze_powder") });
	B.Shapeless(TEXT("fermented_spider_eye"), 1, { TEXT("spider_eye"), TEXT("brown_mushroom"), TEXT("sugar") });
	B.Shaped(TEXT("glistering_melon_slice"), 1, { TEXT("NNN"), TEXT("NMN"), TEXT("NNN") }, { KP(TEXT('N'), TEXT("gold_nugget")), KP(TEXT('M'), TEXT("melon_slice")) });
	B.Shaped(TEXT("end_crystal"), 1, { TEXT("GGG"), TEXT("GEG"), TEXT("GTG") }, { KP(TEXT('G'), TEXT("glass")), KP(TEXT('E'), TEXT("ender_eye")), KP(TEXT('T'), TEXT("ghast_tear")) });
	B.Shaped(TEXT("ender_chest"), 1, { TEXT("OOO"), TEXT("OEO"), TEXT("OOO") }, { KP(TEXT('O'), TEXT("obsidian")), KP(TEXT('E'), TEXT("ender_eye")) });
	B.Shaped(TEXT("beacon"), 1, { TEXT("GGG"), TEXT("GSG"), TEXT("OOO") }, { KP(TEXT('G'), TEXT("glass")), KP(TEXT('S'), TEXT("nether_star")), KP(TEXT('O'), TEXT("obsidian")) });
	B.Shaped(TEXT("conduit"), 1, { TEXT("NNN"), TEXT("NHN"), TEXT("NNN") }, { KP(TEXT('N'), TEXT("nautilus_shell")), KP(TEXT('H'), TEXT("heart_of_the_sea")) });
	B.Shaped(TEXT("respawn_anchor"), 1, { TEXT("CCC"), TEXT("GGG"), TEXT("CCC") }, { KP(TEXT('C'), TEXT("crying_obsidian")), KP(TEXT('G'), TEXT("glowstone")) });
	B.Shaped(TEXT("lodestone"), 1, { TEXT("SSS"), TEXT("SNS"), TEXT("SSS") }, { KP(TEXT('S'), TEXT("chiseled_stone_bricks")), KP(TEXT('N'), TEXT("netherite_ingot")) });
	B.Shaped(TEXT("totem_of_undying"), 1, { TEXT(" E "), TEXT("GEG"), TEXT(" G ") }, { KP(TEXT('E'), TEXT("emerald")), KP(TEXT('G'), TEXT("gold_ingot")) }); // convenience, evoker drop in vanilla
	B.Shaped(TEXT("torch"), 4, { TEXT("C"), TEXT("|") }, { KP(TEXT('C'), TEXT("#coals")), KP(TEXT('|'), TEXT("stick")) });
	B.Shaped(TEXT("soul_torch"), 4, { TEXT("C"), TEXT("|"), TEXT("S") }, { KP(TEXT('C'), TEXT("#coals")), KP(TEXT('|'), TEXT("stick")), KP(TEXT('S'), TEXT("#soul_fire_base_blocks")) });
	B.Shaped(TEXT("copper_torch"), 4, { TEXT("N"), TEXT("C"), TEXT("|") }, { KP(TEXT('N'), TEXT("copper_nugget")), KP(TEXT('C'), TEXT("#coals")), KP(TEXT('|'), TEXT("stick")) });
	B.Shaped(TEXT("lantern"), 1, { TEXT("NNN"), TEXT("NTN"), TEXT("NNN") }, { KP(TEXT('N'), TEXT("iron_nugget")), KP(TEXT('T'), TEXT("torch")) });
	B.Shaped(TEXT("soul_lantern"), 1, { TEXT("NNN"), TEXT("NTN"), TEXT("NNN") }, { KP(TEXT('N'), TEXT("iron_nugget")), KP(TEXT('T'), TEXT("soul_torch")) });
	B.Shaped(TEXT("copper_lantern"), 1, { TEXT("NNN"), TEXT("NTN"), TEXT("NNN") }, { KP(TEXT('N'), TEXT("copper_nugget")), KP(TEXT('T'), TEXT("copper_torch")) });
	B.Shaped(TEXT("jack_o_lantern"), 1, { TEXT("P"), TEXT("T") }, { KP(TEXT('P'), TEXT("carved_pumpkin")), KP(TEXT('T'), TEXT("torch")) });
	B.Shaped(TEXT("end_rod"), 4, { TEXT("B"), TEXT("P") }, { KP(TEXT('B'), TEXT("blaze_rod")), KP(TEXT('P'), TEXT("popped_chorus_fruit")) });
	B.Shaped(TEXT("candle"), 1, { TEXT("S"), TEXT("H") }, { KP(TEXT('S'), TEXT("string")), KP(TEXT('H'), TEXT("honeycomb")) });

	// workstations & containers
	B.Shaped(TEXT("furnace"), 1, { TEXT("###"), TEXT("# #"), TEXT("###") }, { KP(TEXT('#'), TEXT("#stone_crafting_materials")) });
	B.Shaped(TEXT("blast_furnace"), 1, { TEXT("III"), TEXT("IFI"), TEXT("SSS") }, { KP(TEXT('I'), TEXT("iron_ingot")), KP(TEXT('F'), TEXT("furnace")), KP(TEXT('S'), TEXT("smooth_stone")) });
	B.Shaped(TEXT("smoker"), 1, { TEXT(" L "), TEXT("LFL"), TEXT(" L ") }, { KP(TEXT('L'), TEXT("#logs")), KP(TEXT('F'), TEXT("furnace")) });
	B.Shaped(TEXT("anvil"), 1, { TEXT("BBB"), TEXT(" I "), TEXT("III") }, { KP(TEXT('B'), TEXT("iron_block")), KP(TEXT('I'), TEXT("iron_ingot")) });
	B.Shaped(TEXT("enchanting_table"), 1, { TEXT(" B "), TEXT("DOD"), TEXT("OOO") }, { KP(TEXT('B'), TEXT("book")), KP(TEXT('D'), TEXT("diamond")), KP(TEXT('O'), TEXT("obsidian")) });
	B.Shaped(TEXT("brewing_stand"), 1, { TEXT(" R "), TEXT("###") }, { KP(TEXT('R'), TEXT("blaze_rod")), KP(TEXT('#'), TEXT("#stone_crafting_materials")) });
	B.Shaped(TEXT("cauldron"), 1, { TEXT("I I"), TEXT("I I"), TEXT("III") }, { KP(TEXT('I'), TEXT("iron_ingot")) });
	B.Shaped(TEXT("grindstone"), 1, { TEXT("|_|"), TEXT("# #") }, { KP(TEXT('|'), TEXT("stick")), KP(TEXT('_'), TEXT("stone_slab")), KP(TEXT('#'), TEXT("#planks")) });
	B.Shaped(TEXT("stonecutter"), 1, { TEXT(" I "), TEXT("SSS") }, { KP(TEXT('I'), TEXT("iron_ingot")), KP(TEXT('S'), TEXT("stone")) });
	B.Shaped(TEXT("hopper"), 1, { TEXT("I I"), TEXT("ICI"), TEXT(" I ") }, { KP(TEXT('I'), TEXT("iron_ingot")), KP(TEXT('C'), TEXT("chest")) });
	B.Shapeless(TEXT("trapped_chest"), 1, { TEXT("chest"), TEXT("tripwire_hook") });
	B.Shaped(TEXT("copper_chest"), 1, { TEXT("CCC"), TEXT("C C"), TEXT("CCC") }, { KP(TEXT('C'), TEXT("copper_ingot")) });
	B.Shaped(TEXT("shulker_box"), 1, { TEXT("S"), TEXT("C"), TEXT("S") }, { KP(TEXT('S'), TEXT("shulker_shell")), KP(TEXT('C'), TEXT("chest")) });
	B.Shaped(TEXT("crafter"), 1, { TEXT("III"), TEXT("ICI"), TEXT("RDR") }, { KP(TEXT('I'), TEXT("iron_ingot")), KP(TEXT('C'), TEXT("crafting_table")), KP(TEXT('R'), TEXT("redstone")), KP(TEXT('D'), TEXT("dropper")) });
	B.Shaped(TEXT("beehive"), 1, { TEXT("###"), TEXT("HHH"), TEXT("###") }, { KP(TEXT('#'), TEXT("#planks")), KP(TEXT('H'), TEXT("honeycomb")) });

	// ================================================================ redstone
	B.Shaped(TEXT("redstone_torch"), 1, { TEXT("R"), TEXT("|") }, { KP(TEXT('R'), TEXT("redstone")), KP(TEXT('|'), TEXT("stick")) }, RB::Redstone);
	B.Shaped(TEXT("lever"), 1, { TEXT("|"), TEXT("C") }, { KP(TEXT('|'), TEXT("stick")), KP(TEXT('C'), TEXT("cobblestone")) }, RB::Redstone);
	B.Shapeless(TEXT("stone_button"), 1, { TEXT("stone") }, RB::Redstone);
	B.Shapeless(TEXT("polished_blackstone_button"), 1, { TEXT("polished_blackstone") }, RB::Redstone);
	B.Shaped(TEXT("stone_pressure_plate"), 1, { TEXT("##") }, { KP(TEXT('#'), TEXT("stone")) }, RB::Redstone);
	B.Shaped(TEXT("polished_blackstone_pressure_plate"), 1, { TEXT("##") }, { KP(TEXT('#'), TEXT("polished_blackstone")) }, RB::Redstone);
	B.Shaped(TEXT("light_weighted_pressure_plate"), 1, { TEXT("##") }, { KP(TEXT('#'), TEXT("gold_ingot")) }, RB::Redstone);
	B.Shaped(TEXT("heavy_weighted_pressure_plate"), 1, { TEXT("##") }, { KP(TEXT('#'), TEXT("iron_ingot")) }, RB::Redstone);
	B.Shaped(TEXT("repeater"), 1, { TEXT("TRT"), TEXT("SSS") }, { KP(TEXT('T'), TEXT("redstone_torch")), KP(TEXT('R'), TEXT("redstone")), KP(TEXT('S'), TEXT("stone")) }, RB::Redstone);
	B.Shaped(TEXT("comparator"), 1, { TEXT(" T "), TEXT("TQT"), TEXT("SSS") }, { KP(TEXT('T'), TEXT("redstone_torch")), KP(TEXT('Q'), TEXT("quartz")), KP(TEXT('S'), TEXT("stone")) }, RB::Redstone);
	B.Shaped(TEXT("redstone_lamp"), 1, { TEXT(" R "), TEXT("RGR"), TEXT(" R ") }, { KP(TEXT('R'), TEXT("redstone")), KP(TEXT('G'), TEXT("glowstone")) }, RB::Redstone);
	B.Shaped(TEXT("piston"), 1, { TEXT("###"), TEXT("CIC"), TEXT("CRC") }, { KP(TEXT('#'), TEXT("#planks")), KP(TEXT('C'), TEXT("cobblestone")), KP(TEXT('I'), TEXT("iron_ingot")), KP(TEXT('R'), TEXT("redstone")) }, RB::Redstone);
	B.Shaped(TEXT("sticky_piston"), 1, { TEXT("S"), TEXT("P") }, { KP(TEXT('S'), TEXT("slime_ball")), KP(TEXT('P'), TEXT("piston")) }, RB::Redstone);
	B.Shaped(TEXT("observer"), 1, { TEXT("CCC"), TEXT("RRQ"), TEXT("CCC") }, { KP(TEXT('C'), TEXT("cobblestone")), KP(TEXT('R'), TEXT("redstone")), KP(TEXT('Q'), TEXT("quartz")) }, RB::Redstone);
	B.Shaped(TEXT("dispenser"), 1, { TEXT("CCC"), TEXT("CBC"), TEXT("CRC") }, { KP(TEXT('C'), TEXT("cobblestone")), KP(TEXT('B'), TEXT("bow")), KP(TEXT('R'), TEXT("redstone")) }, RB::Redstone);
	B.Shaped(TEXT("dropper"), 1, { TEXT("CCC"), TEXT("C C"), TEXT("CRC") }, { KP(TEXT('C'), TEXT("cobblestone")), KP(TEXT('R'), TEXT("redstone")) }, RB::Redstone);
	B.Shaped(TEXT("daylight_detector"), 1, { TEXT("GGG"), TEXT("QQQ"), TEXT("___") }, { KP(TEXT('G'), TEXT("glass")), KP(TEXT('Q'), TEXT("quartz")), KP(TEXT('_'), TEXT("#wooden_slabs")) }, RB::Redstone);
	B.Shaped(TEXT("target"), 1, { TEXT(" R "), TEXT("RHR"), TEXT(" R ") }, { KP(TEXT('R'), TEXT("redstone")), KP(TEXT('H'), TEXT("hay_block")) }, RB::Redstone);
	B.Shaped(TEXT("lightning_rod"), 1, { TEXT("C"), TEXT("C"), TEXT("C") }, { KP(TEXT('C'), TEXT("copper_ingot")) }, RB::Redstone);
	B.Shaped(TEXT("tnt"), 1, { TEXT("GSG"), TEXT("SGS"), TEXT("GSG") }, { KP(TEXT('G'), TEXT("gunpowder")), KP(TEXT('S'), TEXT("#sand_blocks")) }, RB::Redstone);
	B.Shaped(TEXT("tnt"), 1, { TEXT("GSG"), TEXT("SGS"), TEXT("GSG") }, { KP(TEXT('G'), TEXT("gunpowder")), KP(TEXT('S'), TEXT("sand")) }, RB::Redstone);
	B.Shaped(TEXT("rail"), 16, { TEXT("I I"), TEXT("I|I"), TEXT("I I") }, { KP(TEXT('I'), TEXT("iron_ingot")), KP(TEXT('|'), TEXT("stick")) }, RB::Redstone);
	B.Shaped(TEXT("powered_rail"), 6, { TEXT("G G"), TEXT("G|G"), TEXT("GRG") }, { KP(TEXT('G'), TEXT("gold_ingot")), KP(TEXT('|'), TEXT("stick")), KP(TEXT('R'), TEXT("redstone")) }, RB::Redstone);
	B.Shaped(TEXT("detector_rail"), 6, { TEXT("I I"), TEXT("IPI"), TEXT("IRI") }, { KP(TEXT('I'), TEXT("iron_ingot")), KP(TEXT('P'), TEXT("stone_pressure_plate")), KP(TEXT('R'), TEXT("redstone")) }, RB::Redstone);
	B.Shaped(TEXT("activator_rail"), 6, { TEXT("I|I"), TEXT("ITI"), TEXT("I|I") }, { KP(TEXT('I'), TEXT("iron_ingot")), KP(TEXT('|'), TEXT("stick")), KP(TEXT('T'), TEXT("redstone_torch")) }, RB::Redstone);
	B.Shaped(TEXT("minecart"), 1, { TEXT("I I"), TEXT("III") }, { KP(TEXT('I'), TEXT("iron_ingot")) }, RB::Redstone);
	B.Shapeless(TEXT("chest_minecart"), 1, { TEXT("minecart"), TEXT("chest") }, RB::Redstone);
	B.Shapeless(TEXT("furnace_minecart"), 1, { TEXT("minecart"), TEXT("furnace") }, RB::Redstone);
	B.Shapeless(TEXT("hopper_minecart"), 1, { TEXT("minecart"), TEXT("hopper") }, RB::Redstone);
	B.Shapeless(TEXT("tnt_minecart"), 1, { TEXT("minecart"), TEXT("tnt") }, RB::Redstone);
	B.Shaped(TEXT("iron_door"), 3, { TEXT("II"), TEXT("II"), TEXT("II") }, { KP(TEXT('I'), TEXT("iron_ingot")) }, RB::Redstone);
	B.Shaped(TEXT("iron_trapdoor"), 1, { TEXT("II"), TEXT("II") }, { KP(TEXT('I'), TEXT("iron_ingot")) }, RB::Redstone);
	B.Shaped(TEXT("copper_door"), 3, { TEXT("II"), TEXT("II"), TEXT("II") }, { KP(TEXT('I'), TEXT("copper_ingot")) }, RB::Redstone);
	B.Shaped(TEXT("copper_trapdoor"), 2, { TEXT("III"), TEXT("III") }, { KP(TEXT('I'), TEXT("copper_ingot")) }, RB::Redstone);
	B.Shaped(TEXT("copper_bulb"), 4, { TEXT(" C "), TEXT("CBC"), TEXT(" R ") }, { KP(TEXT('C'), TEXT("copper_block")), KP(TEXT('B'), TEXT("blaze_rod")), KP(TEXT('R'), TEXT("redstone")) }, RB::Redstone);
	B.Shaped(TEXT("copper_grate"), 4, { TEXT(" C "), TEXT("C C"), TEXT(" C ") }, { KP(TEXT('C'), TEXT("copper_block")) }, RB::Building);

	// ================================================================ colours
	const TCHAR* Colors[16] = { TEXT("white"), TEXT("orange"), TEXT("magenta"), TEXT("light_blue"), TEXT("yellow"), TEXT("lime"), TEXT("pink"), TEXT("gray"),
		TEXT("light_gray"), TEXT("cyan"), TEXT("purple"), TEXT("blue"), TEXT("brown"), TEXT("green"), TEXT("red"), TEXT("black") };
	for (const TCHAR* C : Colors)
	{
		const FString Cs = C;
		const FString Dye = Cs + TEXT("_dye");
		B.Shapeless(*(Cs + TEXT("_wool")), 1, { *Dye, TEXT("#wool") });
		B.Shaped(*(Cs + TEXT("_carpet")), 3, { TEXT("##") }, { KP(TEXT('#'), *(Cs + TEXT("_wool"))) }, RB::Building);
		B.Shaped(*(Cs + TEXT("_carpet")), 8, { TEXT("###"), TEXT("#D#"), TEXT("###") }, { KP(TEXT('#'), TEXT("#wool_carpets")), KP(TEXT('D'), *Dye) }, RB::Building);
		B.Shaped(*(Cs + TEXT("_terracotta")), 8, { TEXT("###"), TEXT("#D#"), TEXT("###") }, { KP(TEXT('#'), TEXT("terracotta")), KP(TEXT('D'), *Dye) }, RB::Building);
		B.Shaped(*(Cs + TEXT("_stained_glass")), 8, { TEXT("###"), TEXT("#D#"), TEXT("###") }, { KP(TEXT('#'), TEXT("glass")), KP(TEXT('D'), *Dye) }, RB::Building);
		B.Shaped(*(Cs + TEXT("_stained_glass_pane")), 16, { TEXT("###"), TEXT("###") }, { KP(TEXT('#'), *(Cs + TEXT("_stained_glass"))) }, RB::Building);
		B.Shaped(*(Cs + TEXT("_stained_glass_pane")), 8, { TEXT("###"), TEXT("#D#"), TEXT("###") }, { KP(TEXT('#'), TEXT("glass_pane")), KP(TEXT('D'), *Dye) }, RB::Building);
		B.Shapeless(*(Cs + TEXT("_concrete_powder")), 8, { *Dye, TEXT("sand"), TEXT("sand"), TEXT("sand"), TEXT("sand"), TEXT("gravel"), TEXT("gravel"), TEXT("gravel"), TEXT("gravel") }, RB::Building);
		B.Shaped(*(Cs + TEXT("_bed")), 1, { TEXT("WWW"), TEXT("PPP") }, { KP(TEXT('W'), *(Cs + TEXT("_wool"))), KP(TEXT('P'), TEXT("#planks")) });
		B.Shapeless(*(Cs + TEXT("_bed")), 1, { *Dye, TEXT("#beds") });
		B.Shapeless(*(Cs + TEXT("_candle")), 1, { TEXT("candle"), *Dye });
		B.Shapeless(*(Cs + TEXT("_shulker_box")), 1, { TEXT("shulker_box"), *Dye });
	}
	// dyes from flowers and minerals
	struct FDyeSrc { const TCHAR* Src; const TCHAR* Dye; int32 N; };
	const FDyeSrc DyeSrc[] = {
		{ TEXT("dandelion"), TEXT("yellow_dye"), 1 }, { TEXT("poppy"), TEXT("red_dye"), 1 }, { TEXT("blue_orchid"), TEXT("light_blue_dye"), 1 },
		{ TEXT("allium"), TEXT("magenta_dye"), 1 }, { TEXT("azure_bluet"), TEXT("light_gray_dye"), 1 }, { TEXT("red_tulip"), TEXT("red_dye"), 1 },
		{ TEXT("orange_tulip"), TEXT("orange_dye"), 1 }, { TEXT("white_tulip"), TEXT("light_gray_dye"), 1 }, { TEXT("pink_tulip"), TEXT("pink_dye"), 1 },
		{ TEXT("oxeye_daisy"), TEXT("light_gray_dye"), 1 }, { TEXT("cornflower"), TEXT("blue_dye"), 1 }, { TEXT("lily_of_the_valley"), TEXT("white_dye"), 1 },
		{ TEXT("wither_rose"), TEXT("black_dye"), 1 }, { TEXT("sunflower"), TEXT("yellow_dye"), 2 }, { TEXT("lilac"), TEXT("magenta_dye"), 2 },
		{ TEXT("rose_bush"), TEXT("red_dye"), 2 }, { TEXT("peony"), TEXT("pink_dye"), 2 }, { TEXT("torchflower"), TEXT("orange_dye"), 1 },
		{ TEXT("pitcher_plant"), TEXT("cyan_dye"), 2 }, { TEXT("open_eyeblossom"), TEXT("orange_dye"), 1 }, { TEXT("closed_eyeblossom"), TEXT("gray_dye"), 1 },
		{ TEXT("pink_petals"), TEXT("pink_dye"), 1 }, { TEXT("wildflowers"), TEXT("yellow_dye"), 1 }, { TEXT("ink_sac"), TEXT("black_dye"), 1 },
		{ TEXT("lapis_lazuli"), TEXT("blue_dye"), 1 }, { TEXT("bone_meal"), TEXT("white_dye"), 1 }, { TEXT("beetroot"), TEXT("red_dye"), 1 },
		{ TEXT("cinnabar_dust"), TEXT("red_dye"), 2 }, { TEXT("sulfur_shard"), TEXT("yellow_dye"), 1 } };
	for (const FDyeSrc& D : DyeSrc) B.Shapeless(D.Dye, D.N, { D.Src });
	B.Shapeless(TEXT("orange_dye"), 2, { TEXT("red_dye"), TEXT("yellow_dye") });
	B.Shapeless(TEXT("magenta_dye"), 2, { TEXT("purple_dye"), TEXT("pink_dye") });
	B.Shapeless(TEXT("light_blue_dye"), 2, { TEXT("blue_dye"), TEXT("white_dye") });
	B.Shapeless(TEXT("lime_dye"), 2, { TEXT("green_dye"), TEXT("white_dye") });
	B.Shapeless(TEXT("pink_dye"), 2, { TEXT("red_dye"), TEXT("white_dye") });
	B.Shapeless(TEXT("gray_dye"), 2, { TEXT("black_dye"), TEXT("white_dye") });
	B.Shapeless(TEXT("light_gray_dye"), 2, { TEXT("gray_dye"), TEXT("white_dye") });
	B.Shapeless(TEXT("cyan_dye"), 2, { TEXT("blue_dye"), TEXT("green_dye") });
	B.Shapeless(TEXT("purple_dye"), 2, { TEXT("red_dye"), TEXT("blue_dye") });

	// ================================================================ food
	B.Shaped(TEXT("bread"), 1, { TEXT("WWW") }, { KP(TEXT('W'), TEXT("wheat")) }, RB::Food);
	B.Shaped(TEXT("cake"), 1, { TEXT("MMM"), TEXT("SES"), TEXT("WWW") }, { KP(TEXT('M'), TEXT("milk_bucket")), KP(TEXT('S'), TEXT("sugar")), KP(TEXT('E'), TEXT("egg")), KP(TEXT('W'), TEXT("wheat")) }, RB::Food);
	B.Shaped(TEXT("cookie"), 8, { TEXT("WCW") }, { KP(TEXT('W'), TEXT("wheat")), KP(TEXT('C'), TEXT("brown_dye")) }, RB::Food);
	B.Shapeless(TEXT("pumpkin_pie"), 1, { TEXT("pumpkin"), TEXT("sugar"), TEXT("egg") }, RB::Food);
	B.Shaped(TEXT("golden_apple"), 1, { TEXT("GGG"), TEXT("GAG"), TEXT("GGG") }, { KP(TEXT('G'), TEXT("gold_ingot")), KP(TEXT('A'), TEXT("apple")) }, RB::Food);
	B.Shaped(TEXT("golden_carrot"), 1, { TEXT("NNN"), TEXT("NCN"), TEXT("NNN") }, { KP(TEXT('N'), TEXT("gold_nugget")), KP(TEXT('C'), TEXT("carrot")) }, RB::Food);
	B.Shapeless(TEXT("mushroom_stew"), 1, { TEXT("brown_mushroom"), TEXT("red_mushroom"), TEXT("bowl") }, RB::Food);
	B.Shapeless(TEXT("beetroot_soup"), 1, { TEXT("beetroot"), TEXT("beetroot"), TEXT("beetroot"), TEXT("beetroot"), TEXT("beetroot"), TEXT("beetroot"), TEXT("bowl") }, RB::Food);
	B.Shapeless(TEXT("rabbit_stew"), 1, { TEXT("cooked_rabbit"), TEXT("carrot"), TEXT("baked_potato"), TEXT("brown_mushroom"), TEXT("bowl") }, RB::Food);
	B.Shapeless(TEXT("suspicious_stew"), 1, { TEXT("brown_mushroom"), TEXT("red_mushroom"), TEXT("bowl"), TEXT("#small_flowers") }, RB::Food);
	B.Shapeless(TEXT("dried_kelp"), 9, { TEXT("dried_kelp_block") }, RB::Food);
	B.Shapeless(TEXT("melon_seeds"), 1, { TEXT("melon_slice") });
	B.Shapeless(TEXT("pumpkin_seeds"), 4, { TEXT("pumpkin") });
	B.Shaped(TEXT("leather"), 1, { TEXT("##"), TEXT("##") }, { KP(TEXT('#'), TEXT("rabbit_hide")) });
	B.Shapeless(TEXT("white_wool"), 1, { TEXT("string"), TEXT("string"), TEXT("string"), TEXT("string") });
	B.Shaped(TEXT("white_wool"), 1, { TEXT("##"), TEXT("##") }, { KP(TEXT('#'), TEXT("string")) });
	B.Shaped(TEXT("goat_horn"), 1, { TEXT("#") }, { KP(TEXT('#'), TEXT("goat_horn")) }); // placeholder duplicate guard (ignored if item missing)
}
