// Functional, redstone and decorative crafted-block textures.
#include "Blocks/MCTextures.h"
#include "Blocks/MCTextureDefsCommon.h"

void MCRegisterFunctionalTextures(TFunctionRef<void(const FMCTexDef&)> Add)
{
	using namespace MCTexDSL;
	auto T = [&](const TCHAR* N, EMCTexRecipe R, uint32 A, uint32 B, uint32 C = 0x202020, float P0 = 0, float P1 = 0, float P2 = 0, const TCHAR* Base = nullptr, uint8 Flags = 0)
	{
		Add(Def(N, R, A, B, C, P0, P1, P2, Base, Flags));
	};

	// Workstations
	T(TEXT("crafting_table_top"), EMCTexRecipe::CraftingTop, 0xB8945F, 0x6B5335, 0x8A6B3E);
	T(TEXT("crafting_table_side"), EMCTexRecipe::CraftingSide, 0xB8945F, 0x6B5335, 0x8A8A8A);
	T(TEXT("crafting_table_front"), EMCTexRecipe::CraftingFront, 0xB8945F, 0x6B5335, 0x8A8A8A);
	T(TEXT("furnace_front"), EMCTexRecipe::FurnaceFront, 0x7C7C7C, 0x4A4A4A, 0x1A1A1A, 0);
	T(TEXT("furnace_front_on"), EMCTexRecipe::FurnaceFront, 0x7C7C7C, 0x4A4A4A, 0xFF8A20, 1, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("furnace_side"), EMCTexRecipe::FurnaceSide, 0x7C7C7C, 0x5A5A5A, 0x9A9A9A);
	T(TEXT("furnace_top"), EMCTexRecipe::FurnaceTop, 0x8A8A8A, 0x6A6A6A, 0x9E9E9E);
	T(TEXT("blast_furnace_front"), EMCTexRecipe::FurnaceFront, 0x6A6A6E, 0x3E3E42, 0x1A1A1A, 2);
	T(TEXT("blast_furnace_front_on"), EMCTexRecipe::FurnaceFront, 0x6A6A6E, 0x3E3E42, 0xFF9A30, 3, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("blast_furnace_side"), EMCTexRecipe::FurnaceSide, 0x6A6A6E, 0x4A4A4E, 0xB0B0B4, 1);
	T(TEXT("blast_furnace_top"), EMCTexRecipe::FurnaceTop, 0x707074, 0x505054, 0x9A9A9E, 1);
	T(TEXT("smoker_front"), EMCTexRecipe::FurnaceFront, 0x6E5A42, 0x4A3A28, 0x1A1A1A, 4);
	T(TEXT("smoker_front_on"), EMCTexRecipe::FurnaceFront, 0x6E5A42, 0x4A3A28, 0xFF8A20, 5, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("smoker_side"), EMCTexRecipe::FurnaceSide, 0x6E5A42, 0x4E3E2C, 0x8A8A8A, 2);
	T(TEXT("smoker_top"), EMCTexRecipe::FurnaceTop, 0x5E5E5E, 0x3E3E3E, 0x7A7A7A, 2);
	T(TEXT("smoker_bottom"), EMCTexRecipe::FurnaceTop, 0x5A5A5A, 0x3A3A3A, 0x7A7A7A, 3);
	T(TEXT("barrel_side"), EMCTexRecipe::Panel, 0x7A5A36, 0x4E3A22, 0x5A5A5A, 1);
	T(TEXT("barrel_top"), EMCTexRecipe::Panel, 0x8C6A42, 0x5E452A, 0x5A5A5A, 2);
	T(TEXT("barrel_top_open"), EMCTexRecipe::Panel, 0x8C6A42, 0x2A1E12, 0x5A5A5A, 3);
	T(TEXT("barrel_bottom"), EMCTexRecipe::Panel, 0x7C5B38, 0x5E452A, 0x5A5A5A, 4);
	T(TEXT("bookshelf"), EMCTexRecipe::Bookshelf, 0xB8945F, 0x6B5335, 0x8A2A2A);
	T(TEXT("chiseled_bookshelf_empty"), EMCTexRecipe::Bookshelf, 0xB8945F, 0x3A2A1A, 0x2A1E12, 1);
	T(TEXT("chiseled_bookshelf_occupied"), EMCTexRecipe::Bookshelf, 0xB8945F, 0x6B5335, 0x2A4A8A, 2);
	T(TEXT("enchanting_table_top"), EMCTexRecipe::Panel, 0x8A1E24, 0x2A1E3A, 0x3AE0FF, 5, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("enchanting_table_side"), EMCTexRecipe::Panel, 0x2A1E3A, 0x140E1E, 0x8A1E24, 6);
	T(TEXT("enchanting_table_bottom"), EMCTexRecipe::Obsidian, 0x140E1E, 0x2A1E40, 0x3E2C62);
	T(TEXT("anvil"), EMCTexRecipe::MetalBlock, 0x4A4A4E, 0x6A6A6E, 0x2A2A2E, 2, 0, 0, nullptr, MCTF_Metal);
	T(TEXT("anvil_top"), EMCTexRecipe::MetalBlock, 0x4E4E52, 0x727276, 0x2A2A2E, 3, 0, 0, nullptr, MCTF_Metal);
	T(TEXT("smithing_table_top"), EMCTexRecipe::Panel, 0x3A3A44, 0x22222A, 0x8A8A9A, 7);
	T(TEXT("smithing_table_front"), EMCTexRecipe::Panel, 0x4A3A2A, 0x2A2A34, 0x9A9AAA, 8);
	T(TEXT("smithing_table_side"), EMCTexRecipe::Panel, 0x4A3A2A, 0x2A2A34, 0x9A9AAA, 9);
	T(TEXT("smithing_table_bottom"), EMCTexRecipe::Planks, 0x4E341B, 0x3D2814, 0x21170C);
	T(TEXT("stonecutter_top"), EMCTexRecipe::Polished, 0x9A9A9A, 0x7A7A7A, 0x5A5A5A, 5);
	T(TEXT("stonecutter_side"), EMCTexRecipe::Panel, 0x8A8A8A, 0x5A5A5A, 0x6B5335, 10);
	T(TEXT("stonecutter_saw"), EMCTexRecipe::MetalBlock, 0xB8B8B8, 0xE8E8E8, 0x6A6A6A, 4, 0, 0, nullptr, MCTF_Metal | MCTF_Cutout);
	T(TEXT("loom_top"), EMCTexRecipe::Panel, 0xB8945F, 0x6B5335, 0xE8E0D0, 11);
	T(TEXT("loom_front"), EMCTexRecipe::Panel, 0xB8945F, 0x6B5335, 0xE8E0D0, 12);
	T(TEXT("loom_side"), EMCTexRecipe::Panel, 0xB8945F, 0x6B5335, 0xE8E0D0, 13);
	T(TEXT("grindstone_side"), EMCTexRecipe::Polished, 0x8A8A8A, 0xA0A0A0, 0x6A6A6A, 6);
	T(TEXT("grindstone_pivot"), EMCTexRecipe::Planks, 0x6B5335, 0x4E3A24, 0x3A2A18);
	T(TEXT("composter_side"), EMCTexRecipe::Panel, 0x9A7446, 0x6E5230, 0x4E3A20, 14);
	T(TEXT("composter_top"), EMCTexRecipe::Panel, 0x9A7446, 0x3A2A18, 0x4E3A20, 15, 0, 0, nullptr, MCTF_Cutout);
	T(TEXT("compost"), EMCTexRecipe::Dirt, 0x4A3A20, 0x6A5A2E, 0x2E2410, 5);
	T(TEXT("lectern_top"), EMCTexRecipe::Panel, 0xB8945F, 0x8A6B3E, 0xE8E0D0, 16);
	T(TEXT("lectern_sides"), EMCTexRecipe::Panel, 0xB8945F, 0x8A6B3E, 0x6B5335, 17);
	T(TEXT("lectern_base"), EMCTexRecipe::Planks, 0xB8945F, 0x96744A, 0x6B5335);
	T(TEXT("cartography_table_top"), EMCTexRecipe::Panel, 0x6B5335, 0xE8D8B0, 0x3A5A8A, 18);
	T(TEXT("cartography_table_side"), EMCTexRecipe::Panel, 0x6B5335, 0x4E3A24, 0xE8D8B0, 19);
	T(TEXT("fletching_table_top"), EMCTexRecipe::Panel, 0xD8C88E, 0x6B5335, 0xE8E0D0, 20);
	T(TEXT("fletching_table_side"), EMCTexRecipe::Panel, 0xD8C88E, 0x6B5335, 0xE8E0D0, 21);
	T(TEXT("respawn_anchor_top"), EMCTexRecipe::Obsidian, 0x1A0E2A, 0x2E1A48, 0x9A32FF, 2, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("respawn_anchor_side"), EMCTexRecipe::Obsidian, 0x1A0E2A, 0x2E1A48, 0x6A2AAA, 3);
	T(TEXT("respawn_anchor_side_charged"), EMCTexRecipe::Obsidian, 0x1A0E2A, 0x2E1A48, 0xC060FF, 4, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("respawn_anchor_bottom"), EMCTexRecipe::Obsidian, 0x140E1E, 0x2A1E40, 0x3E2C62, 0);
	T(TEXT("lodestone_top"), EMCTexRecipe::Chiseled, 0x8A8A8E, 0x5A5A5E, 0xB0B0B4, 6);
	T(TEXT("lodestone_side"), EMCTexRecipe::Chiseled, 0x8A8A8E, 0x5A5A5E, 0xB0B0B4, 7);
	T(TEXT("jukebox_side"), EMCTexRecipe::Panel, 0x7A5438, 0x4E3322, 0x2A1A10, 22);
	T(TEXT("jukebox_top"), EMCTexRecipe::Panel, 0x7A5438, 0x2A2A2A, 0x4E3322, 23);
	T(TEXT("note_block"), EMCTexRecipe::Panel, 0x7A5438, 0x4E3322, 0x2A1A10, 24);
	T(TEXT("beacon"), EMCTexRecipe::Crystal, 0x8AF0F0, 0xFFFFFF, 0x4ACAD0, 0, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("spawner"), EMCTexRecipe::Glass, 0x2A2E3A, 0x4A5264, 0x14161C, 2, 0, 0, nullptr, MCTF_Cutout | MCTF_Metal);
	T(TEXT("trial_spawner_side"), EMCTexRecipe::Glass, 0x3A3A44, 0xC0703A, 0x14161C, 5, 0, 0, nullptr, MCTF_Cutout | MCTF_Metal);
	T(TEXT("vault_side"), EMCTexRecipe::MetalBlock, 0x3A3A44, 0xC0703A, 0x14161C, 5, 0, 0, nullptr, MCTF_Metal);
	T(TEXT("crafter_top"), EMCTexRecipe::Machine, 0x6A6A6A, 0x3A3A3A, 0xC0703A, 9);
	T(TEXT("crafter_side"), EMCTexRecipe::Machine, 0x6A6A6A, 0x3A3A3A, 0xC0703A, 10);
	T(TEXT("shelf"), EMCTexRecipe::Planks, 0xB8945F, 0x96744A, 0x6B5335, 0, 0, 3);

	// Glass / bars / light sources
	T(TEXT("glass"), EMCTexRecipe::Glass, 0xDCEEF2, 0xFFFFFF, 0xB8D0D8, 0, 0, 0, nullptr, MCTF_Cutout);
	T(TEXT("tinted_glass"), EMCTexRecipe::Glass, 0x2A2632, 0x4A4458, 0x16141C, 1, 0, 0, nullptr, MCTF_Translucent);
	T(TEXT("iron_bars"), EMCTexRecipe::Glass, 0x9A9A9A, 0xD0D0D0, 0x5A5A5A, 3, 0, 0, nullptr, MCTF_Cutout | MCTF_Metal);
	T(TEXT("chain"), EMCTexRecipe::Glass, 0x4A4E58, 0x7A808C, 0x2A2E36, 4, 0, 0, nullptr, MCTF_Cutout | MCTF_Metal);
	T(TEXT("torch"), EMCTexRecipe::Torch, 0x7A5A36, 0xFFD060, 0xFF8A20, 0, 0, 0, nullptr, MCTF_Cutout | MCTF_Emissive);
	T(TEXT("soul_torch"), EMCTexRecipe::Torch, 0x7A5A36, 0x9AFFFF, 0x2AD8E8, 1, 0, 0, nullptr, MCTF_Cutout | MCTF_Emissive);
	T(TEXT("redstone_torch"), EMCTexRecipe::Torch, 0x7A5A36, 0xFF3A2A, 0xB00C0C, 2, 0, 0, nullptr, MCTF_Cutout | MCTF_Emissive);
	T(TEXT("redstone_torch_off"), EMCTexRecipe::Torch, 0x7A5A36, 0x5A1010, 0x3A0808, 2, 1, 0, nullptr, MCTF_Cutout);
	T(TEXT("lantern"), EMCTexRecipe::Torch, 0x3A3E48, 0xFFD070, 0x2A2E36, 5, 0, 0, nullptr, MCTF_Cutout | MCTF_Emissive);
	T(TEXT("soul_lantern"), EMCTexRecipe::Torch, 0x3A3E48, 0x9AFFFF, 0x2A2E36, 6, 0, 0, nullptr, MCTF_Cutout | MCTF_Emissive);
	T(TEXT("campfire_log"), EMCTexRecipe::LogSide, 0x6B5335, 0x3E301E, 0x2A2A2A, 3, 0);
	T(TEXT("campfire_log_lit"), EMCTexRecipe::LogSide, 0x6B5335, 0x3E301E, 0xFF7A20, 4, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("sea_lantern"), EMCTexRecipe::SeaLantern, 0xB8E0D8, 0xF4FFFC, 0x6AA8A0, 0, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("glowstone"), EMCTexRecipe::Glowstone, 0xC8902A, 0xFFE08A, 0x8A5A1A, 0, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("shroomlight"), EMCTexRecipe::Glowstone, 0xE88A3A, 0xFFC878, 0xB05A20, 1, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("ochre_froglight_side"), EMCTexRecipe::Crystal, 0xF0D890, 0xFFF4C8, 0xC8A860, 1, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("verdant_froglight_side"), EMCTexRecipe::Crystal, 0xB8E8A8, 0xE8FFE0, 0x80B070, 1, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("pearlescent_froglight_side"), EMCTexRecipe::Crystal, 0xF0C8E8, 0xFFECFA, 0xC098B8, 1, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("end_rod"), EMCTexRecipe::Crystal, 0xF4EEE8, 0xFFFFFF, 0xB8A898, 2, 0, 0, nullptr, MCTF_Emissive);

	// Utility blocks
	T(TEXT("ladder"), EMCTexRecipe::Ladder, 0x9A7446, 0x6E5230, 0x4E3A20, 0, 0, 0, nullptr, MCTF_Cutout);
	T(TEXT("scaffolding_top"), EMCTexRecipe::Ladder, 0xD8C080, 0xB09A58, 0x8A7640, 1, 0, 0, nullptr, MCTF_Cutout);
	T(TEXT("scaffolding_side"), EMCTexRecipe::Ladder, 0xD8C080, 0xB09A58, 0x8A7640, 2, 0, 0, nullptr, MCTF_Cutout);
	T(TEXT("cauldron_side"), EMCTexRecipe::MetalBlock, 0x3A3A3E, 0x5A5A5E, 0x222226, 5, 0, 0, nullptr, MCTF_Metal);
	T(TEXT("cauldron_inner"), EMCTexRecipe::MetalBlock, 0x2A2A2E, 0x4A4A4E, 0x18181C, 6, 0, 0, nullptr, MCTF_Metal);
	T(TEXT("hopper_outside"), EMCTexRecipe::MetalBlock, 0x3A3A3E, 0x5A5A5E, 0x222226, 7, 0, 0, nullptr, MCTF_Metal);
	T(TEXT("hopper_inside"), EMCTexRecipe::MetalBlock, 0x2A2A2E, 0x4A4A4E, 0x18181C, 8, 0, 0, nullptr, MCTF_Metal);
	T(TEXT("bell"), EMCTexRecipe::MetalBlock, 0xF2CC3A, 0xFFEA7A, 0xB8901E, 9, 0, 0, nullptr, MCTF_Metal);
	T(TEXT("brewing_stand_base"), EMCTexRecipe::Polished, 0x7C7C7C, 0x5A5A5A, 0x4A4A4A, 7);
	T(TEXT("brewing_stand"), EMCTexRecipe::Torch, 0xE8C050, 0xFFE89A, 0x8A6A2A, 7, 0, 0, nullptr, MCTF_Cutout);
	T(TEXT("end_portal_frame_top"), EMCTexRecipe::EndStone, 0x3E6A5A, 0x2A4A40, 0xDBDEA0, 2);
	T(TEXT("end_portal_frame_side"), EMCTexRecipe::EndStone, 0xDBDEA0, 0xC6C888, 0x3E6A5A, 3);
	T(TEXT("end_portal_frame_eye"), EMCTexRecipe::Crystal, 0x1E6A4A, 0x5AE0A0, 0x0E2A1E, 3, 0, 0, nullptr, MCTF_Emissive | MCTF_Cutout);
	T(TEXT("dragon_egg"), EMCTexRecipe::Obsidian, 0x0E0A14, 0x2A1E3A, 0x8A2ABA, 5);
	T(TEXT("sponge"), EMCTexRecipe::Sponge, 0xC8C040, 0xE0DA60, 0x8E8820);
	T(TEXT("wet_sponge"), EMCTexRecipe::Sponge, 0xA8A030, 0xC0BA48, 0x6E6818, 1);
	T(TEXT("slime_block"), EMCTexRecipe::Crystal, 0x6AC050, 0x9AE880, 0x3E8A2A, 4, 0, 0, nullptr, MCTF_Translucent);
	T(TEXT("honey_block"), EMCTexRecipe::Crystal, 0xF0A020, 0xFFC850, 0xB06A10, 5, 0, 0, nullptr, MCTF_Translucent);
	T(TEXT("honeycomb_block"), EMCTexRecipe::Honeycomb, 0xE89A20, 0xFFC040, 0xA86A10);
	T(TEXT("hay_block_side"), EMCTexRecipe::Hay, 0xC8A02A, 0xE8C850, 0x8A2A1A);
	T(TEXT("hay_block_top"), EMCTexRecipe::Hay, 0xC8A02A, 0xE8C850, 0x8A2A1A, 1);
	T(TEXT("bone_block_side"), EMCTexRecipe::Bone, 0xE8E2CC, 0xF8F4E4, 0xB8B09A);
	T(TEXT("bone_block_top"), EMCTexRecipe::Bone, 0xE8E2CC, 0xF8F4E4, 0xB8B09A, 1);
	T(TEXT("dried_kelp_side"), EMCTexRecipe::Hay, 0x3A4A2A, 0x5A6A3A, 0x1E2A14, 2);
	T(TEXT("dried_kelp_top"), EMCTexRecipe::Hay, 0x3A4A2A, 0x5A6A3A, 0x1E2A14, 3);
	T(TEXT("target_side"), EMCTexRecipe::Hay, 0xE8D8C0, 0xD02A2A, 0xC8B89A, 4);
	T(TEXT("target_top"), EMCTexRecipe::Hay, 0xE8D8C0, 0xD02A2A, 0xC8B89A, 5);
	T(TEXT("pumpkin_side"), EMCTexRecipe::Pumpkin, 0xD87A18, 0xF09A30, 0x9A5010, 0);
	T(TEXT("pumpkin_top"), EMCTexRecipe::Pumpkin, 0xD87A18, 0xF09A30, 0x5A6A20, 1);
	T(TEXT("carved_pumpkin"), EMCTexRecipe::Pumpkin, 0xD87A18, 0xF09A30, 0x3A1A04, 2);
	T(TEXT("jack_o_lantern"), EMCTexRecipe::Pumpkin, 0xD87A18, 0xF09A30, 0xFFD040, 3, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("melon_side"), EMCTexRecipe::Melon, 0x6A9A2A, 0x9AC848, 0x3A6A18, 0);
	T(TEXT("melon_top"), EMCTexRecipe::Melon, 0x6A9A2A, 0x9AC848, 0x3A6A18, 1);
	T(TEXT("cactus_side"), EMCTexRecipe::Cactus, 0x3E7A2A, 0x5A9A3A, 0xE8E0C0, 0);
	T(TEXT("cactus_top"), EMCTexRecipe::Cactus, 0x4E8A32, 0x6AAA46, 0xE8E0C0, 1);
	T(TEXT("cactus_bottom"), EMCTexRecipe::Cactus, 0x4E8A32, 0x6AAA46, 0xE8E0C0, 2);
	T(TEXT("brown_mushroom_block"), EMCTexRecipe::Mushroom, 0x946A4A, 0xAE8462, 0xD8C8A8, 0);
	T(TEXT("red_mushroom_block"), EMCTexRecipe::Mushroom, 0xC02020, 0xE03A30, 0xF8F0E8, 1);
	T(TEXT("mushroom_stem"), EMCTexRecipe::Mushroom, 0xD8D0C0, 0xEAE4D8, 0xB8B0A0, 2);
	T(TEXT("mushroom_block_inside"), EMCTexRecipe::Mushroom, 0xD8C8A8, 0xE8DCC0, 0xB8A888, 3);
	T(TEXT("tnt_side"), EMCTexRecipe::TNTSide, 0xD02A20, 0xE8E0C8, 0x2A2A2A);
	T(TEXT("tnt_top"), EMCTexRecipe::TNTTop, 0xD02A20, 0xE8E0C8, 0x2A2A2A);
	T(TEXT("tnt_bottom"), EMCTexRecipe::TNTBottom, 0xD02A20, 0xE8E0C8, 0x2A2A2A);
	T(TEXT("prismarine"), EMCTexRecipe::Prismarine, 0x5AA898, 0x7AC8B8, 0x3A7A6E, 0, 0, 0, nullptr, MCTF_Animated);
	T(TEXT("prismarine_bricks"), EMCTexRecipe::Prismarine, 0x62B0A0, 0x82D0C0, 0x3A7A6E, 1);
	T(TEXT("dark_prismarine"), EMCTexRecipe::Prismarine, 0x345A4E, 0x4A7A6A, 0x1E3A32, 2);
	T(TEXT("sculk"), EMCTexRecipe::Sculk, 0x0C1A24, 0x1A3A4A, 0x2AE8F0, 0, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("sculk_catalyst_top"), EMCTexRecipe::Sculk, 0x0C1A24, 0xE8E2CC, 0x2AE8F0, 1, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("sculk_catalyst_side"), EMCTexRecipe::Sculk, 0x0C1A24, 0xE8E2CC, 0x2AE8F0, 2, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("sculk_sensor_side"), EMCTexRecipe::Sculk, 0x0C1A24, 0x1A3A4A, 0x2AE8F0, 3, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("sculk_shrieker_side"), EMCTexRecipe::Sculk, 0x0C1A24, 0xE8E2CC, 0x2AE8F0, 4, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("sculk_vein"), EMCTexRecipe::Plant, 0x0C1A24, 0x1A3A4A, 0x2AE8F0, (float)EMCPlantKind::GlowLichen, 1, 0, nullptr, MCTF_Cutout | MCTF_Emissive);

	// Redstone components
	T(TEXT("redstone_dust_dot"), EMCTexRecipe::RedstoneDust, 0xC8C8C8, 0xFFFFFF, 0x8A8A8A, 0, 0, 0, nullptr, MCTF_Cutout | MCTF_Tinted);
	T(TEXT("redstone_dust_line"), EMCTexRecipe::RedstoneDust, 0xC8C8C8, 0xFFFFFF, 0x8A8A8A, 1, 0, 0, nullptr, MCTF_Cutout | MCTF_Tinted);
	T(TEXT("redstone_lamp"), EMCTexRecipe::Lamp, 0x6A3A1A, 0x9A5A2A, 0x3A2010, 0);
	T(TEXT("redstone_lamp_on"), EMCTexRecipe::Lamp, 0xD8A050, 0xFFE0A0, 0x8A5A20, 1, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("repeater"), EMCTexRecipe::Polished, 0x9E9E9E, 0x8A8A8A, 0x5A1010, 8);
	T(TEXT("repeater_on"), EMCTexRecipe::Polished, 0x9E9E9E, 0x8A8A8A, 0xFF3A2A, 9, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("comparator"), EMCTexRecipe::Polished, 0x9E9E9E, 0x8A8A8A, 0x5A1010, 10);
	T(TEXT("comparator_on"), EMCTexRecipe::Polished, 0x9E9E9E, 0x8A8A8A, 0xFF3A2A, 11, 0, 0, nullptr, MCTF_Emissive);
	T(TEXT("lever"), EMCTexRecipe::Planks, 0x9A7446, 0x6E5230, 0x4E3A20, 0, 0, 4);
	T(TEXT("piston_side"), EMCTexRecipe::Machine, 0x7C7C7C, 0xB8945F, 0x4A4A4A, 0);
	T(TEXT("piston_top"), EMCTexRecipe::Machine, 0xB8945F, 0x96744A, 0x6A6A6A, 1);
	T(TEXT("piston_top_sticky"), EMCTexRecipe::Machine, 0xB8945F, 0x6AC050, 0x6A6A6A, 2);
	T(TEXT("piston_bottom"), EMCTexRecipe::Machine, 0x7C7C7C, 0x5A5A5A, 0x3A3A3A, 3);
	T(TEXT("piston_inner"), EMCTexRecipe::Machine, 0x6A6A6A, 0x4A4A4A, 0x2A2A2A, 4);
	T(TEXT("dispenser_front"), EMCTexRecipe::Machine, 0x7C7C7C, 0x4A4A4A, 0x1A1A1A, 5);
	T(TEXT("dispenser_front_vertical"), EMCTexRecipe::Machine, 0x7C7C7C, 0x4A4A4A, 0x1A1A1A, 6);
	T(TEXT("dropper_front"), EMCTexRecipe::Machine, 0x7C7C7C, 0x4A4A4A, 0x1A1A1A, 7);
	T(TEXT("dropper_front_vertical"), EMCTexRecipe::Machine, 0x7C7C7C, 0x4A4A4A, 0x1A1A1A, 8);
	T(TEXT("observer_front"), EMCTexRecipe::Machine, 0x6A6A6A, 0x3A3A3A, 0x9A9A9A, 11);
	T(TEXT("observer_back"), EMCTexRecipe::Machine, 0x6A6A6A, 0x3A3A3A, 0x5A1010, 12);
	T(TEXT("observer_back_on"), EMCTexRecipe::Machine, 0x6A6A6A, 0x3A3A3A, 0xFF3A2A, 12, 1, 0, nullptr, MCTF_Emissive);
	T(TEXT("observer_side"), EMCTexRecipe::Machine, 0x6A6A6A, 0x3A3A3A, 0x9A9A9A, 13);
	T(TEXT("observer_top"), EMCTexRecipe::Machine, 0x6A6A6A, 0x3A3A3A, 0x9A9A9A, 14);
	T(TEXT("daylight_detector_top"), EMCTexRecipe::Glass, 0xB8945F, 0x3A5A8A, 0x6B5335, 6);
	T(TEXT("rail"), EMCTexRecipe::Rail, 0x6B5335, 0x9A9A9A, 0x4A4A4A, 0, 0, 0, nullptr, MCTF_Cutout);
	T(TEXT("rail_corner"), EMCTexRecipe::Rail, 0x6B5335, 0x9A9A9A, 0x4A4A4A, 1, 0, 0, nullptr, MCTF_Cutout);
	T(TEXT("powered_rail"), EMCTexRecipe::Rail, 0x6B5335, 0xE8C23A, 0x5A1010, 2, 0, 0, nullptr, MCTF_Cutout);
	T(TEXT("powered_rail_on"), EMCTexRecipe::Rail, 0x6B5335, 0xE8C23A, 0xFF3A2A, 2, 1, 0, nullptr, MCTF_Cutout | MCTF_Emissive);
	T(TEXT("detector_rail"), EMCTexRecipe::Rail, 0x6B5335, 0x9A9A9A, 0x5A1010, 3, 0, 0, nullptr, MCTF_Cutout);
	T(TEXT("detector_rail_on"), EMCTexRecipe::Rail, 0x6B5335, 0x9A9A9A, 0xFF3A2A, 3, 1, 0, nullptr, MCTF_Cutout | MCTF_Emissive);
	T(TEXT("activator_rail"), EMCTexRecipe::Rail, 0x6B5335, 0x9A9A9A, 0x8A1A1A, 4, 0, 0, nullptr, MCTF_Cutout);
	T(TEXT("activator_rail_on"), EMCTexRecipe::Rail, 0x6B5335, 0x9A9A9A, 0xFF3A2A, 4, 1, 0, nullptr, MCTF_Cutout | MCTF_Emissive);

	// Bricks & misc building
	T(TEXT("bricks"), EMCTexRecipe::Bricks, 0x9A4E3A, 0xB8B0A0, 0x7A3A2A);
	T(TEXT("white_marble"), EMCTexRecipe::Polished, 0xECEAE4, 0xFAF8F4, 0xC8C4BC, 12);
}
