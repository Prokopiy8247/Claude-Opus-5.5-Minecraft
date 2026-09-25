// Data-driven block registry.
//
// Every block type owns a contiguous range of global block-state ids (uint16).
// A state = BaseState + meta, where meta layout is block specific (facing, age, ...).
#pragma once

#include "CoreMinimal.h"
#include "Core/MCCore.h"

using FMCState = uint16;
using FMCBlockId = uint16;

class FMCBlockBehavior;
class FMCWorld;

enum class EMCShape : uint8
{
	Air,        // nothing rendered, no collision
	Cube,       // full 1x1x1 cube, per-face textures
	Cross,      // two diagonal quads (flowers, saplings, grass)
	Crop,       // four quads in a # pattern (wheat, carrots...)
	Liquid,     // water / lava
	Model,      // list of boxes (slabs, stairs, torches, doors...), may depend on neighbours
	Invisible   // barrier-like: collision but no render
};

enum class EMCLayer : uint8
{
	Opaque = 0,
	Cutout = 1,
	Translucent = 2,
	Water = 3,
	Lava = 4,
	Portal = 5,
	EndPortal = 6,
	Count = 7
};

enum class EMCTool : uint8
{
	None, Pickaxe, Axe, Shovel, Hoe, Sword, Shears
};

/** Tool tiers: 0 hand, 1 wood/gold, 2 stone, 3 copper, 4 iron, 5 diamond, 6 netherite. */
namespace MCTier
{
	constexpr uint8 Hand = 0, Wood = 1, Gold = 1, Stone = 2, Copper = 3, Iron = 4, Diamond = 5, Netherite = 6;
}

enum class EMCTint : uint8
{
	None, Grass, Foliage, Water, Birch, Spruce, Custom, Redstone, Stem, Lily
};

enum class EMCSound : uint8
{
	Stone, Wood, Gravel, Grass, Sand, Snow, Glass, Wool, Metal, Slime, Honey, Netherrack, Nylium,
	Bone, SoulSand, Amethyst, Sculk, Deepslate, Mud, Copper, Chain, Lantern, Ladder, Scaffolding,
	Crop, Coral, Wart, Fungus, Basalt, Cloth, Candle, Moss, Froglight, Lodestone, Bamboo, Cherry,
	Count
};

enum class EMCTab : uint8
{
	Building, Colored, Natural, Functional, Redstone, Tools, Combat, Food, Ingredients, SpawnEggs, OpOnly, None
};

/** Model families used by EMCShape::Model blocks. */
enum class EMCModel : uint8
{
	None,
	Slab, Stairs, Fence, FenceGate, Wall, Pane, Bars, Door, Trapdoor,
	Torch, Ladder, Rail, RedstoneWire, Carpet, SnowLayer, PressurePlate, Button, Lever,
	Repeater, Comparator, Bed, Chest, Cactus, Farmland, Path, Vine, Lantern, Chain,
	Anvil, EnchantingTable, BrewingStand, Cauldron, Hopper, EndPortalFrame, Campfire, Cake,
	FlowerPot, Piston, PistonHead, Portal, EndPortal, Fire, Lectern, Grindstone, Stonecutter,
	Bell, Scaffolding, Candle, DragonEgg, Skull, Bamboo, LilyPad, Daylight, Composter,
	EndRod, ChorusPlant, Conduit, Beacon, Spawner, Slabish, SculkSensor, Dripleaf, Kelp,
	CopperGolemStatue, Shelf, Dripstone, Lodestone, Head, Pot, BigDripleaf, Crafter, Honey,
	SeaPickle, TurtleEgg, Sniffer, ChiseledBookshelf, GlowLichen, AmethystCluster, CaveVines,
	Count
};

enum EMCBlockFlags : uint32
{
	MCB_None = 0,
	MCB_Solid = 1u << 0,          // has collision
	MCB_Opaque = 1u << 1,         // full opaque cube: occludes faces, full light block, AO
	MCB_Replaceable = 1u << 2,    // can be replaced by placing (air, grass, water, snow layer)
	MCB_Flammable = 1u << 3,
	MCB_RandomTick = 1u << 4,
	MCB_Gravity = 1u << 5,        // sand / gravel falling
	MCB_Climbable = 1u << 6,      // ladders, vines
	MCB_Interact = 1u << 7,       // right click does something (prevents placement)
	MCB_Container = 1u << 8,
	MCB_NeedsSupport = 1u << 9,   // plants/torches that break without support
	MCB_Fluid = 1u << 10,
	MCB_NoItem = 1u << 11,        // no block item (technical blocks)
	MCB_CullSame = 1u << 12,      // glass-like: faces between same blocks are culled
	MCB_Leaves = 1u << 13,
	MCB_Unbreakable = 1u << 14,
	MCB_Hot = 1u << 15,           // magma / campfire: damages entities standing on it
	MCB_Redstone = 1u << 16,      // participates in redstone updates
	MCB_BlockEntity = 1u << 17,   // has a block entity (container, furnace, sign...)
	MCB_FullBright = 1u << 18,    // renders emissive regardless of light
	MCB_Log = 1u << 19,
	MCB_Plant = 1u << 20,
	MCB_Soil = 1u << 21,          // plants can grow on it
	MCB_Sand = 1u << 22,          // cactus / sugar cane / dead bush soil
	MCB_Waterlogged = 1u << 23,   // renders as if water is present too (kelp, seagrass)
	MCB_AuthoredMesh = 1u << 24,  // rendered by an authored static mesh (ISM) when available
	MCB_NoCollisionMobs = 1u << 25,
	MCB_Portal = 1u << 26,
	MCB_Slippery = 1u << 27,
	MCB_Wool = 1u << 28,
	MCB_Stone = 1u << 29,         // stone-like (replaceable by ores / carvers)
	MCB_Dirt = 1u << 30,
	MCB_Cutout = 1u << 31
};

enum class EMCDropType : uint8
{
	Self,        // drop the block item
	None,
	Item,        // fixed item with count range (fortune optional)
	SilkOnly,    // drops only with silk touch (glass, ice)
	Special      // handled by behaviour / loot table
};

struct FMCDrop
{
	EMCDropType Type = EMCDropType::Self;
	FName Item;
	uint8 Min = 1, Max = 1;
	bool bFortune = false;
	bool bSilkSelf = true; // silk touch yields the block itself
	FName LootTable;
};

/** How a cube's face textures depend on its state meta. */
enum class EMCCubeOrient : uint8
{
	None,
	Axis,        // logs / pillars: meta 0 = vertical, 1 = X, 2 = Y
	Facing4,     // front texture (Tex[North]) faces meta facing (2 bits)
	Facing4Lit,  // + bit 2 = lit (front uses TexAlt[0])
	Facing6,     // meta 3 bits = EMCFace of the front (dispenser, observer)
	Facing6Lit,  // + bit 3 = active (back uses TexAlt[0])
	LitToggle,   // bit 0 = lit: all faces use TexAlt[0..] (lamps, redstone ore)
	Upper,       // bit 0 = upper half of a double plant: all faces use TexAlt[0]
	AgeStages,   // crops: stage = meta*4/NumStates selects TexAlt[stage]
	Snowy        // bit 0 = snowy: side faces use TexAlt[0] (grass block)
};

/** One textured box of a block model. Coordinates in block units (0..1). */
struct FMCModelBox
{
	FVector3f Min = FVector3f::ZeroVector;
	FVector3f Max = FVector3f::OneVector;
	int16 Tex[6] = { -1, -1, -1, -1, -1, -1 };
	FVector4f UV[6];          // u0 v0 u1 v1 per face (auto derived when bAutoUV)
	uint8 CullFaces = 0;      // faces culled when the neighbour on that side is opaque
	uint8 TintFaces = 0;
	uint8 UVRot[6] = { 0, 0, 0, 0, 0, 0 };
	bool bAutoUV = true;
	bool bEmissive = false;
	bool bDoubleSided = false;
	uint8 Layer = 255;        // 255 = inherit block layer
	FColor Color = FColor::White; // extra multiply (redstone power, custom tint)

	FMCModelBox() { for (FVector4f& U : UV) U = FVector4f(0, 0, 1, 1); }
	FMCModelBox(const FMCBox& B, int16 AllTex)
	{
		Min = FVector3f(B.Min); Max = FVector3f(B.Max);
		for (int32 i = 0; i < 6; ++i) { Tex[i] = AllTex; UV[i] = FVector4f(0, 0, 1, 1); }
	}
	FMCModelBox& SetTex(EMCFace F, int16 T) { Tex[(int32)F] = T; return *this; }
	FMCModelBox& Hide(EMCFace F) { Tex[(int32)F] = -1; return *this; }
	FMCModelBox& Cull(EMCFace F) { CullFaces |= 1 << (int32)F; return *this; }
	void AutoCull()
	{
		if (Min.Z <= 0.0001f) CullFaces |= 1 << (int32)EMCFace::Down;
		if (Max.Z >= 0.9999f) CullFaces |= 1 << (int32)EMCFace::Up;
		if (Min.Y <= 0.0001f) CullFaces |= 1 << (int32)EMCFace::North;
		if (Max.Y >= 0.9999f) CullFaces |= 1 << (int32)EMCFace::South;
		if (Min.X <= 0.0001f) CullFaces |= 1 << (int32)EMCFace::West;
		if (Max.X >= 0.9999f) CullFaces |= 1 << (int32)EMCFace::East;
	}
	FMCBox ToBox() const { return FMCBox(FVector(Min), FVector(Max)); }
	void RotateY(int32 Quarter);
};

/** Free quad (slopes, cross plants, flat decals). Positions in block units. */
struct FMCModelQuad
{
	FVector3f P[4];            // counter-clockwise when seen from the front
	FVector2f UV[4];
	int16 Tex = 0;
	bool bDoubleSided = true;
	bool bTint = false;
	bool bEmissive = false;
	bool bWave = false;
	uint8 Layer = 255;
	FColor Color = FColor::White;
	int8 LightFace = -1;       // -1 = use own cell light, else neighbour in that face direction

	FMCModelQuad() = default;
	FMCModelQuad(const FVector3f& A, const FVector3f& B, const FVector3f& C, const FVector3f& D, int16 InTex)
	{
		P[0] = A; P[1] = B; P[2] = C; P[3] = D; Tex = InTex;
		UV[0] = FVector2f(0, 1); UV[1] = FVector2f(1, 1); UV[2] = FVector2f(1, 0); UV[3] = FVector2f(0, 0);
	}
	void SetUV(float U0, float V0, float U1, float V1)
	{
		UV[0] = FVector2f(U0, V1); UV[1] = FVector2f(U1, V1); UV[2] = FVector2f(U1, V0); UV[3] = FVector2f(U0, V0);
	}
	void RotateY(int32 Quarter);
};

struct FMCBlockModel
{
	TArray<FMCModelBox> Boxes;
	TArray<FMCModelQuad> Quads;
	TArray<FMCBox> Collision;   // collision boxes (block units)
	TArray<FMCBox> Outline;     // selection outline boxes (defaults to collision/boxes)
	bool bExplicitCollision = false; // true: Collision is authoritative even when empty
	void Add(const FMCModelBox& B) { Boxes.Add(B); }
	void RotateY(int32 Quarter)
	{
		for (FMCModelBox& B : Boxes) B.RotateY(Quarter);
		for (FMCModelQuad& Q : Quads) Q.RotateY(Quarter);
		for (FMCBox& B : Collision) B = B.RotateY(Quarter);
		for (FMCBox& B : Outline) B = B.RotateY(Quarter);
	}
};

/** Neighbour access for dynamic models / collision (thread-safe snapshot or live world). */
class FMCBlockGetter
{
public:
	virtual ~FMCBlockGetter() = default;
	virtual FMCState GetState(const FMCBlockPos& P) const = 0;
};

struct FMCBlock
{
	FMCBlockId Id = 0;
	FName Name;
	FString DisplayName;
	FMCState BaseState = 0;
	uint16 NumStates = 1;
	uint8 MetaBits = 0;

	EMCShape Shape = EMCShape::Cube;
	EMCLayer Layer = EMCLayer::Opaque;
	EMCModel Model = EMCModel::None;
	uint32 Flags = MCB_Solid | MCB_Opaque;

	int16 Tex[6] = { 0, 0, 0, 0, 0, 0 };   // default face textures (Down, Up, N, S, W, E)
	int16 TexAlt[4] = { -1, -1, -1, -1 };  // alternate textures used by behaviours (lit front, open top...)
	FName TexNames[6];

	uint8 LightEmission = 0;   // 0..15
	uint8 LightOpacity = 15;   // 0..15
	float Hardness = 1.0f;     // < 0 unbreakable
	float BlastResistance = 1.0f;
	EMCTool Tool = EMCTool::None;
	uint8 MinTier = 0;
	bool bRequiresTool = false;
	EMCSound Sound = EMCSound::Stone;
	EMCTint Tint = EMCTint::None;
	FColor TintColor = FColor::White;
	FColor MapColor = FColor(128, 128, 128);
	EMCTab Tab = EMCTab::Building;
	float Friction = 0.6f;       // Minecraft slipperiness
	float SpeedFactor = 1.0f;    // soul sand / honey
	float JumpFactor = 1.0f;
	uint8 FireSpread = 0;        // encouragement
	uint8 FireBurn = 0;          // flammability
	int32 FuelTicks = 0;         // furnace fuel value
	FMCDrop Drop;
	int32 ItemId = 0;            // block item id (0 = none)
	FName ItemName;              // explicit item to give on pick-block (defaults to own)
	FName MeshAsset;             // authored static mesh for MCB_AuthoredMesh blocks
	FName Family;                // e.g. "oak", "stone" (used by recipes / tags)
	FName Variant;               // e.g. "planks", "stairs"
	TArray<FName> Tags;          // block tags (#logs, #planks, #mineable/pickaxe...)
	FMCBlockBehavior* Behavior = nullptr;
	float XPMin = 0.f, XPMax = 0.f; // XP dropped when mined (ores)
	EMCCubeOrient Orient = EMCCubeOrient::None; // how cube face textures follow the state
	bool bRandomRotateTop = false; // rotate top texture per position to hide tiling
	uint8 FluidLevelBits = 0;      // liquids: meta = level

	FORCEINLINE bool Has(uint32 F) const { return (Flags & F) != 0; }
	FORCEINLINE FMCState State(uint16 Meta = 0) const { return BaseState + FMath::Min<uint16>(Meta, NumStates - 1); }
	bool HasTag(FName Tag) const { return Tags.Contains(Tag); }
};

/** Flattened, cache-friendly per-state data used in hot paths (meshing, lighting, physics). */
struct FMCStateInfo
{
	FMCBlockId Block = 0;
	uint8 Meta = 0;
	uint8 Light = 0;
	uint8 Opacity = 15;
	EMCShape Shape = EMCShape::Air;
	EMCLayer Layer = EMCLayer::Opaque;
	uint32 Flags = 0;
	int16 Tex[6] = { 0, 0, 0, 0, 0, 0 };
	uint8 UVRot[6] = { 0, 0, 0, 0, 0, 0 };
	int32 StaticModel = -1;   // index into cached model table (for state-only models)
	bool bDynamicModel = false; // model depends on neighbours
	bool bFullCollision = false;
	bool bNoCollision = true;
};

class UNREAL_MINECRAFT_API FMCBlocks
{
public:
	static void Init();
	static bool IsInitialized();

	static const FMCBlock& Get(FMCBlockId Id);
	static const FMCBlock& GetByState(FMCState S) { return Get(StateInfos[S].Block); }
	static const FMCBlock* Find(FName Name);
	static FMCBlockId FindId(FName Name);
	static FMCState FindState(FName Name, uint16 Meta = 0);
	static int32 NumBlocks();
	static int32 NumStates() { return StateInfos.Num(); }

	static FORCEINLINE const FMCStateInfo& Info(FMCState S) { return StateInfos[S]; }
	static FORCEINLINE FMCBlockId BlockOf(FMCState S) { return StateInfos[S].Block; }
	static FORCEINLINE uint8 MetaOf(FMCState S) { return StateInfos[S].Meta; }
	static FORCEINLINE bool IsAir(FMCState S) { return S == 0; }
	static FORCEINLINE bool IsOpaque(FMCState S) { return (StateInfos[S].Flags & MCB_Opaque) != 0; }
	static FORCEINLINE bool IsSolid(FMCState S) { return (StateInfos[S].Flags & MCB_Solid) != 0; }
	static FORCEINLINE bool IsFluid(FMCState S) { return (StateInfos[S].Flags & MCB_Fluid) != 0; }
	static FORCEINLINE bool IsReplaceable(FMCState S) { return (StateInfos[S].Flags & MCB_Replaceable) != 0; }
	static FORCEINLINE bool Is(FMCState S, FMCBlockId B) { return StateInfos[S].Block == B; }

	/** Model for a state; for dynamic models Getter/Pos provide neighbour context. */
	static void BuildModel(FMCState S, const FMCBlockGetter* Getter, const FMCBlockPos& Pos, FMCBlockModel& Out);
	static const FMCBlockModel* GetStaticModel(FMCState S);
	/** Collision boxes in block-local units. */
	static void GetCollision(FMCState S, const FMCBlockGetter* Getter, const FMCBlockPos& Pos, TArray<FMCBox>& Out);
	static void GetOutline(FMCState S, const FMCBlockGetter* Getter, const FMCBlockPos& Pos, TArray<FMCBox>& Out);

	static const TArray<FMCBlock>& All() { return Blocks; }
	static TArray<FMCBlock>& Mutable() { return Blocks; }

	/** Common state ids cached after init. */
	struct FCommon
	{
		FMCState Air = 0, Stone, Dirt, Grass, Sand, RedSand, Gravel, Water, Lava, Bedrock, Deepslate, Cobblestone,
			Netherrack, EndStone, Obsidian, Glass, Snow, SnowBlock, Ice, Clay, Sandstone, RedSandstone, Terracotta,
			Podzol, Mycelium, CoarseDirt, Mud, Calcite, Tuff, PackedIce, BlueIce, SoulSand, SoulSoil, Basalt,
			Blackstone, Magma, Glowstone, CrimsonNylium, WarpedNylium, Fire, SoulFire, NetherPortal, EndPortal,
			Farmland, PowderSnow, MossBlock, Sulfur, Cinnabar, PotentSulfur, Air2;
		FMCBlockId AirId = 0, WaterId, LavaId, FireId, NetherPortalId, TNTId, GrassId, DirtId, FarmlandId;
	};
	static FCommon C;

private:
	friend class FMCBlockRegistrar;
	static TArray<FMCBlock> Blocks;
	static TArray<FMCStateInfo> StateInfos;
	static TMap<FName, FMCBlockId> NameToId;
	static TArray<FMCBlockModel> StaticModels;
	static bool bInitialized;
	static void Finalize();
};

/** Meta layout helpers shared by behaviours and models. */
namespace MCMeta
{
	// Horizontal facing in 2 bits: 0 N, 1 S, 2 W, 3 E  (== EMCFace - 2)
	FORCEINLINE EMCFace Facing4(uint8 M, int32 Shift = 0) { return (EMCFace)(2 + ((M >> Shift) & 3)); }
	FORCEINLINE uint8 FromFacing4(EMCFace F) { return (uint8)F - 2; }
	// Six facing in 3 bits: EMCFace value
	FORCEINLINE EMCFace Facing6(uint8 M, int32 Shift = 0) { return (EMCFace)FMath::Min<uint8>((M >> Shift) & 7, 5); }
	// Axis in 2 bits: 0 = Z (vertical), 1 = X, 2 = Y
	FORCEINLINE uint8 Axis(uint8 M) { return M & 3; }
	FORCEINLINE bool Bit(uint8 M, int32 B) { return ((M >> B) & 1) != 0; }
	FORCEINLINE uint8 SetBit(uint8 M, int32 B, bool V) { return V ? (M | (1 << B)) : (M & ~(1 << B)); }
	/** Quarter turns needed to rotate a model authored facing North into facing F. */
	FORCEINLINE int32 QuarterFromNorth(EMCFace F)
	{
		switch (F) { case EMCFace::North: return 0; case EMCFace::East: return 1; case EMCFace::South: return 2; case EMCFace::West: return 3; default: return 0; }
	}
}
