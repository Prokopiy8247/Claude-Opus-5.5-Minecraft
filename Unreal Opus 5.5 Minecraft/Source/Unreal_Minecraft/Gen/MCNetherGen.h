// Nether generator: cavernous 128-high terrain with a lava sea, five biomes, fortresses and bastions.
#pragma once

#include "CoreMinimal.h"
#include "Gen/MCWorldGen.h"
#include "Gen/MCGenCommon.h"
#include "Core/MCNoise.h"

class UNREAL_MINECRAFT_API FMCNetherGen : public FMCWorldGenerator
{
public:
	static constexpr int32 FloorZ = 0;
	static constexpr int32 RoofZ = 127;
	static constexpr int32 LavaLevel = 31;

	FMCNetherGen(uint64 InSeed);

	virtual void Generate(FMCChunk& C) const override;
	virtual uint8 GetBiomeAt(int32 X, int32 Y, int32 Z) const override;
	virtual int32 GetSurfaceHeight(int32 X, int32 Y) const override;
	virtual FMCBlockPos FindSpawn() const override { return FMCBlockPos(0, 0, 70); }
	virtual bool LocateStructure(FName Type, const FMCBlockPos& Origin, int32 MaxChunks, FMCBlockPos& Out) const override;
	virtual void GetStructureNames(TArray<FName>& Out) const override;
	virtual FName GetStructureAt(const FMCBlockPos& P) const override;

	double Density(double X, double Y, double Z) const;

private:
	FMCOctaveNoise NTerrain, NTerrain2, NTemp, NHumid, NSurface, NPatch, NDelta;
	TArray<FMCStructureSet> StructureSets;
	mutable FMCStructureCache StructCache;

	TSharedPtr<const FMCStructureStart> GetStructureStart(FName Type, const FMCChunkPos& SC) const;
	TSharedPtr<FMCStructureStart> CreateStructure(FName Type, const FMCChunkPos& SC) const;
	void PlaceStructures(FMCGenWriter& W, FMCChunk& C) const;
	void Decorate(FMCGenWriter& W, int32 OCX, int32 OCY) const;
};
