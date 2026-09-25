// Building block sets (stone families, bricks, glass) and coloured block families.
#include "Blocks/MCBlockRegistrar.h"
#include "Blocks/MCTextureDefsCommon.h"

void MCRegisterBuildingBlocks(FMCBlockRegistrar& R)
{
	using T = EMCTab;
	R.Add(TEXT("cobblestone")).Hard(2.f, 6).Pick().Tab(T::Building).Flag(MCB_Stone).Fam(TEXT("cobblestone"), TEXT("base"));
	R.Add(TEXT("mossy_cobblestone")).Hard(2.f, 6).Pick().Tab(T::Building).Fam(TEXT("mossy_cobblestone"), TEXT("base"));
	R.Add(TEXT("stone_bricks")).Hard(1.5f, 6).Pick().Tab(T::Building).Fam(TEXT("stone_bricks"), TEXT("base"));
	R.Add(TEXT("mossy_stone_bricks")).Hard(1.5f, 6).Pick().Tab(T::Building).Fam(TEXT("mossy_stone_bricks"), TEXT("base"));
	R.Add(TEXT("cracked_stone_bricks")).Hard(1.5f, 6).Pick().Tab(T::Building);
	R.Add(TEXT("chiseled_stone_bricks")).Hard(1.5f, 6).Pick().Tab(T::Building);
	R.Add(TEXT("smooth_stone")).Hard(2.f, 6).Pick().Tab(T::Building);
	R.Add(TEXT("bricks")).Hard(2.f, 6).Pick().Tab(T::Building).Fam(TEXT("bricks"), TEXT("base"));
	R.Add(TEXT("prismarine")).Hard(1.5f, 6).Pick().Tab(T::Building);
	R.Add(TEXT("prismarine_bricks")).Hard(1.5f, 6).Pick().Tab(T::Building);
	R.Add(TEXT("dark_prismarine")).Hard(1.5f, 6).Pick().Tab(T::Building);
	R.Add(TEXT("sea_lantern")).Hard(0.3f).DropItem(TEXT("prismarine_crystals"), 2, 3, true).Light(15).Snd(EMCSound::Glass).Tab(T::Functional);

	R.StoneSet(TEXT("stone"), false);
	R.Button(TEXT("stone"), TEXT("stone_button"), false);
	R.PressurePlate(TEXT("stone"), TEXT("stone_pressure_plate"), false);
	R.StoneSet(TEXT("cobblestone"), true);
	R.StoneSet(TEXT("mossy_cobblestone"), true);
	R.StoneSet(TEXT("stone_bricks"), true);
	R.StoneSet(TEXT("mossy_stone_bricks"), true);
	R.Slab(TEXT("smooth_stone"), TEXT("smooth_stone_slab")).TexTBS(TEXT("smooth_stone"), TEXT("smooth_stone"), TEXT("smooth_stone_slab_side"));
	R.StoneSet(TEXT("granite"), true);
	R.StoneSet(TEXT("polished_granite"), false);
	R.StoneSet(TEXT("diorite"), true);
	R.StoneSet(TEXT("polished_diorite"), false);
	R.StoneSet(TEXT("andesite"), true);
	R.StoneSet(TEXT("polished_andesite"), false);
	R.StoneSet(TEXT("cobbled_deepslate"), true);
	R.StoneSet(TEXT("polished_deepslate"), true);
	R.StoneSet(TEXT("deepslate_bricks"), true);
	R.StoneSet(TEXT("deepslate_tiles"), true);
	R.StoneSet(TEXT("tuff"), true);
	R.StoneSet(TEXT("polished_tuff"), true);
	R.StoneSet(TEXT("tuff_bricks"), true);
	R.StoneSet(TEXT("bricks"), true);
	R.StoneSet(TEXT("mud_bricks"), true);
	R.StoneSet(TEXT("sandstone"), true);
	R.StoneSet(TEXT("smooth_sandstone"), false);
	R.Slab(TEXT("cut_sandstone"), TEXT("cut_sandstone_slab"));
	R.StoneSet(TEXT("red_sandstone"), true);
	R.StoneSet(TEXT("smooth_red_sandstone"), false);
	R.Slab(TEXT("cut_red_sandstone"), TEXT("cut_red_sandstone_slab"));
	R.StoneSet(TEXT("prismarine"), true);
	R.StoneSet(TEXT("prismarine_bricks"), false);
	R.StoneSet(TEXT("dark_prismarine"), false);
	R.StoneSet(TEXT("quartz_block"), false, TEXT("quartz_stairs"), TEXT("quartz_slab"));
	R.StoneSet(TEXT("smooth_quartz"), false);
	R.StoneSet(TEXT("sulfur"), true);
	R.StoneSet(TEXT("polished_sulfur"), true);
	R.StoneSet(TEXT("sulfur_bricks"), true);
	R.StoneSet(TEXT("cinnabar"), true);
	R.StoneSet(TEXT("polished_cinnabar"), true);
	R.StoneSet(TEXT("cinnabar_bricks"), true);
	const TCHAR* CopperStages[4] = { TEXT("cut_copper"), TEXT("exposed_cut_copper"), TEXT("weathered_cut_copper"), TEXT("oxidized_cut_copper") };
	for (const TCHAR* C : CopperStages) R.StoneSet(C, false);

	// glass
	R.Add(TEXT("glass")).Hard(0.3f).DropSilk().Snd(EMCSound::Glass).Tab(T::Building).Cutout().Opacity(0).Flag(MCB_CullSame).Map(0xDCEEF2);
	R.Add(TEXT("tinted_glass")).Hard(0.3f).Snd(EMCSound::Glass).Tab(T::Building).Translucent().Opacity(15).Flag(MCB_CullSame);
	R.Add(TEXT("glass_pane")).Tex(TEXT("glass")).Model(EMCModel::Pane).Hard(0.3f).DropSilk().Snd(EMCSound::Glass).Tab(T::Building).Tag(TEXT("panes"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("iron_bars")).Model(EMCModel::Bars).Hard(5.f, 6).Pick().Snd(EMCSound::Metal).Tab(T::Building).Tag(TEXT("panes"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
}

void MCRegisterColoredBlocks(FMCBlockRegistrar& R)
{
	using T = EMCTab;
	using namespace MCTexDSL;
	R.Add(TEXT("terracotta")).Hard(1.25f, 4.2f).Pick().Tab(T::Colored).Flag(MCB_Stone).Map(0x985E43);
	for (int32 i = 0; i < 16; ++i)
	{
		const FString C = DyeColors[i].Name;
		const FColor Col = Hex(DyeColors[i].Rgb);
		const FColor TCol = Hex(TerracottaColors[i]);
		R.Add(*(C + TEXT("_wool"))).Tex(TEXT("wool")).Tint(EMCTint::Custom, Col).Hard(0.8f).Tool(EMCTool::Shears, 0, false).Snd(EMCSound::Wool)
			.Tab(T::Colored).Burn(30, 60).Flag(MCB_Wool).Tag(TEXT("wool")).Fam(*C, TEXT("wool")).Map(DyeColors[i].Rgb).Fuel(100);
		R.Add(*(C + TEXT("_carpet"))).Tex(TEXT("wool")).Tint(EMCTint::Custom, Col).Model(EMCModel::Carpet).Hard(0.1f).Snd(EMCSound::Wool)
			.Tab(T::Colored).Burn(60, 20).Flag(MCB_NeedsSupport).Tag(TEXT("wool_carpets")).Fam(*C, TEXT("carpet")).Fuel(67);
		R.Add(*(C + TEXT("_concrete"))).Tex(TEXT("concrete")).Tint(EMCTint::Custom, Col).Hard(1.8f).Pick().Tab(T::Colored).Fam(*C, TEXT("concrete")).Map(DyeColors[i].Rgb);
		R.Add(*(C + TEXT("_concrete_powder"))).Tex(TEXT("concrete_powder")).Tint(EMCTint::Custom, Col).Hard(0.5f).Shovel().Snd(EMCSound::Sand)
			.Tab(T::Colored).Beh(EMCBeh::ConcretePowder).Flag(MCB_Gravity).Fam(*C, TEXT("concrete_powder"));
		R.Add(*(C + TEXT("_terracotta"))).Tex(TEXT("terracotta_gray")).Tint(EMCTint::Custom, TCol).Hard(1.25f, 4.2f).Pick().Tab(T::Colored)
			.Tag(TEXT("terracotta")).Fam(*C, TEXT("terracotta")).Map(TerracottaColors[i]).Flag(MCB_Stone);
		R.Add(*(C + TEXT("_glazed_terracotta"))).Tex(TEXT("glazed_terracotta")).Tint(EMCTint::Custom, Col).Hard(1.4f).Pick()
			.Orient(EMCCubeOrient::Facing4).Beh(EMCBeh::FacingToPlayer).Tab(T::Colored).Fam(*C, TEXT("glazed_terracotta"));
		R.Add(*(C + TEXT("_stained_glass"))).Tex(TEXT("stained_glass")).Tint(EMCTint::Custom, Col).Hard(0.3f).DropSilk().Snd(EMCSound::Glass)
			.Tab(T::Colored).Translucent().Opacity(0).Flag(MCB_CullSame).Fam(*C, TEXT("stained_glass"));
		R.Add(*(C + TEXT("_stained_glass_pane"))).Tex(TEXT("stained_glass")).Tint(EMCTint::Custom, Col).Model(EMCModel::Pane).Hard(0.3f).DropSilk()
			.Snd(EMCSound::Glass).Tab(T::Colored).Tag(TEXT("panes")).Fam(*C, TEXT("stained_glass_pane"));
		R.Blocks.Last().Layer = EMCLayer::Translucent;
		R.Add(*(C + TEXT("_bed"))).TexTBS(TEXT("bed_cloth"), TEXT("oak_planks"), TEXT("bed_cloth")).Tint(EMCTint::Custom, Col).Model(EMCModel::Bed, 4)
			.Beh(EMCBeh::Bed).Hard(0.2f).Snd(EMCSound::Wood).Tab(T::Functional).Flag(MCB_Interact).DropSpecial().Tag(TEXT("beds")).Fam(*C, TEXT("bed"))
			.Mesh(TEXT("SM_Bed"));
		R.Add(*(C + TEXT("_candle"))).Tex(TEXT("candle")).Tint(EMCTint::Custom, Col).Model(EMCModel::Candle, 3).Beh(EMCBeh::Candle).Hard(0.1f)
			.Snd(EMCSound::Candle).Tab(T::Colored).Flag(MCB_Interact).Tag(TEXT("candles")).Fam(*C, TEXT("candle"));
		R.Add(*(C + TEXT("_shulker_box"))).TexTS(TEXT("shulker_box_top"), TEXT("shulker_box")).Tint(EMCTint::Custom, Col).Hard(2.f).Pick(0)
			.Beh(EMCBeh::ShulkerBox).Flag(MCB_Interact | MCB_Container | MCB_BlockEntity).Tab(T::Colored)
			.Tag(TEXT("shulker_boxes")).Fam(*C, TEXT("shulker_box")).DropSpecial();
	}
	R.Add(TEXT("candle")).Tex(TEXT("candle")).Tint(EMCTint::Custom, FColor(232, 220, 196)).Model(EMCModel::Candle, 3).Beh(EMCBeh::Candle).Hard(0.1f)
		.Snd(EMCSound::Candle).Tab(T::Colored).Flag(MCB_Interact).Tag(TEXT("candles"));
	R.Add(TEXT("shulker_box")).TexTS(TEXT("shulker_box_top"), TEXT("shulker_box")).Tint(EMCTint::Custom, FColor(150, 110, 150)).Hard(2.f).Pick(0)
		.Beh(EMCBeh::ShulkerBox).Flag(MCB_Interact | MCB_Container | MCB_BlockEntity).Tab(T::Colored).Tag(TEXT("shulker_boxes")).DropSpecial();
}
