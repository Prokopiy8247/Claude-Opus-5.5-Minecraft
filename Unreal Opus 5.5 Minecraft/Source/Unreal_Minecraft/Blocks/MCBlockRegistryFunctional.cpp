// Functional blocks (workstations, containers, light sources, utility) and redstone components.
#include "Blocks/MCBlockRegistrar.h"

void MCRegisterFunctionalBlocks(FMCBlockRegistrar& R)
{
	using T = EMCTab;
	const uint32 Int = MCB_Interact;
	const uint32 Cont = MCB_Interact | MCB_Container | MCB_BlockEntity;

	R.Add(TEXT("crafting_table")).TexFBSTB(TEXT("crafting_table_front"), TEXT("crafting_table_side"), TEXT("crafting_table_side"), TEXT("crafting_table_top"), TEXT("oak_planks"))
		.Orient(EMCCubeOrient::Facing4).Beh(EMCBeh::CraftingTable).Hard(2.5f).Axe().Snd(EMCSound::Wood).Tab(T::Functional).Flag(Int).Burn(5, 20).Fuel(300);
	R.Add(TEXT("furnace")).TexFBSTB(TEXT("furnace_front"), TEXT("furnace_side"), TEXT("furnace_side"), TEXT("furnace_top"), TEXT("furnace_top"))
		.Orient(EMCCubeOrient::Facing4Lit).Alt(0, TEXT("furnace_front_on")).Beh(EMCBeh::Furnace).Hard(3.5f).Pick().Tab(T::Functional).Flag(Cont);
	R.Add(TEXT("blast_furnace")).TexFBSTB(TEXT("blast_furnace_front"), TEXT("blast_furnace_side"), TEXT("blast_furnace_side"), TEXT("blast_furnace_top"), TEXT("blast_furnace_top"))
		.Orient(EMCCubeOrient::Facing4Lit).Alt(0, TEXT("blast_furnace_front_on")).Beh(EMCBeh::BlastFurnace).Hard(3.5f).Pick().Tab(T::Functional).Flag(Cont);
	R.Add(TEXT("smoker")).TexFBSTB(TEXT("smoker_front"), TEXT("smoker_side"), TEXT("smoker_side"), TEXT("smoker_top"), TEXT("smoker_bottom"))
		.Orient(EMCCubeOrient::Facing4Lit).Alt(0, TEXT("smoker_front_on")).Beh(EMCBeh::Smoker).Hard(3.5f).Pick().Tab(T::Functional).Flag(Cont);
	R.Add(TEXT("chest")).Tex(TEXT("oak_planks")).Model(EMCModel::Chest, 4).Beh(EMCBeh::Chest).Hard(2.5f).Axe().Snd(EMCSound::Wood).Tab(T::Functional)
		.Flag(Cont).Burn(5, 20).Fuel(300).Mesh(TEXT("SM_Chest"));
	R.Add(TEXT("trapped_chest")).Tex(TEXT("oak_planks")).Model(EMCModel::Chest, 4).Beh(EMCBeh::TrappedChest).Hard(2.5f).Axe().Snd(EMCSound::Wood)
		.Tab(T::Redstone).Flag(Cont | MCB_Redstone).Fuel(300).Mesh(TEXT("SM_TrappedChest"));
	R.Add(TEXT("ender_chest")).Tex(TEXT("obsidian")).Model(EMCModel::Chest, 4).Beh(EMCBeh::EnderChest).Hard(22.5f, 600).Pick().DropItem(TEXT("obsidian"), 8, 8)
		.Light(7).Tab(T::Functional).Flag(Int).Mesh(TEXT("SM_EnderChest"));
	R.Add(TEXT("copper_chest")).Tex(TEXT("copper_block")).Model(EMCModel::Chest, 4).Beh(EMCBeh::CopperChest).Hard(3.f, 6).Pick().Snd(EMCSound::Copper)
		.Tab(T::Functional).Flag(Cont).Mesh(TEXT("SM_CopperChest"));
	R.Add(TEXT("barrel")).TexFBSTB(TEXT("barrel_top"), TEXT("barrel_bottom"), TEXT("barrel_side"), TEXT("barrel_side"), TEXT("barrel_side"))
		.Orient(EMCCubeOrient::Facing6).Alt(1, TEXT("barrel_top")).Beh(EMCBeh::Barrel).Hard(2.5f).Axe().Snd(EMCSound::Wood).Tab(T::Functional).Flag(Cont).Fuel(300);
	R.Add(TEXT("bookshelf")).TexTS(TEXT("oak_planks"), TEXT("bookshelf")).Hard(1.5f).Axe().Snd(EMCSound::Wood).DropItem(TEXT("book"), 3, 3).Tab(T::Functional).Burn(30, 20).Fuel(300).Tag(TEXT("enchantment_power"));
	R.Add(TEXT("chiseled_bookshelf")).TexFBSTB(TEXT("chiseled_bookshelf_occupied"), TEXT("oak_planks"), TEXT("oak_planks"), TEXT("oak_planks"), TEXT("oak_planks"))
		.Orient(EMCCubeOrient::Facing4).Beh(EMCBeh::ChiseledBookshelf).Hard(1.5f).Axe().Snd(EMCSound::Wood).Tab(T::Functional).Flag(Int).Fuel(300);
	R.Add(TEXT("enchanting_table")).TexTBS(TEXT("enchanting_table_top"), TEXT("enchanting_table_bottom"), TEXT("enchanting_table_side"))
		.Model(EMCModel::EnchantingTable).Beh(EMCBeh::EnchantingTable).Hard(5.f, 1200).Pick().Light(7).Tab(T::Functional).Flag(Int).Mesh(TEXT("SM_EnchantingTable"));
	R.Add(TEXT("anvil")).TexTS(TEXT("anvil_top"), TEXT("anvil")).Model(EMCModel::Anvil, 2).Beh(EMCBeh::Anvil).Hard(5.f, 1200).Pick().Snd(EMCSound::Metal)
		.Tab(T::Functional).Flag(Int | MCB_Gravity).Mesh(TEXT("SM_Anvil"));
	R.Add(TEXT("chipped_anvil")).TexTS(TEXT("anvil_top"), TEXT("anvil")).Model(EMCModel::Anvil, 2).Beh(EMCBeh::Anvil).Hard(5.f, 1200).Pick().Snd(EMCSound::Metal)
		.Tab(T::Functional).Flag(Int | MCB_Gravity).Mesh(TEXT("SM_Anvil"));
	R.Add(TEXT("damaged_anvil")).TexTS(TEXT("anvil_top"), TEXT("anvil")).Model(EMCModel::Anvil, 2).Beh(EMCBeh::Anvil).Hard(5.f, 1200).Pick().Snd(EMCSound::Metal)
		.Tab(T::Functional).Flag(Int | MCB_Gravity).Mesh(TEXT("SM_Anvil"));
	R.Add(TEXT("brewing_stand")).TexTBS(TEXT("brewing_stand"), TEXT("brewing_stand_base"), TEXT("brewing_stand")).Model(EMCModel::BrewingStand, 3)
		.Beh(EMCBeh::BrewingStand).Hard(0.5f).Pick().Light(1).Snd(EMCSound::Metal).Tab(T::Functional).Flag(Cont).Mesh(TEXT("SM_BrewingStand"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("smithing_table")).TexFBSTB(TEXT("smithing_table_front"), TEXT("smithing_table_front"), TEXT("smithing_table_side"), TEXT("smithing_table_top"), TEXT("smithing_table_bottom"))
		.Beh(EMCBeh::SmithingTable).Hard(2.5f).Axe().Snd(EMCSound::Wood).Tab(T::Functional).Flag(Int).Fuel(300);
	R.Add(TEXT("stonecutter")).TexTBS(TEXT("stonecutter_top"), TEXT("stonecutter_top"), TEXT("stonecutter_side")).Model(EMCModel::Stonecutter, 2)
		.Beh(EMCBeh::Stonecutter).Hard(3.5f).Pick().Tab(T::Functional).Flag(Int).Mesh(TEXT("SM_Stonecutter"));
	R.Add(TEXT("loom")).TexFBSTB(TEXT("loom_front"), TEXT("loom_side"), TEXT("loom_side"), TEXT("loom_top"), TEXT("oak_planks"))
		.Orient(EMCCubeOrient::Facing4).Beh(EMCBeh::Loom).Hard(2.5f).Axe().Snd(EMCSound::Wood).Tab(T::Functional).Flag(Int).Fuel(300);
	R.Add(TEXT("grindstone")).TexTBS(TEXT("grindstone_side"), TEXT("grindstone_side"), TEXT("grindstone_side")).Model(EMCModel::Grindstone, 4)
		.Beh(EMCBeh::Grindstone).Hard(2.f, 6).Pick().Tab(T::Functional).Flag(Int).Mesh(TEXT("SM_Grindstone"));
	R.Add(TEXT("cartography_table")).TexFBSTB(TEXT("cartography_table_side"), TEXT("cartography_table_side"), TEXT("cartography_table_side"), TEXT("cartography_table_top"), TEXT("dark_oak_planks"))
		.Beh(EMCBeh::CartographyTable).Hard(2.5f).Axe().Snd(EMCSound::Wood).Tab(T::Functional).Flag(Int).Fuel(300);
	R.Add(TEXT("fletching_table")).TexFBSTB(TEXT("fletching_table_side"), TEXT("fletching_table_side"), TEXT("fletching_table_side"), TEXT("fletching_table_top"), TEXT("birch_planks"))
		.Hard(2.5f).Axe().Snd(EMCSound::Wood).Tab(T::Functional).Fuel(300);
	R.Add(TEXT("composter")).TexTBS(TEXT("composter_top"), TEXT("composter_side"), TEXT("composter_side")).Model(EMCModel::Composter, 4)
		.Beh(EMCBeh::Composter).Hard(0.6f).Axe().Snd(EMCSound::Wood).Tab(T::Functional).Flag(Int).Fuel(300);
	R.Add(TEXT("lectern")).TexTBS(TEXT("lectern_top"), TEXT("oak_planks"), TEXT("lectern_sides")).Model(EMCModel::Lectern, 3).Beh(EMCBeh::Lectern)
		.Hard(2.5f).Axe().Snd(EMCSound::Wood).Tab(T::Functional).Flag(Int | MCB_Redstone).Fuel(300).Mesh(TEXT("SM_Lectern"));
	R.Add(TEXT("jukebox")).TexTS(TEXT("jukebox_top"), TEXT("jukebox_side")).Beh(EMCBeh::Jukebox).Hard(2.f, 6).Axe().Snd(EMCSound::Wood).Tab(T::Functional).Flag(Int).Fuel(300);
	R.Add(TEXT("beacon")).Model(EMCModel::Beacon).Beh(EMCBeh::Beacon).Hard(3.f).Light(15).Snd(EMCSound::Glass).Tab(T::Functional).Flag(Int | MCB_BlockEntity).Mesh(TEXT("SM_Beacon"));
	R.Blocks.Last().Layer = EMCLayer::Translucent;
	R.Add(TEXT("respawn_anchor")).TexTBS(TEXT("respawn_anchor_top"), TEXT("respawn_anchor_bottom"), TEXT("respawn_anchor_side")).Meta(3)
		.Beh(EMCBeh::RespawnAnchor).Hard(50.f, 1200).Pick(MCTier::Diamond).Tab(T::Functional).Flag(Int).Alt(0, TEXT("respawn_anchor_side_charged"));
	R.Add(TEXT("lodestone")).TexTS(TEXT("lodestone_top"), TEXT("lodestone_side")).Hard(3.5f).Pick().Snd(EMCSound::Lodestone).Tab(T::Functional);
	R.Add(TEXT("spawner")).Tex(TEXT("spawner")).Beh(EMCBeh::Spawner).Hard(5.f).Pick().DropNone().Tab(T::OpOnly).Cutout().Flag(MCB_BlockEntity | MCB_Solid).XP(15, 43);
	R.Add(TEXT("bell")).Tex(TEXT("bell")).Model(EMCModel::Bell, 4).Beh(EMCBeh::Bell).Hard(5.f).Pick().Snd(EMCSound::Metal).Tab(T::Functional).Flag(Int | MCB_Redstone).Mesh(TEXT("SM_Bell"));
	R.Add(TEXT("cauldron")).TexTBS(TEXT("cauldron_side"), TEXT("cauldron_side"), TEXT("cauldron_side")).Model(EMCModel::Cauldron, 4).Beh(EMCBeh::Cauldron)
		.Hard(2.f).Pick().Snd(EMCSound::Metal).Tab(T::Functional).Flag(Int).Mesh(TEXT("SM_Cauldron"));
	R.Add(TEXT("cake")).Model(EMCModel::Cake, 3).Beh(EMCBeh::Cake).Hard(0.5f).DropNone().Snd(EMCSound::Wool).Tab(T::Food).Flag(Int).Tex(TEXT("wool"));
	R.Add(TEXT("crafter")).TexTBS(TEXT("crafter_top"), TEXT("crafter_top"), TEXT("crafter_side")).Beh(EMCBeh::Crafter).Hard(1.5f, 3.5f).Pick().Tab(T::Redstone).Flag(Cont | MCB_Redstone);
	R.Add(TEXT("shelf")).Tex(TEXT("shelf")).Model(EMCModel::Shelf, 2).Beh(EMCBeh::Shelf).Hard(2.f).Axe().Snd(EMCSound::Wood).Tab(T::Functional).Flag(Cont);

	// light sources
	R.Add(TEXT("torch")).Model(EMCModel::Torch, 3).Beh(EMCBeh::Torch).NoCollision().Light(14).Hard(0.f).Snd(EMCSound::Wood).Tab(T::Functional)
		.Flag(MCB_NeedsSupport).Mesh(TEXT("SM_Torch"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("soul_torch")).Model(EMCModel::Torch, 3).Beh(EMCBeh::Torch).NoCollision().Light(10).Hard(0.f).Snd(EMCSound::Wood).Tab(T::Functional)
		.Flag(MCB_NeedsSupport).Mesh(TEXT("SM_Torch_Soul"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("lantern")).Model(EMCModel::Lantern, 1).Beh(EMCBeh::Lantern).Light(15).Hard(3.5f).Pick().Snd(EMCSound::Lantern).Tab(T::Functional).Mesh(TEXT("SM_Lantern"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("soul_lantern")).Model(EMCModel::Lantern, 1).Beh(EMCBeh::Lantern).Light(10).Hard(3.5f).Pick().Snd(EMCSound::Lantern).Tab(T::Functional).Mesh(TEXT("SM_Lantern_Soul"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("campfire")).TexTBS(TEXT("campfire_log_lit"), TEXT("campfire_log"), TEXT("campfire_log")).Model(EMCModel::Campfire, 3).Beh(EMCBeh::Campfire)
		.Light(15).Hard(2.f).Axe().Snd(EMCSound::Wood).Tab(T::Functional).Flag(MCB_Interact | MCB_BlockEntity | MCB_Hot).DropItem(TEXT("charcoal"), 2, 2).Mesh(TEXT("SM_Campfire"));
	R.Add(TEXT("soul_campfire")).TexTBS(TEXT("campfire_log_lit"), TEXT("campfire_log"), TEXT("campfire_log")).Model(EMCModel::Campfire, 3).Beh(EMCBeh::Campfire)
		.Light(10).Hard(2.f).Axe().Snd(EMCSound::Wood).Tab(T::Functional).Flag(MCB_Interact | MCB_BlockEntity | MCB_Hot).DropItem(TEXT("soul_soil"), 1, 1).Mesh(TEXT("SM_Campfire_Soul"));
	R.Add(TEXT("glowstone")).Hard(0.3f).DropItem(TEXT("glowstone_dust"), 2, 4, true).Light(15).Snd(EMCSound::Glass).Tab(T::Natural);
	R.Add(TEXT("shroomlight")).Hard(1.f).Hoe().Light(15).Snd(EMCSound::Froglight).Tab(T::Natural);
	R.Add(TEXT("jack_o_lantern")).TexFBSTB(TEXT("jack_o_lantern"), TEXT("pumpkin_side"), TEXT("pumpkin_side"), TEXT("pumpkin_top"), TEXT("pumpkin_top"))
		.Orient(EMCCubeOrient::Facing4).Beh(EMCBeh::Pumpkin).Hard(1.f).Axe().Light(15).Snd(EMCSound::Wood).Tab(T::Building);
	R.Add(TEXT("ochre_froglight")).TexTS(TEXT("ochre_froglight_side"), TEXT("ochre_froglight_side")).Orient(EMCCubeOrient::Axis).Beh(EMCBeh::Axis).Hard(0.3f).Light(15).Snd(EMCSound::Froglight).Tab(T::Natural);
	R.Add(TEXT("verdant_froglight")).TexTS(TEXT("verdant_froglight_side"), TEXT("verdant_froglight_side")).Orient(EMCCubeOrient::Axis).Beh(EMCBeh::Axis).Hard(0.3f).Light(15).Snd(EMCSound::Froglight).Tab(T::Natural);
	R.Add(TEXT("pearlescent_froglight")).TexTS(TEXT("pearlescent_froglight_side"), TEXT("pearlescent_froglight_side")).Orient(EMCCubeOrient::Axis).Beh(EMCBeh::Axis).Hard(0.3f).Light(15).Snd(EMCSound::Froglight).Tab(T::Natural);
	R.Add(TEXT("end_rod")).Model(EMCModel::EndRod, 3).Beh(EMCBeh::EndRod).Light(14).Hard(0.f).Snd(EMCSound::Wood).Tab(T::Functional).Mesh(TEXT("SM_EndRod"));

	// utility
	R.Add(TEXT("ladder")).Model(EMCModel::Ladder, 2).Beh(EMCBeh::Ladder).Hard(0.4f).Axe().Snd(EMCSound::Ladder).Tab(T::Functional)
		.Flag(MCB_Climbable | MCB_NeedsSupport).Fuel(300);
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("scaffolding")).TexTBS(TEXT("scaffolding_top"), TEXT("scaffolding_top"), TEXT("scaffolding_side")).Model(EMCModel::Scaffolding, 4)
		.Beh(EMCBeh::Scaffolding).Hard(0.f).Snd(EMCSound::Scaffolding).Tab(T::Functional).Flag(MCB_Climbable).Fuel(50);
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("chain")).Model(EMCModel::Chain, 2).Beh(EMCBeh::Chain).Hard(5.f, 6).Pick().Snd(EMCSound::Chain).Tab(T::Building);
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("cobweb")).Shape(EMCShape::Cross).Beh(EMCBeh::Cobweb).Hard(4.f).Tool(EMCTool::Sword, 0, true).DropItem(TEXT("string")).Tab(T::Natural).Opacity(1);
	R.Add(TEXT("tnt")).TexTBS(TEXT("tnt_top"), TEXT("tnt_bottom"), TEXT("tnt_side")).Meta(1).Beh(EMCBeh::TNT).Hard(0.f).Snd(EMCSound::Grass)
		.Tab(T::Redstone).Flag(MCB_Interact | MCB_Redstone).Burn(15, 100);
	R.Add(TEXT("sponge")).Hard(0.6f).Hoe().Snd(EMCSound::Grass).Tab(T::Building).Beh(EMCBeh::Sponge);
	R.Add(TEXT("wet_sponge")).Hard(0.6f).Hoe().Snd(EMCSound::Grass).Tab(T::Building);
	R.Add(TEXT("slime_block")).Hard(0.f).Snd(EMCSound::Slime).Tab(T::Redstone).Translucent().Opacity(1).Beh(EMCBeh::Slime).Friction(0.8f).Flag(MCB_CullSame);
	R.Add(TEXT("honey_block")).Hard(0.f).Snd(EMCSound::Honey).Tab(T::Redstone).Translucent().Opacity(1).Beh(EMCBeh::Honey).Speed(0.4f).Jump(0.5f).Flag(MCB_CullSame);
	R.Add(TEXT("honeycomb_block")).Hard(0.6f).Snd(EMCSound::Coral).Tab(T::Building);
	R.Add(TEXT("hay_block")).TexTS(TEXT("hay_block_top"), TEXT("hay_block_side")).Orient(EMCCubeOrient::Axis).Beh(EMCBeh::Axis).Hard(0.5f).Hoe()
		.Snd(EMCSound::Grass).Tab(T::Building).Burn(60, 20);
	R.Add(TEXT("bone_block")).TexTS(TEXT("bone_block_top"), TEXT("bone_block_side")).Orient(EMCCubeOrient::Axis).Beh(EMCBeh::Axis).Hard(2.f).Pick().Snd(EMCSound::Bone).Tab(T::Building);
	R.Add(TEXT("dried_kelp_block")).TexTS(TEXT("dried_kelp_top"), TEXT("dried_kelp_side")).Hard(0.5f).Hoe().Snd(EMCSound::Grass).Tab(T::Building).Burn(30, 60).Fuel(4000);
	R.Add(TEXT("target")).TexTS(TEXT("target_top"), TEXT("target_side")).Beh(EMCBeh::Target).Meta(4).Hard(0.5f).Hoe().Snd(EMCSound::Grass).Tab(T::Redstone).Flag(MCB_Redstone);
	R.Add(TEXT("barrier")).Shape(EMCShape::Invisible).Flag(MCB_Solid).Hard(-1.f, 3600000).DropNone().Tab(T::OpOnly).Tex(TEXT("glass"));
	R.Add(TEXT("light")).Shape(EMCShape::Air).Hard(-1.f).Light(15).Tab(T::OpOnly).Replaceable();
	R.Blocks.Last().Flags = MCB_Replaceable;
	R.Blocks.Last().LightEmission = 15;

	// sculk
	R.Add(TEXT("sculk")).Hard(0.2f).Hoe().Snd(EMCSound::Sculk).Tab(T::Natural).XP(1, 1).DropSilk();
	R.Add(TEXT("sculk_catalyst")).TexTBS(TEXT("sculk_catalyst_top"), TEXT("sculk"), TEXT("sculk_catalyst_side")).Hard(3.f).Hoe().Light(6).Snd(EMCSound::Sculk).Tab(T::Natural).XP(5, 5).DropSilk();
	R.Add(TEXT("sculk_sensor")).TexTBS(TEXT("sculk"), TEXT("sculk"), TEXT("sculk_sensor_side")).Model(EMCModel::SculkSensor, 2).Hard(1.5f).Hoe().Light(1).Snd(EMCSound::Sculk).Tab(T::Redstone).Flag(MCB_Redstone).XP(5, 5).DropSilk();
	R.Add(TEXT("sculk_shrieker")).TexTBS(TEXT("sculk"), TEXT("sculk"), TEXT("sculk_shrieker_side")).Model(EMCModel::SculkSensor, 2).Hard(3.f).Hoe().Snd(EMCSound::Sculk).Tab(T::Redstone).XP(5, 5).DropSilk();
	R.Add(TEXT("sculk_vein")).Model(EMCModel::GlowLichen, 6).Beh(EMCBeh::GlowLichen).NoCollision().Hard(0.2f).Hoe().Snd(EMCSound::Sculk).Tab(T::Natural).DropSilk().Replaceable();
	R.Blocks.Last().Layer = EMCLayer::Cutout;
}

void MCRegisterRedstoneBlocks(FMCBlockRegistrar& R)
{
	using T = EMCTab;
	const uint32 RS = MCB_Redstone;
	R.Add(TEXT("redstone_wire"), TEXT("Redstone Dust")).Model(EMCModel::RedstoneWire, 4).Beh(EMCBeh::RedstoneWire).NoCollision().Hard(0.f)
		.Tint(EMCTint::Redstone).Tab(T::Redstone).Flag(RS | MCB_NeedsSupport).Item(TEXT("redstone")).NoItem().DropItem(TEXT("redstone"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("redstone_torch")).Model(EMCModel::Torch, 4).Beh(EMCBeh::RedstoneTorch).NoCollision().Light(7).Hard(0.f).Snd(EMCSound::Wood)
		.Tab(T::Redstone).Flag(RS | MCB_NeedsSupport).Mesh(TEXT("SM_Torch_Redstone"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("redstone_lamp")).Orient(EMCCubeOrient::LitToggle).Alt(0, TEXT("redstone_lamp_on")).Beh(EMCBeh::RedstoneLamp).Hard(0.3f).Snd(EMCSound::Glass).Tab(T::Redstone).Flag(RS);
	R.Add(TEXT("lever")).Model(EMCModel::Lever, 5).Beh(EMCBeh::Lever).NoCollision().Hard(0.5f).Snd(EMCSound::Wood).Tab(T::Redstone).Flag(RS | MCB_Interact | MCB_NeedsSupport).Tex(TEXT("cobblestone"));
	R.Add(TEXT("repeater")).Tex(TEXT("repeater")).Model(EMCModel::Repeater, 6).Beh(EMCBeh::Repeater).Hard(0.f).Snd(EMCSound::Stone).Tab(T::Redstone).Flag(RS | MCB_Interact | MCB_NeedsSupport);
	R.Add(TEXT("comparator")).Tex(TEXT("comparator")).Model(EMCModel::Comparator, 4).Beh(EMCBeh::Comparator).Hard(0.f).Snd(EMCSound::Stone).Tab(T::Redstone).Flag(RS | MCB_Interact | MCB_NeedsSupport | MCB_BlockEntity);
	R.Add(TEXT("piston")).TexFBSTB(TEXT("piston_top"), TEXT("piston_bottom"), TEXT("piston_side"), TEXT("piston_side"), TEXT("piston_side"))
		.Orient(EMCCubeOrient::Facing6).Model(EMCModel::Piston, 4).Beh(EMCBeh::Piston).Hard(1.5f).Pick(0).Tab(T::Redstone).Flag(RS).Opacity(15);
	R.Add(TEXT("sticky_piston")).TexFBSTB(TEXT("piston_top_sticky"), TEXT("piston_bottom"), TEXT("piston_side"), TEXT("piston_side"), TEXT("piston_side"))
		.Orient(EMCCubeOrient::Facing6).Model(EMCModel::Piston, 4).Beh(EMCBeh::Piston).Hard(1.5f).Pick(0).Tab(T::Redstone).Flag(RS).Opacity(15);
	R.Add(TEXT("piston_head")).TexFBSTB(TEXT("piston_top"), TEXT("piston_top"), TEXT("piston_side"), TEXT("piston_side"), TEXT("piston_side"))
		.Model(EMCModel::PistonHead, 5).Beh(EMCBeh::PistonHead).Hard(1.5f).DropNone().NoItem().Tab(T::None);
	R.Add(TEXT("observer")).TexFBSTB(TEXT("observer_front"), TEXT("observer_back"), TEXT("observer_side"), TEXT("observer_top"), TEXT("observer_top"))
		.Orient(EMCCubeOrient::Facing6Lit).Alt(0, TEXT("observer_back_on")).Beh(EMCBeh::Observer).Hard(3.f).Pick().Tab(T::Redstone).Flag(RS);
	R.Add(TEXT("dispenser")).TexFBSTB(TEXT("dispenser_front"), TEXT("furnace_top"), TEXT("furnace_side"), TEXT("furnace_top"), TEXT("furnace_top"))
		.Orient(EMCCubeOrient::Facing6Lit).Alt(1, TEXT("dispenser_front_vertical")).Beh(EMCBeh::Dispenser).Hard(3.5f).Pick().Tab(T::Redstone).Flag(RS | MCB_Interact | MCB_Container | MCB_BlockEntity);
	R.Add(TEXT("dropper")).TexFBSTB(TEXT("dropper_front"), TEXT("furnace_top"), TEXT("furnace_side"), TEXT("furnace_top"), TEXT("furnace_top"))
		.Orient(EMCCubeOrient::Facing6Lit).Alt(1, TEXT("dropper_front_vertical")).Beh(EMCBeh::Dropper).Hard(3.5f).Pick().Tab(T::Redstone).Flag(RS | MCB_Interact | MCB_Container | MCB_BlockEntity);
	R.Add(TEXT("hopper")).TexTBS(TEXT("hopper_inside"), TEXT("hopper_outside"), TEXT("hopper_outside")).Model(EMCModel::Hopper, 4).Beh(EMCBeh::Hopper)
		.Hard(3.f, 4.8f).Pick().Snd(EMCSound::Metal).Tab(T::Redstone).Flag(RS | MCB_Interact | MCB_Container | MCB_BlockEntity).Mesh(TEXT("SM_Hopper"));
	R.Add(TEXT("daylight_detector")).TexTBS(TEXT("daylight_detector_top"), TEXT("oak_planks"), TEXT("oak_planks")).Model(EMCModel::Daylight, 5)
		.Beh(EMCBeh::DaylightDetector).Hard(0.2f).Axe().Snd(EMCSound::Wood).Tab(T::Redstone).Flag(RS | MCB_Interact).Fuel(300);
	R.Add(TEXT("note_block")).Beh(EMCBeh::NoteBlock).Meta(6).Hard(0.8f).Axe().Snd(EMCSound::Wood).Tab(T::Redstone).Flag(RS | MCB_Interact).Fuel(300);
	R.Add(TEXT("heavy_weighted_pressure_plate")).Tex(TEXT("iron_block")).Model(EMCModel::PressurePlate, 4).Beh(EMCBeh::WeightedPressurePlate).NoCollision()
		.Hard(0.5f).Pick().Snd(EMCSound::Metal).Tab(T::Redstone).Flag(RS | MCB_NeedsSupport);
	R.Add(TEXT("light_weighted_pressure_plate")).Tex(TEXT("gold_block")).Model(EMCModel::PressurePlate, 4).Beh(EMCBeh::WeightedPressurePlate).NoCollision()
		.Hard(0.5f).Pick().Snd(EMCSound::Metal).Tab(T::Redstone).Flag(RS | MCB_NeedsSupport);
	R.Add(TEXT("rail")).Tex(TEXT("rail")).Model(EMCModel::Rail, 4).Beh(EMCBeh::Rail).Hard(0.7f).Pick(0).NoCollision().Snd(EMCSound::Metal).Tab(T::Redstone).Flag(MCB_NeedsSupport).Tag(TEXT("rails"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("powered_rail")).Tex(TEXT("powered_rail")).Alt(0, TEXT("powered_rail_on")).Model(EMCModel::Rail, 4).Beh(EMCBeh::Rail).Hard(0.7f).Pick(0).NoCollision().Snd(EMCSound::Metal)
		.Tab(T::Redstone).Flag(MCB_NeedsSupport | RS).Tag(TEXT("rails"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("detector_rail")).Tex(TEXT("detector_rail")).Alt(0, TEXT("detector_rail_on")).Model(EMCModel::Rail, 4).Beh(EMCBeh::Rail).Hard(0.7f).Pick(0).NoCollision().Snd(EMCSound::Metal)
		.Tab(T::Redstone).Flag(MCB_NeedsSupport | RS).Tag(TEXT("rails"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("activator_rail")).Tex(TEXT("activator_rail")).Alt(0, TEXT("activator_rail_on")).Model(EMCModel::Rail, 4).Beh(EMCBeh::Rail).Hard(0.7f).Pick(0).NoCollision().Snd(EMCSound::Metal)
		.Tab(T::Redstone).Flag(MCB_NeedsSupport | RS).Tag(TEXT("rails"));
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("iron_door")).TexTBS(TEXT("iron_door_top"), TEXT("iron_door_bottom"), TEXT("iron_door_bottom")).Model(EMCModel::Door, 5).Beh(EMCBeh::Door)
		.Hard(5.f).Pick().Snd(EMCSound::Metal).Tab(T::Redstone).Flag(MCB_Redstone);
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("iron_trapdoor")).Tex(TEXT("iron_trapdoor")).Model(EMCModel::Trapdoor, 4).Beh(EMCBeh::Trapdoor).Hard(5.f).Pick().Snd(EMCSound::Metal)
		.Tab(T::Redstone).Flag(MCB_Redstone);
	R.Blocks.Last().Layer = EMCLayer::Cutout;
	R.Add(TEXT("lightning_rod")).Tex(TEXT("copper_block")).Model(EMCModel::EndRod, 3).Beh(EMCBeh::EndRod).Hard(3.f, 6).Pick(MCTier::Stone).Snd(EMCSound::Copper).Tab(T::Redstone).Flag(RS);
}
