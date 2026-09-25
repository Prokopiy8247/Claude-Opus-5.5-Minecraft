// Plants, crops, flowers, aquatic vegetation, mushrooms, special vegetation.
#include "Blocks/MCBlockRegistrar.h"

void MCRegisterPlantBlocks(FMCBlockRegistrar& R)
{
	using T = EMCTab;
	const uint32 Plant = MCB_Plant | MCB_NeedsSupport;

	auto Flower = [&](const TCHAR* Name, int32 Compost = 65)
	{
		return R.Add(Name).Shape(EMCShape::Cross).Beh(EMCBeh::Plant).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant).Tag(TEXT("flowers")).Tag(TEXT("small_flowers"));
	};
	R.Add(TEXT("short_grass")).Shape(EMCShape::Cross).Beh(EMCBeh::Plant).Tint(EMCTint::Grass).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant).Replaceable().DropSpecial();
	R.Add(TEXT("fern")).Shape(EMCShape::Cross).Beh(EMCBeh::Plant).Tint(EMCTint::Grass).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant).Replaceable().DropSpecial();
	R.Add(TEXT("tall_grass")).Shape(EMCShape::Cross).Tex(TEXT("tall_grass_bottom")).Orient(EMCCubeOrient::Upper).Alt(0, TEXT("tall_grass_top")).Beh(EMCBeh::TallPlant)
		.Tint(EMCTint::Grass).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant).Replaceable().DropSpecial();
	R.Add(TEXT("large_fern")).Shape(EMCShape::Cross).Tex(TEXT("large_fern_bottom")).Orient(EMCCubeOrient::Upper).Alt(0, TEXT("large_fern_top")).Beh(EMCBeh::TallPlant)
		.Tint(EMCTint::Grass).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant).Replaceable().DropSpecial();
	R.Add(TEXT("dead_bush")).Shape(EMCShape::Cross).Beh(EMCBeh::Plant).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant).Replaceable().DropItem(TEXT("stick"), 0, 2).Burn(60, 100);
	R.Add(TEXT("bush")).Shape(EMCShape::Cross).Beh(EMCBeh::Plant).Tint(EMCTint::Grass).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant).Replaceable();
	R.Add(TEXT("firefly_bush")).Shape(EMCShape::Cross).Beh(EMCBeh::Plant).Light(2).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant);
	R.Add(TEXT("leaf_litter")).Model(EMCModel::Carpet).Beh(EMCBeh::Plant).NoCollision().Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant).Replaceable();
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("wildflowers")).Model(EMCModel::Carpet).Beh(EMCBeh::Plant).NoCollision().Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant).Replaceable();
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("pink_petals")).Model(EMCModel::Carpet).Beh(EMCBeh::Plant).NoCollision().Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant).Replaceable();
	R.Blocks.Last().Layer = EMCLayer::Cutout;

	Flower(TEXT("dandelion")); Flower(TEXT("poppy")); Flower(TEXT("blue_orchid")); Flower(TEXT("allium")); Flower(TEXT("azure_bluet"));
	Flower(TEXT("red_tulip")); Flower(TEXT("orange_tulip")); Flower(TEXT("white_tulip")); Flower(TEXT("pink_tulip")); Flower(TEXT("oxeye_daisy"));
	Flower(TEXT("cornflower")); Flower(TEXT("lily_of_the_valley")); Flower(TEXT("torchflower"));
	Flower(TEXT("wither_rose"));
	Flower(TEXT("open_eyeblossom")).Tex(TEXT("eyeblossom")).Light(3);
	Flower(TEXT("closed_eyeblossom")).Tex(TEXT("closed_eyeblossom"));
	auto Tall = [&](const TCHAR* Name, const TCHAR* Bottom, const TCHAR* Top)
	{
		R.Add(Name).Shape(EMCShape::Cross).Tex(Bottom).Orient(EMCCubeOrient::Upper).Alt(0, Top).Beh(EMCBeh::TallPlant).Hard(0.f).Snd(EMCSound::Grass)
			.Tab(T::Natural).Flag(Plant).Tag(TEXT("flowers")).Tag(TEXT("tall_flowers"));
	};
	Tall(TEXT("sunflower"), TEXT("sunflower_bottom"), TEXT("sunflower_top"));
	Tall(TEXT("lilac"), TEXT("lilac_bottom"), TEXT("lilac_top"));
	Tall(TEXT("rose_bush"), TEXT("rose_bush_bottom"), TEXT("rose_bush_top"));
	Tall(TEXT("peony"), TEXT("peony_bottom"), TEXT("peony_top"));
	Tall(TEXT("pitcher_plant"), TEXT("pitcher_plant"), TEXT("pitcher_plant"));

	R.Add(TEXT("brown_mushroom")).Shape(EMCShape::Cross).Beh(EMCBeh::Mushroom).Light(1).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant).Ticks();
	R.Add(TEXT("red_mushroom")).Shape(EMCShape::Cross).Beh(EMCBeh::Mushroom).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant).Ticks();
	R.Add(TEXT("brown_mushroom_block")).Hard(0.2f).Axe().Snd(EMCSound::Wood).Tab(T::Natural).DropItem(TEXT("brown_mushroom"), 0, 2);
	R.Add(TEXT("red_mushroom_block")).Hard(0.2f).Axe().Snd(EMCSound::Wood).Tab(T::Natural).DropItem(TEXT("red_mushroom"), 0, 2);
	R.Add(TEXT("mushroom_stem")).Hard(0.2f).Axe().Snd(EMCSound::Wood).Tab(T::Natural).DropNone();

	// crops (meta = age)
	R.Add(TEXT("wheat")).Shape(EMCShape::Crop).Tex(TEXT("wheat_stage0")).Orient(EMCCubeOrient::AgeStages).Alt(0, TEXT("wheat_stage0")).Alt(1, TEXT("wheat_stage1"))
		.Alt(2, TEXT("wheat_stage2")).Alt(3, TEXT("wheat_stage3")).Meta(3).Beh(EMCBeh::Crop).Hard(0.f).Snd(EMCSound::Crop).Tab(T::None).Flag(Plant).Ticks().NoItem().Item(TEXT("wheat_seeds")).DropSpecial();
	R.Add(TEXT("carrots")).Shape(EMCShape::Crop).Tex(TEXT("carrots_stage0")).Orient(EMCCubeOrient::AgeStages).Alt(0, TEXT("carrots_stage0")).Alt(1, TEXT("carrots_stage1"))
		.Alt(2, TEXT("carrots_stage2")).Alt(3, TEXT("carrots_stage3")).Meta(3).Beh(EMCBeh::Crop).Hard(0.f).Snd(EMCSound::Crop).Tab(T::None).Flag(Plant).Ticks().NoItem().Item(TEXT("carrot")).DropSpecial();
	R.Add(TEXT("potatoes")).Shape(EMCShape::Crop).Tex(TEXT("potatoes_stage0")).Orient(EMCCubeOrient::AgeStages).Alt(0, TEXT("potatoes_stage0")).Alt(1, TEXT("potatoes_stage1"))
		.Alt(2, TEXT("potatoes_stage2")).Alt(3, TEXT("potatoes_stage3")).Meta(3).Beh(EMCBeh::Crop).Hard(0.f).Snd(EMCSound::Crop).Tab(T::None).Flag(Plant).Ticks().NoItem().Item(TEXT("potato")).DropSpecial();
	R.Add(TEXT("beetroots")).Shape(EMCShape::Crop).Tex(TEXT("beetroots_stage0")).Orient(EMCCubeOrient::AgeStages).Alt(0, TEXT("beetroots_stage0")).Alt(1, TEXT("beetroots_stage1"))
		.Alt(2, TEXT("beetroots_stage2")).Alt(3, TEXT("beetroots_stage3")).Meta(2).Beh(EMCBeh::Crop).Hard(0.f).Snd(EMCSound::Crop).Tab(T::None).Flag(Plant).Ticks().NoItem().Item(TEXT("beetroot_seeds")).DropSpecial();
	R.Add(TEXT("torchflower_crop")).Shape(EMCShape::Crop).Tex(TEXT("torchflower_crop")).Meta(1).Beh(EMCBeh::Crop).Hard(0.f).Snd(EMCSound::Crop).Tab(T::None).Flag(Plant).Ticks().NoItem().Item(TEXT("torchflower_seeds")).DropSpecial();
	R.Add(TEXT("melon_stem")).Shape(EMCShape::Crop).Tint(EMCTint::Stem).Meta(3).Beh(EMCBeh::Stem).Hard(0.f).Snd(EMCSound::Crop).Tab(T::None).Flag(Plant).Ticks().NoItem().Item(TEXT("melon_seeds")).DropSpecial();
	R.Add(TEXT("pumpkin_stem")).Shape(EMCShape::Crop).Tint(EMCTint::Stem).Meta(3).Beh(EMCBeh::Stem).Hard(0.f).Snd(EMCSound::Crop).Tab(T::None).Flag(Plant).Ticks().NoItem().Item(TEXT("pumpkin_seeds")).DropSpecial();
	R.Add(TEXT("melon")).TexTS(TEXT("melon_top"), TEXT("melon_side")).Hard(1.f).Axe().Snd(EMCSound::Wood).Tab(T::Natural).DropItem(TEXT("melon_slice"), 3, 7, true);
	R.Add(TEXT("pumpkin")).TexTS(TEXT("pumpkin_top"), TEXT("pumpkin_side")).Hard(1.f).Axe().Snd(EMCSound::Wood).Tab(T::Natural).Beh(EMCBeh::Pumpkin);
	R.Add(TEXT("carved_pumpkin")).TexFBSTB(TEXT("carved_pumpkin"), TEXT("pumpkin_side"), TEXT("pumpkin_side"), TEXT("pumpkin_top"), TEXT("pumpkin_top"))
		.Orient(EMCCubeOrient::Facing4).Beh(EMCBeh::Pumpkin).Hard(1.f).Axe().Snd(EMCSound::Wood).Tab(T::Natural);
	R.Add(TEXT("sugar_cane")).Shape(EMCShape::Cross).Tint(EMCTint::Grass).Beh(EMCBeh::SugarCane).Meta(4).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant).Ticks();
	R.Add(TEXT("cactus")).TexTBS(TEXT("cactus_top"), TEXT("cactus_bottom"), TEXT("cactus_side")).Model(EMCModel::Cactus, 4).Beh(EMCBeh::Cactus)
		.Hard(0.4f).Snd(EMCSound::Wool).Tab(T::Natural).Flag(MCB_Solid | MCB_NeedsSupport | MCB_Hot).Ticks();
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("bamboo")).Tex(TEXT("bamboo_stalk")).Model(EMCModel::Bamboo, 3).Beh(EMCBeh::Bamboo).Hard(1.f).Axe().Snd(EMCSound::Bamboo).Tab(T::Natural).Flag(MCB_NeedsSupport).Ticks().Fuel(50);
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("bamboo_sapling")).Shape(EMCShape::Cross).Beh(EMCBeh::Bamboo).Hard(1.f).Snd(EMCSound::Bamboo).Tab(T::None).Flag(Plant).Ticks().NoItem().Item(TEXT("bamboo"));
	R.Add(TEXT("sweet_berry_bush")).Shape(EMCShape::Cross).Tex(TEXT("sweet_berry_bush_young")).Orient(EMCCubeOrient::AgeStages).Alt(0, TEXT("sweet_berry_bush_young"))
		.Alt(1, TEXT("sweet_berry_bush_young")).Alt(2, TEXT("sweet_berry_bush")).Alt(3, TEXT("sweet_berry_bush")).Meta(2).Beh(EMCBeh::SweetBerryBush)
		.Hard(0.f).Snd(EMCSound::Grass).Tab(T::None).Flag(Plant | MCB_Interact).Ticks().NoItem().Item(TEXT("sweet_berries"));
	R.Add(TEXT("vine")).Model(EMCModel::Vine, 5).Beh(EMCBeh::Vine).NoCollision().Tint(EMCTint::Foliage).Hard(0.2f).Snd(EMCSound::Grass).Tab(T::Natural)
		.Flag(MCB_Climbable | MCB_Replaceable).Ticks().Burn(15, 100);
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("glow_lichen")).Model(EMCModel::GlowLichen, 6).Beh(EMCBeh::GlowLichen).NoCollision().Light(7).Hard(0.2f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(MCB_Replaceable);
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("lily_pad")).Model(EMCModel::LilyPad).Tint(EMCTint::Lily).Beh(EMCBeh::LilyPad).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(MCB_NeedsSupport);
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("hanging_roots")).Shape(EMCShape::Cross).Beh(EMCBeh::Plant).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant).Replaceable();
	R.Add(TEXT("spore_blossom")).Shape(EMCShape::Cross).Beh(EMCBeh::Plant).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant);
	R.Add(TEXT("cave_vines")).Shape(EMCShape::Cross).Orient(EMCCubeOrient::LitToggle).Alt(0, TEXT("cave_vines_lit")).Beh(EMCBeh::CaveVines).Hard(0.f)
		.Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant | MCB_Climbable | MCB_Interact).Ticks().Item(TEXT("glow_berries"));
	R.Add(TEXT("small_dripleaf")).Shape(EMCShape::Cross).Beh(EMCBeh::Plant).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant);
	R.Add(TEXT("big_dripleaf")).Tex(TEXT("big_dripleaf_top")).Model(EMCModel::BigDripleaf, 2).Beh(EMCBeh::Dripleaf).Hard(0.1f).Axe().Snd(EMCSound::Grass).Tab(T::Natural).Flag(MCB_Solid | MCB_NeedsSupport);
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("pale_hanging_moss")).Shape(EMCShape::Cross).Beh(EMCBeh::Plant).Hard(0.f).Snd(EMCSound::Moss).Tab(T::Natural).Flag(Plant).Replaceable();
	R.Add(TEXT("moss_carpet")).Tex(TEXT("moss_block")).Model(EMCModel::Carpet).Hard(0.1f).Snd(EMCSound::Moss).Tab(T::Natural).Flag(MCB_NeedsSupport);
	R.Add(TEXT("pale_moss_carpet")).Tex(TEXT("pale_moss_block")).Model(EMCModel::Carpet).Hard(0.1f).Snd(EMCSound::Moss).Tab(T::Natural).Flag(MCB_NeedsSupport);

	// aquatic
	R.Add(TEXT("seagrass")).Shape(EMCShape::Cross).Beh(EMCBeh::WaterPlant).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant | MCB_Waterlogged).Replaceable().DropNone();
	R.Add(TEXT("tall_seagrass")).Shape(EMCShape::Cross).Tex(TEXT("tall_seagrass_bottom")).Orient(EMCCubeOrient::Upper).Alt(0, TEXT("tall_seagrass_top")).Beh(EMCBeh::WaterPlant)
		.Hard(0.f).Snd(EMCSound::Grass).Tab(T::None).Flag(Plant | MCB_Waterlogged).Replaceable().DropNone();
	R.Add(TEXT("kelp")).Shape(EMCShape::Cross).Beh(EMCBeh::WaterPlant).Hard(0.f).Snd(EMCSound::Grass).Tab(T::Natural).Flag(Plant | MCB_Waterlogged).Ticks();
	R.Add(TEXT("kelp_plant")).Shape(EMCShape::Cross).Beh(EMCBeh::WaterPlant).Hard(0.f).Snd(EMCSound::Grass).Tab(T::None).Flag(Plant | MCB_Waterlogged).Item(TEXT("kelp")).NoItem().DropItem(TEXT("kelp"));
	R.Add(TEXT("sea_pickle")).Model(EMCModel::SeaPickle, 2).Beh(EMCBeh::WaterPlant).Light(6).Hard(0.f).Snd(EMCSound::Slime).Tab(T::Natural).Flag(MCB_Waterlogged | MCB_NeedsSupport).Tex(TEXT("moss_block"));
	const TCHAR* Corals[5] = { TEXT("tube"), TEXT("brain"), TEXT("bubble"), TEXT("fire"), TEXT("horn") };
	for (const TCHAR* C : Corals)
	{
		R.Add(*(FString(C) + TEXT("_coral_block"))).Hard(1.5f, 6).Pick().Snd(EMCSound::Coral).Tab(T::Natural).DropItem(*(FString(TEXT("dead_")) + C + TEXT("_coral_block")));
		R.Blocks.Last().Drop.bSilkSelf = true;
		R.Add(*(FString(TEXT("dead_")) + C + TEXT("_coral_block"))).Hard(1.5f, 6).Pick().Snd(EMCSound::Stone).Tab(T::Natural);
		R.Add(*(FString(C) + TEXT("_coral"))).Shape(EMCShape::Cross).Beh(EMCBeh::WaterPlant).Hard(0.f).Snd(EMCSound::Coral).Tab(T::Natural).Flag(Plant | MCB_Waterlogged).DropSilk();
	}
}

void MCRegisterNetherEndBlocks(FMCBlockRegistrar& R)
{
	using T = EMCTab;
	const uint32 Plant = MCB_Plant | MCB_NeedsSupport;
	R.Add(TEXT("netherrack")).Hard(0.4f).Pick().Snd(EMCSound::Netherrack).Tab(T::Natural).Flag(MCB_Stone).Map(0x700200).RandomRotate();
	R.Add(TEXT("nether_bricks")).Hard(2.f, 6).Pick().Snd(EMCSound::Netherrack).Tab(T::Building).Fam(TEXT("nether_bricks"), TEXT("base"));
	R.Add(TEXT("cracked_nether_bricks")).Hard(2.f, 6).Pick().Snd(EMCSound::Netherrack).Tab(T::Building);
	R.Add(TEXT("chiseled_nether_bricks")).Hard(2.f, 6).Pick().Snd(EMCSound::Netherrack).Tab(T::Building);
	R.Add(TEXT("red_nether_bricks")).Hard(2.f, 6).Pick().Snd(EMCSound::Netherrack).Tab(T::Building).Fam(TEXT("red_nether_bricks"), TEXT("base"));
	R.StoneSet(TEXT("nether_bricks"), true);
	R.Fence(TEXT("nether_bricks"), TEXT("nether_brick_fence"), true).Pick();
	R.StoneSet(TEXT("red_nether_bricks"), true);
	R.Add(TEXT("soul_sand")).Hard(0.5f).Shovel().Snd(EMCSound::SoulSand).Tab(T::Natural).Speed(0.4f).Beh(EMCBeh::SoulSand).Flag(MCB_Soil).Map(0x54402F);
	R.Blocks.Last().Flags &= ~MCB_Opaque;
	R.Blocks.Last().Shape = EMCShape::Model;
	R.Blocks.Last().Model = EMCModel::Farmland;
	R.Blocks.Last().LightOpacity = 15;
	R.Add(TEXT("soul_soil")).Hard(0.5f).Shovel().Snd(EMCSound::SoulSand).Tab(T::Natural).Flag(MCB_Soil).Map(0x4B392A);
	R.Add(TEXT("basalt")).TexTS(TEXT("basalt_top"), TEXT("basalt_side")).Orient(EMCCubeOrient::Axis).Beh(EMCBeh::Axis).Hard(1.25f, 4.2f).Pick().Snd(EMCSound::Basalt).Tab(T::Natural).Map(0x4D4C52);
	R.Add(TEXT("polished_basalt")).TexTS(TEXT("polished_basalt_top"), TEXT("polished_basalt_side")).Orient(EMCCubeOrient::Axis).Beh(EMCBeh::Axis).Hard(1.25f, 4.2f).Pick().Snd(EMCSound::Basalt).Tab(T::Building);
	R.Add(TEXT("smooth_basalt")).Hard(1.25f, 4.2f).Pick().Snd(EMCSound::Basalt).Tab(T::Building);
	R.Add(TEXT("blackstone")).TexTS(TEXT("blackstone_top"), TEXT("blackstone")).Hard(1.5f, 6).Pick().Tab(T::Building).Fam(TEXT("blackstone"), TEXT("base")).Map(0x2B2629);
	R.Add(TEXT("polished_blackstone")).Hard(2.f, 6).Pick().Tab(T::Building).Fam(TEXT("polished_blackstone"), TEXT("base"));
	R.Add(TEXT("polished_blackstone_bricks")).Hard(1.5f, 6).Pick().Tab(T::Building).Fam(TEXT("polished_blackstone_bricks"), TEXT("base"));
	R.Add(TEXT("cracked_polished_blackstone_bricks")).Hard(1.5f, 6).Pick().Tab(T::Building);
	R.Add(TEXT("chiseled_polished_blackstone")).Hard(1.5f, 6).Pick().Tab(T::Building);
	R.Add(TEXT("gilded_blackstone")).Hard(1.5f, 6).Pick().DropSpecial().Tab(T::Natural);
	R.StoneSet(TEXT("blackstone"), true);
	R.StoneSet(TEXT("polished_blackstone"), true);
	R.Button(TEXT("polished_blackstone"), TEXT("polished_blackstone_button"), false);
	R.PressurePlate(TEXT("polished_blackstone"), TEXT("polished_blackstone_pressure_plate"), false);
	R.StoneSet(TEXT("polished_blackstone_bricks"), true);
	R.Add(TEXT("magma_block")).Tex(TEXT("magma")).Hard(0.5f).Pick().Light(3).Beh(EMCBeh::Magma).Snd(EMCSound::Stone).Tab(T::Natural).Flag(MCB_Hot).Ticks().Map(0x9A3A0A);
	R.Add(TEXT("crimson_nylium")).TexTBS(TEXT("crimson_nylium"), TEXT("netherrack"), TEXT("crimson_nylium_side")).Hard(0.4f).Pick().DropItem(TEXT("netherrack"))
		.Snd(EMCSound::Nylium).Tab(T::Natural).Flag(MCB_Soil).Map(0xBD3031).RandomRotate();
	R.Add(TEXT("warped_nylium")).TexTBS(TEXT("warped_nylium"), TEXT("netherrack"), TEXT("warped_nylium_side")).Hard(0.4f).Pick().DropItem(TEXT("netherrack"))
		.Snd(EMCSound::Nylium).Tab(T::Natural).Flag(MCB_Soil).Map(0x167E86).RandomRotate();
	R.Add(TEXT("nether_wart_block")).Hard(1.f).Hoe().Snd(EMCSound::Wart).Tab(T::Natural).Map(0x7A0B0E);
	R.Add(TEXT("warped_wart_block")).Hard(1.f).Hoe().Snd(EMCSound::Wart).Tab(T::Natural).Map(0x167C84);
	R.Add(TEXT("nether_wart")).Shape(EMCShape::Crop).Tex(TEXT("nether_wart_stage0")).Orient(EMCCubeOrient::AgeStages).Alt(0, TEXT("nether_wart_stage0"))
		.Alt(1, TEXT("nether_wart_stage1")).Alt(2, TEXT("nether_wart_stage1")).Alt(3, TEXT("nether_wart_stage2")).Meta(2).Beh(EMCBeh::NetherWart)
		.Hard(0.f).Snd(EMCSound::Wart).Tab(T::Ingredients).Flag(Plant).Ticks().DropSpecial();
	R.Add(TEXT("crimson_fungus")).Shape(EMCShape::Cross).Beh(EMCBeh::NetherPlant).Hard(0.f).Snd(EMCSound::Fungus).Tab(T::Natural).Flag(Plant);
	R.Add(TEXT("warped_fungus")).Shape(EMCShape::Cross).Beh(EMCBeh::NetherPlant).Hard(0.f).Snd(EMCSound::Fungus).Tab(T::Natural).Flag(Plant);
	R.Add(TEXT("crimson_roots")).Shape(EMCShape::Cross).Beh(EMCBeh::NetherPlant).Hard(0.f).Snd(EMCSound::Fungus).Tab(T::Natural).Flag(Plant).Replaceable();
	R.Add(TEXT("warped_roots")).Shape(EMCShape::Cross).Beh(EMCBeh::NetherPlant).Hard(0.f).Snd(EMCSound::Fungus).Tab(T::Natural).Flag(Plant).Replaceable();
	R.Add(TEXT("nether_sprouts")).Shape(EMCShape::Cross).Tex(TEXT("warped_roots")).Beh(EMCBeh::NetherPlant).Hard(0.f).Snd(EMCSound::Fungus).Tab(T::Natural).Flag(Plant).Replaceable().DropSilk();
	R.Add(TEXT("weeping_vines")).Shape(EMCShape::Cross).Tex(TEXT("crimson_roots")).Beh(EMCBeh::Plant).Hard(0.f).Snd(EMCSound::Fungus).Tab(T::Natural).Flag(Plant | MCB_Climbable);
	R.Add(TEXT("twisting_vines")).Shape(EMCShape::Cross).Tex(TEXT("warped_roots")).Beh(EMCBeh::Plant).Hard(0.f).Snd(EMCSound::Fungus).Tab(T::Natural).Flag(Plant | MCB_Climbable);
	R.Add(TEXT("fire"), TEXT("Fire")).Model(EMCModel::Fire, 4).Beh(EMCBeh::Fire).NoCollision().Light(15).Hard(0.f).DropNone().NoItem().Tab(T::None)
		.Flag(MCB_Replaceable | MCB_Hot).Tex(TEXT("fire"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("soul_fire"), TEXT("Soul Fire")).Model(EMCModel::Fire, 4).Beh(EMCBeh::Fire).NoCollision().Light(10).Hard(0.f).DropNone().NoItem().Tab(T::None)
		.Flag(MCB_Replaceable | MCB_Hot).Tex(TEXT("soul_fire"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("nether_portal")).Model(EMCModel::Portal, 1).Beh(EMCBeh::NetherPortal).NoCollision().Light(11).Hard(-1.f).DropNone().NoItem().Tab(T::None)
		.Flag(MCB_Portal).Tex(TEXT("nether_portal"));
	R.Blocks.Last().Layer = EMCLayer::Portal;

	// End
	R.Add(TEXT("end_stone")).Hard(3.f, 9).Pick().Tab(T::Natural).Map(0xDBDEA0).RandomRotate();
	R.Add(TEXT("end_stone_bricks")).Hard(3.f, 9).Pick().Tab(T::Building).Fam(TEXT("end_stone_bricks"), TEXT("base"));
	R.StoneSet(TEXT("end_stone_bricks"), true);
	R.Add(TEXT("purpur_block")).Hard(1.5f, 6).Pick().Tab(T::Building).Fam(TEXT("purpur"), TEXT("base")).Map(0xA87BA8);
	R.Add(TEXT("purpur_pillar")).TexTS(TEXT("purpur_pillar_top"), TEXT("purpur_pillar")).Orient(EMCCubeOrient::Axis).Beh(EMCBeh::Axis).Hard(1.5f, 6).Pick().Tab(T::Building);
	R.StoneSet(TEXT("purpur_block"), false);
	R.Add(TEXT("chorus_plant")).Model(EMCModel::ChorusPlant).Beh(EMCBeh::ChorusPlant).Hard(0.4f).Axe().Snd(EMCSound::Wood).Tab(T::Natural).DropItem(TEXT("chorus_fruit"), 0, 1);
	R.Add(TEXT("chorus_flower")).Model(EMCModel::ChorusPlant, 3).Beh(EMCBeh::ChorusFlower).Hard(0.4f).Axe().Snd(EMCSound::Wood).Tab(T::Natural).Ticks();
	R.Add(TEXT("end_portal_frame")).TexTBS(TEXT("end_portal_frame_top"), TEXT("end_stone"), TEXT("end_portal_frame_side")).Model(EMCModel::EndPortalFrame, 3)
		.Beh(EMCBeh::EndPortalFrame).Hard(-1.f, 3600000).Light(1).DropNone().Tab(T::Functional).Flag(MCB_Interact).Mesh(TEXT("SM_EndPortalFrame"));
	R.Add(TEXT("end_portal")).Model(EMCModel::EndPortal).Beh(EMCBeh::EndPortal).NoCollision().Light(15).Hard(-1.f, 3600000).DropNone().NoItem().Tab(T::None).Flag(MCB_Portal);
	R.Blocks.Last().Layer = EMCLayer::EndPortal;
	R.Add(TEXT("end_gateway")).Shape(EMCShape::Cube).Beh(EMCBeh::EndGateway).NoFlag(MCB_Solid | MCB_Opaque).Light(15).Hard(-1.f, 3600000).DropNone().NoItem()
		.Tab(T::None).Flag(MCB_Portal | MCB_BlockEntity).Opacity(0);
	R.Blocks.Last().Layer = EMCLayer::EndPortal;
	R.Add(TEXT("dragon_egg")).Model(EMCModel::DragonEgg).Beh(EMCBeh::DragonEgg).Hard(3.f, 9).Light(1).Tab(T::Functional).Flag(MCB_Gravity | MCB_Interact).Mesh(TEXT("SM_DragonEgg"));

	// heads (meta: 0-15 floor rotation, bit 4 = wall + facing in bits 0-1)
	const TCHAR* Heads[] = { TEXT("skeleton_skull"), TEXT("wither_skeleton_skull"), TEXT("zombie_head"), TEXT("creeper_head"), TEXT("piglin_head"), TEXT("dragon_head"), TEXT("player_head") };
	const TCHAR* HeadTex[] = { TEXT("bone_block_side"), TEXT("blackstone"), TEXT("moss_block"), TEXT("moss_block"), TEXT("terracotta"), TEXT("obsidian"), TEXT("dirt") };
	for (int32 i = 0; i < 7; ++i)
	{
		R.Add(Heads[i]).Tex(TEXT("bone_block_side")).Model(EMCModel::Skull, 5).Beh(EMCBeh::Skull).Hard(1.f).Snd(EMCSound::Stone).Tab(T::Functional).Mesh(*(FString(TEXT("SM_Head_")) + Heads[i]));
		R.Blocks.Last().TexNames[0] = R.Blocks.Last().TexNames[1] = R.Blocks.Last().TexNames[2] = R.Blocks.Last().TexNames[3] = R.Blocks.Last().TexNames[4] = R.Blocks.Last().TexNames[5] = FName(HeadTex[i]);
	}
	R.Add(TEXT("flower_pot")).Tex(TEXT("terracotta")).Model(EMCModel::FlowerPot, 5).Beh(EMCBeh::FlowerPot).Hard(0.f).Snd(EMCSound::Stone).Tab(T::Functional).Flag(MCB_Interact);
	R.Add(TEXT("decorated_pot")).Tex(TEXT("terracotta")).Model(EMCModel::Pot).Hard(0.f).Snd(EMCSound::Stone).Tab(T::Functional).Mesh(TEXT("SM_DecoratedPot"));
	R.Add(TEXT("conduit")).Tex(TEXT("prismarine")).Model(EMCModel::Conduit).Hard(3.f).Pick().Light(15).Tab(T::Functional).Mesh(TEXT("SM_Conduit"));
	R.Add(TEXT("water_cauldron")).TexTBS(TEXT("cauldron_side"), TEXT("cauldron_side"), TEXT("cauldron_side")).Model(EMCModel::Cauldron, 4).Beh(EMCBeh::Cauldron)
		.Hard(2.f).Pick().Snd(EMCSound::Metal).Tab(T::None).Flag(MCB_Interact).NoItem().Item(TEXT("cauldron")).DropItem(TEXT("cauldron"));
	R.Add(TEXT("lava_cauldron")).TexTBS(TEXT("cauldron_side"), TEXT("cauldron_side"), TEXT("cauldron_side")).Model(EMCModel::Cauldron, 4).Beh(EMCBeh::Cauldron)
		.Hard(2.f).Pick().Light(15).Snd(EMCSound::Metal).Tab(T::None).Flag(MCB_Interact).NoItem().Item(TEXT("cauldron")).DropItem(TEXT("cauldron"));
}
