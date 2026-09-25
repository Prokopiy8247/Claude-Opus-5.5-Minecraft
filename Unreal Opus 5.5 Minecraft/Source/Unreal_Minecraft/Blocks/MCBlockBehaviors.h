// Catalogue of block behaviour singletons referenced by the block registry.
#pragma once

#include "CoreMinimal.h"
#include "Blocks/MCBlockBehavior.h"

enum class EMCBeh : uint8
{
	None,
	// placement / orientation
	Axis, FacingToPlayer, FacingAwayFromPlayer, Facing6ToPlayer, Facing6Away, Slab, Stairs, Door, Trapdoor, FenceGate,
	Torch, Ladder, Lever, Button, PressurePlate, WeightedPressurePlate, Lantern, Chain, EndRod, Carpet,
	// redstone
	RedstoneWire, RedstoneTorch, Repeater, Comparator, RedstoneLamp, RedstoneBlock, Piston, PistonHead,
	Observer, Dispenser, Dropper, Hopper, Target, DaylightDetector, NoteBlock, TNT, RedstoneOre, CopperBulb,
	// plants & farming
	Plant, TallPlant, Crop, Stem, Sapling, SugarCane, Cactus, Bamboo, WaterPlant, Vine, Mushroom, NetherWart,
	SweetBerryBush, Grass, Mycelium, Farmland, Leaves, CaveVines, Dripleaf, ChorusPlant, ChorusFlower, NetherPlant,
	LilyPad, GlowLichen,
	// physics / world
	Falling, ConcretePowder, Ice, SnowLayer, Fire, NetherPortal, EndPortal, EndPortalFrame, EndGateway,
	Water, Lava, Magma, SoulSand, Honey, Slime, Cobweb, PowderSnow, Sponge, CopperOxidize, Budding,
	PointedDripstone, PotentSulfur, CreakingHeart, Scaffolding, DragonEgg, Bedrock,
	// functional / containers
	CraftingTable, Furnace, BlastFurnace, Smoker, Chest, TrappedChest, EnderChest, Barrel, ShulkerBox,
	Anvil, EnchantingTable, BrewingStand, SmithingTable, Stonecutter, Loom, Grindstone, CartographyTable,
	Composter, Lectern, Bed, Campfire, Cauldron, Beacon, RespawnAnchor, Jukebox, Spawner, Bell, Cake,
	Candle, Bookshelf, ChiseledBookshelf, CopperChest, Crafter, Shelf,
	// extra
	FlowerPot, Skull, Rail, Pumpkin, Beehive, Infested, TurtleEgg, TrialSpawner, Vault, Cocoa, Frogspawn, Sign, Tripwire, SculkSensor,
	Count
};

namespace MCBehaviors
{
	UNREAL_MINECRAFT_API FMCBlockBehavior* Get(EMCBeh Id);
	void RegisterAll();
}
