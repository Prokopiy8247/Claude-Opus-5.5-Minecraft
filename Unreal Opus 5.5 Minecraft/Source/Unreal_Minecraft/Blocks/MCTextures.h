// Registry of generated block texture layers (shared by runtime and the editor asset generator).
// Every entry becomes one slice of the terrain Texture2DArrays (albedo / normal / ORME).
#pragma once

#include "CoreMinimal.h"

/** Procedural recipe family used by the editor texture synthesizer. */
enum class EMCTexRecipe : uint8
{
	Noise,            // generic mottled surface (C0 dark, C1 light)
	Stone,            // layered natural stone
	Speckled,         // granite / diorite / andesite style (C2 speckle)
	Deepslate,        // dark layered stone with horizontal strata
	DeepslateTop,
	Cobble,           // rounded stones with mortar
	Dirt,
	GrassTop,         // grayscale grass (tinted), alpha=tint mask
	GrassSide,        // dirt with grass fringe on top (alpha = tint mask)
	SnowSide,         // dirt with snow cap
	PodzolTop,
	MyceliumTop,
	Sand,
	Gravel,
	Clay,
	Mud,
	Snow,
	Ice,
	PackedIce,
	Planks,           // boards; C0 base, C1 grain
	LogSide,          // bark
	LogTop,           // rings + bark rim
	StrippedSide,
	StrippedTop,
	Leaves,           // cutout clusters (grayscale if tinted)
	Plant,            // cutout cross plant: P0 = kind
	Ore,              // Base texture + mineral blobs of C0/C1 (emissive if flag)
	MetalBlock,       // riveted metal plates
	GemBlock,         // cut gem tiles
	RawBlock,         // rough raw ore chunks
	Bricks,           // clay bricks (C0 brick, C1 mortar)
	StoneBricks,      // big stone bricks (Base colors)
	Tiles,            // small square tiles
	Polished,         // smooth polished stone with border
	Chiseled,         // chiseled pattern
	PillarSide,
	PillarTop,
	Wool,             // grayscale fibre (tinted)
	Concrete,         // grayscale smooth (tinted)
	ConcretePowder,   // grayscale grainy (tinted)
	Terracotta,       // grayscale clay (tinted)
	GlazedTerracotta, // grayscale ornament (tinted)
	Glass,            // cutout glass frame
	Water,
	Lava,
	Portal,
	Netherrack,
	NyliumTop,
	NyliumSide,
	WartBlock,
	Glowstone,
	SoulSand,
	BasaltSide,
	BasaltTop,
	Magma,
	EndStone,
	Purpur,
	Obsidian,
	Bedrock,
	Sandstone,        // layered sandstone side
	SandstoneTop,
	CraftingTop,
	CraftingSide,
	CraftingFront,
	FurnaceFront,
	FurnaceSide,
	FurnaceTop,
	Bookshelf,
	TNTSide,
	TNTTop,
	TNTBottom,
	Door,             // cutout door panel (P0: 0 = bottom, 1 = top)
	Trapdoor,
	Ladder,
	Rail,             // cutout rail (P0: 0 straight, 1 corner; C1 = rail metal)
	Torch,
	RedstoneDust,     // grayscale cutout
	Lamp,             // redstone lamp (emissive when P0=1)
	Machine,          // generic machine face (piston/dispenser/observer) P0 = variant
	Pumpkin,          // P0: 0 side, 1 top, 2 face, 3 lit face
	Melon,
	Cactus,
	Hay,
	Mushroom,         // mushroom block cap / stem / inside
	Sponge,
	Sculk,
	Amethyst,
	CopperBlock,      // oxidation via colors
	Honeycomb,
	Prismarine,
	SeaLantern,
	Coral,
	Bone,
	Sulfur,
	Cinnabar,
	Crystal,          // glowing crystal / emissive block
	Moss,
	Dripstone,
	Calcite,
	Tuff,
	Bamboo,
	Panel,            // generic framed wooden/metal panel (functional block faces)
	Flat              // flat colour (used for UI/utility layers)
};

/** Kinds of cutout cross/plant textures (EMCTexRecipe::Plant, stored in P0). */
enum class EMCPlantKind : uint8
{
	ShortGrass, Fern, DeadBush, Dandelion, Poppy, BlueOrchid, Allium, AzureBluet, RedTulip, OrangeTulip,
	WhiteTulip, PinkTulip, OxeyeDaisy, Cornflower, LilyOfTheValley, Sapling, BrownMushroom, RedMushroom,
	SugarCane, Wheat, Carrots, Potatoes, Beetroots, NetherWart, SweetBerryBush, Kelp, Seagrass, CrimsonRoots,
	WarpedRoots, CrimsonFungus, WarpedFungus, NetherSprouts, WeepingVines, TwistingVines, TallGrassBottom,
	TallGrassTop, TallFlowerBottom, TallFlowerTop, Cobweb, Torchflower, WitherRose, Azalea, HangingRoots,
	CaveVines, SporeBlossom, PinkPetals, PaleHangingMoss, Eyeblossom, BambooSapling, Vine, LilyPad,
	AmethystCluster, PointedDripstone, SulfurSpike, GlowLichen, Fire, SoulFire, Stem, Propagule, ChorusFlower,
	LeafLitter, Wildflowers, FireflyBush, Bush, Dripleaf, Sunflower, Count
};

enum EMCTexFlags : uint8
{
	MCTF_None = 0,
	MCTF_Cutout = 1 << 0,   // alpha is opacity
	MCTF_Tinted = 1 << 1,   // grayscale, alpha = tint mask (opaque) / fully tinted (cutout)
	MCTF_Emissive = 1 << 2, // writes emissive into ORME.a
	MCTF_Metal = 1 << 3,
	MCTF_Translucent = 1 << 4,
	MCTF_Animated = 1 << 5
};

struct FMCTexDef
{
	FName Name;
	EMCTexRecipe Recipe = EMCTexRecipe::Noise;
	FColor C0 = FColor(128, 128, 128);   // sRGB palette colours
	FColor C1 = FColor(200, 200, 200);
	FColor C2 = FColor(40, 40, 40);
	float P0 = 0.f, P1 = 0.f, P2 = 0.f;
	FName Base;          // base texture (e.g. stone for ores)
	uint8 Flags = 0;
	uint32 Seed = 0;
};

class UNREAL_MINECRAFT_API FMCTextures
{
public:
	static void Init();
	static const TArray<FMCTexDef>& Defs();
	/** Layer index for a texture name, or the fallback layer (0) when missing. */
	static int16 Find(FName Name);
	static int16 FindChecked(const TCHAR* Name);
	static bool Has(FName Name);
	static int32 Num();
	static FColor AverageColor(int16 Layer);
	static const FMCTexDef* FindDef(FName Name);

private:
	static void Add(const FMCTexDef& Def);
	static void RegisterAll();
};
