// A single dimension's voxel world: chunk storage, block updates, light, ticking, streaming, entities.
#pragma once

#include "CoreMinimal.h"
#include "Core/MCCore.h"
#include "Blocks/MCBlocks.h"
#include "World/MCChunk.h"
#include "Containers/Queue.h"
#include "Tasks/Task.h"

class FMCWorldGenerator;
class FMCBlockEntity;
class AMCGame;
class AMCEntity;
class AMCPlayer;
class AMCVoxelRenderer;
struct FMCItemStack;

enum EMCSetFlags : uint32
{
	MCSet_None = 0,
	MCSet_Neighbors = 1 << 0,   // send neighbour updates
	MCSet_Render = 1 << 1,      // remesh
	MCSet_Light = 1 << 2,       // update light
	MCSet_KeepEntity = 1 << 3,  // keep existing block entity
	MCSet_NoSupport = 1 << 4,   // skip support / placement side effects
	MCSet_Silent = 1 << 5,
	MCSet_Default = MCSet_Neighbors | MCSet_Render | MCSet_Light,
	MCSet_Gen = MCSet_Render | MCSet_Light
};

struct FMCRayHit
{
	bool bHit = false;
	FMCBlockPos Pos;
	EMCFace Face = EMCFace::Up;
	FMCState State = 0;
	FVector Point = FVector::ZeroVector;   // block units
	FVector HitFrac = FVector(0.5);        // hit point inside the block (0..1)
	double Distance = 0.0;                 // blocks
};

struct FMCScheduledTick
{
	FMCBlockPos Pos;
	FMCBlockId Block = 0;
	int64 DueTick = 0;
	int32 Priority = 0;
};

/** Result handed from generation workers to the game thread. */
struct FMCGenResult
{
	TSharedPtr<FMCChunk> Chunk;
};

class UNREAL_MINECRAFT_API FMCWorld : public FMCBlockGetter
{
public:
	FMCWorld(EMCDimension InDim, uint64 InSeed, AMCGame* InGame);
	virtual ~FMCWorld();

	EMCDimension Dim;
	uint64 Seed;
	AMCGame* Game = nullptr;
	TSharedPtr<FMCWorldGenerator> Generator;
	AMCVoxelRenderer* Renderer = nullptr;
	FString SaveDir;
	bool bActive = false;
	int64 GameTick = 0;
	FMCRandom Rand;

	TMap<FMCChunkPos, TSharedPtr<FMCChunk>> Chunks;

	// ---------------------------------------------------------------- block access
	FORCEINLINE FMCChunk* GetChunk(const FMCChunkPos& C) const
	{
		if (C == CachedChunkPos && CachedChunk) return CachedChunk;
		const TSharedPtr<FMCChunk>* Found = Chunks.Find(C);
		if (!Found) return nullptr;
		CachedChunkPos = C; CachedChunk = Found->Get();
		return CachedChunk;
	}
	FORCEINLINE FMCChunk* GetChunkAt(const FMCBlockPos& P) const { return GetChunk(FMCChunkPos::FromBlock(P)); }
	/** Chunk exists and has final light (usable for gameplay). */
	bool IsReady(const FMCChunkPos& C) const;
	bool IsReadyAt(const FMCBlockPos& P) const { return IsReady(FMCChunkPos::FromBlock(P)); }

	virtual FMCState GetState(const FMCBlockPos& P) const override
	{
		const FMCChunk* C = GetChunkAt(P);
		return C ? C->Get(P.X & 15, P.Y & 15, P.Z) : (P.Z < MC::MinZ ? 0 : 0);
	}
	FORCEINLINE FMCState GetState(int32 X, int32 Y, int32 Z) const { return GetState(FMCBlockPos(X, Y, Z)); }
	FORCEINLINE const FMCBlock& GetBlock(const FMCBlockPos& P) const { return FMCBlocks::GetByState(GetState(P)); }
	FORCEINLINE bool IsAir(const FMCBlockPos& P) const { return GetState(P) == 0; }

	/** Set a block with side effects. Returns false if nothing changed or the chunk is missing. */
	bool SetState(const FMCBlockPos& P, FMCState S, uint32 Flags = MCSet_Default);
	/** Player/explosion break: drops, sounds, particles, behaviour hooks. */
	bool DestroyBlock(const FMCBlockPos& P, bool bDrop, AMCEntity* Breaker = nullptr, const FMCItemStack* Tool = nullptr, bool bEffects = true);
	/** Place a block as if by a player (placement rules + OnPlaced). */
	bool PlaceBlock(const FMCBlockPos& P, FMCState S, AMCPlayer* Player);

	uint8 GetSkyLight(const FMCBlockPos& P) const;
	uint8 GetBlockLight(const FMCBlockPos& P) const;
	/** Combined light 0..15 given the current sky darkening (0 = full day). */
	int32 GetLight(const FMCBlockPos& P, int32 SkyDarken = -1) const;
	int32 GetHeight(int32 X, int32 Y) const;      // highest light blocking block Z (or MinZ-1)
	int32 GetTopSolidZ(int32 X, int32 Y) const;   // highest collidable / fluid block
	uint8 GetBiome(const FMCBlockPos& P) const;
	bool CanSeeSky(const FMCBlockPos& P) const { return P.Z > GetHeight(P.X, P.Y); }

	FMCBlockEntity* GetBlockEntity(const FMCBlockPos& P) const;
	TSharedPtr<FMCBlockEntity> GetBlockEntityShared(const FMCBlockPos& P) const;
	void SetBlockEntity(const FMCBlockPos& P, TSharedPtr<FMCBlockEntity> BE);
	void MarkModified(const FMCBlockPos& P);

	// ---------------------------------------------------------------- updates / ticking
	void NotifyNeighbors(const FMCBlockPos& P);
	void UpdateNeighbor(const FMCBlockPos& P, const FMCBlockPos& From);
	void ScheduleTick(const FMCBlockPos& P, FMCBlockId Block, int32 DelayTicks, int32 Priority = 0);
	bool HasScheduledTick(const FMCBlockPos& P, FMCBlockId Block) const;
	/** Advance one 20 Hz game tick. */
	void Tick(const FVector& PlayerPosBlocks);
	int32 RandomTickSpeed = 3;

	// ---------------------------------------------------------------- redstone
	int32 GetWeakPowerFrom(const FMCBlockPos& From, EMCFace Dir) const; // power emitted by From towards Dir
	int32 GetStrongPowerInto(const FMCBlockPos& P) const;               // strong power delivered to block P
	int32 GetInputPower(const FMCBlockPos& P, EMCFace Dir) const;        // power arriving at P from neighbour in Dir
	int32 GetBestNeighborPower(const FMCBlockPos& P) const;              // max input power from all sides
	bool IsPowered(const FMCBlockPos& P) const { return GetBestNeighborPower(P) > 0; }

	// ---------------------------------------------------------------- light
	void UpdateLightAt(const FMCBlockPos& P, FMCState OldS, FMCState NewS);
	void ComputeChunkLight(FMCChunk& C);         // local light (worker thread safe)
	void StitchChunkLight(FMCChunk& C);          // game thread: propagate across borders
	void MarkLightDirty(const FMCBlockPos& P);

	// ---------------------------------------------------------------- physics
	/** Collision boxes (world block units) overlapping Region. */
	void GetCollisionBoxes(const FMCBox& Region, TArray<FMCBox>& Out, bool bIncludeFluids = false) const;
	bool IsRegionFree(const FMCBox& Region) const;
	bool Raycast(const FVector& StartBlocks, const FVector& Dir, double MaxDist, FMCRayHit& Out, bool bFluids = false, bool bOutlineShapes = true) const;
	bool IsInFluid(const FMCBox& Box, FMCBlockId Fluid, double* OutSurfaceZ = nullptr) const;
	float GetFluidHeight(const FMCBlockPos& P) const; // 0..1 fill of the cell (0 if no fluid)

	// ---------------------------------------------------------------- entities
	TArray<AMCEntity*> Entities;
	void RegisterEntity(AMCEntity* E);
	void UnregisterEntity(AMCEntity* E);
	void GetEntitiesInBox(const FMCBox& Box, TArray<AMCEntity*>& Out, const AMCEntity* Except = nullptr) const;
	AMCEntity* SpawnMob(FName MobId, const FVector& PosBlocks, bool bNatural = false, int32 Variant = -1);
	void SpawnItem(const FVector& PosBlocks, const FMCItemStack& Stack, bool bRandomVelocity = true, float PickupDelay = 0.5f);
	void SpawnXP(const FVector& PosBlocks, int32 Amount);
	int32 CountMobs(bool bHostile) const;
	/** Serialise the entities standing in chunk C into C.SavedEntities (optionally removing them from the world). */
	void CaptureEntities(FMCChunk& C, bool bRemove);
	/** Re-create entities stored in C.SavedEntities. */
	void RestoreEntities(FMCChunk& C);

	// ---------------------------------------------------------------- explosions / effects
	void Explode(const FVector& CenterBlocks, float Power, bool bFire, bool bBreakBlocks, AActor* Source = nullptr);
	void PlaySound(FName Sound, const FVector& PosBlocks, float Volume = 1.f, float Pitch = 1.f) const;
	void PlayBlockSound(FMCState S, int32 Kind /*0 break 1 place 2 step 3 hit*/, const FVector& PosBlocks) const;
	void SpawnBlockBreakParticles(const FMCBlockPos& P, FMCState S) const;
	void SpawnParticles(FName Type, const FVector& PosBlocks, int32 Count, float Spread = 0.3f, const FVector& Vel = FVector::ZeroVector, FColor Color = FColor::White) const;

	// ---------------------------------------------------------------- streaming
	void UpdateStreaming(const FVector& CenterBlocks, int32 Radius, double TimeBudgetMs);
	void ForceLoadArea(const FMCChunkPos& Center, int32 Radius, double TimeoutSeconds = 30.0);
	bool IsAreaReady(const FMCChunkPos& Center, int32 Radius) const;
	void UnloadAll(bool bSave);
	int32 NumPendingGeneration() const { return InFlight.Num(); }
	void RemeshAll();
	void MarkChunkDirty(const FMCChunkPos& C, int32 Z = INT_MIN);
	void MarkBlockDirty(const FMCBlockPos& P);
	TArray<FMCChunkPos> TakeDirtyChunks();

	// ---------------------------------------------------------------- persistence
	void SaveChunk(FMCChunk& C);
	TSharedPtr<FMCChunk> LoadChunk(const FMCChunkPos& P);
	void SaveAllChunks();
	FString ChunkFilePath(const FMCChunkPos& P) const;

	// ---------------------------------------------------------------- misc queries
	FMCBlockPos FindSafeSpawn(int32 X, int32 Y) const;  // in loaded data
	int32 FindGroundZ(int32 X, int32 Y, int32 StartZ) const;
	TArray<FMCBlockPos> PortalPOIs;                     // known portal blocks (nether portal linking)
	void AddPortalPOI(const FMCBlockPos& P);

	// statistics
	int32 StatChunksGenerated = 0;
	double StatGenMs = 0.0;

	/** Ticking block entities (furnaces, hoppers...). */
	TArray<TWeakPtr<FMCBlockEntity>> TickingBlockEntities;
	/** Shared with worker tasks so they can outlive the world safely. */
	TSharedPtr<struct FMCWorldAsync> Async;
	void InvalidateChunkCache() const { CachedChunk = nullptr; CachedChunkPos = FMCChunkPos(INT_MAX, INT_MAX); }

private:
	mutable FMCChunkPos CachedChunkPos = FMCChunkPos(INT_MAX, INT_MAX);
	mutable FMCChunk* CachedChunk = nullptr;

	TArray<FMCScheduledTick> TickQueue;         // min-heap by due tick
	TSet<uint64> TickKeys;
	TSet<FMCChunkPos> DirtyChunks;
	TSet<FMCChunkPos> InFlight;
	TSet<FMCChunkPos> LightInFlight;
	int32 NeighborUpdateDepth = 0;
	FMCChunkPos LastStreamCenter = FMCChunkPos(INT_MAX, INT_MAX);
	TArray<FMCChunkPos> StreamOrder;           // cached spiral offsets

	void OnChunkArrived(TSharedPtr<FMCChunk> C);
	void RequestChunk(const FMCChunkPos& P);
	void RequestLight(const FMCChunkPos& P);
	void UnloadChunk(const FMCChunkPos& P, bool bSave);
	void TickRandomBlocks(const FVector& PlayerPosBlocks);
	void TickBlockEntities();
	void ProcessScheduledTicks();
	void ProcessPendingSpawns(FMCChunk& C);
	uint8 GetLightValue(const FMCBlockPos& P, bool bSky) const;
	void SetLightValue(const FMCBlockPos& P, bool bSky, uint8 V);
	void RelightChannel(const FMCBlockPos& P, bool bSky, uint8 NewEmission, uint8 OldOpacity, uint8 NewOpacity);
	static uint64 TickKey(const FMCBlockPos& P, FMCBlockId B);
};
