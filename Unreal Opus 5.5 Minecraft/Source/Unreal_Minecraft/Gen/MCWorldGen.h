// World generator interface (one per dimension). Generate() must be deterministic and thread safe.
#pragma once

#include "CoreMinimal.h"
#include "Core/MCCore.h"
#include "World/MCChunk.h"

class UNREAL_MINECRAFT_API FMCWorldGenerator
{
public:
	FMCWorldGenerator(uint64 InSeed, EMCDimension InDim) : Seed(InSeed), Dim(InDim) {}
	virtual ~FMCWorldGenerator() = default;

	uint64 Seed;
	EMCDimension Dim;

	/** Fill the chunk with terrain, features, structures, biomes and pending spawns. */
	virtual void Generate(FMCChunk& C) const = 0;
	/** Biome at a block position (deterministic, callable before the chunk exists). */
	virtual uint8 GetBiomeAt(int32 X, int32 Y, int32 Z) const = 0;
	/** Approximate surface height for spawn search / structure placement. */
	virtual int32 GetSurfaceHeight(int32 X, int32 Y) const { return 64; }
	/** Suggested world spawn (block coordinates of the standing position). */
	virtual FMCBlockPos FindSpawn() const { return FMCBlockPos(0, 0, 100); }
	/** Nearest structure start of Type from Origin within MaxChunks. */
	virtual bool LocateStructure(FName Type, const FMCBlockPos& Origin, int32 MaxChunks, FMCBlockPos& Out) const { return false; }
	/** Nearest biome by id within radius (blocks). */
	virtual bool LocateBiome(uint8 Biome, const FMCBlockPos& Origin, int32 Radius, FMCBlockPos& Out) const;
	virtual void GetStructureNames(TArray<FName>& Out) const {}
	/** Structures whose bounding box contains P (for mob spawning rules, e.g. fortress blazes). */
	virtual FName GetStructureAt(const FMCBlockPos& P) const { return NAME_None; }

	static TSharedPtr<FMCWorldGenerator> Create(EMCDimension Dim, uint64 Seed);
};
