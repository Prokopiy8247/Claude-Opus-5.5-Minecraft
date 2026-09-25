// Plants, crops, flowers, fluids and coloured block families.
#include "Blocks/MCTextures.h"
#include "Blocks/MCTextureDefsCommon.h"

void MCRegisterPlantTextures(TFunctionRef<void(const FMCTexDef&)> Add)
{
	using namespace MCTexDSL;
	auto P = [&](const TCHAR* N, EMCPlantKind K, uint32 A, uint32 B, uint32 C = 0x3A5A20, float P1 = 0, uint8 Extra = 0)
	{
		Add(Def(N, EMCTexRecipe::Plant, A, B, C, (float)K, P1, 0, nullptr, (uint8)(MCTF_Cutout | Extra)));
	};

	// Grass-like (tinted)
	P(TEXT("short_grass"), EMCPlantKind::ShortGrass, 0x9A9A9A, 0xC8C8C8, 0x6A6A6A, 0, MCTF_Tinted);
	P(TEXT("tall_grass_bottom"), EMCPlantKind::TallGrassBottom, 0x9A9A9A, 0xC8C8C8, 0x6A6A6A, 0, MCTF_Tinted);
	P(TEXT("tall_grass_top"), EMCPlantKind::TallGrassTop, 0x9A9A9A, 0xC8C8C8, 0x6A6A6A, 0, MCTF_Tinted);
	P(TEXT("fern"), EMCPlantKind::Fern, 0x969696, 0xC4C4C4, 0x606060, 0, MCTF_Tinted);
	P(TEXT("large_fern_bottom"), EMCPlantKind::Fern, 0x969696, 0xC4C4C4, 0x606060, 1, MCTF_Tinted);
	P(TEXT("large_fern_top"), EMCPlantKind::Fern, 0x969696, 0xC4C4C4, 0x606060, 2, MCTF_Tinted);
	P(TEXT("vine"), EMCPlantKind::Vine, 0x8E8E8E, 0xBABABA, 0x5A5A5A, 0, MCTF_Tinted);
	P(TEXT("lily_pad"), EMCPlantKind::LilyPad, 0x9A9A9A, 0xC6C6C6, 0x5A5A5A, 0, MCTF_Tinted);
	P(TEXT("sugar_cane"), EMCPlantKind::SugarCane, 0x9AAA8A, 0xC8D8B4, 0x6A7A5A, 0, MCTF_Tinted);
	P(TEXT("dead_bush"), EMCPlantKind::DeadBush, 0x7A5A30, 0x9A7446, 0x4E3A1E);
	P(TEXT("bush"), EMCPlantKind::Bush, 0x9A9A9A, 0xC8C8C8, 0x6A6A6A, 0, MCTF_Tinted);
	P(TEXT("firefly_bush"), EMCPlantKind::FireflyBush, 0x4E6A2A, 0x6A8E36, 0xE8FF7A, 0, MCTF_Emissive);
	P(TEXT("leaf_litter"), EMCPlantKind::LeafLitter, 0x8A6A3A, 0xB08A4A, 0x5E4424);
	P(TEXT("wildflowers"), EMCPlantKind::Wildflowers, 0x5E8A2E, 0xF0E060, 0xF0F0F0);

	// Flowers: C0 stem/leaf, C1 petal, C2 centre
	P(TEXT("dandelion"), EMCPlantKind::Dandelion, 0x4E8A2A, 0xFFD726, 0xE8A010);
	P(TEXT("poppy"), EMCPlantKind::Poppy, 0x4E7A2A, 0xD8201A, 0x1A1A1A);
	P(TEXT("blue_orchid"), EMCPlantKind::BlueOrchid, 0x4E8A3A, 0x2AA8E8, 0x8AD8FF);
	P(TEXT("allium"), EMCPlantKind::Allium, 0x4E7A2A, 0xB45AE0, 0xE0A8FF);
	P(TEXT("azure_bluet"), EMCPlantKind::AzureBluet, 0x5A8A3A, 0xE8EEF6, 0xF0D040);
	P(TEXT("red_tulip"), EMCPlantKind::RedTulip, 0x4E8A2A, 0xE0301E, 0x8A1010);
	P(TEXT("orange_tulip"), EMCPlantKind::OrangeTulip, 0x4E8A2A, 0xF08A20, 0xA85010);
	P(TEXT("white_tulip"), EMCPlantKind::WhiteTulip, 0x4E8A2A, 0xF2F2EE, 0xC0C8B8);
	P(TEXT("pink_tulip"), EMCPlantKind::PinkTulip, 0x4E8A2A, 0xF0A0C0, 0xC06A8A);
	P(TEXT("oxeye_daisy"), EMCPlantKind::OxeyeDaisy, 0x4E8A2A, 0xF4F4F0, 0xF0C020);
	P(TEXT("cornflower"), EMCPlantKind::Cornflower, 0x4E8A2A, 0x4A6AE8, 0x2A3A9A);
	P(TEXT("lily_of_the_valley"), EMCPlantKind::LilyOfTheValley, 0x3E8A2A, 0xF8F8F4, 0xD8E0D0);
	P(TEXT("torchflower"), EMCPlantKind::Torchflower, 0x5A7A2A, 0xF0A020, 0xFFE070, 0, MCTF_Emissive);
	P(TEXT("wither_rose"), EMCPlantKind::WitherRose, 0x2A2A1A, 0x1A1A1A, 0x3A3A2A);
	P(TEXT("eyeblossom"), EMCPlantKind::Eyeblossom, 0x6A6A5A, 0xE8E0D8, 0xFF8A2A, 1, MCTF_Emissive);
	P(TEXT("closed_eyeblossom"), EMCPlantKind::Eyeblossom, 0x6A6A5A, 0x8A7E88, 0x4A4048, 0);
	P(TEXT("pink_petals"), EMCPlantKind::PinkPetals, 0x5E8A2E, 0xF4A8C8, 0xFFE0EC);
	P(TEXT("sunflower_bottom"), EMCPlantKind::TallFlowerBottom, 0x4E8A2A, 0x5E9A36, 0x3A6A1E, 0);
	P(TEXT("sunflower_top"), EMCPlantKind::Sunflower, 0x4E8A2A, 0xFFD020, 0x6A3A10);
	P(TEXT("lilac_bottom"), EMCPlantKind::TallFlowerBottom, 0x4E8A2A, 0x5E9A36, 0x3A6A1E, 1);
	P(TEXT("lilac_top"), EMCPlantKind::TallFlowerTop, 0x4E8A2A, 0xC88AD8, 0xE8B8F0, 1);
	P(TEXT("rose_bush_bottom"), EMCPlantKind::TallFlowerBottom, 0x3E7A2A, 0x4E8A36, 0x2E5A1E, 2);
	P(TEXT("rose_bush_top"), EMCPlantKind::TallFlowerTop, 0x3E7A2A, 0xD02A2A, 0x8A1414, 2);
	P(TEXT("peony_bottom"), EMCPlantKind::TallFlowerBottom, 0x4E8A2A, 0x5E9A36, 0x3A6A1E, 3);
	P(TEXT("peony_top"), EMCPlantKind::TallFlowerTop, 0x4E8A2A, 0xF0B0D0, 0xD88AB0, 3);
	P(TEXT("pitcher_plant"), EMCPlantKind::TallFlowerTop, 0x3E7A5A, 0x6AA8C8, 0x2E5A4A, 4);
	P(TEXT("brown_mushroom"), EMCPlantKind::BrownMushroom, 0xD8CCB0, 0x9A7456, 0x6E5038);
	P(TEXT("red_mushroom"), EMCPlantKind::RedMushroom, 0xE8E0D0, 0xD02020, 0xF8F8F8);
	P(TEXT("cobweb"), EMCPlantKind::Cobweb, 0xE8E8E8, 0xFFFFFF, 0xC8C8C8);
	P(TEXT("sweet_berry_bush"), EMCPlantKind::SweetBerryBush, 0x3E6A2A, 0xB01A2A, 0x2A4A1A);
	P(TEXT("sweet_berry_bush_young"), EMCPlantKind::SweetBerryBush, 0x3E6A2A, 0x3E6A2A, 0x2A4A1A, 1);
	P(TEXT("hanging_roots"), EMCPlantKind::HangingRoots, 0xA07A5A, 0xC49A74, 0x6E5038);
	P(TEXT("spore_blossom"), EMCPlantKind::SporeBlossom, 0x5E8A2E, 0xE870B8, 0xFFB0E0);
	P(TEXT("cave_vines"), EMCPlantKind::CaveVines, 0x4E7A2A, 0x6A9A36, 0x2E4A1A, 0);
	P(TEXT("cave_vines_lit"), EMCPlantKind::CaveVines, 0x4E7A2A, 0xFFA830, 0xFFE070, 1, MCTF_Emissive);
	P(TEXT("glow_lichen"), EMCPlantKind::GlowLichen, 0x6A7A6A, 0x9AE0B0, 0xD8FFE0, 0, MCTF_Emissive);
	P(TEXT("pale_hanging_moss"), EMCPlantKind::PaleHangingMoss, 0x9CA396, 0xBEC4B6, 0x7A8074);
	P(TEXT("bamboo_sapling"), EMCPlantKind::BambooSapling, 0x5E8A2E, 0x7AA83E, 0x3E6A1E);
	P(TEXT("bamboo_stalk"), EMCPlantKind::Stem, 0x7E9A32, 0x9AB84A, 0x5A7A22, 1);
	P(TEXT("small_dripleaf"), EMCPlantKind::Dripleaf, 0x4E8A2A, 0x6AAA3A, 0x3A6A1E);
	P(TEXT("big_dripleaf_top"), EMCPlantKind::Dripleaf, 0x5E9A2E, 0x7ABA3E, 0x3A6A1E, 1);
	P(TEXT("seagrass"), EMCPlantKind::Seagrass, 0x3E8A3A, 0x5AAA4A, 0x2A5A2A);
	P(TEXT("tall_seagrass_bottom"), EMCPlantKind::Seagrass, 0x3E8A3A, 0x5AAA4A, 0x2A5A2A, 1);
	P(TEXT("tall_seagrass_top"), EMCPlantKind::Seagrass, 0x3E8A3A, 0x5AAA4A, 0x2A5A2A, 2);
	P(TEXT("kelp"), EMCPlantKind::Kelp, 0x4E7A2A, 0x6E9A36, 0x3A5A1E);
	P(TEXT("kelp_plant"), EMCPlantKind::Kelp, 0x4E7A2A, 0x6E9A36, 0x3A5A1E, 1);
	P(TEXT("pointed_dripstone"), EMCPlantKind::PointedDripstone, 0x866B5C, 0x9E8474, 0x6E5648);
	P(TEXT("sulfur_spike"), EMCPlantKind::SulfurSpike, 0xC9B23A, 0xF0DA5A, 0x8E7A1E, 0, MCTF_Emissive);
	P(TEXT("fire"), EMCPlantKind::Fire, 0xFF6A10, 0xFFD040, 0xFF2A00, 0, MCTF_Emissive);
	P(TEXT("soul_fire"), EMCPlantKind::SoulFire, 0x2AD8E8, 0x9AFFFF, 0x107A9A, 0, MCTF_Emissive);
	P(TEXT("melon_stem"), EMCPlantKind::Stem, 0x9A9A9A, 0xC0C0C0, 0x6A6A6A, 0, MCTF_Tinted);
	P(TEXT("pumpkin_stem"), EMCPlantKind::Stem, 0x9A9A9A, 0xC0C0C0, 0x6A6A6A, 0, MCTF_Tinted);

	// Crops (stage in P1: 0..3 maps age ranges)
	for (int32 S = 0; S < 4; ++S)
	{
		P(*FString::Printf(TEXT("wheat_stage%d"), S), EMCPlantKind::Wheat, 0x5E9A2E, 0xD8B84A, 0x3E6A1E, (float)S);
		P(*FString::Printf(TEXT("carrots_stage%d"), S), EMCPlantKind::Carrots, 0x4E9A2E, 0xF08A20, 0x2E6A1E, (float)S);
		P(*FString::Printf(TEXT("potatoes_stage%d"), S), EMCPlantKind::Potatoes, 0x4E8A2E, 0xC8A060, 0x2E6A1E, (float)S);
		P(*FString::Printf(TEXT("beetroots_stage%d"), S), EMCPlantKind::Beetroots, 0x4E8A2E, 0xA01A3A, 0x2E6A1E, (float)S);
	}
	for (int32 S = 0; S < 3; ++S)
	{
		P(*FString::Printf(TEXT("nether_wart_stage%d"), S), EMCPlantKind::NetherWart, 0x8A1A1A, 0xC02A2A, 0x5A0E0E, (float)S);
	}
	P(TEXT("torchflower_crop"), EMCPlantKind::Torchflower, 0x5A7A2A, 0x6A9A36, 0x3E6A1E, 1);

	// Water & lava base layers (animated by materials)
	Add(Def(TEXT("water_still"), EMCTexRecipe::Water, 0xB0B0B0, 0xD8D8D8, 0x8A8A8A, 0, 0, 0, nullptr, MCTF_Tinted | MCTF_Animated));
	Add(Def(TEXT("water_flow"), EMCTexRecipe::Water, 0xB0B0B0, 0xD8D8D8, 0x8A8A8A, 1, 0, 0, nullptr, MCTF_Tinted | MCTF_Animated));
	Add(Def(TEXT("lava_still"), EMCTexRecipe::Lava, 0xE8520A, 0xFFB030, 0x8A1A00, 0, 0, 0, nullptr, MCTF_Emissive | MCTF_Animated));
	Add(Def(TEXT("lava_flow"), EMCTexRecipe::Lava, 0xE8520A, 0xFFB030, 0x8A1A00, 1, 0, 0, nullptr, MCTF_Emissive | MCTF_Animated));

	// Coloured families (grayscale + tint)
	Add(Def(TEXT("wool"), EMCTexRecipe::Wool, 0xC8C8C8, 0xF0F0F0, 0x9A9A9A, 0, 0, 0, nullptr, MCTF_Tinted));
	Add(Def(TEXT("concrete"), EMCTexRecipe::Concrete, 0xD8D8D8, 0xE6E6E6, 0xC0C0C0, 0, 0, 0, nullptr, MCTF_Tinted));
	Add(Def(TEXT("concrete_powder"), EMCTexRecipe::ConcretePowder, 0xC8C8C8, 0xF0F0F0, 0xA0A0A0, 0, 0, 0, nullptr, MCTF_Tinted));
	Add(Def(TEXT("stained_glass"), EMCTexRecipe::Glass, 0xD8D8D8, 0xFFFFFF, 0xB0B0B0, 1, 0, 0, nullptr, MCTF_Tinted | MCTF_Translucent));
	Add(Def(TEXT("candle"), EMCTexRecipe::Concrete, 0xE0E0D8, 0xF8F8F0, 0xC0C0B8, 2, 0, 0, nullptr, MCTF_Tinted));
	Add(Def(TEXT("shulker_box"), EMCTexRecipe::Purpur, 0xC8C8C8, 0xE8E8E8, 0x8A8A8A, 2, 0, 0, nullptr, MCTF_Tinted));
	Add(Def(TEXT("shulker_box_top"), EMCTexRecipe::Purpur, 0xC8C8C8, 0xE8E8E8, 0x8A8A8A, 3, 0, 0, nullptr, MCTF_Tinted));
	Add(Def(TEXT("bed_cloth"), EMCTexRecipe::Wool, 0xC8C8C8, 0xF0F0F0, 0x9A9A9A, 1, 0, 0, nullptr, MCTF_Tinted));
	Add(Def(TEXT("bed_pillow"), EMCTexRecipe::Wool, 0xF4F4F4, 0xFFFFFF, 0xDADADA, 2, 0, 0, nullptr, 0));

	// Coral (alive & dead)
	struct FCoral { const TCHAR* N; uint32 A, B; };
	const FCoral Corals[5] = {
		{ TEXT("tube"), 0x3450D8, 0x5A7AF0 }, { TEXT("brain"), 0xD85AA0, 0xF08AC0 }, { TEXT("bubble"), 0xA01AA8, 0xC84AD0 },
		{ TEXT("fire"), 0xC82A2A, 0xE8583A }, { TEXT("horn"), 0xD8C83A, 0xF0E060 }
	};
	for (const FCoral& C : Corals)
	{
		Add(Def(*(FString(C.N) + TEXT("_coral_block")), EMCTexRecipe::Coral, C.A, C.B, 0x202020, 0, 0, 0, nullptr, 0));
		Add(Def(*(FString(TEXT("dead_")) + C.N + TEXT("_coral_block")), EMCTexRecipe::Coral, 0x8A847E, 0xA69E98, 0x5A5450, 1, 0, 0, nullptr, 0));
		Add(Def(*(FString(C.N) + TEXT("_coral")), EMCTexRecipe::Plant, C.A, C.B, 0x202020, (float)EMCPlantKind::Kelp, 5, 0, nullptr, MCTF_Cutout));
	}
}
