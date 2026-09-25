// Overworld generator: multi-noise terrain, caves, aquifers, ores, biomes, features and structures.
#pragma once

#include "CoreMinimal.h"
#include "Gen/MCWorldGen.h"
#include "Gen/MCGenCommon.h"
#include "Core/MCNoise.h"

/** Terrain-only chunk used as input for features/structures of neighbouring chunks. */
struct FMCProtoChunk
{
	FMCChunkPos Pos;
	TArray<uint16> Blocks;       // 16*16*384, index = x + y*16 + (z-MinZ)*256
	int16 Surface[256];          // top solid (non-fluid) block z
	int16 WaterTop[256];         // top fluid block z (or MinZ-1)
	uint8 Biome[256];            // surface biome
	float Temp[256];
	float Humid[256];
	FORCEINLINE FMCState Get(int32 LX, int32 LY, int32 Z) const
	{
		if (Z < MC::MinZ || Z > MC::MaxZ) return 0;
		return Blocks[LX + LY * 16 + (Z - MC::MinZ) * 256];
	}
};

/** Climate sample for one column. */
struct FMCClimate
{
	float C = 0, E = 0, W = 0, PV = 0, T = 0, H = 0;
	float BaseHeight = 64.f;   // spline height
	float Scale = 8.f;         // 3D noise amplitude in blocks
	uint8 Biome = 0;
	bool bRiver = false;
};

class UNREAL_MINECRAFT_API FMCOverworldGen : public FMCWorldGenerator
{
public:
	FMCOverworldGen(uint64 InSeed);

	virtual void Generate(FMCChunk& C) const override;
	virtual uint8 GetBiomeAt(int32 X, int32 Y, int32 Z) const override;
	virtual int32 GetSurfaceHeight(int32 X, int32 Y) const override;
	virtual FMCBlockPos FindSpawn() const override;
	virtual bool LocateStructure(FName Type, const FMCBlockPos& Origin, int32 MaxChunks, FMCBlockPos& Out) const override;
	virtual void GetStructureNames(TArray<FName>& Out) const override;
	virtual FName GetStructureAt(const FMCBlockPos& P) const override;

	FMCClimate SampleClimate(double X, double Y) const;
	/** 3D terrain noise (world-aligned grid value). */
	double TerrainNoise3(int32 GX, int32 GY, int32 GZ) const;
	uint8 PickSurfaceBiome(const FMCClimate& Cl, int32 SurfaceZ) const;
	uint8 PickCaveBiome(int32 X, int32 Y, int32 Z, const FMCClimate& Cl) const;
	TSharedPtr<const FMCProtoChunk> GetProto(int32 CX, int32 CY) const;
	/** Surface z at a world column using proto chunks (exact). */
	int32 ProtoSurface(int32 X, int32 Y) const;

	/** Stronghold ring positions (block coords), deterministic. */
	const TArray<FMCBlockPos>& GetStrongholds() const { return Strongholds; }

private:
	FMCOctaveNoise NContinental, NErosion, NWeird, NTemp, NHumid, NShiftX, NShiftY, NRidge;
	FMCOctaveNoise NDetail, NCheese, NSpag1, NSpag2, NSpagWidth, NNoodle1, NNoodle2, NAquifer, NCaveBiome1, NCaveBiome2;
	FMCOctaveNoise NSurface, NBand, NClay, NIceberg, NPillar, NVein, NVeinGap, NSulfurBand, NRiver, NEntrance, NMushroom;
	TArray<FMCBlockPos> Strongholds;
	mutable FMCStructureCache StructCache;

	struct FProtoCache
	{
		FCriticalSection Lock;
		TMap<uint64, TSharedPtr<const FMCProtoChunk>> Map;
		TArray<uint64> Order;
	};
	TSharedPtr<FProtoCache> Protos;

	TSharedPtr<FMCProtoChunk> BuildProto(int32 CX, int32 CY) const;
	void PlaceOres(FMCGenWriter& W, const FMCProtoChunk& P, int32 OCX, int32 OCY) const;
	void PlaceFeatures(FMCGenWriter& W, int32 OCX, int32 OCY) const;
	void PlaceStructures(FMCGenWriter& W, FMCChunk& C) const;
	void PlaceCaveDecor(FMCGenWriter& W, const FMCProtoChunk& P) const;
	void SpawnInitialAnimals(FMCGenWriter& W, const FMCProtoChunk& P) const;
	TSharedPtr<const FMCStructureStart> GetStructureStart(FName Type, const FMCChunkPos& StartChunk) const;
	TSharedPtr<FMCStructureStart> CreateStructure(FName Type, const FMCChunkPos& StartChunk) const;
	void ComputeStrongholds();
	int32 SampleSurfaceEstimate(int32 X, int32 Y) const;

public:
	/** Structure placement sets (public for locate / debug UI). */
	TArray<FMCStructureSet> StructureSets;
};
