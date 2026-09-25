// Nether and End texture definitions (distinct material language per dimension).
#include "Blocks/MCTextures.h"
#include "Blocks/MCTextureDefsCommon.h"

void MCRegisterNetherEndTextures(TFunctionRef<void(const FMCTexDef&)> Add)
{
	using namespace MCTexDSL;
	auto T = [&](const TCHAR* N, EMCTexRecipe R, uint32 A, uint32 B, uint32 C = 0x202020, float P0 = 0, float P1 = 0, float P2 = 0, const TCHAR* Base = nullptr, uint8 Flags = 0)
	{
		Add(Def(N, R, A, B, C, P0, P1, P2, Base, Flags));
	};

	// --- Nether
	T(TEXT("netherrack"), EMCTexRecipe::Netherrack, 0x6A2A2A, 0x8A3A36, 0x4A1A1A);
	T(TEXT("nether_bricks"), EMCTexRecipe::Bricks, 0x3A1C21, 0x1E0E12, 0x4E2830, 2);
	T(TEXT("cracked_nether_bricks"), EMCTexRecipe::Bricks, 0x381A20, 0x1C0C10, 0x4A262E, 3);
	T(TEXT("chiseled_nether_bricks"), EMCTexRecipe::Chiseled, 0x3A1C21, 0x1E0E12, 0x4E2830, 8);
	T(TEXT("red_nether_bricks"), EMCTexRecipe::Bricks, 0x5A0F12, 0x2E0608, 0x761A1E, 2);
	T(TEXT("nether_wart_block"), EMCTexRecipe::WartBlock, 0x7A0B0E, 0x9E1A1A, 0x4A0606);
	T(TEXT("warped_wart_block"), EMCTexRecipe::WartBlock, 0x167C84, 0x2AA0A6, 0x0A4A50, 1);
	T(TEXT("crimson_nylium"), EMCTexRecipe::NyliumTop, 0x8A1F2A, 0xB03040, 0x5A0E18);
	T(TEXT("crimson_nylium_side"), EMCTexRecipe::NyliumSide, 0x8A1F2A, 0xB03040, 0x5A0E18, 0, 0, 0, TEXT("netherrack"));
	T(TEXT("warped_nylium"), EMCTexRecipe::NyliumTop, 0x1D7F72, 0x2EA896, 0x0E4A42, 1);
	T(TEXT("warped_nylium_side"), EMCTexRecipe::NyliumSide, 0x1D7F72, 0x2EA896, 0x0E4A42, 1, 0, 0, TEXT("netherrack"));
	T(TEXT("soul_sand"), EMCTexRecipe::SoulSand, 0x54402F, 0x6E5640, 0x2E2218);
	T(TEXT("soul_soil"), EMCTexRecipe::SoulSand, 0x4B392A, 0x62503C, 0x2A1E14, 1);
	T(TEXT("basalt_side"), EMCTexRecipe::BasaltSide, 0x4D4C52, 0x626168, 0x2E2E34);
	T(TEXT("basalt_top"), EMCTexRecipe::BasaltTop, 0x4D4C52, 0x626168, 0x2E2E34);
	T(TEXT("polished_basalt_side"), EMCTexRecipe::BasaltSide, 0x55545A, 0x6A6970, 0x36363C, 1);
	T(TEXT("polished_basalt_top"), EMCTexRecipe::BasaltTop, 0x55545A, 0x6A6970, 0x36363C, 1);
	T(TEXT("smooth_basalt"), EMCTexRecipe::Polished, 0x4A4A50, 0x58585E, 0x36363C, 13);
	T(TEXT("blackstone"), EMCTexRecipe::Stone, 0x2B2629, 0x3E373B, 0x1A1618, 2);
	T(TEXT("blackstone_top"), EMCTexRecipe::Stone, 0x2D282B, 0x403A3D, 0x1C181A, 3);
	T(TEXT("polished_blackstone"), EMCTexRecipe::Polished, 0x353034, 0x433D41, 0x221E21, 0, 1);
	T(TEXT("polished_blackstone_bricks"), EMCTexRecipe::StoneBricks, 0x353034, 0x1A1618, 0x46404A, 0, 5);
	T(TEXT("cracked_polished_blackstone_bricks"), EMCTexRecipe::StoneBricks, 0x332E32, 0x181416, 0x443E48, 2, 5);
	T(TEXT("chiseled_polished_blackstone"), EMCTexRecipe::Chiseled, 0x353034, 0x1A1618, 0x46404A, 9);
	T(TEXT("gilded_blackstone"), EMCTexRecipe::Ore, 0xE8C23A, 0xFFE878, 0xA8841E, 1, 3, 0, TEXT("blackstone"), MCTF_Metal);
	T(TEXT("magma"), EMCTexRecipe::Magma, 0x5A1E0A, 0xFF7A1A, 0x2A0A04, 0, 0, 0, nullptr, MCTF_Emissive | MCTF_Animated);

	// --- End
	T(TEXT("end_stone"), EMCTexRecipe::EndStone, 0xDBDEA0, 0xC6C888, 0xE8EAB6);
	T(TEXT("end_stone_bricks"), EMCTexRecipe::StoneBricks, 0xDBDEA0, 0xB0B278, 0xE8EAB6, 0, 6);
	T(TEXT("purpur_block"), EMCTexRecipe::Purpur, 0xA87BA8, 0x8E648E, 0xC498C4);
	T(TEXT("purpur_pillar"), EMCTexRecipe::PillarSide, 0xA87BA8, 0x8E648E, 0xC498C4, 1);
	T(TEXT("purpur_pillar_top"), EMCTexRecipe::PillarTop, 0xA87BA8, 0x8E648E, 0xC498C4, 1);
	T(TEXT("chorus_plant"), EMCTexRecipe::Purpur, 0x5E3A5E, 0x8A5A8A, 0x3A203A, 4);
	T(TEXT("chorus_flower"), EMCTexRecipe::Purpur, 0x9A6A9A, 0xD8B0D8, 0x6A3A6A, 5);
	T(TEXT("chorus_flower_dead"), EMCTexRecipe::Purpur, 0x6A5A5A, 0x8A7A7A, 0x3A2E2E, 6);
	T(TEXT("end_portal"), EMCTexRecipe::Portal, 0x0A1418, 0x3AE0C0, 0x000000, 1, 0, 0, nullptr, MCTF_Emissive | MCTF_Animated);
	T(TEXT("nether_portal"), EMCTexRecipe::Portal, 0x5A12B8, 0xB060FF, 0x2A0060, 0, 0, 0, nullptr, MCTF_Emissive | MCTF_Animated | MCTF_Translucent);
	T(TEXT("end_gateway"), EMCTexRecipe::Portal, 0x0A1418, 0xC8F0FF, 0x000000, 2, 0, 0, nullptr, MCTF_Emissive | MCTF_Animated);
}
