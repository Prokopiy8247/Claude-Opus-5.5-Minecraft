// Ores, mineral storage blocks, copper family, quartz, amethyst.
#include "Blocks/MCTextures.h"
#include "Blocks/MCTextureDefsCommon.h"

void MCRegisterMineralTextures(TFunctionRef<void(const FMCTexDef&)> Add)
{
	using namespace MCTexDSL;
	auto T = [&](const TCHAR* N, EMCTexRecipe R, uint32 A, uint32 B, uint32 C = 0x202020, float P0 = 0, float P1 = 0, float P2 = 0, const TCHAR* Base = nullptr, uint8 Flags = 0)
	{
		Add(Def(N, R, A, B, C, P0, P1, P2, Base, Flags));
	};

	// Ore style (P0): 0 = coal lumps, 1 = metal flecks, 2 = gem crystals, 3 = glowing dust, 4 = lapis veins, 5 = quartz shards
	struct FOre { const TCHAR* Name; uint32 A, B, C; float Style; uint8 Flags; };
	const FOre Ores[] = {
		{ TEXT("coal"),     0x1E1E1E, 0x3A3A3A, 0x0E0E0E, 0, 0 },
		{ TEXT("iron"),     0xC89A7C, 0xE2BCA2, 0x8E6A54, 1, MCTF_Metal },
		{ TEXT("copper"),   0xC06A3E, 0x6FB09A, 0x8E4A2A, 1, MCTF_Metal },
		{ TEXT("gold"),     0xE8C23A, 0xFFE878, 0xA8841E, 1, MCTF_Metal },
		{ TEXT("redstone"), 0xC80E0E, 0xFF4A3A, 0x7A0808, 3, MCTF_Emissive },
		{ TEXT("lapis"),    0x1E48B8, 0x4A74E0, 0x102C7A, 4, 0 },
		{ TEXT("diamond"),  0x5EE2DA, 0xB8FFF8, 0x2A9E98, 2, 0 },
		{ TEXT("emerald"),  0x18C24A, 0x7AF09A, 0x0A7A2A, 2, 0 },
	};
	for (const FOre& O : Ores)
	{
		T(*(FString(O.Name) + TEXT("_ore")), EMCTexRecipe::Ore, O.A, O.B, O.C, O.Style, 0, 0, TEXT("stone"), O.Flags);
		T(*(FString(TEXT("deepslate_")) + O.Name + TEXT("_ore")), EMCTexRecipe::Ore, O.A, O.B, O.C, O.Style, 1, 0, TEXT("deepslate"), O.Flags);
	}
	T(TEXT("nether_gold_ore"), EMCTexRecipe::Ore, 0xE8C23A, 0xFFE878, 0xA8841E, 1, 2, 0, TEXT("netherrack"), MCTF_Metal);
	T(TEXT("nether_quartz_ore"), EMCTexRecipe::Ore, 0xE8E0D6, 0xFFFFFF, 0xB8AEA2, 5, 2, 0, TEXT("netherrack"), 0);
	T(TEXT("ancient_debris_side"), EMCTexRecipe::RawBlock, 0x5E4238, 0x7A5A4E, 0x3A2620, 2);
	T(TEXT("ancient_debris_top"), EMCTexRecipe::RawBlock, 0x5A3E34, 0x76564A, 0x36241E, 3);

	// Storage blocks
	T(TEXT("coal_block"), EMCTexRecipe::GemBlock, 0x141414, 0x2A2A2A, 0x080808, 1);
	T(TEXT("iron_block"), EMCTexRecipe::MetalBlock, 0xD8D8D8, 0xF2F2F2, 0x9A9A9A, 0, 0, 0, nullptr, MCTF_Metal);
	T(TEXT("gold_block"), EMCTexRecipe::MetalBlock, 0xF2CC3A, 0xFFEA7A, 0xB8901E, 0, 0, 0, nullptr, MCTF_Metal);
	T(TEXT("diamond_block"), EMCTexRecipe::GemBlock, 0x62E6DE, 0xC0FFFA, 0x2E9E98, 0);
	T(TEXT("emerald_block"), EMCTexRecipe::GemBlock, 0x2AD05A, 0x8AF6AA, 0x0E8A34, 0);
	T(TEXT("lapis_block"), EMCTexRecipe::GemBlock, 0x1E48B8, 0x4A74E0, 0x102C7A, 2);
	T(TEXT("redstone_block"), EMCTexRecipe::GemBlock, 0xB00C0C, 0xFF3A2A, 0x5E0606, 3, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("netherite_block"), EMCTexRecipe::MetalBlock, 0x3E3A3C, 0x5E585A, 0x2A2628, 1, 0, 0, nullptr, MCTF_Metal);
	T(TEXT("raw_iron_block"), EMCTexRecipe::RawBlock, 0xA6806A, 0xC8A28A, 0x6E5242, 0);
	T(TEXT("raw_copper_block"), EMCTexRecipe::RawBlock, 0x9A5A36, 0xC47A4E, 0x5E341E, 0);
	T(TEXT("raw_gold_block"), EMCTexRecipe::RawBlock, 0xD6A62A, 0xF6CE52, 0x946E16, 0);

	// Quartz
	T(TEXT("quartz_block_side"), EMCTexRecipe::Polished, 0xEAE4DC, 0xF6F2EC, 0xCEC6BA, 3);
	T(TEXT("quartz_block_top"), EMCTexRecipe::Polished, 0xECE6DE, 0xF8F4EE, 0xD0C8BC, 3, 1);
	T(TEXT("quartz_block_bottom"), EMCTexRecipe::Polished, 0xE6E0D8, 0xF2EEE8, 0xCAC2B6, 3, 2);
	T(TEXT("chiseled_quartz_block"), EMCTexRecipe::Chiseled, 0xEAE4DC, 0xC6BEB2, 0xFAF6F0, 5);
	T(TEXT("chiseled_quartz_block_top"), EMCTexRecipe::Polished, 0xEAE4DC, 0xC6BEB2, 0xFAF6F0, 4);
	T(TEXT("quartz_bricks"), EMCTexRecipe::StoneBricks, 0xEAE4DC, 0xC0B8AC, 0xF8F4EE, 0, 4);
	T(TEXT("quartz_pillar"), EMCTexRecipe::PillarSide, 0xEAE4DC, 0xC6BEB2, 0xFAF6F0);
	T(TEXT("quartz_pillar_top"), EMCTexRecipe::PillarTop, 0xEAE4DC, 0xC6BEB2, 0xFAF6F0);

	// Amethyst
	T(TEXT("amethyst_block"), EMCTexRecipe::Amethyst, 0x8A5CC8, 0xB88AF0, 0x5A3A8E);
	T(TEXT("budding_amethyst"), EMCTexRecipe::Amethyst, 0x8458C2, 0xC89AF8, 0x54368A, 1);
	T(TEXT("amethyst_cluster"), EMCTexRecipe::Plant, 0xA070E0, 0xD8B0FF, 0x6A44A8, (float)EMCPlantKind::AmethystCluster, 0, 0, nullptr, MCTF_Cutout | MCTF_Emissive);

	// Copper oxidation family: 4 stages
	struct FCopper { const TCHAR* Prefix; uint32 A, B, C; };
	const FCopper Stages[4] = {
		{ TEXT(""),           0xC0704A, 0xE08E62, 0x8E4A2E },
		{ TEXT("exposed_"),   0xA67A62, 0xC89A80, 0x7A8A6E },
		{ TEXT("weathered_"), 0x6C9A7E, 0x8AB89A, 0x4E7A62 },
		{ TEXT("oxidized_"),  0x52A68A, 0x74C6A8, 0x3A8468 },
	};
	for (int32 S = 0; S < 4; ++S)
	{
		const FCopper& C = Stages[S];
		const FString P = C.Prefix;
		T(*(P + TEXT("copper_block")), EMCTexRecipe::CopperBlock, C.A, C.B, C.C, 0, (float)S, 0, nullptr, MCTF_Metal);
		T(*(P + TEXT("cut_copper")), EMCTexRecipe::CopperBlock, C.A, C.B, C.C, 1, (float)S, 0, nullptr, MCTF_Metal);
		T(*(P + TEXT("chiseled_copper")), EMCTexRecipe::CopperBlock, C.A, C.B, C.C, 2, (float)S, 0, nullptr, MCTF_Metal);
		T(*(P + TEXT("copper_grate")), EMCTexRecipe::CopperBlock, C.A, C.B, C.C, 3, (float)S, 0, nullptr, MCTF_Metal | MCTF_Cutout);
		T(*(P + TEXT("copper_bulb")), EMCTexRecipe::CopperBlock, C.A, C.B, C.C, 4, (float)S, 0, nullptr, MCTF_Metal);
		T(*(P + TEXT("copper_bulb_lit")), EMCTexRecipe::CopperBlock, C.A, C.B, 0xFFD890, 5, (float)S, 0, nullptr, MCTF_Metal | MCTF_Emissive);
		T(*(P + TEXT("copper_door_bottom")), EMCTexRecipe::Door, C.A, C.B, C.C, 0, 20 + (float)S, 0, nullptr, MCTF_Metal | MCTF_Cutout);
		T(*(P + TEXT("copper_door_top")), EMCTexRecipe::Door, C.A, C.B, C.C, 1, 20 + (float)S, 0, nullptr, MCTF_Metal | MCTF_Cutout);
		T(*(P + TEXT("copper_trapdoor")), EMCTexRecipe::Trapdoor, C.A, C.B, C.C, 0, 20 + (float)S, 0, nullptr, MCTF_Metal | MCTF_Cutout);
	}
	T(TEXT("iron_door_bottom"), EMCTexRecipe::Door, 0xC8C8C8, 0xE6E6E6, 0x6E6E6E, 0, 30, 0, nullptr, MCTF_Metal | MCTF_Cutout);
	T(TEXT("iron_door_top"), EMCTexRecipe::Door, 0xC8C8C8, 0xE6E6E6, 0x6E6E6E, 1, 30, 0, nullptr, MCTF_Metal | MCTF_Cutout);
	T(TEXT("iron_trapdoor"), EMCTexRecipe::Trapdoor, 0xC8C8C8, 0xE6E6E6, 0x6E6E6E, 0, 30, 0, nullptr, MCTF_Metal | MCTF_Cutout);
	T(TEXT("copper_bars"), EMCTexRecipe::Glass, 0xC0704A, 0xE08E62, 0x8E4A2E, 3, 0, 0, nullptr, MCTF_Cutout | MCTF_Metal);
	T(TEXT("copper_chain"), EMCTexRecipe::Glass, 0xC0704A, 0xE08E62, 0x8E4A2E, 4, 0, 0, nullptr, MCTF_Cutout | MCTF_Metal);
	T(TEXT("copper_lantern"), EMCTexRecipe::Torch, 0xC0704A, 0x8AF0C0, 0x8E4A2E, 3, 0, 0, nullptr, MCTF_Cutout | MCTF_Emissive);
	T(TEXT("copper_torch"), EMCTexRecipe::Torch, 0x7A5A36, 0x9AF0C8, 0xC0704A, 4, 0, 0, nullptr, MCTF_Cutout | MCTF_Emissive);
}
