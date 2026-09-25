// Terrain / natural stone texture definitions.
#include "Blocks/MCTextures.h"
#include "Blocks/MCTextureDefsCommon.h"

void MCRegisterTerrainTextures(TFunctionRef<void(const FMCTexDef&)> Add)
{
	using namespace MCTexDSL;
	auto T = [&](const TCHAR* N, EMCTexRecipe R, uint32 A, uint32 B, uint32 C = 0x202020, float P0 = 0, float P1 = 0, float P2 = 0, const TCHAR* Base = nullptr, uint8 Flags = 0)
	{
		Add(Def(N, R, A, B, C, P0, P1, P2, Base, Flags));
	};

	// --- Stone family
	T(TEXT("stone"), EMCTexRecipe::Stone, 0x6E6E6E, 0x8A8A8A, 0x5A5A5A);
	T(TEXT("cobblestone"), EMCTexRecipe::Cobble, 0x7C7C7C, 0x4E4E4E, 0x9A9A9A);
	T(TEXT("mossy_cobblestone"), EMCTexRecipe::Cobble, 0x7A7C74, 0x4B4E45, 0x5D7A32, 1.0f);
	T(TEXT("stone_bricks"), EMCTexRecipe::StoneBricks, 0x7E7E7E, 0x5A5A5A, 0x969696);
	T(TEXT("mossy_stone_bricks"), EMCTexRecipe::StoneBricks, 0x7A7C74, 0x55574F, 0x5E7B33, 1.0f);
	T(TEXT("cracked_stone_bricks"), EMCTexRecipe::StoneBricks, 0x7A7A7A, 0x525252, 0x8E8E8E, 2.0f);
	T(TEXT("chiseled_stone_bricks"), EMCTexRecipe::Chiseled, 0x7E7E7E, 0x585858, 0x9A9A9A);
	T(TEXT("smooth_stone"), EMCTexRecipe::Polished, 0x9E9E9E, 0x8A8A8A, 0x707070);
	T(TEXT("smooth_stone_slab_side"), EMCTexRecipe::Polished, 0x9C9C9C, 0x888888, 0x6A6A6A, 1.0f);
	T(TEXT("granite"), EMCTexRecipe::Speckled, 0x8F5E4B, 0xA9735E, 0xC99A86, 1.0f);
	T(TEXT("polished_granite"), EMCTexRecipe::Polished, 0x9A6651, 0xB07A64, 0x7A4B3A, 0.0f, 1.0f);
	T(TEXT("diorite"), EMCTexRecipe::Speckled, 0xBEBEBC, 0xDCDCDA, 0x6E6E6C, 2.0f);
	T(TEXT("polished_diorite"), EMCTexRecipe::Polished, 0xC4C4C2, 0xDADAD8, 0x9A9A98, 0.0f, 1.0f);
	T(TEXT("andesite"), EMCTexRecipe::Speckled, 0x818180, 0x949492, 0x6A6A68, 3.0f);
	T(TEXT("polished_andesite"), EMCTexRecipe::Polished, 0x868684, 0x9A9A98, 0x6C6C6A, 0.0f, 1.0f);
	T(TEXT("tuff"), EMCTexRecipe::Tuff, 0x6C6D66, 0x5A5B54, 0x8B8C7F);
	T(TEXT("polished_tuff"), EMCTexRecipe::Polished, 0x6E7069, 0x5C5E57, 0x4A4C45, 0.0f, 1.0f);
	T(TEXT("tuff_bricks"), EMCTexRecipe::StoneBricks, 0x6A6C64, 0x4E5049, 0x80827A, 0.0f, 1.0f);
	T(TEXT("chiseled_tuff"), EMCTexRecipe::Chiseled, 0x6C6E66, 0x50524B, 0x86887F, 1.0f);
	T(TEXT("chiseled_tuff_top"), EMCTexRecipe::Polished, 0x6C6E66, 0x5A5C55, 0x4A4C45, 2.0f);
	T(TEXT("calcite"), EMCTexRecipe::Calcite, 0xDFE0DA, 0xC9CBC4, 0xF0F0EC);
	T(TEXT("dripstone_block"), EMCTexRecipe::Dripstone, 0x866B5C, 0x6E5648, 0x9E8474);

	// --- Deepslate family
	T(TEXT("deepslate"), EMCTexRecipe::Deepslate, 0x4A4A4F, 0x3A3A3F, 0x5E5E63);
	T(TEXT("deepslate_top"), EMCTexRecipe::DeepslateTop, 0x4C4C51, 0x3A3A3F, 0x5A5A5F);
	T(TEXT("cobbled_deepslate"), EMCTexRecipe::Cobble, 0x505055, 0x2E2E33, 0x68686D, 0.0f, 1.0f);
	T(TEXT("polished_deepslate"), EMCTexRecipe::Polished, 0x48484D, 0x3C3C41, 0x2C2C31, 0.0f, 1.0f);
	T(TEXT("deepslate_bricks"), EMCTexRecipe::StoneBricks, 0x4E4E53, 0x2C2C31, 0x5E5E63, 0.0f, 2.0f);
	T(TEXT("cracked_deepslate_bricks"), EMCTexRecipe::StoneBricks, 0x4A4A4F, 0x2A2A2F, 0x5A5A5F, 2.0f, 2.0f);
	T(TEXT("deepslate_tiles"), EMCTexRecipe::Tiles, 0x46464B, 0x28282D, 0x56565B);
	T(TEXT("cracked_deepslate_tiles"), EMCTexRecipe::Tiles, 0x44444A, 0x26262B, 0x54545A, 1.0f);
	T(TEXT("chiseled_deepslate"), EMCTexRecipe::Chiseled, 0x4A4A4F, 0x2C2C31, 0x5E5E63, 2.0f);
	T(TEXT("reinforced_deepslate_side"), EMCTexRecipe::MetalBlock, 0x3E4347, 0x585E63, 0x2A2D30, 2.0f);
	T(TEXT("reinforced_deepslate_top"), EMCTexRecipe::MetalBlock, 0x3E4347, 0x5C6267, 0x2A2D30, 3.0f);

	// --- Soils
	T(TEXT("dirt"), EMCTexRecipe::Dirt, 0x6E4A30, 0x8A5E3E, 0x4E3320);
	T(TEXT("coarse_dirt"), EMCTexRecipe::Dirt, 0x6A4630, 0x876048, 0x5A5550, 1.0f);
	T(TEXT("rooted_dirt"), EMCTexRecipe::Dirt, 0x6C4A32, 0x8C6244, 0xB09070, 2.0f);
	T(TEXT("grass_block_top"), EMCTexRecipe::GrassTop, 0x8C8C8C, 0xC8C8C8, 0x5A5A5A, 0, 0, 0, nullptr, MCTF_Tinted);
	T(TEXT("grass_block_side"), EMCTexRecipe::GrassSide, 0x6E4A30, 0x8A5E3E, 0xA8A8A8, 0, 0, 0, TEXT("dirt"), MCTF_Tinted);
	T(TEXT("grass_block_snow"), EMCTexRecipe::SnowSide, 0x6E4A30, 0x8A5E3E, 0xF2F6FA, 0, 0, 0, TEXT("dirt"));
	T(TEXT("podzol_top"), EMCTexRecipe::PodzolTop, 0x5B3D1F, 0x7A5230, 0x3E2A15);
	T(TEXT("podzol_side"), EMCTexRecipe::GrassSide, 0x6E4A30, 0x8A5E3E, 0x5E4020, 1.0f, 0, 0, TEXT("dirt"));
	T(TEXT("mycelium_top"), EMCTexRecipe::MyceliumTop, 0x6E6068, 0x8E7C8A, 0x5A4E56);
	T(TEXT("mycelium_side"), EMCTexRecipe::GrassSide, 0x6E4A30, 0x8A5E3E, 0x7A6A76, 2.0f, 0, 0, TEXT("dirt"));
	T(TEXT("dirt_path_top"), EMCTexRecipe::Dirt, 0x8A6A40, 0xA07C4C, 0x6E5230, 3.0f);
	T(TEXT("dirt_path_side"), EMCTexRecipe::GrassSide, 0x6E4A30, 0x8A5E3E, 0x947246, 3.0f, 0, 0, TEXT("dirt"));
	T(TEXT("farmland"), EMCTexRecipe::Dirt, 0x6A4630, 0x8A5E3E, 0x3E2818, 4.0f);
	T(TEXT("farmland_moist"), EMCTexRecipe::Dirt, 0x4A3020, 0x5E3E28, 0x2A1A10, 4.0f, 1.0f);
	T(TEXT("mud"), EMCTexRecipe::Mud, 0x3C3437, 0x4A4144, 0x2A2427);
	T(TEXT("packed_mud"), EMCTexRecipe::Mud, 0x8C6B51, 0x9E7C60, 0x6E5240, 1.0f);
	T(TEXT("mud_bricks"), EMCTexRecipe::Bricks, 0x8A6A50, 0x5E4838, 0xA07E62, 1.0f);
	T(TEXT("muddy_mangrove_roots_side"), EMCTexRecipe::Mud, 0x4A3E36, 0x6A4E36, 0x2E2622, 2.0f);
	T(TEXT("muddy_mangrove_roots_top"), EMCTexRecipe::Mud, 0x4A3E36, 0x6A4E36, 0x2E2622, 3.0f);
	T(TEXT("moss_block"), EMCTexRecipe::Moss, 0x4E6A26, 0x6A8A36, 0x38501A);
	T(TEXT("pale_moss_block"), EMCTexRecipe::Moss, 0x9CA396, 0xB6BDAF, 0x7E8578, 1.0f);

	// --- Sands, gravel, clay
	T(TEXT("sand"), EMCTexRecipe::Sand, 0xD8C98E, 0xE8DCAA, 0xB8A873);
	T(TEXT("red_sand"), EMCTexRecipe::Sand, 0xB25A26, 0xC86E34, 0x8E4418);
	T(TEXT("suspicious_sand"), EMCTexRecipe::Sand, 0xD4C48A, 0xE4D6A2, 0xA8986A, 1.0f);
	T(TEXT("gravel"), EMCTexRecipe::Gravel, 0x837D78, 0x9A948E, 0x5E5854);
	T(TEXT("suspicious_gravel"), EMCTexRecipe::Gravel, 0x807A75, 0x98928C, 0x5C5652, 1.0f);
	T(TEXT("clay"), EMCTexRecipe::Clay, 0x9EA4B0, 0xB0B6C2, 0x8A909C);
	T(TEXT("sandstone"), EMCTexRecipe::Sandstone, 0xD8C98E, 0xC6B67A, 0xE6DAA6);
	T(TEXT("sandstone_top"), EMCTexRecipe::SandstoneTop, 0xDCCD92, 0xCABB80, 0xEADEAA);
	T(TEXT("sandstone_bottom"), EMCTexRecipe::Sandstone, 0xD4C58A, 0xC0B074, 0xE0D49E, 1.0f);
	T(TEXT("cut_sandstone"), EMCTexRecipe::Sandstone, 0xDACB90, 0xC8B87C, 0xE8DCA8, 2.0f);
	T(TEXT("chiseled_sandstone"), EMCTexRecipe::Chiseled, 0xD8C98E, 0xB8A870, 0xE6DAA6, 3.0f);
	T(TEXT("red_sandstone"), EMCTexRecipe::Sandstone, 0xB5622E, 0x9C5226, 0xC8743C);
	T(TEXT("red_sandstone_top"), EMCTexRecipe::SandstoneTop, 0xB8652F, 0x9E5427, 0xCA763E);
	T(TEXT("red_sandstone_bottom"), EMCTexRecipe::Sandstone, 0xB05E2C, 0x984E24, 0xC2703A, 1.0f);
	T(TEXT("cut_red_sandstone"), EMCTexRecipe::Sandstone, 0xB6632F, 0x9D5327, 0xC9753D, 2.0f);
	T(TEXT("chiseled_red_sandstone"), EMCTexRecipe::Chiseled, 0xB5622E, 0x8E4A22, 0xC8743C, 3.0f);

	// --- Snow & ice
	T(TEXT("snow"), EMCTexRecipe::Snow, 0xEEF4F8, 0xFFFFFF, 0xD2DEE8);
	T(TEXT("powder_snow"), EMCTexRecipe::Snow, 0xF4F8FC, 0xFFFFFF, 0xDCE6EE, 1.0f);
	T(TEXT("ice"), EMCTexRecipe::Ice, 0x8FB8EE, 0xB8D6FA, 0x6E9AD8, 0, 0, 0, nullptr, MCTF_Translucent);
	T(TEXT("packed_ice"), EMCTexRecipe::PackedIce, 0x8AB0E6, 0xA8C8F2, 0x6E94D0);
	T(TEXT("blue_ice"), EMCTexRecipe::PackedIce, 0x6E9FEE, 0x8CB8F6, 0x5080D8, 1.0f);
	T(TEXT("frosted_ice"), EMCTexRecipe::Ice, 0x9CC2F0, 0xC4DEFA, 0x7AA4DC, 1.0f, 0, 0, nullptr, MCTF_Translucent);

	// --- Deep / special
	T(TEXT("bedrock"), EMCTexRecipe::Bedrock, 0x2E2E2E, 0x6A6A6A, 0x141414);
	T(TEXT("obsidian"), EMCTexRecipe::Obsidian, 0x140E1E, 0x2A1E40, 0x3E2C62);
	T(TEXT("crying_obsidian"), EMCTexRecipe::Obsidian, 0x1A0E2A, 0x2E1A48, 0x9A32FF, 1.0f, 0, 0, nullptr, MCTF_Emissive);

	// --- Terracotta (grayscale base is tinted per colour); natural badlands colours are separate
	T(TEXT("terracotta"), EMCTexRecipe::Terracotta, 0x985E43, 0xA66A4D, 0x86523A);
	T(TEXT("terracotta_gray"), EMCTexRecipe::Terracotta, 0xB4B4B4, 0xC6C6C6, 0x9A9A9A, 0, 0, 0, nullptr, MCTF_Tinted);
	T(TEXT("glazed_terracotta"), EMCTexRecipe::GlazedTerracotta, 0xB8B8B8, 0xE8E8E8, 0x6A6A6A, 0, 0, 0, nullptr, MCTF_Tinted);

	// --- Sulfur caves (Minecraft 26.2 content)
	T(TEXT("sulfur"), EMCTexRecipe::Sulfur, 0xC9B23A, 0xE2CE58, 0x8E7A1E);
	T(TEXT("potent_sulfur"), EMCTexRecipe::Sulfur, 0xE6D24A, 0xFFF07A, 0xB09A24, 1.0f, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("polished_sulfur"), EMCTexRecipe::Polished, 0xCDB63E, 0xE0CA52, 0xA08A28, 0.0f, 1.0f);
	T(TEXT("sulfur_bricks"), EMCTexRecipe::StoneBricks, 0xC8B03A, 0x8A7620, 0xDEC858, 0.0f, 3.0f);
	T(TEXT("cinnabar"), EMCTexRecipe::Cinnabar, 0x9E2A24, 0xC23C30, 0x6A1814);
	T(TEXT("polished_cinnabar"), EMCTexRecipe::Polished, 0xA82E26, 0xC03A30, 0x7E2019, 0.0f, 1.0f);
	T(TEXT("cinnabar_bricks"), EMCTexRecipe::StoneBricks, 0xA02C25, 0x6A1A15, 0xC23E32, 0.0f, 3.0f);
}
