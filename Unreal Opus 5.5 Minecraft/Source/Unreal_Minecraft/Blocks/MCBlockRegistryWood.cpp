// Wood families.
#include "Blocks/MCBlockRegistrar.h"

void MCRegisterWoodBlocks(FMCBlockRegistrar& R)
{
	using T = EMCTab;
	struct FWood { const TCHAR* Name; bool bNether; bool bBamboo; EMCTint LeafTint; uint32 Map; };
	const FWood Woods[] = {
		{ TEXT("oak"), false, false, EMCTint::Foliage, 0x8F7748 },
		{ TEXT("spruce"), false, false, EMCTint::Spruce, 0x815631 },
		{ TEXT("birch"), false, false, EMCTint::Birch, 0xF7E9A3 },
		{ TEXT("jungle"), false, false, EMCTint::Foliage, 0x976D4D },
		{ TEXT("acacia"), false, false, EMCTint::Foliage, 0xD87F33 },
		{ TEXT("dark_oak"), false, false, EMCTint::Foliage, 0x664C33 },
		{ TEXT("mangrove"), false, false, EMCTint::Foliage, 0x993333 },
		{ TEXT("cherry"), false, false, EMCTint::None, 0xD1B1A1 },
		{ TEXT("pale_oak"), false, false, EMCTint::None, 0xE8DED5 },
		{ TEXT("bamboo"), false, true, EMCTint::None, 0xE5E533 },
		{ TEXT("crimson"), true, false, EMCTint::None, 0x943F61 },
		{ TEXT("warped"), true, false, EMCTint::None, 0x3A8E8C },
	};

	for (const FWood& W : Woods)
	{
		const FString N = W.Name;
		const bool bN = W.bNether;
		const FString LogName = bN ? N + TEXT("_stem") : (W.bBamboo ? TEXT("bamboo_block") : N + TEXT("_log"));
		const FString WoodName = bN ? N + TEXT("_hyphae") : N + TEXT("_wood");
		const FString SideTex = W.bBamboo ? TEXT("bamboo_block") : N + TEXT("_log");
		const FString TopTex = W.bBamboo ? TEXT("bamboo_block_top") : N + TEXT("_log_top");
		const FString SSideTex = W.bBamboo ? TEXT("stripped_bamboo_block") : TEXT("stripped_") + N + TEXT("_log");
		const FString STopTex = W.bBamboo ? TEXT("stripped_bamboo_block_top") : TEXT("stripped_") + N + TEXT("_log_top");
		const EMCSound Snd = bN ? EMCSound::Nylium : (W.bBamboo ? EMCSound::Bamboo : (N == TEXT("cherry") ? EMCSound::Cherry : EMCSound::Wood));

		auto Log = R.Add(*LogName).TexTS(*TopTex, *SideTex).Orient(EMCCubeOrient::Axis).Beh(EMCBeh::Axis).Hard(2.f).Axe()
			.Snd(Snd).Tab(T::Natural).Flag(MCB_Log).Tag(TEXT("logs")).Fam(W.Name, TEXT("log")).Map(W.Map);
		if (!bN) Log.Burn(5, 5).Fuel(300).Tag(TEXT("logs_that_burn"));
		if (!W.bBamboo)
		{
			auto Wd = R.Add(*WoodName).Tex(*SideTex).Orient(EMCCubeOrient::Axis).Beh(EMCBeh::Axis).Hard(2.f).Axe().Snd(Snd).Tab(T::Building)
				.Flag(MCB_Log).Tag(TEXT("logs")).Fam(W.Name, TEXT("wood"));
			if (!bN) Wd.Burn(5, 5).Fuel(300);
		}
		const FString StrippedLog = W.bBamboo ? TEXT("stripped_bamboo_block") : (bN ? TEXT("stripped_") + N + TEXT("_stem") : TEXT("stripped_") + N + TEXT("_log"));
		auto SL = R.Add(*StrippedLog).TexTS(*STopTex, *SSideTex).Orient(EMCCubeOrient::Axis).Beh(EMCBeh::Axis).Hard(2.f).Axe().Snd(Snd).Tab(T::Building)
			.Flag(MCB_Log).Tag(TEXT("logs")).Fam(W.Name, TEXT("stripped_log"));
		if (!bN) SL.Burn(5, 5).Fuel(300);
		if (!W.bBamboo)
		{
			const FString StrippedWood = bN ? TEXT("stripped_") + N + TEXT("_hyphae") : TEXT("stripped_") + N + TEXT("_wood");
			auto SW = R.Add(*StrippedWood).Tex(*SSideTex).Orient(EMCCubeOrient::Axis).Beh(EMCBeh::Axis).Hard(2.f).Axe().Snd(Snd).Tab(T::Building)
				.Flag(MCB_Log).Tag(TEXT("logs")).Fam(W.Name, TEXT("stripped_wood"));
			if (!bN) SW.Burn(5, 5).Fuel(300);
		}

		const FString Planks = N + TEXT("_planks");
		auto P = R.Add(*Planks).Hard(2.f, 3).Axe().Snd(Snd).Tab(T::Building).Tag(TEXT("planks")).Fam(W.Name, TEXT("planks")).Map(W.Map);
		if (!bN) P.Burn(5, 20).Fuel(300);
		if (W.bBamboo)
		{
			R.Add(TEXT("bamboo_mosaic")).Hard(2.f, 3).Axe().Snd(Snd).Tab(T::Building).Burn(5, 20).Fuel(300).Fam(W.Name, TEXT("mosaic"));
			R.Stairs(TEXT("bamboo_mosaic"), TEXT("bamboo_mosaic_stairs")).Fuel(300);
			R.Slab(TEXT("bamboo_mosaic"), TEXT("bamboo_mosaic_slab")).Fuel(150);
		}

		R.Stairs(*Planks, *(N + TEXT("_stairs"))).Axe().Fuel(bN ? 0 : 300).Tag(TEXT("wooden_stairs"));
		R.Slab(*Planks, *(N + TEXT("_slab"))).Axe().Fuel(bN ? 0 : 150).Tag(TEXT("wooden_slabs"));
		R.Fence(*Planks, *(N + TEXT("_fence")), bN).Axe();
		R.FenceGate(*Planks, *(N + TEXT("_fence_gate"))).Axe().Fuel(bN ? 0 : 300);
		R.Add(*(N + TEXT("_door"))).TexTBS(*(N + TEXT("_door_top")), *(N + TEXT("_door_bottom")), *(N + TEXT("_door_bottom")))
			.Model(EMCModel::Door, 5).Beh(EMCBeh::Door).Hard(3.f).Axe().Snd(Snd).Tab(T::Redstone).Flag(MCB_Interact | MCB_Redstone)
			.Fuel(bN ? 0 : 200).Tag(TEXT("wooden_doors")).Fam(W.Name, TEXT("door"));
		R.Blocks.Last().Layer = EMCLayer::Cutout;
		R.Add(*(N + TEXT("_trapdoor"))).Tex(*(N + TEXT("_trapdoor"))).Model(EMCModel::Trapdoor, 4).Beh(EMCBeh::Trapdoor).Hard(3.f).Axe()
			.Snd(Snd).Tab(T::Redstone).Flag(MCB_Interact | MCB_Redstone).Fuel(bN ? 0 : 300).Tag(TEXT("wooden_trapdoors")).Fam(W.Name, TEXT("trapdoor"));
		R.Blocks.Last().Layer = EMCLayer::Cutout;
		R.Button(*Planks, *(N + TEXT("_button")), true).Tag(TEXT("wooden_buttons")).Fuel(bN ? 0 : 100);
		R.PressurePlate(*Planks, *(N + TEXT("_pressure_plate")), true).Tag(TEXT("wooden_pressure_plates")).Fuel(bN ? 0 : 300);

		if (!bN && !W.bBamboo)
		{
			const FString Leaves = N + TEXT("_leaves");
			auto L = R.Add(*Leaves).Cutout().Opacity(1).Hard(0.2f).Hoe().Snd(N == TEXT("cherry") ? EMCSound::Cherry : EMCSound::Grass)
				.Tab(T::Natural).Flag(MCB_Leaves | MCB_Solid).Beh(EMCBeh::Leaves).Ticks().Meta(4).DropSpecial().Burn(30, 60)
				.Tag(TEXT("leaves")).Fam(W.Name, TEXT("leaves")).Map(0x007C00);
			if (W.LeafTint != EMCTint::None) L.Tint(W.LeafTint);
			if (N == TEXT("mangrove"))
			{
				R.Add(TEXT("mangrove_propagule")).Shape(EMCShape::Cross).Tex(TEXT("mangrove_propagule")).Beh(EMCBeh::Sapling).Meta(1)
					.Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(MCB_Plant | MCB_NeedsSupport).Ticks().Fuel(100);
			}
			else
			{
				R.Add(*(N + TEXT("_sapling"))).Shape(EMCShape::Cross).Tex(*(N + TEXT("_sapling"))).Beh(EMCBeh::Sapling).Meta(1)
					.Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(MCB_Plant | MCB_NeedsSupport).Ticks().Fuel(100).Tag(TEXT("saplings")).Fam(W.Name, TEXT("sapling"));
			}
		}
	}

	R.Add(TEXT("mangrove_roots")).Tex(TEXT("mangrove_roots")).Cutout().Hard(0.7f).Axe().Snd(EMCSound::Wood).Tab(T::Natural).Burn(5, 20).Flag(MCB_CullSame);
	R.Add(TEXT("azalea_leaves")).Cutout().Opacity(1).Hard(0.2f).Hoe().Snd(EMCSound::Grass).Tab(T::Natural).Flag(MCB_Leaves | MCB_Solid)
		.Beh(EMCBeh::Leaves).Ticks().Meta(4).DropSpecial().Burn(30, 60).Tag(TEXT("leaves")).Fam(TEXT("azalea"), TEXT("leaves"));
	R.Add(TEXT("flowering_azalea_leaves")).Cutout().Opacity(1).Hard(0.2f).Hoe().Snd(EMCSound::Grass).Tab(T::Natural).Flag(MCB_Leaves | MCB_Solid)
		.Beh(EMCBeh::Leaves).Ticks().Meta(4).DropSpecial().Burn(30, 60).Tag(TEXT("leaves")).Fam(TEXT("azalea"), TEXT("leaves"));
	R.Add(TEXT("azalea")).TexTBS(TEXT("azalea_top"), TEXT("azalea_side"), TEXT("azalea_side")).Shape(EMCShape::Cross).Beh(EMCBeh::Sapling).Meta(1)
		.Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(MCB_Plant | MCB_NeedsSupport).Tex(TEXT("azalea_side"));
	R.Add(TEXT("flowering_azalea")).Shape(EMCShape::Cross).Beh(EMCBeh::Sapling).Meta(1).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural)
		.Flag(MCB_Plant | MCB_NeedsSupport).Tex(TEXT("flowering_azalea_side"));
	R.Add(TEXT("creaking_heart")).TexTS(TEXT("creaking_heart_top"), TEXT("creaking_heart")).Orient(EMCCubeOrient::Axis).Meta(3)
		.Beh(EMCBeh::CreakingHeart).Hard(10.f, 10).Axe().Snd(EMCSound::Wood).Tab(T::Natural).Alt(0, TEXT("creaking_heart_awake")).DropItem(TEXT("resin_clump"), 1, 3).Ticks();
	R.Add(TEXT("resin_block")).Hard(0.f).Snd(EMCSound::Mud).Tab(T::Building);
	R.Add(TEXT("resin_bricks")).Hard(1.5f, 6).Pick().Tab(T::Building);
	R.Add(TEXT("chiseled_resin_bricks")).Hard(1.5f, 6).Pick().Tab(T::Building);
	R.StoneSet(TEXT("resin_bricks"), true, TEXT("resin_brick_stairs"), TEXT("resin_brick_slab"), TEXT("resin_brick_wall"));
}
