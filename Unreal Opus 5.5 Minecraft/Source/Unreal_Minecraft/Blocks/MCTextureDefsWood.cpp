// Wood family textures (logs, planks, leaves, saplings, doors, trapdoors).
#include "Blocks/MCTextures.h"
#include "Blocks/MCTextureDefsCommon.h"

void MCRegisterWoodTextures(TFunctionRef<void(const FMCTexDef&)> Add)
{
	using namespace MCTexDSL;
	for (int32 WI = 0; WI < 12; ++WI)
	{
		const FWoodPalette& W = Woods[WI];
		const FString N = W.Name;
		const float Wood = (float)WI;
		const bool bBamboo = N == TEXT("bamboo");

		if (bBamboo)
		{
			Add(Def(TEXT("bamboo_block"), EMCTexRecipe::Bamboo, W.Bark, W.BarkDark, W.Planks, 0, Wood, 0, nullptr, 0));
			Add(Def(TEXT("bamboo_block_top"), EMCTexRecipe::Bamboo, W.Bark, W.BarkDark, W.Planks, 1, Wood, 0, nullptr, 0));
			Add(Def(TEXT("stripped_bamboo_block"), EMCTexRecipe::Bamboo, W.Stripped, W.StrippedDark, W.Planks, 0, Wood, 1, nullptr, 0));
			Add(Def(TEXT("stripped_bamboo_block_top"), EMCTexRecipe::Bamboo, W.Stripped, W.StrippedDark, W.Planks, 1, Wood, 1, nullptr, 0));
			Add(Def(TEXT("bamboo_planks"), EMCTexRecipe::Planks, W.Planks, W.PlanksGrain, W.BarkDark, 0, Wood, 1, nullptr, 0));
			Add(Def(TEXT("bamboo_mosaic"), EMCTexRecipe::Planks, W.Planks, W.PlanksGrain, W.BarkDark, 0, Wood, 2, nullptr, 0));
		}
		else
		{
			Add(Def(*(N + TEXT("_log")), EMCTexRecipe::LogSide, W.Bark, W.BarkDark, W.Top, 0, Wood, 0, nullptr, 0));
			Add(Def(*(N + TEXT("_log_top")), EMCTexRecipe::LogTop, W.Top, W.TopRing, W.Bark, 0, Wood, 0, nullptr, W.bNether ? MCTF_Emissive : 0));
			Add(Def(*(TEXT("stripped_") + N + TEXT("_log")), EMCTexRecipe::StrippedSide, W.Stripped, W.StrippedDark, W.Top, 0, Wood, 0, nullptr, 0));
			Add(Def(*(TEXT("stripped_") + N + TEXT("_log_top")), EMCTexRecipe::StrippedTop, W.Top, W.TopRing, W.Stripped, 0, Wood, 0, nullptr, 0));
			Add(Def(*(N + TEXT("_planks")), EMCTexRecipe::Planks, W.Planks, W.PlanksGrain, W.BarkDark, 0, Wood, 0, nullptr, 0));
		}

		// Doors & trapdoors (cutout panels with windows for some woods)
		Add(Def(*(N + TEXT("_door_bottom")), EMCTexRecipe::Door, W.Planks, W.PlanksGrain, W.BarkDark, 0, Wood, 0, nullptr, MCTF_Cutout));
		Add(Def(*(N + TEXT("_door_top")), EMCTexRecipe::Door, W.Planks, W.PlanksGrain, W.BarkDark, 1, Wood, 0, nullptr, MCTF_Cutout));
		Add(Def(*(N + TEXT("_trapdoor")), EMCTexRecipe::Trapdoor, W.Planks, W.PlanksGrain, W.BarkDark, 0, Wood, 0, nullptr, MCTF_Cutout));

		if (!W.bNether && !bBamboo)
		{
			uint8 LeafFlags = MCTF_Cutout;
			if (W.bTintedLeaves) LeafFlags |= MCTF_Tinted;
			Add(Def(*(N + TEXT("_leaves")), EMCTexRecipe::Leaves, W.Leaves, W.Leaves, W.BarkDark, 0, Wood, 0, nullptr, LeafFlags));
			Add(Def(*(N + TEXT("_sapling")), EMCTexRecipe::Plant, 0x4E7A2A, W.Bark, W.Leaves, (float)EMCPlantKind::Sapling, Wood, 0, nullptr, MCTF_Cutout));
		}
	}

	// Mangrove specials
	Add(Def(TEXT("mangrove_roots"), EMCTexRecipe::LogSide, 0x4A3B2A, 0x2E2419, 0x5E4A34, 1, 6, 0, nullptr, MCTF_Cutout));
	Add(Def(TEXT("mangrove_propagule"), EMCTexRecipe::Plant, 0x5E8A2E, 0x6E5A34, 0x9ACD4A, (float)EMCPlantKind::Propagule, 6, 0, nullptr, MCTF_Cutout));
	// Azalea
	Add(Def(TEXT("azalea_leaves"), EMCTexRecipe::Leaves, 0x5E7E2A, 0x4A6A20, 0x3A2A18, 1, 0, 0, nullptr, MCTF_Cutout));
	Add(Def(TEXT("flowering_azalea_leaves"), EMCTexRecipe::Leaves, 0x5E7E2A, 0xD86EC4, 0x3A2A18, 2, 0, 0, nullptr, MCTF_Cutout));
	Add(Def(TEXT("azalea_top"), EMCTexRecipe::Leaves, 0x6A8E2E, 0x5A7A24, 0x3A2A18, 3, 0, 0, nullptr, MCTF_Cutout));
	Add(Def(TEXT("azalea_side"), EMCTexRecipe::Plant, 0x6A8E2E, 0x5A4A30, 0x4E6E22, (float)EMCPlantKind::Azalea, 0, 0, nullptr, MCTF_Cutout));
	Add(Def(TEXT("flowering_azalea_top"), EMCTexRecipe::Leaves, 0x6A8E2E, 0xD86EC4, 0x3A2A18, 4, 0, 0, nullptr, MCTF_Cutout));
	Add(Def(TEXT("flowering_azalea_side"), EMCTexRecipe::Plant, 0x6A8E2E, 0x5A4A30, 0xD86EC4, (float)EMCPlantKind::Azalea, 1, 0, nullptr, MCTF_Cutout));
	// Pale garden resin
	Add(Def(TEXT("resin_block"), EMCTexRecipe::Honeycomb, 0xD9661E, 0xF08A30, 0xA04A12, 1, 0, 0, nullptr, 0));
	Add(Def(TEXT("resin_bricks"), EMCTexRecipe::Bricks, 0xD2641E, 0x8E3E10, 0xEA842E, 2, 0, 0, nullptr, 0));
	Add(Def(TEXT("chiseled_resin_bricks"), EMCTexRecipe::Chiseled, 0xD2641E, 0x8E3E10, 0xEA842E, 4, 0, 0, nullptr, 0));
	Add(Def(TEXT("creaking_heart"), EMCTexRecipe::LogSide, 0x6B645E, 0x4A4540, 0xE07A2A, 2, 8, 0, nullptr, 0));
	Add(Def(TEXT("creaking_heart_top"), EMCTexRecipe::LogTop, 0xE0D6CB, 0xC4B8AA, 0x6B645E, 1, 8, 0, nullptr, 0));
	Add(Def(TEXT("creaking_heart_awake"), EMCTexRecipe::LogSide, 0x6B645E, 0x4A4540, 0xFF9A2A, 3, 8, 0, nullptr, MCTF_Emissive));
}
