// Shared helpers for world generation: chunk writer, block shortcuts, structure framework.
#pragma once

#include "CoreMinimal.h"
#include "Core/MCCore.h"
#include "Core/MCNoise.h"
#include "Blocks/MCBlocks.h"
#include "World/MCChunk.h"
#include "Gen/MCBiomes.h"

class FMCBlockEntity;

/** Writes world-space blocks into one chunk, silently clipping everything outside it. */
class UNREAL_MINECRAFT_API FMCGenWriter
{
public:
	explicit FMCGenWriter(FMCChunk& InChunk) : C(InChunk), X0(InChunk.Pos.MinBlockX()), Y0(InChunk.Pos.MinBlockY()) {}
	FMCChunk& C;
	const int32 X0, Y0;

	FORCEINLINE bool Inside(int32 X, int32 Y, int32 Z) const
	{
		return X >= X0 && X < X0 + 16 && Y >= Y0 && Y < Y0 + 16 && Z >= MC::MinZ && Z <= MC::MaxZ;
	}
	FORCEINLINE bool InsideXY(int32 X, int32 Y) const { return X >= X0 && X < X0 + 16 && Y >= Y0 && Y < Y0 + 16; }
	FORCEINLINE FMCState Get(int32 X, int32 Y, int32 Z) const
	{
		return Inside(X, Y, Z) ? C.Get(X - X0, Y - Y0, Z) : 0;
	}
	FORCEINLINE void Set(int32 X, int32 Y, int32 Z, FMCState S)
	{
		if (Inside(X, Y, Z)) C.SetRaw(X - X0, Y - Y0, Z, S);
	}
	FORCEINLINE void Set(const FMCBlockPos& P, FMCState S) { Set(P.X, P.Y, P.Z, S); }
	/** Set only if the current block is air or replaceable (plants, snow...). */
	FORCEINLINE void SetSoft(int32 X, int32 Y, int32 Z, FMCState S)
	{
		if (!Inside(X, Y, Z)) return;
		const FMCState Cur = C.Get(X - X0, Y - Y0, Z);
		if (Cur == 0 || FMCBlocks::IsReplaceable(Cur) || (FMCBlocks::Info(Cur).Flags & MCB_Plant)) C.SetRaw(X - X0, Y - Y0, Z, S);
	}
	/** Replace only natural stone-like blocks (ores, cave decorations). */
	FORCEINLINE void SetIfStone(int32 X, int32 Y, int32 Z, FMCState S)
	{
		if (!Inside(X, Y, Z)) return;
		const FMCState Cur = C.Get(X - X0, Y - Y0, Z);
		if (Cur != 0 && (FMCBlocks::Info(Cur).Flags & MCB_Stone)) C.SetRaw(X - X0, Y - Y0, Z, S);
	}
	void Fill(int32 X0_, int32 Y0_, int32 Z0_, int32 X1, int32 Y1, int32 Z1, FMCState S, bool bHollow = false, FMCState Inner = 0);
	void FillIfAir(int32 X0_, int32 Y0_, int32 Z0_, int32 X1, int32 Y1, int32 Z1, FMCState S);
	void AddBlockEntity(int32 X, int32 Y, int32 Z, TSharedPtr<FMCBlockEntity> BE);
	void AddChest(int32 X, int32 Y, int32 Z, EMCFace Facing, FName LootTable, uint64 Seed, FName ChestBlock = TEXT("chest"));
	void AddSpawner(int32 X, int32 Y, int32 Z, FName Mob);
	void AddSpawn(FName Mob, double X, double Y, double Z, int32 Variant = -1, FName Extra = NAME_None);
	void AddTick(int32 X, int32 Y, int32 Z);
	void SetBiomeColumn(int32 X, int32 Y, uint8 Biome);
};

/** Frequently used block states, resolved once. */
struct UNREAL_MINECRAFT_API FMCGenBlocks
{
	FMCState Air = 0, Stone, Deepslate, Dirt, Grass, GrassSnowy, Sand, RedSand, Sandstone, RedSandstone, Gravel, Clay, Water, Lava, Bedrock,
		Snow, SnowBlock, Ice, PackedIce, BlueIce, Podzol, CoarseDirt, Mycelium, Mud, Calcite, Tuff, Granite, Diorite, Andesite, Terracotta,
		Obsidian, MossBlock, PaleMoss, RootedDirt, Cobblestone, MossyCobble, StoneBricks, MossyStoneBricks, CrackedStoneBricks, Planks,
		OakLog, OakLeaves, BirchLog, BirchLeaves, SpruceLog, SpruceLeaves, JungleLog, JungleLeaves, AcaciaLog, AcaciaLeaves,
		DarkOakLog, DarkOakLeaves, MangroveLog, MangroveLeaves, MangroveRoots, MuddyMangroveRoots, CherryLog, CherryLeaves, PaleOakLog, PaleOakLeaves,
		AzaleaLeaves, FloweringAzaleaLeaves, ShortGrass, TallGrass, TallGrassTop, Fern, LargeFern, LargeFernTop, DeadBush, Cactus, SugarCane,
		Dandelion, Poppy, Cornflower, OxeyeDaisy, AzureBluet, Allium, BlueOrchid, RedTulip, OrangeTulip, WhiteTulip, PinkTulip, LilyOfValley,
		Sunflower, SunflowerTop, Lilac, LilacTop, RoseBush, RoseBushTop, Peony, PeonyTop, BrownMushroom, RedMushroom, BrownMushroomBlock,
		RedMushroomBlock, MushroomStem, Pumpkin, Melon, SweetBerries, Vine, LilyPad, Seagrass, TallSeagrass, TallSeagrassTop, Kelp, KelpPlant,
		Bamboo, BambooLeaves, Cocoa, PinkPetals, Wildflowers, LeafLitter, Bush, FireflyBush, PaleHangingMoss, CreakingHeart, OpenEyeblossom,
		ClosedEyeblossom, GlowLichen, CaveVines, CaveVinesLit, Dripleaf, SmallDripleaf, SporeBlossom, Azalea, FloweringAzalea, HangingRoots,
		MossCarpet, PaleMossCarpet, DripstoneBlock, PointedDripUp, PointedDripDown, Sculk, SculkVein, SculkSensor, SculkShrieker, SculkCatalyst,
		Amethyst, BuddingAmethyst, AmethystCluster, SmoothBasalt, Magma, Sulfur, PotentSulfur, Cinnabar, SulfurSpikeUp, SulfurSpikeDown,
		CoalOre, IronOre, CopperOre, GoldOre, RedstoneOre, LapisOre, DiamondOre, EmeraldOre,
		DCoalOre, DIronOre, DCopperOre, DGoldOre, DRedstoneOre, DLapisOre, DDiamondOre, DEmeraldOre,
		RawIronBlock, RawCopperBlock, Netherrack, SoulSand, SoulSoil, Basalt, Blackstone, Glowstone, CrimsonNylium, WarpedNylium,
		CrimsonStem, WarpedStem, NetherWartBlock, WarpedWartBlock, Shroomlight, CrimsonFungus, WarpedFungus, CrimsonRoots, WarpedRoots,
		NetherSprouts, WeepingVines, TwistingVines, NetherGoldOre, NetherQuartzOre, AncientDebris, BoneBlock, NetherBricks, NetherBrickFence,
		RedNetherBricks, GildedBlackstone, PolishedBlackstoneBricks, CrackedPBB, PolishedBlackstone, ChiseledPB, Fire, SoulFire, EndStone,
		EndStoneBricks, Purpur, PurpurPillar, ChorusPlant, ChorusFlower, EndRod, CoralBlocks[5], Corals[5], SeaPickle, PrismarineBricks,
		Prismarine, DarkPrismarine, SeaLantern, Glass, Torch, WallTorchN, WallTorchS, WallTorchW, WallTorchE, Lantern, HangingLantern, Chain, Rail;
	void Init();
	static const FMCGenBlocks& Get();
};

/** A placed structure piece: bounding box + a callback that writes blocks clipped to the chunk. */
struct FMCStructurePiece
{
	FIntVector Min, Max;       // inclusive world bounds
	TFunction<void(FMCGenWriter&, FMCRandom&)> Build;
	uint64 Seed = 0;
};

struct FMCStructureStart
{
	FName Type;
	FMCBlockPos Origin;
	FIntVector Min, Max;       // overall bounds
	TArray<FMCStructurePiece> Pieces;
	bool bValid = false;
};

/** Grid based structure placement (Minecraft "random spread"). */
struct FMCStructureSet
{
	FName Type;
	int32 Spacing = 32;        // chunks
	int32 Separation = 8;      // chunks
	uint32 Salt = 0;
	int32 MaxRadiusChunks = 8; // how far pieces may extend from the start chunk
	bool bTriangular = false;
};

class UNREAL_MINECRAFT_API FMCStructureCache
{
public:
	TSharedPtr<const FMCStructureStart> Find(uint64 Key) const;
	void Add(uint64 Key, TSharedPtr<const FMCStructureStart> S);
private:
	mutable FCriticalSection Lock;
	TMap<uint64, TSharedPtr<const FMCStructureStart>> Map;
	TArray<uint64> Order;
};

namespace MCGen
{
	/** Chunk-local random seeded from world seed + chunk coords + salt. */
	FORCEINLINE FMCRandom ChunkRandom(uint64 Seed, int32 CX, int32 CY, uint32 Salt)
	{
		return FMCRandom(MCHash::Hash3(Seed ^ ((uint64)Salt * 0x9E3779B97F4A7C15ull), CX, CY, (int32)Salt));
	}
	/** Start chunk of a structure grid cell (returns false if the cell does not contain one). */
	bool GetStructureStartChunk(const FMCStructureSet& Set, uint64 Seed, int32 RegionX, int32 RegionY, FMCChunkPos& Out);
	/** Set a horizontal-facing block with meta. */
	FMCState Facing(FMCState Base, EMCFace F);
	FMCState WithMeta(const TCHAR* Block, uint8 Meta);
	FMCState S(const TCHAR* Block);
}
